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

#include "buffer.h"
#include "re2/re2.h"

namespace {

class RegexHighlightCollaborator final : public SyncCollaborator {
 public:
  RegexHighlightCollaborator(
      const Buffer* buffer,
      const std::vector<std::pair<std::string, std::string>>& config)
      : SyncCollaborator("regex_highlight", absl::Seconds(0), absl::Seconds(0)),
        ed_(site()) {
    for (const auto& p : config) {
      regex_to_scope_.emplace_back(std::unique_ptr<RE2>(new RE2(p.first)),
                                   p.second);
    }
  }

  EditResponse Edit(const EditNotification& notification) {
    std::string text_str;
    std::vector<ID> markers;
    AnnotatedString::Iterator it(notification.content,
                                 AnnotatedString::Begin());
    it.MoveNext();
    while (!it.is_end()) {
      text_str += it.value();
      markers.push_back(it.id());
      it.MoveNext();
    }
    re2::StringPiece text(text_str);
    re2::StringPiece orig(text);

    EditResponse r;
    AnnotationEditor::ScopedEdit edit(&ed_, &r.content_updates);
    while (!text.empty()) {
      Log() << "SCAN c=" << (text.data() - orig.data())
            << " l=" << text.length();
      for (const auto& p : regex_to_scope_) {
        auto bef = text;
        bool hit = RE2::Consume(&text, *p.first);
        bool moved = text.data() != bef.data();
        Log() << " " << hit << "/" << moved;
        if (hit && moved) {
          Attribute t;
          t.mutable_tags()->add_tags(p.second);
          ed_.Mark(markers[bef.data() - orig.data()],
                   markers[text.data() - orig.data()], t);
          goto next;
        }
      }
      text.remove_prefix(1);
    next:;
    }

    return r;
  }

 private:
  AnnotationEditor ed_;
  std::vector<std::pair<std::unique_ptr<RE2>, std::string>> regex_to_scope_;
};

class Register {
 public:
  Register Type(std::vector<std::string> ext,
                std::vector<std::pair<std::string, std::string>> args) {
    Buffer::RegisterCollaborator([=](Buffer* buffer) {
      for (const auto& e : ext) {
        if (buffer->filename().extension() == e) {
          buffer->MakeCollaborator<RegexHighlightCollaborator>(args);
          return;
        }
      }
    });
    return *this;
  }
};

Register registerer = Register().Type(
    {".asm", ".s"},
    {{"(?i)(\\s*)%?\\b(?:"
      "ax|ah|al|eax|rax|bx|bh|bl|ebx|rbx|cx|ch|cl|ecx|rcx|dx|dh|dl|edx|rdx|si|"
      "esi|rsi|di|edi|rdi|sp|esp|rsp|bp|ebp|rbp|ip|eip|rip|"
      "r(?:[8-9]|(?:1[0-5]))d?|"
      "cs|ds|ss|es|fs|gs|"
      "mm[0-7]+|"
      "[xy]mm[0-9]+"
      ")\\b",
      "variable.registers.x86.assembly"},
     {"(?i)(\\s*)\\b(?:aaa|aad|aam|aas|adc|adcb|adcl|adcq|adcw|add|addb|"
      "addl|addq|addw|and|andb|andl|andn|andnl|andnq|andq|andw|arpl|arplw|"
      "bextr|"
      "bextrl|bextrq|blcfill|blcfilll|blcfillq|blci|blcic|blcicl|blcicq|blcil|"
      "blciq|blcmsk|blcmskl|blcmskq|blcs|blcsl|blcsq|blsfill|blsfilll|blsfillq|"
      "blsi|blsic|blsicl|blsicq|blsil|blsiq|blsmsk|blsmskl|blsmskq|blsr|blsrl|"
      "blsrq|bound|boundl|boundw|bsf|bsfl|bsfq|bsfw|bsr|bsrl|bsrq|bsrw|bswap|"
      "bswapl|bswapq|bt|btc|btcl|btcq|btcw|btl|btq|btr|btrl|btrq|btrw|bts|btsl|"
      "btsq|btsw|btw|bzhi|bzhil|bzhiq|call|calll|callld|callq|callw|cbtw|cbw|"
      "cdq|cdqe|clc|cld|clflush|clgi|cli|clr|clrb|clrl|clrq|clrw|cltd|cltq|"
      "clts|cmc|cmova|cmovae|cmovael|cmovaeq|cmovaew|cmoval|cmovaq|cmovaw|"
      "cmovb|cmovbe|cmovbel|cmovbeq|cmovbew|cmovbl|cmovbq|cmovbw|cmovc|cmovcl|"
      "cmovcq|cmovcw|cmove|cmovel|cmoveq|cmovew|cmovg|cmovge|cmovgel|cmovgeq|"
      "cmovgew|cmovgl|cmovgq|cmovgw|cmovl|cmovle|cmovlel|cmovleq|cmovlew|"
      "cmovll|cmovlq|cmovlw|cmovna|cmovnae|cmovnael|cmovnaeq|cmovnaew|cmovnal|"
      "cmovnaq|cmovnaw|cmovnb|cmovnbe|cmovnbel|cmovnbeq|cmovnbew|cmovnbl|"
      "cmovnbq|cmovnbw|cmovnc|cmovncl|cmovncq|cmovncw|cmovne|cmovnel|cmovneq|"
      "cmovnew|cmovng|cmovnge|cmovngel|cmovngeq|cmovngew|cmovngl|cmovngq|"
      "cmovngw|cmovnl|cmovnle|cmovnlel|cmovnleq|cmovnlew|cmovnll|cmovnlq|"
      "cmovnlw|cmovno|cmovnol|cmovnoq|cmovnow|cmovnp|cmovnpl|cmovnpq|cmovnpw|"
      "cmovns|cmovnsl|cmovnsq|cmovnsw|cmovnz|cmovnzl|cmovnzq|cmovnzw|cmovo|"
      "cmovol|cmovoq|cmovow|cmovp|cmovpe|cmovpel|cmovpeq|cmovpew|cmovpl|cmovpo|"
      "cmovpol|cmovpoq|cmovpow|cmovpq|cmovpw|cmovs|cmovsl|cmovsq|cmovsw|cmovz|"
      "cmovzl|cmovzq|cmovzw|cmp|cmpb|cmpl|cmpq|cmps|cmpsb|cmpsd|cmpsl|cmpsq|"
      "cmpsw|cmpw|cmpxchg|cmpxchg16b|cmpxchg8b|cmpxchg8bq|cmpxchgb|cmpxchgl|"
      "cmpxchgq|cmpxchgw|cpuid|cqo|cqto|crc32|crc32b|crc32l|crc32q|crc32w|cwd|"
      "cwde|cwtd|cwtl|daa|das|dec|decb|decl|decq|decw|div|divb|divl|divq|divw|"
      "emms|enter|enterl|enterq|enterw|fcmova|fcmovae|fcmovb|fcmovbe|fcmove|"
      "fcmovna|fcmovnae|fcmovnb|fcmovnbe|fcmovne|fcmovnu|fcmovu|fcomi|fcomip|"
      "fcompi|fcos|fdisi|femms|feni|ffreep|fisttp|fisttpl|fisttpll|fisttpq|"
      "fisttps|fndisi|fneni|fnsetpm|fnstsw|fprem1|frstpm|fsetpm|fsin|fsincos|"
      "fstsw|fucom|fucomi|fucomip|fucomp|fucompi|fucompp|fxrstor|fxrstor64|"
      "fxrstor64q|fxrstorq|fxsave|fxsave64|fxsave64q|fxsaveq|getsec|hlt|idiv|"
      "idivb|idivl|idivq|idivw|imul|imulb|imull|imulq|imulw|in|inb|inc|incb|"
      "incl|incq|incw|inl|ins|insb|insl|insw|int|int3|into|invd|invept|invlpg|"
      "invlpga|invpcid|invvpid|inw|iret|iretl|iretq|iretw|ja|jae|jb|jbe|jc|"
      "jcxz|je|jecxz|jg|jge|jl|jle|jmp|jmpl|jmpld|jmpq|jmpw|jna|jnae|jnb|jnbe|"
      "jnc|jne|jng|jnge|jnl|jnle|jno|jnp|jns|jnz|jo|jp|jpe|jpo|jrcxz|js|jz|"
      "lahf|lar|larl|larq|larw|lcall|lcalll|lcallw|ldmxcsr|lds|ldsl|ldsw|lea|"
      "leal|leaq|leave|leavel|leaveq|leavew|leaw|les|lesl|lesw|lfence|lfs|lfsl|"
      "lfsw|lgdt|lgdtl|lgdtq|lgdtw|lgs|lgsl|lgsw|lidt|lidtl|lidtq|lidtw|ljmp|"
      "ljmpl|ljmpw|lldt|lldtw|llwpcb|lmsw|lmsww|lods|lodsb|lodsl|lodsq|lodsw|"
      "loop|loope|loopel|loopeq|loopew|loopl|loopne|loopnel|loopneq|loopnew|"
      "loopnz|loopnzl|loopnzq|loopnzw|loopq|loopw|loopz|loopzl|loopzq|loopzw|"
      "lret|lretl|lretq|lretw|lsl|lsll|lslq|lslw|lss|lssl|lssw|ltr|ltrw|lwpins|"
      "lwpval|lzcnt|lzcntl|lzcntq|lzcntw|mfence|monitor|montmul|mov|movabs|"
      "movabsb|movabsl|movabsq|movabsw|movb|movbe|movbel|movbeq|movbew|movl|"
      "movnti|movntil|movntiq|movq|movs|movsb|movsbl|movsbq|movsbw|movsd|movsl|"
      "movslq|movsq|movsw|movswl|movswq|movsx|movsxb|movsxd|movsxdl|movsxl|"
      "movsxw|movw|movzb|movzbl|movzbq|movzbw|movzwl|movzwq|movzx|movzxb|"
      "movzxw|mul|mulb|mull|mulq|mulw|mulx|mulxl|mulxq|mwait|neg|negb|negl|"
      "negq|negw|nop|nopl|nopq|nopw|not|notb|notl|notq|notw|or|orb|orl|orq|orw|"
      "out|outb|outl|outs|outsb|outsl|outsw|outw|pause|pdep|pdepl|pdepq|pext|"
      "pextl|pextq|pop|popa|popal|popaw|popcnt|popcntl|popcntq|popcntw|popf|"
      "popfl|popfq|popfw|popl|popq|popw|prefetch|prefetchnta|prefetcht0|"
      "prefetcht1|prefetcht2|prefetchw|push|pusha|pushal|pushaw|pushf|pushfl|"
      "pushfq|pushfw|pushl|pushq|pushw|rcl|rclb|rcll|rclq|rclw|rcr|rcrb|rcrl|"
      "rcrq|rcrw|rdfsbase|rdgsbase|rdmsr|rdpmc|rdrand|rdtsc|rdtscp|ret|retf|"
      "retfl|retfq|retfw|retl|retq|retw|rol|rolb|roll|rolq|rolw|ror|rorb|rorl|"
      "rorq|rorw|rorx|rorxl|rorxq|rsm|sahf|sal|salb|sall|salq|salw|sar|sarb|"
      "sarl|sarq|sarw|sarx|sarxl|sarxq|sbb|sbbb|sbbl|sbbq|sbbw|scas|scasb|"
      "scasl|scasq|scasw|scmp|scmpb|scmpl|scmpq|scmpw|seta|setab|setae|setaeb|"
      "setb|setbb|setbe|setbeb|setc|setcb|sete|seteb|setg|setgb|setge|setgeb|"
      "setl|setlb|setle|setleb|setna|setnab|setnae|setnaeb|setnb|setnbb|setnbe|"
      "setnbeb|setnc|setncb|setne|setneb|setng|setngb|setnge|setngeb|setnl|"
      "setnlb|setnle|setnleb|setno|setnob|setnp|setnpb|setns|setnsb|setnz|"
      "setnzb|seto|setob|setp|setpb|setpe|setpeb|setpo|setpob|sets|setsb|setz|"
      "setzb|sfence|sgdt|sgdtl|sgdtq|sgdtw|shl|shlb|shld|shldl|shldq|shldw|"
      "shll|shlq|shlw|shlx|shlxl|shlxq|shr|shrb|shrd|shrdl|shrdq|shrdw|shrl|"
      "shrq|shrw|shrx|shrxl|shrxq|sidt|sidtl|sidtq|sidtw|skinit|sldt|sldtl|"
      "sldtq|sldtw|slod|slodb|slodl|slodq|slodw|slwpcb|smov|smovb|smovl|smovq|"
      "smovw|smsw|smswl|smswq|smsww|ssca|sscab|sscal|sscaq|sscaw|ssto|sstob|"
      "sstol|sstoq|sstow|stc|std|stgi|sti|stmxcsr|stos|stosb|stosl|stosq|stosw|"
      "str|strl|strq|strw|sub|subb|subl|subq|subw|swapgs|syscall|sysenter|"
      "sysexit|sysret|sysretl|sysretq|t1mskc|t1mskcl|t1mskcq|test|testb|testl|"
      "testq|testw|tzcnt|tzcntl|tzcntq|tzcntw|tzmsk|tzmskl|tzmskq|ud1|ud2|ud2a|"
      "ud2b|verr|verrw|verw|verww|vldmxcsr|vmcall|vmclear|vmfunc|vmlaunch|"
      "vmload|vmmcall|vmptrld|vmptrst|vmread|vmreadl|vmreadq|vmresume|vmrun|"
      "vmsave|vmwrite|vmwritel|vmwriteq|vmxoff|vmxon|vstmxcsr|vzeroall|"
      "vzeroupper|wbinvd|wrfsbase|wrgsbase|wrmsr|xabort|xadd|xaddb|xaddl|xaddq|"
      "xaddw|xbegin|xchg|xchgb|xchgl|xchgq|xchgw|xcrypt-cbc|xcrypt-cfb|xcrypt-"
      "ctr|xcrypt-ecb|xcrypt-ofb|xcryptcbc|xcryptcfb|xcryptctr|xcryptecb|"
      "xcryptofb|xend|xgetbv|xlat|xlatb|xor|xorb|xorl|xorq|xorw|xrstor|"
      "xrstor64|xrstor64q|xrstorq|xsave|xsave64|xsave64q|xsaveopt|xsaveopt64|"
      "xsaveopt64q|xsaveoptq|xsaveq|xsetbv|xsha1|xsha256|xstore|xstore-rng|"
      "xstorerng|xtest)\\b",
      "keyword.x86.assembly"},
     {"(?i)(\\s*)\\b(?:addpd|addps|addsd|addss|addsubpd|addsubps|aesdec|"
      "aesdeclast|aesenc|aesenclast|aesimc|aeskeygenassist|andnpd|andnps|andpd|"
      "andps|blendpd|blendps|blendvpd|blendvps|cmpeqpd|cmpeqps|cmpeqsd|cmpeqss|"
      "cmplepd|cmpleps|cmplesd|cmpless|cmpltpd|cmpltps|cmpltsd|cmpltss|"
      "cmpneqpd|cmpneqps|cmpneqsd|cmpneqss|cmpnlepd|cmpnleps|cmpnlesd|cmpnless|"
      "cmpnltpd|cmpnltps|cmpnltsd|cmpnltss|cmpordpd|cmpordps|cmpordsd|cmpordss|"
      "cmppd|cmpps|cmpsd|cmpss|cmpunordpd|cmpunordps|cmpunordsd|cmpunordss|"
      "comisd|comiss|cvtdq2pd|cvtdq2ps|cvtpd2dq|cvtpd2pi|cvtpd2ps|cvtpi2pd|"
      "cvtpi2ps|cvtps2dq|cvtps2pd|cvtps2pi|cvtsd2si|cvtsd2sil|cvtsd2siq|"
      "cvtsd2ss|cvtsi2sd|cvtsi2sdl|cvtsi2sdq|cvtsi2ss|cvtsi2ssl|cvtsi2ssq|"
      "cvtss2sd|cvtss2si|cvtss2sil|cvtss2siq|cvttpd2dq|cvttpd2pi|cvttps2dq|"
      "cvttps2pi|cvttsd2si|cvttsd2sil|cvttsd2siq|cvttss2si|cvttss2sil|"
      "cvttss2siq|divpd|divps|divsd|divss|dppd|dpps|extractps|extrq|f2xm1|fabs|"
      "fadd|faddl|faddp|fadds|fbld|fbldld|fbstp|fbstpld|fchs|fclex|fcom|fcoml|"
      "fcomp|fcompl|fcompp|fcomps|fcoms|fdecstp|fdiv|fdivl|fdivp|fdivr|fdivrl|"
      "fdivrp|fdivrs|fdivs|ffree|fiadd|fiaddl|fiadds|ficom|ficoml|ficomp|"
      "ficompl|ficomps|ficoms|fidiv|fidivl|fidivr|fidivrl|fidivrs|fidivs|fild|"
      "fildl|fildll|fildq|filds|fimul|fimull|fimuls|fincstp|finit|fist|fistl|"
      "fistp|fistpl|fistpll|fistpq|fistps|fists|fisub|fisubl|fisubr|fisubrl|"
      "fisubrs|fisubs|fld|fld1|fldcw|fldcww|fldenv|fldenvl|fldenvs|fldl|fldl2e|"
      "fldl2t|fldld|fldlg2|fldln2|fldpi|flds|fldt|fldz|fmul|fmull|fmulp|fmuls|"
      "fnclex|fninit|fnop|fnsave|fnsavel|fnsaves|fnstcw|fnstcww|fnstenv|"
      "fnstenvl|fnstenvs|fnstsw|fnstsww|fpatan|fprem|fptan|frndint|frstor|"
      "frstorl|frstors|fsave|fsavel|fsaves|fscale|fsqrt|fst|fstcw|fstcww|"
      "fstenv|fstenvl|fstenvs|fstl|fstp|fstpl|fstpld|fstps|fstpt|fsts|fstsw|"
      "fstsww|fsub|fsubl|fsubp|fsubr|fsubrl|fsubrp|fsubrs|fsubs|ftst|fwait|"
      "fxam|fxch|fxtract|fyl2x|fyl2xp1|haddpd|haddps|hsubpd|hsubps|insertps|"
      "insertq|lddqu|maskmovdqu|maskmovq|maxpd|maxps|maxsd|maxss|minpd|minps|"
      "minsd|minss|movapd|movaps|movd|movddup|movdq2q|movdqa|movdqu|movhlps|"
      "movhpd|movhps|movlhps|movlpd|movlps|movmskpd|movmskpdl|movmskpdq|"
      "movmskps|movmskpsl|movmskpsq|movntdq|movntdqa|movntpd|movntps|movntq|"
      "movntsd|movntss|movq|movq2dq|movsd|movshdup|movsldup|movss|movupd|"
      "movups|mpsadbw|mulpd|mulps|mulsd|mulss|orpd|orps|pabsb|pabsd|pabsw|"
      "packssdw|packsswb|packusdw|packuswb|paddb|paddd|paddq|paddsb|paddsw|"
      "paddusb|paddusw|paddw|palignr|pand|pandn|pavgb|pavgusb|pavgw|pblendvb|"
      "pblendw|pclmulhqhqdq|pclmulhqlqdq|pclmullqhqdq|pclmullqlqdq|pclmulqdq|"
      "pcmpeqb|pcmpeqd|pcmpeqq|pcmpeqw|pcmpestri|pcmpestrm|pcmpgtb|pcmpgtd|"
      "pcmpgtq|pcmpgtw|pcmpistri|pcmpistrm|pextrb|pextrd|pextrq|pextrw|pextrwl|"
      "pextrwq|pf2id|pf2iw|pfacc|pfadd|pfcmpeq|pfcmpge|pfcmpgt|pfmax|pfmin|"
      "pfmul|pfnacc|pfpnacc|pfrcp|pfrcpit1|pfrcpit2|pfrsqit1|pfrsqrt|pfsub|"
      "pfsubr|phaddd|phaddsw|phaddw|phminposuw|phsubd|phsubsw|phsubw|pi2fd|"
      "pi2fw|pinsrb|pinsrd|pinsrq|pinsrw|pinsrwl|pinsrwq|pmaddubsw|pmaddwd|"
      "pmaxsb|pmaxsd|pmaxsw|pmaxub|pmaxud|pmaxuw|pminsb|pminsd|pminsw|pminub|"
      "pminud|pminuw|pmovmskb|pmovmskbl|pmovmskbq|pmovsxbd|pmovsxbq|pmovsxbw|"
      "pmovsxdq|pmovsxwd|pmovsxwq|pmovzxbd|pmovzxbq|pmovzxbw|pmovzxdq|pmovzxwd|"
      "pmovzxwq|pmuldq|pmulhrsw|pmulhrw|pmulhuw|pmulhw|pmulld|pmullw|pmuludq|"
      "por|psadbw|pshufb|pshufd|pshufhw|pshuflw|pshufw|psignb|psignd|psignw|"
      "pslld|pslldq|psllq|psllw|psrad|psraw|psrld|psrldq|psrlq|psrlw|psubb|"
      "psubd|psubq|psubsb|psubsw|psubusb|psubusw|psubw|pswapd|ptest|punpckhbw|"
      "punpckhdq|punpckhqdq|punpckhwd|punpcklbw|punpckldq|punpcklqdq|punpcklwd|"
      "pxor|rcpps|rcpss|roundpd|roundps|roundsd|roundss|rsqrtps|rsqrtss|shufpd|"
      "shufps|sqrtpd|sqrtps|sqrtsd|sqrtss|subpd|subps|subsd|subss|ucomisd|"
      "ucomiss|unpckhpd|unpckhps|unpcklpd|unpcklps|vaddpd|vaddps|vaddsd|vaddss|"
      "vaddsubpd|vaddsubps|vaesdec|vaesdeclast|vaesenc|vaesenclast|vaesimc|"
      "vaeskeygenassist|vandnpd|vandnps|vandpd|vandps|vblendpd|vblendps|"
      "vblendvpd|vblendvps|vbroadcastf128|vbroadcasti128|vbroadcastsd|"
      "vbroadcastss|vcmpeq_ospd|vcmpeq_osps|vcmpeq_ossd|vcmpeq_osss|vcmpeq_"
      "uqpd|vcmpeq_uqps|vcmpeq_uqsd|vcmpeq_uqss|vcmpeq_uspd|vcmpeq_usps|vcmpeq_"
      "ussd|vcmpeq_usss|vcmpeqpd|vcmpeqps|vcmpeqsd|vcmpeqss|vcmpfalse_ospd|"
      "vcmpfalse_osps|vcmpfalse_ossd|vcmpfalse_osss|vcmpfalsepd|vcmpfalseps|"
      "vcmpfalsesd|vcmpfalsess|vcmpge_oqpd|vcmpge_oqps|vcmpge_oqsd|vcmpge_oqss|"
      "vcmpgepd|vcmpgeps|vcmpgesd|vcmpgess|vcmpgt_oqpd|vcmpgt_oqps|vcmpgt_oqsd|"
      "vcmpgt_oqss|vcmpgtpd|vcmpgtps|vcmpgtsd|vcmpgtss|vcmple_oqpd|vcmple_oqps|"
      "vcmple_oqsd|vcmple_oqss|vcmplepd|vcmpleps|vcmplesd|vcmpless|vcmplt_oqpd|"
      "vcmplt_oqps|vcmplt_oqsd|vcmplt_oqss|vcmpltpd|vcmpltps|vcmpltsd|vcmpltss|"
      "vcmpneq_oqpd|vcmpneq_oqps|vcmpneq_oqsd|vcmpneq_oqss|vcmpneq_ospd|"
      "vcmpneq_osps|vcmpneq_ossd|vcmpneq_osss|vcmpneq_uspd|vcmpneq_usps|"
      "vcmpneq_ussd|vcmpneq_usss|vcmpneqpd|vcmpneqps|vcmpneqsd|vcmpneqss|"
      "vcmpnge_uqpd|vcmpnge_uqps|vcmpnge_uqsd|vcmpnge_uqss|vcmpngepd|vcmpngeps|"
      "vcmpngesd|vcmpngess|vcmpngt_uqpd|vcmpngt_uqps|vcmpngt_uqsd|vcmpngt_uqss|"
      "vcmpngtpd|vcmpngtps|vcmpngtsd|vcmpngtss|vcmpnle_uqpd|vcmpnle_uqps|"
      "vcmpnle_uqsd|vcmpnle_uqss|vcmpnlepd|vcmpnleps|vcmpnlesd|vcmpnless|"
      "vcmpnlt_uqpd|vcmpnlt_uqps|vcmpnlt_uqsd|vcmpnlt_uqss|vcmpnltpd|vcmpnltps|"
      "vcmpnltsd|vcmpnltss|vcmpord_spd|vcmpord_sps|vcmpord_ssd|vcmpord_sss|"
      "vcmpordpd|vcmpordps|vcmpordsd|vcmpordss|vcmppd|vcmpps|vcmpsd|vcmpss|"
      "vcmptrue_uspd|vcmptrue_usps|vcmptrue_ussd|vcmptrue_usss|vcmptruepd|"
      "vcmptrueps|vcmptruesd|vcmptruess|vcmpunord_spd|vcmpunord_sps|vcmpunord_"
      "ssd|vcmpunord_sss|vcmpunordpd|vcmpunordps|vcmpunordsd|vcmpunordss|"
      "vcomisd|vcomiss|vcvtdq2pd|vcvtdq2ps|vcvtpd2dq|vcvtpd2dqx|vcvtpd2dqy|"
      "vcvtpd2ps|vcvtpd2psx|vcvtpd2psy|vcvtph2ps|vcvtps2dq|vcvtps2pd|vcvtps2ph|"
      "vcvtsd2si|vcvtsd2sil|vcvtsd2siq|vcvtsd2ss|vcvtsi2sd|vcvtsi2sdl|"
      "vcvtsi2sdq|vcvtsi2ss|vcvtsi2ssl|vcvtsi2ssq|vcvtss2sd|vcvtss2si|"
      "vcvtss2sil|vcvtss2siq|vcvttpd2dq|vcvttpd2dqx|vcvttpd2dqy|vcvttps2dq|"
      "vcvttsd2si|vcvttsd2sil|vcvttsd2siq|vcvttss2si|vcvttss2sil|vcvttss2siq|"
      "vdivpd|vdivps|vdivsd|vdivss|vdppd|vdpps|vextractf128|vextracti128|"
      "vextractps|vfmadd132pd|vfmadd132ps|vfmadd132sd|vfmadd132ss|vfmadd213pd|"
      "vfmadd213ps|vfmadd213sd|vfmadd213ss|vfmadd231pd|vfmadd231ps|vfmadd231sd|"
      "vfmadd231ss|vfmaddpd|vfmaddps|vfmaddsd|vfmaddss|vfmaddsub132pd|"
      "vfmaddsub132ps|vfmaddsub213pd|vfmaddsub213ps|vfmaddsub231pd|"
      "vfmaddsub231ps|vfmaddsubpd|vfmaddsubps|vfmsub132pd|vfmsub132ps|"
      "vfmsub132sd|vfmsub132ss|vfmsub213pd|vfmsub213ps|vfmsub213sd|vfmsub213ss|"
      "vfmsub231pd|vfmsub231ps|vfmsub231sd|vfmsub231ss|vfmsubadd132pd|"
      "vfmsubadd132ps|vfmsubadd213pd|vfmsubadd213ps|vfmsubadd231pd|"
      "vfmsubadd231ps|vfmsubaddpd|vfmsubaddps|vfmsubpd|vfmsubps|vfmsubsd|"
      "vfmsubss|vfnmadd132pd|vfnmadd132ps|vfnmadd132sd|vfnmadd132ss|"
      "vfnmadd213pd|vfnmadd213ps|vfnmadd213sd|vfnmadd213ss|vfnmadd231pd|"
      "vfnmadd231ps|vfnmadd231sd|vfnmadd231ss|vfnmaddpd|vfnmaddps|vfnmaddsd|"
      "vfnmaddss|vfnmsub132pd|vfnmsub132ps|vfnmsub132sd|vfnmsub132ss|"
      "vfnmsub213pd|vfnmsub213ps|vfnmsub213sd|vfnmsub213ss|vfnmsub231pd|"
      "vfnmsub231ps|vfnmsub231sd|vfnmsub231ss|vfnmsubpd|vfnmsubps|vfnmsubsd|"
      "vfnmsubss|vfrczpd|vfrczps|vfrczsd|vfrczss|vgatherdpd|vgatherdps|"
      "vgatherqpd|vgatherqps|vhaddpd|vhaddps|vhsubpd|vhsubps|vinsertf128|"
      "vinserti128|vinsertps|vlddqu|vmaskmovdqu|vmaskmovpd|vmaskmovps|vmaxpd|"
      "vmaxps|vmaxsd|vmaxss|vminpd|vminps|vminsd|vminss|vmovapd|vmovaps|vmovd|"
      "vmovddup|vmovdqa|vmovdqu|vmovhlps|vmovhpd|vmovhps|vmovlhps|vmovlpd|"
      "vmovlps|vmovmskpd|vmovmskpdl|vmovmskpdq|vmovmskps|vmovmskpsl|vmovmskpsq|"
      "vmovntdq|vmovntdqa|vmovntpd|vmovntps|vmovq|vmovsd|vmovshdup|vmovsldup|"
      "vmovss|vmovupd|vmovups|vmpsadbw|vmulpd|vmulps|vmulsd|vmulss|vorpd|vorps|"
      "vpabsb|vpabsd|vpabsw|vpackssdw|vpacksswb|vpackusdw|vpackuswb|vpaddb|"
      "vpaddd|vpaddq|vpaddsb|vpaddsw|vpaddusb|vpaddusw|vpaddw|vpalignr|vpand|"
      "vpandn|vpavgb|vpavgw|vpblendd|vpblendvb|vpblendw|vpbroadcastb|"
      "vpbroadcastd|vpbroadcastq|vpbroadcastw|vpclmulhqhqdq|vpclmulhqlqdq|"
      "vpclmullqhqdq|vpclmullqlqdq|vpclmulqdq|vpcmov|vpcmpeqb|vpcmpeqd|"
      "vpcmpeqq|vpcmpeqw|vpcmpestri|vpcmpestrm|vpcmpgtb|vpcmpgtd|vpcmpgtq|"
      "vpcmpgtw|vpcmpistri|vpcmpistrm|vpcomb|vpcomd|vpcomeqb|vpcomeqd|vpcomeqq|"
      "vpcomequb|vpcomequd|vpcomequq|vpcomequw|vpcomeqw|vpcomfalseb|"
      "vpcomfalsed|vpcomfalseq|vpcomfalseub|vpcomfalseud|vpcomfalseuq|"
      "vpcomfalseuw|vpcomfalsew|vpcomgeb|vpcomged|vpcomgeq|vpcomgeub|vpcomgeud|"
      "vpcomgeuq|vpcomgeuw|vpcomgew|vpcomgtb|vpcomgtd|vpcomgtq|vpcomgtub|"
      "vpcomgtud|vpcomgtuq|vpcomgtuw|vpcomgtw|vpcomleb|vpcomled|vpcomleq|"
      "vpcomleub|vpcomleud|vpcomleuq|vpcomleuw|vpcomlew|vpcomltb|vpcomltd|"
      "vpcomltq|vpcomltub|vpcomltud|vpcomltuq|vpcomltuw|vpcomltw|vpcomneqb|"
      "vpcomneqd|vpcomneqq|vpcomnequb|vpcomnequd|vpcomnequq|vpcomnequw|"
      "vpcomneqw|vpcomq|vpcomtrueb|vpcomtrued|vpcomtrueq|vpcomtrueub|"
      "vpcomtrueud|vpcomtrueuq|vpcomtrueuw|vpcomtruew|vpcomub|vpcomud|vpcomuq|"
      "vpcomuw|vpcomw|vperm2f128|vperm2i128|vpermd|vpermil2pd|vpermil2ps|"
      "vpermilpd|vpermilps|vpermpd|vpermps|vpermq|vpextrb|vpextrd|vpextrq|"
      "vpextrw|vpextrwl|vpextrwq|vpgatherdd|vpgatherdq|vpgatherqd|vpgatherqq|"
      "vphaddbd|vphaddbq|vphaddbw|vphaddd|vphadddq|vphaddsw|vphaddubd|"
      "vphaddubq|vphaddubw|vphaddudq|vphadduwd|vphadduwq|vphaddw|vphaddwd|"
      "vphaddwq|vphminposuw|vphsubbw|vphsubd|vphsubdq|vphsubsw|vphsubw|"
      "vphsubwd|vpinsrb|vpinsrd|vpinsrq|vpinsrw|vpinsrwl|vpinsrwq|vpmacsdd|"
      "vpmacsdqh|vpmacsdql|vpmacssdd|vpmacssdqh|vpmacssdql|vpmacsswd|vpmacssww|"
      "vpmacswd|vpmacsww|vpmadcsswd|vpmadcswd|vpmaddubsw|vpmaddwd|vpmaskmovd|"
      "vpmaskmovq|vpmaxsb|vpmaxsd|vpmaxsw|vpmaxub|vpmaxud|vpmaxuw|vpminsb|"
      "vpminsd|vpminsw|vpminub|vpminud|vpminuw|vpmovmskb|vpmovmskbl|vpmovmskbq|"
      "vpmovsxbd|vpmovsxbq|vpmovsxbw|vpmovsxdq|vpmovsxwd|vpmovsxwq|vpmovzxbd|"
      "vpmovzxbq|vpmovzxbw|vpmovzxdq|vpmovzxwd|vpmovzxwq|vpmuldq|vpmulhrsw|"
      "vpmulhuw|vpmulhw|vpmulld|vpmullw|vpmuludq|vpor|vpperm|vprotb|vprotbb|"
      "vprotd|vprotdb|vprotq|vprotqb|vprotw|vprotwb|vpsadbw|vpshab|vpshabb|"
      "vpshad|vpshadb|vpshaq|vpshaqb|vpshaw|vpshawb|vpshlb|vpshlbb|vpshld|"
      "vpshldb|vpshlq|vpshlqb|vpshlw|vpshlwb|vpshufb|vpshufd|vpshufhw|vpshuflw|"
      "vpsignb|vpsignd|vpsignw|vpslld|vpslldq|vpsllq|vpsllvd|vpsllvq|vpsllw|"
      "vpsrad|vpsravd|vpsraw|vpsrld|vpsrldq|vpsrlq|vpsrlvd|vpsrlvq|vpsrlw|"
      "vpsubb|vpsubd|vpsubq|vpsubsb|vpsubsw|vpsubusb|vpsubusw|vpsubw|vptest|"
      "vpunpckhbw|vpunpckhdq|vpunpckhqdq|vpunpckhwd|vpunpcklbw|vpunpckldq|"
      "vpunpcklqdq|vpunpcklwd|vpxor|vrcpps|vrcpss|vroundpd|vroundps|vroundsd|"
      "vroundss|vrsqrtps|vrsqrtss|vshufpd|vshufps|vsqrtpd|vsqrtps|vsqrtsd|"
      "vsqrtss|vsubpd|vsubps|vsubsd|vsubss|vtestpd|vtestps|vucomisd|vucomiss|"
      "vunpckhpd|vunpckhps|vunpcklpd|vunpcklps|vxorpd|vxorps|xorpd|xorps)"
      "\\b",
      "support.function.fpu.x86.assembly"},
     {"(?:--|\\/\\/|\\#|;).*$", "comment.x86.assembly"},
     {"\\b(?:0x)?[0-9a-fA-F]+\\b", "constant.numeric.x86.assembly"}});

}  // namespace
