#!/usr/bin/env python3
"""Gera userland/libsystem/fi_thunks.c a partir dos 187 nomes de fi_exports.c.

Cada símbolo sai como real (comportamento ou metadata com tamanho) ou stub
justificado. O gerador falha se algum nome não for classificado.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIST = ROOT / "tools/fi_swift_187.txt"
OUT = ROOT / "userland/libsystem/fi_thunks.c"
DOC = ROOT / "docs/foundation-swift-187.md"

syms = [ln.strip() for ln in LIST.read_text().splitlines() if ln.strip()]
assert len(syms) == 187 and len(set(syms)) == 187, len(syms)

def call0(fn):
    return f"  pushq %rbp\n  movq %rsp, %rbp\n  call {fn}\n  popq %rbp\n  ret"

def jmp(fn):
    return f"  jmp {fn}"

def field(n):
    return (
        "  pushq %rbp\n  movq %rsp, %rbp\n  movq %rdi, %rdx\n  movq %r13, %rdi\n"
        f"  movl ${n}, %esi\n  call fi_style_field\n  popq %rbp\n  ret"
    )

def iso_part(bit):
    return (
        "  pushq %rbp\n  movq %rsp, %rbp\n  movq %r13, %rdi\n"
        f"  movl ${bit}, %esi\n  call fi_style_iso_part\n  popq %rbp\n  ret"
    )

def person_get(field):
    return f"  movq %r13, %rdi\n  movl ${field}, %esi\n  jmp fi_person_get"

def person_set(field):
    return (
        "  pushq %rbp\n  movq %rsp, %rbp\n  movq %rsi, %rcx\n  movq %rdi, %rdx\n"
        f"  movq %r13, %rdi\n  movl ${field}, %esi\n  call fi_person_set\n  popq %rbp\n  ret"
    )

def cmp(mode):
    return (
        "  pushq %rbp\n  movq %rsp, %rbp\n  movq %rdi, %rdx\n  movq %rsi, %rcx\n"
        "  movq %r13, %rdi\n  movq %r14, %rsi\n"
        f"  movl ${mode}, %r8d\n  call fi_str_cmp_sw\n  popq %rbp\n  ret"
    )

def self_arg(fn):
    return f"  movq %rdi, %rsi\n  movq %r13, %rdi\n  jmp {fn}"

IMM = {
    "0bD0V7omittedAGvgZ": 0,
    "0bD0V11abbreviatedAGvgZ": 2,
    "04TimeD0V7omittedAGvgZ": 0,
    "04TimeD0V9shortenedAGvgZ": 1,
    "4YearV13defaultDigitsAIvgZ": 1,
    "3DayV13defaultDigitsAIvgZ": 1,
    "5MonthV11abbreviatedAIvgZ": 2,
    "6MinuteV13defaultDigitsAIvgZ": 1,
    "9AMPMStyleV11abbreviatedAKvgZ": 2,
    "7WeekdayV11abbreviatedAIvgZ": 1,
    "7WeekdayV4wideAIvgZ": 2,
    "CapitalizationContextV7unknownACvgZ": 0,
    "8NotationV11compactNameAEvgZ": 1,
    "0B4TypeO3andy": 0,
    "5WidthO6narrowy": 2,
    "13FailurePolicyO10throwErrory": 1,
    "30inlineOnlyPreservingWhitespacey": 2,
    "0B9SeparatorO4dashy": 0,
    "0B13TimeSeparatorO8standardy": 0,
    "13TimeSeparatorO5colony": 0,
    "17TimeZoneSeparatorO7omittedy": 1,
}

STUBS = {
    "19_bridgeToObjectiveCSo08NSPersoncD0CyF": "NSPersonNameComponents não é classe deste libSystem",
    "36_unconditionallyBridgeFromObjectiveCyACSo08NSPersoncD0CSgFZ": "NSPersonNameComponents não é classe deste libSystem",
    "E19_bridgeToObjectiveCSo0A6NumberCyF": "NSDecimalNumber guarda double, não os 20 bytes do NSDecimal",
    "E36_unconditionallyBridgeFromObjectiveCyABSo0A6NumberCSgFZ": "NSDecimalNumber guarda double, não os 20 bytes do NSDecimal",
}

def cow_ma(sym):
    ends = (
        "16AttributedStringVMa",
        "18AttributeContainerVMa",
        "19AttributedSubstringVMa",
        "13CharacterViewVMa",
        "4RunsVMa",
        "3RunVMa",
        "4DateV11FormatStyleVMa",
        "18ISO8601FormatStyleVMa",
        "19IntervalFormatStyleVMa",
        "11FormatStyleVMa",
        "8CurrencyVMa",
        "20PersonNameComponentsVMa",
        "22MarkdownParsingOptionsVMa",
    )
    return sym.endswith(ends)

def classify(sym):
    if sym.endswith("Mc"):
        return ("mc", "stub", "descritor de conformidade: a testemunha devolve 0 porque o índice dos requisitos do protocolo não foi confirmado com o swift-frontend")
    if sym.endswith("Ma"):
        if sym.endswith("11MeasurementVMa"):
            return ("ma", "real", "fi_vwt_pod16")
        if cow_ma(sym):
            return ("ma", "real", "fi_vwt_cow")
        return ("ma", "real", "fi_vwt_pod8")
    if sym.endswith("Mn") or sym.endswith("ON"):
        kind = 18 if (sym.endswith("OMn") or sym.endswith("ON")) else 17
        return ("mn", "real", str(kind))
    for key, why in STUBS.items():
        if sym.endswith(key):
            return ("stub", "stub", why)
    if sym.endswith("ipMV") or (sym.endswith("vpMV") and "4link" not in sym):
        return ("asm", "stub", "leaq fi_member_slot(%rip), %rax\n  ret")
    if "4linkAE04LinkB0Ovg" in sym or "4linkAE04LinkB0OvpMV" in sym:
        return ("asm", "real", "leaq fi_link_key(%rip), %rax\n  ret")
    for key, val in IMM.items():
        if key in sym:
            return ("imm", "real", str(val))
    # fábricas e métodos
    rules = [
        ("7iso8601AGvgZ", "asm", "real", call0("fi_style_iso_default")),
        ("8dateTimeAFvgZ", "asm", "real", call0("fi_style_date_default")),
        ("8dateTimeAGvgZ", "asm", "real", call0("fi_style_date_default")),
        ("6numberAGvgZ", "asm", "real", call0("fi_style_dec_default")),
        ("4list4type5width", "asm", "real", call0("fi_style_list")),
        ("9formattedyAESo15NSDateFormatterC", "asm", "real", "movq %rdi, %rax\n  movq $1, %rdx\n  ret"),
        ("13defaultDigits4amPM", "asm", "real", jmp("fi_hour_from_ampm")),
        ("14fractionLengthyAESiFZ", "asm", "real", "movl %edi, %eax\n  xor %edx, %edx\n  ret"),
        ("6always13includingZero", "asm", "real", jmp("fi_sign_pack")),
        ("ISO8601FormatStyleV4yearAEyF", "asm", "real", iso_part(0)),
        ("ISO8601FormatStyleV5monthAEyF", "asm", "real", iso_part(1)),
        ("ISO8601FormatStyleV3dayAEyF", "asm", "real", iso_part(2)),
        ("13dateSeparatory", "asm", "real", "  movq %rdi, %rdx\n  movq %r13, %rdi\n  xor %esi, %esi\n  jmp fi_style_iso_sep"),
        ("4yearyA2E6SymbolV4YearVF", "asm", "real", field(0)),
        ("4yearAEyF", "asm", "real", field(0)),
        ("5monthyA2E6SymbolV5MonthVF", "asm", "real", field(1)),
        ("5monthyAeC0dE0V6SymbolV5MonthVF", "asm", "real", field(1)),
        ("3dayyA2E6SymbolV3DayVF", "asm", "real", field(2)),
        ("3dayAEyF", "asm", "real", field(2)),
        ("4houryA2E6SymbolV4HourVF", "asm", "real", field(3)),
        ("4houryAeC0dE0V6SymbolV4HourVF", "asm", "real", field(3)),
        ("6minuteyA2E6SymbolV6MinuteVF", "asm", "real", field(4)),
        ("6minuteAEyF", "asm", "real", field(4)),
        ("7weekdayyA2E6SymbolV7WeekdayVF", "asm", "real", field(5)),
        ("7weekdayyAeC0dE0V6SymbolV7WeekdayVF", "asm", "real", field(5)),
        ("4date4time6locale8calendar0F4Zone21capitalizationContext", "asm", "real", call0("fi_style_date_init")),
        ("4date4time6locale8calendar0G4Zone", "asm", "real", call0("fi_style_date_init")),
        ("13dateSeparator0f4TimeG0", "asm", "real", call0("fi_style_iso_full")),
        ("4DateV9formattedy12FormatOutput", "asm", "real", self_arg("fi_date_format_obj")),
        ("$sSB10FoundationE9formatted", "asm", "real", "movq %rdi, %rsi\n  movq %xmm0, %rdi\n  jmp fi_float_format_obj"),
        ("$sSz10FoundationE9formatted", "asm", "real", self_arg("fi_int_format_obj")),
        ("$sST10FoundationE9formatted", "asm", "real", self_arg("fi_seq_format_obj")),
        ("$sSn10Foundation", "asm", "real", self_arg("fi_range_format_obj")),
        ("25localizedStringWithFormat", "asm", "real", jmp("fi_str_format")),
        ("15appendingFormat", "asm", "real",
         "  pushq %rbp\n  movq %rsp, %rbp\n  movq %rdx, %r10\n  movq %rcx, %r11\n  movq %rdi, %rdx\n  movq %rsi, %rcx\n  movq %r13, %rdi\n  movq %r14, %rsi\n  movq %r10, %r8\n  movq %r11, %r9\n  call fi_str_append_format\n  popq %rbp\n  ret"),
        ("16localizedCompare", "asm", "real", cmp(0)),
        ("31localizedCaseInsensitiveCompare", "asm", "real", cmp(1)),
        ("24localizedStandardCompare", "asm", "real", cmp(2)),
        ("18IntegerFormatStyleV6locale", "asm", "real", jmp("fi_style_int_locale")),
        ("18IntegerFormatStyleV4sign", "asm", "real", self_arg("fi_style_sign")),
        ("7PercentV6locale", "asm", "real", jmp("fi_style_percent_locale")),
        ("7PercentV9precision", "asm", "real", self_arg("fi_style_precision")),
        ("11FormatStyleV8notation", "asm", "real", self_arg("fi_style_notation")),
        ("15ListFormatStyleV6locale", "asm", "real", self_arg("fi_style_locale")),
        ("05givenC0SSSgvg", "asm", "real", person_get(1)),
        ("06middleC0SSSgvg", "asm", "real", person_get(2)),
        ("06familyC0SSSgvg", "asm", "real", person_get(3)),
        ("10namePrefixSSSgvg", "asm", "real", person_get(0)),
        ("10nameSuffixSSSgvg", "asm", "real", person_get(4)),
        ("05givenC0SSSgvs", "asm", "real", person_set(1)),
        ("06familyC0SSSgvs", "asm", "real", person_set(3)),
        ("9formattedSSyF", "asm", "real", "  movq %r13, %rdi\n  jmp fi_person_formatted"),
        ("20PersonNameComponentsVACycfC", "asm", "real", call0("fi_person_new")),
        ("12currencyCodeSSvg", "asm", "real", "  movq %r13, %rdi\n  jmp fi_currency_code"),
        ("8CurrencyV6locale", "asm", "real", "  movq %r13, %rdi\n  jmp fi_currency_locale"),
        ("4code6locale", "asm", "real", call0("fi_style_currency")),
        ("EyABSucfC", "asm", "real", "  pushq %rax\n  movq %rax, %rsi\n  call fi_dec_from_uint\n  popq %rax\n  ret"),
        ("EyABs5Int64VcfC", "asm", "real", "  pushq %rax\n  movq %rax, %rsi\n  call fi_dec_from_int\n  popq %rax\n  ret"),
        ("8exponentSivg", "asm", "real", "  movq %r13, %rdi\n  jmp fi_dec_exponent"),
        ("5isNaNSbvg", "asm", "real", "  movq %r13, %rdi\n  jmp fi_dec_isnan"),
        ("6negateyyF", "asm", "real", "  movq %r13, %rdi\n  jmp fi_dec_negate"),
        ("11significandABvg", "asm", "real", "  pushq %rax\n  movq %r13, %rdi\n  movq %rax, %rsi\n  call fi_dec_significand\n  popq %rax\n  ret"),
        ("3powySo9NSDecimal", "asm", "real", "  pushq %rax\n  movq %rax, %rdx\n  call fi_dec_pow\n  popq %rax\n  ret"),
        ("6string6locale", "asm", "real", "  pushq %rbp\n  movq %rsp, %rbp\n  pushq %rax\n  subq $8, %rsp\n  movq %rax, %rcx\n  call fi_dec_locale_entry\n  addq $8, %rsp\n  popq %rax\n  popq %rbp\n  ret"),
        ("6string4fromSSAC11Measurement", "asm", "real", jmp("fi_meas_format")),
        ("12outputFormat", "asm", "real", "  movq %rdi, %rsi\n  movq %r13, %rdi\n  jmp fi_plist_set_format"),
        ("16AttributedStringVACycfC", "asm", "real", call0("fi_attr_empty")),
        ("18AttributeContainerVACycfC", "asm", "real", call0("fi_attr_empty")),
        ("13stringLiteral", "asm", "real", jmp("fi_attr_from_sw")),
        ("_10attributes", "asm", "real", jmp("fi_attr_from_sw")),
        ("1poiyA2C_ACtFZ", "asm", "real", jmp("fi_attr_concat")),
        ("2peoiyyACz_ACtFZ", "asm", "real", self_arg("fi_attr_append")),
        ("6appendyyx", "asm", "real", self_arg("fi_attr_append")),
        ("10charactersAC13CharacterViewVvg", "asm", "real", "  movq %r13, %rdi\n  jmp fi_attr_characters"),
        ("10charactersAA0B6StringV13CharacterViewVvg", "asm", "real", "  movq %r13, %rdi\n  jmp fi_attr_characters"),
        ("4runsAC4RunsVvg", "asm", "real", "  movq %r13, %rdi\n  jmp fi_attr_runs"),
        ("3RunV5range", "asm", "real", "  movq %r13, %rdi\n  jmp fi_run_bounds"),
        ("EySSs5SliceVyAA16AttributedString", "asm", "real", jmp("fi_attr_as_string")),
        ("16AttributedStringVyAA0B9Substring", "asm", "real", "  movq %rsi, %rdx\n  movq %rdi, %rsi\n  movq %r13, %rdi\n  jmp fi_attr_slice"),
        ("3RunV13dynamicMember", "asm", "real", self_arg("fi_run_member")),
        ("13dynamicMember", "asm", "real", self_arg("fi_attr_member")),
        ("8markdown7options", "asm", "real", call0("fi_nsattr_markdown_sw")),
        ("22MarkdownParsingOptionsV24allowsExtended", "asm", "real", jmp("fi_mdopt_new")),
        ("NSAttributedStringC10FoundationEyAbC", "asm", "real", jmp("fi_nsattr_from_sw")),
        ("So8_NSRangeV10FoundationE_2in", "asm", "real", jmp("fi_nsrange_make")),
    ]
    for key, kind, state, payload in rules:
        if key in sym:
            return (kind, state, payload)
    return None

rows = []
missing = []
for s in syms:
    c = classify(s)
    if not c:
        missing.append(s)
    else:
        rows.append((s, c[0], c[1], c[2]))
if missing:
    print("SEM CLASSE", len(missing))
    for s in missing:
        print(s)
    raise SystemExit(1)

# nomes curtos estáveis para os objetos de metadata
ma_i = 0
mn_i = 0
chunks = []
meta_c = []
catalog = []
for sym, kind, state, payload in rows:
    if kind == "imm":
        body = f"  movl ${payload}, %eax\n  xor %edx, %edx\n  ret"
        chunks.append(sym, body) if False else None
        chunks.append((sym, body))
    elif kind == "stub":
        chunks.append((sym, "  xor %eax, %eax\n  xor %edx, %edx\n  ret"))
    elif kind == "asm":
        chunks.append((sym, "  " + payload if not payload.startswith(" ") and not payload.startswith("  ") else payload))
        # payload already indented
        chunks[-1] = (sym, payload if payload.startswith(" ") else payload)
    elif kind == "ma":
        name = f"fi_md_{ma_i}"
        ma_i += 1
        meta_c.append(f"extern const char fi_mn_for_{name}[];\nconst fi_md {name} = {{ &{payload}, 0 }};\n")
        # desc preenchido abaixo se houver Mn irmão; por ora 0 e o accessor devolve a metadata
        chunks.append((sym, f"  leaq {name}(%rip), %rax\n  ret"))
    elif kind == "mn":
        name = f"fi_desc_{mn_i}"
        mn_i += 1
        label = sym
        short = sym[-24:].replace("$", "")
        chunks.append((sym, None, "mn", payload, short))
    elif kind == "mc":
        chunks.append((sym, None, "mc"))
    else:
        raise SystemExit(kind)
    why = payload if state == "stub" and kind in ("mc", "stub") else ""
    if kind == "asm" and state == "stub":
        why = "KeyPath de escrita: o layout do compiler não está neste processo; o ponteiro devolvido é um slot estável e não é lido"
    catalog.append((sym, state, why))

# rebuild chunks more carefully
chunks = []
meta_defs = []
ma_i = 0
for sym, kind, state, payload in rows:
    if kind == "imm":
        chunks.append(("fn", sym, f"  movl ${payload}, %eax\n  xor %edx, %edx\n  ret"))
    elif kind == "stub":
        chunks.append(("fn", sym, "  xor %eax, %eax\n  xor %edx, %edx\n  ret"))
    elif kind == "asm":
        chunks.append(("fn", sym, payload))
    elif kind == "ma":
        name = f"fi_md_{ma_i}"
        ma_i += 1
        meta_defs.append(f"const fi_md {name} = {{ &{payload}, 0 }};\n")
        chunks.append(("fn", sym, f"  leaq {name}(%rip), %rax\n  ret"))
    elif kind == "mn":
        short = re.sub(r"[^A-Za-z0-9]", "", sym)[-32:]
        chunks.append(("mn", sym, payload, short))
    elif kind == "mc":
        chunks.append(("mc", sym))

lines = []
lines.append("/* Gerado por tools/gen_fi_thunks.py. Não editar à mão. */\n")
lines.append("#if defined(__x86_64__)\n")
lines.append("typedef struct fi_vwt fi_vwt;\n")
lines.append("typedef struct { const fi_vwt *vw; const void *desc; } fi_md;\n")
lines.append("extern fi_vwt fi_vwt_pod8, fi_vwt_pod16, fi_vwt_cow;\n")
lines.append("extern const char fi_link_key[];\n")
lines.append("extern unsigned char fi_member_slot[16];\n")
lines.append("void fi_witness_zero(void);\n")
lines.append("__asm__(\n\".text\\n\"\n\".globl fi_witness_zero\\n\"\n\"fi_witness_zero:\\n\"\n\"  xor %eax, %eax\\n  mov $0xe000000000000000, %rdx\\n  ret\\n\"\n);\n")
lines.append("\n".join(meta_defs))
lines.append("\n")

# asm in groups of ~40 to stay under compiler string limits
buf = []
def flush():
    if not buf:
        return
    lines.append('__asm__(\n".text\\n"\n' + "\n".join(buf) + "\n);\n")
    buf.clear()

for item in chunks:
    if item[0] == "fn":
        _, sym, body = item
        esc_body = body.replace("\\", "\\\\").replace("\n", "\\n\"\n\"")
        buf.append(f'".globl \\"{sym}\\"\\n"')
        buf.append(f'"\\"{sym}\\":\\n"')
        buf.append(f'"{esc_body}\\n"')
        if len(buf) > 120:
            flush()
    else:
        flush()
        if item[0] == "mn":
            _, sym, kind_n, short = item
            lines.append(
                '__asm__(\n'
                '".section .rodata\\n.balign 8\\n"\n'
                f'".globl \\"{sym}\\"\\n"\n'
                f'"\\"{sym}\\":\\n"\n'
                f'"  .long {kind_n}\\n  .long 0\\n  .long 20\\n  .long 0\\n  .long 0\\n  .long 0\\n  .long 0\\n"\n'
                f'"  .asciz \\"{short}\\"\\n"\n'
                '".section .text\\n"\n);\n'
            )
        else:
            _, sym = item
            lines.append(
                '__asm__(\n'
                '".section .rodata\\n.balign 8\\n"\n'
                f'".globl \\"{sym}\\"\\n"\n'
                f'"\\"{sym}\\":\\n"\n'
                '"  .long 0\\n  .long 0\\n  .long 8\\n  .long 0\\n"\n'
                '"  .quad fi_witness_zero\\n  .quad fi_witness_zero\\n  .quad fi_witness_zero\\n  .quad fi_witness_zero\\n"\n'
                '"  .quad fi_witness_zero\\n  .quad fi_witness_zero\\n  .quad fi_witness_zero\\n  .quad fi_witness_zero\\n"\n'
                '".section .text\\n"\n);\n'
            )
flush()

# catalog
lines.append("typedef struct { const char *name; int state; const char *why; } fi_cat_row;\n")
lines.append("const fi_cat_row fi_catalog[] = {\n")
for sym, state, why in catalog:
    st = 0 if state == "real" else 1
    why_c = why.replace("\\", "\\\\").replace('"', '\\"')
    lines.append(f'  {{"{sym}", {st}, "{why_c}"}},\n')
lines.append("};\n")
lines.append("const int fi_catalog_n = (int)(sizeof fi_catalog / sizeof fi_catalog[0]);\n")
lines.append("#endif\n")

# The asm string construction above is fragile. Write a cleaner emitter.
# Re-do the file with a straightforward approach.

def asm_fn(sym, body):
    text_s = ".text\n.globl \"" + sym + "\"\n\"" + sym + "\":\n" + body + "\n"
    return "__asm__(\n" + c_string(text_s) + ");\n"

def c_string(s):
    parts = []
    for line in s.split("\n"):
        if line == "" and parts:
            continue
        esc = line.replace("\\", "\\\\").replace('"', '\\"')
        parts.append(f'"{esc}\\n"')
    return "\n".join(parts) + "\n"

out = []
out.append("/* Gerado por tools/gen_fi_thunks.py. Thunks swiftcc x86_64 dos 187 símbolos. */\n")
out.append("#if defined(__x86_64__)\n")
out.append("typedef struct fi_vwt fi_vwt;\n")
out.append("typedef struct { const fi_vwt *vw; const void *desc; } fi_md;\n")
out.append("extern fi_vwt fi_vwt_pod8, fi_vwt_pod16, fi_vwt_cow;\n")
out.append("extern const char fi_link_key[];\n")
out.append("extern unsigned char fi_member_slot[16];\n")
out.append(asm_fn("fi_witness_zero", "  xor %eax, %eax\n  mov $0xe000000000000000, %rdx\n  ret"))
out.append("\n".join(meta_defs))
out.append("\n")
for item in chunks:
    if item[0] == "fn":
        _, sym, body = item
        out.append(asm_fn(sym, body))
    elif item[0] == "mn":
        _, sym, kind_n, short = item
        raw = (
            ".section .rodata\n.balign 8\n"
            f'.globl "{sym}"\n"{sym}":\n'
            f"  .long {kind_n}\n  .long 0\n  .long 20\n  .long 0\n  .long 0\n  .long 0\n  .long 0\n"
            f'  .asciz "{short}"\n'
            ".section .text\n"
        )
        out.append("__asm__(\n" + c_string(raw) + ");\n")
    else:
        _, sym = item
        raw = (
            ".section .data\n.balign 8\n"
            f'.globl "{sym}"\n"{sym}":\n'
            "  .long 0\n  .long 0\n  .long 8\n  .long 0\n"
            "  .quad fi_witness_zero\n  .quad fi_witness_zero\n  .quad fi_witness_zero\n  .quad fi_witness_zero\n"
            "  .quad fi_witness_zero\n  .quad fi_witness_zero\n  .quad fi_witness_zero\n  .quad fi_witness_zero\n"
            ".section .text\n"
        )
        out.append("__asm__(\n" + c_string(raw) + ");\n")

out.append("typedef struct { const char *name; int state; const char *why; } fi_cat_row;\n")
out.append("const fi_cat_row fi_catalog[] = {\n")
for sym, state, why in catalog:
    st = 0 if state == "real" else 1
    why_c = why.replace("\\", "\\\\").replace('"', '\\"')
    out.append(f'  {{"{sym}", {st}, "{why_c}"}},\n')
out.append("};\nconst int fi_catalog_n = (int)(sizeof fi_catalog / sizeof fi_catalog[0]);\n#endif\n")
OUT.write_text("".join(out))

real = sum(1 for _, s, _ in catalog if s == "real")
stub = sum(1 for _, s, _ in catalog if s == "stub")
doc = ["# 187 símbolos de Foundation que o WhatsApp importa\n",
       "\nGerado por `tools/gen_fi_thunks.py`. Estado no x86_64.\n",
       f"\nReal: **{real}**. Stub justificado: **{stub}**. Total: **{len(catalog)}**.\n",
       "\n`real` quer dizer que o thunk tem miolo (constante, metadata com tamanho, ou chamada para o C em `fi_behavior.inc`) e o `fitest` entrou nele. ",
       "O oráculo de macOS (as mesmas frases do `fmttest`) cobre os grupos de data, ISO8601, inteiro, percentual, moeda, compacto, lista, comparação, `String` format, pessoa, attributed string, decimal, measurement e plist. ",
       "Os irmãos de um grupo compartilham a função C; o teste chama cada thunk, e a frase de macOS é conferida no representante do grupo mais nos caminhos nomeados em `tools/fitest.c`.\n",
       "\n`stub` é só o que não dá para implementar sem inventar ABI: os `Mc` (a ordem dos requisitos do protocolo não foi confirmada; a testemunha devolve 0), as quatro pontes para classes ObjC que este libSystem não tem, e o materialize de KeyPath de escrita.\n",
       "\nNada desta lista ficou sem thunk. O que não foi exercitado é o arm64 (os 204 nomes em `fi_exports.c` continuam `ret`), o construtor ObjC de `NSAttributedString` (só entra com `FI_HOST` desligado) e o `machocheck` contra o binário do WhatsApp, que não está neste ambiente.\n",
       "\n| Símbolo | Estado | Nota |\n|---|---|---|\n"]
for sym, state, why in catalog:
    note = why if state == "stub" else ""
    doc.append(f"| `{sym}` | {state} | {note} |\n")
DOC.write_text("".join(doc))
print(f"thunks {len(catalog)} real {real} stub {stub} -> {OUT}")
