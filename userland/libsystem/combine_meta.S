/*
 * Descritores Swift dos $s7Combine que o Claude e o ChatGPT importam.
 * Ponteiro relativo: o .long guarda destino - endereço deste campo (ABI Swift).
 * Kind no flags: módulo 0, protocolo 3, classe 16, struct 17, enum 18.
 * Bit 0x80: genérico. Requisito: 1 método, 3 getter, 7 tipo associado.
 */
    .section .data
    .p2align 3

.globl d_mod
d_mod:
    .long 0
    .long 0
    .long 1f - .
1:
    .asciz "Combine"
    .p2align 3

.globl d_pubs
d_pubs:
    .long 0x12
    .long d_mod - .
    .long 1f - .
    .long 0
    .long 0
1:
    .asciz "Publishers"
    .p2align 3

.globl d_publisher
d_publisher:
    .long 3
    .long d_mod - .
    .long 1f - .
    .long 2
    .long 2
    .long 2f - .
    .long 1
    .long 0
    .long 7
    .long 0
1:
    .asciz "Publisher"
2:
    .asciz "Output"
    .p2align 3

.macro gen parent nparams sym mangled name acc
    .globl \sym
    .globl "\mangled"
\sym:
"\mangled":
    .long 0x91
    .long \parent - .
    .long 1f - .
    .long \acc - .
    .long 0
    .short \nparams
    .short 0
    .short \nparams
    .short 0
    .p2align 3
1:
    .asciz "\name"
    .p2align 3
.endm

gen d_pubs 2 d_cmap "$s7Combine10PublishersO10CompactMapVMn" "CompactMap" acc_cmap
gen d_pubs 1 d_dedup "$s7Combine10PublishersO16RemoveDuplicatesVMn" "RemoveDuplicates" acc_dedup
gen d_pubs 1 d_drop "$s7Combine10PublishersO4DropVMn" "Drop" acc_drop
gen d_pubs 1 d_first "$s7Combine10PublishersO5FirstVMn" "First" acc_first
gen d_pubs 1 d_filt "$s7Combine10PublishersO6FilterVMn" "Filter" acc_filt
gen d_pubs 2 d_thr "$s7Combine10PublishersO8ThrottleVMn" "Throttle" acc_thr
gen d_pubs 2 d_recv "$s7Combine10PublishersO9ReceiveOnVMn" "ReceiveOn" acc_recv
gen d_mod 2 d_any "$s7Combine12AnyPublisherVMn" "AnyPublisher" acc_any
gen d_pub 1 d_pubp "$s7Combine9PublishedV9PublisherVMn" "Publisher" acc_pubp
gen d_mod 1 d_pub "$s7Combine9PublishedVMn" "Published" acc_pub

.macro classd sym mangled name acc flags
    .globl \sym
    .globl "\mangled"
\sym:
"\mangled":
    .long \flags
    .long d_mod - .
    .long 1f - .
    .long \acc - .
    .long 0
    .long 0
    .long 0
    .long 14
    .long 0
    .long 0
    .long 0
    .p2align 3
1:
    .asciz "\name"
    .p2align 3
.endm

classd d_can "$s7Combine14AnyCancellableCMn" "AnyCancellable" acc_can 0x10
classd d_pass "$s7Combine18PassthroughSubjectCMn" "PassthroughSubject" acc_pass 0x90
classd d_owp "$s7Combine25ObservableObjectPublisherCMn" "ObservableObjectPublisher" acc_owp 0x10

.globl d_oo
.globl "$s7Combine16ObservableObjectMp"
d_oo:
"$s7Combine16ObservableObjectMp":
    .long 3
    .long d_mod - .
    .long 1f - .
    .long 2
    .long 2
    .long 2f - .
.globl d_oo_tn
.globl "$s7Combine16ObservableObjectP0C19WillChangePublisherAC_AA0F0Tn"
d_oo_tn:
"$s7Combine16ObservableObjectP0C19WillChangePublisherAC_AA0F0Tn":
    .long 7
    .long 0
.globl d_oo_tq
.globl "$s7Combine16ObservableObjectP16objectWillChange0ceF9PublisherQzvgTq"
d_oo_tq:
"$s7Combine16ObservableObjectP16objectWillChange0ceF9PublisherQzvgTq":
    .long 3
    .long 0
1:
    .asciz "ObservableObject"
2:
    .asciz "ObjectWillChangePublisher"
    .p2align 3

.macro conf sym mangled type wt
    .globl \sym
    .globl "\mangled"
\sym:
"\mangled":
    .long d_publisher - .
    .long \type - .
    .long \wt - .
    .long 0
.endm

conf mc_cmap "$s7Combine10PublishersO10CompactMapVy_xq_GAA9PublisherAAMc" d_cmap wt_cmap
conf mc_dedup "$s7Combine10PublishersO16RemoveDuplicatesVy_xGAA9PublisherAAMc" d_dedup wt_dedup
conf mc_drop "$s7Combine10PublishersO4DropVy_xGAA9PublisherAAMc" d_drop wt_drop
conf mc_first "$s7Combine10PublishersO5FirstVy_xGAA9PublisherAAMc" d_first wt_first
conf mc_filt "$s7Combine10PublishersO6FilterVy_xGAA9PublisherAAMc" d_filt wt_filt
conf mc_thr "$s7Combine10PublishersO8ThrottleVy_xq_GAA9PublisherAAMc" d_thr wt_thr
conf mc_recv "$s7Combine10PublishersO9ReceiveOnVy_xq_GAA9PublisherAAMc" d_recv wt_recv
conf mc_any "$s7Combine12AnyPublisherVyxq_GAA0C0AAMc" d_any wt_any
conf mc_pubp "$s7Combine9PublishedV9PublisherVyx_GAadAMc" d_pubp wt_pubp

.macro wtab sym
    .globl \sym
    .p2align 3
\sym:
    .quad recv_witness
    .quad output_witness
.endm

wtab wt_cmap
wtab wt_dedup
wtab wt_drop
wtab wt_first
wtab wt_filt
wtab wt_thr
wtab wt_recv
wtab wt_any
wtab wt_pubp

.globl wt_owp
.globl "$s7Combine25ObservableObjectPublisherCAA0D0AAWP"
    .p2align 3
wt_owp:
"$s7Combine25ObservableObjectPublisherCAA0D0AAWP":
    .quad owp_witness
    .quad output_witness

.section .note.GNU-stack,"",@progbits
