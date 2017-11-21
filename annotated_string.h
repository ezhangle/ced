// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <stdint.h>
#include "absl/strings/string_view.h"
#include "annotation.pb.h"
#include "avl.h"

union ID {
  ID() { id = 0; }
  ID(uint64_t i) { id = i; }
  ID(uint16_t s, uint64_t i) {
    site = s;
    clock = i;
  }
  uint64_t id;
  struct {
    uint64_t clock : (64 - 16);
    uint64_t site : 16;
  };
};

inline bool operator<(ID a, ID b) { return a.id < b.id; }
inline bool operator>(ID a, ID b) { return a.id > b.id; }
inline bool operator<=(ID a, ID b) { return a.id <= b.id; }
inline bool operator>=(ID a, ID b) { return a.id >= b.id; }
inline bool operator==(ID a, ID b) { return a.id == b.id; }
inline bool operator!=(ID a, ID b) { return a.id != b.id; }

class Site {
 public:
  Site() : id_(id_gen_.fetch_add(1, std::memory_order_relaxed)) {
    if (id_ == 0) abort();
  }

  Site(const Site&) = delete;
  Site& operator=(const Site&) = delete;

  ID GenerateID() {
    return ID(id_, clock_.fetch_add(1, std::memory_order_relaxed));
  }

  std::pair<ID, ID> GenerateIDBlock(uint64_t n) {
    uint64_t first = clock_.fetch_add(n, std::memory_order_relaxed);
    return std::make_pair(ID(id_, first), ID(id_, first + n));
  }

  uint16_t site_id() const { return id_; }

 private:
  Site(uint16_t id) : id_(id) {}
  const uint16_t id_;
  std::atomic<uint64_t> clock_{0};
  static std::atomic<uint16_t> id_gen_;
};

class AnnotatedString {
 public:
  AnnotatedString();

  static ID Begin() { return ID(0, 1); }
  static ID End() { return ID(0, 2); }

  static ID MakeRawInsert(CommandSet* commands, Site* site,
                          absl::string_view chars, ID after, ID before);

  ID MakeInsert(CommandSet* commands, Site* site, absl::string_view chars,
                ID after) const {
    return MakeRawInsert(commands, site, chars, after,
                         chars_.Lookup(after)->next);
  }

  static void MakeDelete(CommandSet* commands, ID id);
  static void MakeDelete(CommandSet* commands, ID beg, ID end);
  static void MakeDelDecl(CommandSet* commands, ID id);
  static void MakeDelMark(CommandSet* commands, ID id);
  static ID MakeDecl(CommandSet* commands, Site* site,
                     const Attribute& attribute);
  static ID MakeMark(CommandSet* commands, Site* site,
                     const Annotation& annotation);

  AnnotatedString Integrate(const CommandSet& commands) const;

  std::string Render() const;
  std::string Render(ID begin, ID end) const;

  bool SameContentIdentity(const AnnotatedString& other) const {
    return chars_.SameIdentity(other.chars_);
  }

  bool SameTotalIdentity(const AnnotatedString& other) const {
    return chars_.SameIdentity(other.chars_) &&
           attributes_by_type_.SameIdentity(other.attributes_by_type_) &&
           annotations_by_type_.SameIdentity(other.annotations_by_type_);
  }

  // F(ID annid, ID begin, ID end, const Attribute& attr)
  template <class F>
  void ForEachAnnotation(Attribute::DataCase type, F&& f) const {
    const auto* m = annotations_by_type_.Lookup(type);
    if (!m) return;
    const auto* am = attributes_by_type_.Lookup(type);
    assert(am);
    m->ForEach([f, am](ID id, const Annotation& ann) {
      const Attribute* attr = am->Lookup(ann.attribute());
      if (attr == nullptr) return;
      f(id, ann.begin(), ann.end(), *attr);
    });
  }

  // F(ID attrid, const Attribute& attr)
  template <class F>
  void ForEachAttribute(Attribute::DataCase type, F&& f) const {
    const auto* m = attributes_by_type_.Lookup(type);
    if (!m) return;
    m->ForEach(f);
  }

 private:
  void IntegrateInsert(ID id, const InsertCommand& cmd);
  void IntegrateDelChar(ID id);
  void IntegrateDecl(ID id, const Attribute& decl);
  void IntegrateDelDecl(ID id);
  void IntegrateMark(ID id, const Annotation& annotation);
  void IntegrateDelMark(ID id);

  void IntegrateInsertChar(ID id, char c, ID after, ID before);

  struct CharInfo {
    bool visible;
    char chr;
    // next/prev in document
    ID next;
    ID prev;
    // before/after in insert order (according to creator)
    ID after;
    ID before;
    // cache of which annotations are on this character
    AVL<ID> annotations;
  };

  struct LineBreak {
    ID prev;
    ID next;
  };

  AVL<ID, CharInfo> chars_;
  AVL<ID, LineBreak> line_breaks_;
  AVL<ID, Attribute::DataCase> attributes_;
  AVL<Attribute::DataCase, AVL<ID, Attribute>> attributes_by_type_;
  AVL<ID, Attribute::DataCase> annotations_;
  AVL<Attribute::DataCase, AVL<ID, Annotation>> annotations_by_type_;

 public:
  class AllIterator {
   public:
    AllIterator(const AnnotatedString& str, ID where)
        : str_(&str), pos_(where), cur_(str_->chars_.Lookup(pos_)) {}

    bool is_end() const { return pos_ == End(); }
    bool is_begin() const { return pos_ == Begin(); }

    ID id() const { return pos_; }
    char value() const { return cur_->chr; }
    bool is_visible() const { return cur_->visible; }

    void MoveNext() {
      pos_ = cur_->next;
      cur_ = str_->chars_.Lookup(pos_);
    }
    void MovePrev() {
      pos_ = cur_->prev;
      cur_ = str_->chars_.Lookup(pos_);
    }

   private:
    const AnnotatedString* str_;
    ID pos_;
    const CharInfo* cur_;
  };

  class Iterator {
   public:
    Iterator(const AnnotatedString& str, ID where) : it_(str, where) {
      while (!is_begin() && !it_.is_visible()) {
        it_.MovePrev();
      }
    }

    bool is_end() const { return it_.is_end(); }
    bool is_begin() const { return it_.is_begin(); }

    ID id() const { return it_.id(); }
    char value() const { return it_.value(); }

    void MoveNext() {
      if (!is_end()) it_.MoveNext();
      while (!is_end() && !it_.is_visible()) it_.MoveNext();
    }

    void MovePrev() {
      if (!is_begin()) it_.MovePrev();
      while (!is_begin() && !it_.is_visible()) it_.MovePrev();
    }

    Iterator Next() {
      Iterator i(*this);
      i.MoveNext();
      return i;
    }

    Iterator Prev() {
      Iterator i(*this);
      i.MovePrev();
      return i;
    }

   private:
    AllIterator it_;
  };

  class LineIterator {
   public:
    LineIterator(const AnnotatedString& str, ID where) : str_(&str) {
      Iterator it(str, where);
      const LineBreak* lb = str.line_breaks_.Lookup(it.id());
      while (lb == nullptr) {
        it.MovePrev();
        lb = str.line_breaks_.Lookup(it.id());
      }
      id_ = it.id();
    }

    bool is_end() const { return id_ == End(); }
    bool is_begin() const { return id_ == Begin(); }

    void MovePrev() {
      if (id_ == Begin()) return;
      id_ = str_->line_breaks_.Lookup(id_)->prev;
    }

    void MoveNext() {
      if (id_ == End()) return;
      id_ = str_->line_breaks_.Lookup(id_)->next;
    }

    LineIterator Next() {
      LineIterator tmp(*this);
      tmp.MoveNext();
      return tmp;
    }

    Iterator AsIterator() { return Iterator(*str_, id_); }
    AllIterator AsAllIterator() { return AllIterator(*str_, id_); }

    ID id() const { return id_; }

   private:
    const AnnotatedString* str_;
    ID id_;
  };
};

class AnnotationEditor {
 public:
  AnnotationEditor(Site* site) : site_(site) {}
  void BeginEdit(CommandSet* commands) { commands_ = commands; }
  void EndEdit();

  class ScopedEdit {
   public:
    ScopedEdit(AnnotationEditor* editor, CommandSet* commands)
        : editor_(editor) {
      editor_->BeginEdit(commands);
    }
    ~ScopedEdit() { editor_->EndEdit(); }

   private:
    AnnotationEditor* const editor_;
  };

  ID AttrID(const Attribute& attr);
  ID Mark(const Annotation& anno);
  ID Mark(ID beg, ID end, ID attr);
  ID Mark(ID beg, ID end, const Attribute& attr) {
    return Mark(beg, end, AttrID(attr));
  }

 private:
  typedef std::unordered_map<std::string, ID> ValToID;
  Site* const site_;
  CommandSet* commands_ = nullptr;
  ValToID last_attr2id_;
  ValToID new_attr2id_;
  ValToID last_ann2id_;
  ValToID new_ann2id_;
};