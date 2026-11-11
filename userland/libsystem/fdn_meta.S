/*
 * Descritores e metadata Swift dos tipos-valor do Foundation que o libSystem implementa
 * em fdn_tipos.c (Date, UUID, CocoaError, POSIXError, URLError, Notification) e o
 * protocolo LocalizedError.
 *
 * Esses tipos não são @frozen no Foundation da Apple: o app não conhece o layout, só
 * o que a metadata diz. O cliente pede a metadata ao acessor (Ma), lê o tamanho e as
 * funções na tabela de value witnesses (fdn_vwt_*, em fdn_tipos.c) e passa o valor por
 * endereço. Por isso o layout interno é nosso; o formato dos descritores é o da ABI 5.
 *
 * Ponteiro relativo: o .long guarda destino - endereço do campo.
 * Flags de contexto: tipo no bit 0-4 (módulo 0, protocolo 3, struct 17, enum 18),
 * 0x40 = único. Metadata de struct: kind 0x200, palavra -1 = value witnesses.
 */
#if defined(__x86_64__)
    .section .rodata
    .p2align 2
    .globl "$s10FoundationMXM"
"$s10FoundationMXM":
    .long 0
    .long 0
    .long fdn_nome_modulo - .
fdn_nome_modulo:
    .asciz "Foundation"

/* struct não genérica: descritor (Mn), metadata (N, local se não exportada) e acessor (Ma). */
.macro fdn_struct desc, parent, nome, ma, md, vwt
    .section .rodata
    .p2align 2
    .globl "\desc"
"\desc":
    .long 0x51
    .long \parent - .
    .long 1f - .
    .long "\ma" - .
    .long 0
    .long 0
    .long 2
1:
    .asciz "\nome"
    .section .data
    .p2align 3
    .quad \vwt
\md:
    .quad 0x200
    .quad "\desc"
    .text
    .globl "\ma"
    .p2align 4
"\ma":
    leaq \md(%rip), %rax
    xorl %edx, %edx
    ret
.endm

fdn_struct "$s10Foundation4DateVMn", "$s10FoundationMXM", "Date", "$s10Foundation4DateVMa", fdn_md_date, fdn_vwt_pod8
fdn_struct "$s10Foundation4UUIDVMn", "$s10FoundationMXM", "UUID", "$s10Foundation4UUIDVMa", fdn_md_uuid, fdn_vwt_pod16
fdn_struct "$s10Foundation10CocoaErrorVMn", "$s10FoundationMXM", "CocoaError", "$s10Foundation10CocoaErrorVMa", fdn_md_cocoa, fdn_vwt_obj
fdn_struct "$s10Foundation10CocoaErrorV4CodeVMn", "$s10Foundation10CocoaErrorVMn", "Code", "$s10Foundation10CocoaErrorV4CodeVMa", fdn_md_cocoa_code, fdn_vwt_pod8
fdn_struct "$s10Foundation10POSIXErrorVMn", "$s10FoundationMXM", "POSIXError", "$s10Foundation10POSIXErrorVMa", fdn_md_posix, fdn_vwt_obj
fdn_struct "$s10Foundation8URLErrorVMn", "$s10FoundationMXM", "URLError", "$s10Foundation8URLErrorVMa", fdn_md_urlerr, fdn_vwt_obj
fdn_struct "$s10Foundation8URLErrorV4CodeVMn", "$s10Foundation8URLErrorVMn", "Code", "$s10Foundation8URLErrorV4CodeVMa", fdn_md_urlerr_code, fdn_vwt_pod8
fdn_struct "$s10Foundation3URLVMn", "$s10FoundationMXM", "URL", "$s10Foundation3URLVMa", fdn_md_url, fdn_vwt_obj
fdn_struct "$s10Foundation6LocaleVMn", "$s10FoundationMXM", "Locale", "$s10Foundation6LocaleVMa", fdn_md_locale, fdn_vwt_obj
fdn_struct "$s10Foundation6LocaleV6RegionVMn", "$s10Foundation6LocaleVMn", "Region", "$s10Foundation6LocaleV6RegionVMa", fdn_md_region, fdn_vwt_obj
fdn_struct "$s10Foundation8TimeZoneVMn", "$s10FoundationMXM", "TimeZone", "$s10Foundation8TimeZoneVMa", fdn_md_tz, fdn_vwt_obj
fdn_struct "$s10Foundation12CharacterSetVMn", "$s10FoundationMXM", "CharacterSet", "$s10Foundation12CharacterSetVMa", fdn_md_cset, fdn_vwt_cset
fdn_struct "$s10Foundation12DateIntervalVMn", "$s10FoundationMXM", "DateInterval", "$s10Foundation12DateIntervalVMa", fdn_md_dint, fdn_vwt_pod16
fdn_struct "$s10Foundation9IndexPathVMn", "$s10FoundationMXM", "IndexPath", "$s10Foundation9IndexPathVMa", fdn_md_ipath, fdn_vwt_box
fdn_struct "$s10Foundation12URLQueryItemVMn", "$s10FoundationMXM", "URLQueryItem", "$s10Foundation12URLQueryItemVMa", fdn_md_qitem, fdn_vwt_box
fdn_struct "$s10Foundation13URLComponentsVMn", "$s10FoundationMXM", "URLComponents", "$s10Foundation13URLComponentsVMa", fdn_md_ucomp, fdn_vwt_box
fdn_struct "$s10Foundation12NotificationVMn", "$s10FoundationMXM", "Notification", "$s10Foundation12NotificationVMa", fdn_md_note, fdn_vwt_obj

/* enum sem payload: NumPayloadCasesAndPayloadSizeOffset = 0, NumEmptyCases; metadata kind 0x201. */
.macro fdn_enum desc, parent, nome, ma, md, vwt, casos
    .section .rodata
    .p2align 2
    .globl "\desc"
"\desc":
    .long 0x52
    .long \parent - .
    .long 1f - .
    .long "\ma" - .
    .long 0
    .long 0
    .long \casos
1:
    .asciz "\nome"
    .section .data
    .p2align 3
    .quad \vwt
\md:
    .quad 0x201
    .quad "\desc"
    .text
    .globl "\ma"
    .p2align 4
"\ma":
    leaq \md(%rip), %rax
    xorl %edx, %edx
    ret
.endm

fdn_enum "$s10Foundation3URLV13DirectoryHintOMn", "$s10Foundation3URLVMn", "DirectoryHint", "$s10Foundation3URLV13DirectoryHintOMa", fdn_md_url_hint, fdn_evwt_tag1, 4

/*
 * protocol LocalizedError : Error { errorDescription, failureReason, recoverySuggestion, helpAnchor }
 * Flags 0x30043: protocolo, único, sem restrição de classe (bit 16) e resiliente (bit 17).
 * Assinatura: Self : Error (requisito de protocolo com argumento-chave, 0x80).
 * Requisitos na ordem da tabela de testemunhas: protocolo base, depois os getters (3 | 0x10 de instância),
 * cada um com a implementação padrão da extensão (todas devolvem nil).
 * O ponteiro para o $ss5ErrorMp é indireto (bit 0): a célula fica em .data.
 */
    .section .data
    .p2align 3
fdn_cel_error:
    .quad "$ss5ErrorMp"

    .section .rodata
    .p2align 2
    .globl "$s10Foundation14LocalizedErrorMp"
"$s10Foundation14LocalizedErrorMp":
    .long 0x30043
    .long "$s10FoundationMXM" - .
    .long fdn_nome_le - .
    .long 1
    .long 5
    .long 0
    /* assinatura: Self : Error */
    .long 0x80
    .long fdn_param_x - .
    .long fdn_cel_error - . + 1
    /* requisitos */
    .globl "$s10Foundation14LocalizedErrorPs0C0Tb"
"$s10Foundation14LocalizedErrorPs0C0Tb":
    .long 0
    .long 0
    .globl "$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTq"
"$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTq":
    .long 0x13
    .long "$s10Foundation14LocalizedErrorPAAE16errorDescriptionSSSgvg" - .
    .globl "$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTq"
"$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTq":
    .long 0x13
    .long "$s10Foundation14LocalizedErrorPAAE13failureReasonSSSgvg" - .
    .globl "$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTq"
"$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTq":
    .long 0x13
    .long "$s10Foundation14LocalizedErrorPAAE18recoverySuggestionSSSgvg" - .
    .globl "$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTq"
"$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTq":
    .long 0x13
    .long "$s10Foundation14LocalizedErrorPAAE10helpAnchorSSSgvg" - .
fdn_nome_le:
    .asciz "LocalizedError"
fdn_param_x:
    .asciz "x"

/*
 * Implementações padrão: String? nil = os dois words zerados (o inabitante extra 0 do
 * BridgeObject da String). Testemunha de getter: self em r13, Self em rdi, tabela em rsi.
 */
    .text
    .p2align 4
    .globl "$s10Foundation14LocalizedErrorPAAE16errorDescriptionSSSgvg"
    .globl "$s10Foundation14LocalizedErrorPAAE13failureReasonSSSgvg"
    .globl "$s10Foundation14LocalizedErrorPAAE18recoverySuggestionSSSgvg"
    .globl "$s10Foundation14LocalizedErrorPAAE10helpAnchorSSSgvg"
"$s10Foundation14LocalizedErrorPAAE16errorDescriptionSSSgvg":
"$s10Foundation14LocalizedErrorPAAE13failureReasonSSSgvg":
"$s10Foundation14LocalizedErrorPAAE18recoverySuggestionSSSgvg":
"$s10Foundation14LocalizedErrorPAAE10helpAnchorSSSgvg":
    xorl %eax, %eax
    xorl %edx, %edx
    ret

/*
 * Thunks de despacho (Tj): chamam o requisito pela tabela de testemunhas do app.
 * Palavra 0 = descritor de conformidade, 1 = Error (base), 2..5 = os getters.
 */
    .p2align 4
    .globl "$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTj"
"$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTj":
    jmpq *16(%rsi)
    .p2align 4
    .globl "$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTj"
"$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTj":
    jmpq *24(%rsi)
    .p2align 4
    .globl "$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTj"
"$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTj":
    jmpq *32(%rsi)
    .p2align 4
    .globl "$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTj"
"$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTj":
    jmpq *40(%rsi)
#endif
