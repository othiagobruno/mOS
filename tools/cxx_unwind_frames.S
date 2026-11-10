.text
.globl mos_cu_call_immd
mos_cu_call_immd:
  .cfi_startproc
  pushq %rax
  .cfi_adjust_cfa_offset 8
  movq $0x1111, %rbx
  movq $0x2222, %rbp
  movq $0x3333, %r12
  movq $0x4444, %r13
  movq $0x5555, %r14
  movq $0x6666, %r15
  call mos_cu_immd
  popq %rax
  .cfi_adjust_cfa_offset -8
  ret
  .cfi_endproc

.globl mos_cu_call_ind
mos_cu_call_ind:
  .cfi_startproc
  pushq %rax
  .cfi_adjust_cfa_offset 8
  movq $0x1010, %rbx
  movq $0x2020, %rbp
  movq $0x3030, %r12
  movq $0x4040, %r13
  movq $0x5050, %r14
  movq $0x6060, %r15
  call mos_cu_ind
  popq %rax
  .cfi_adjust_cfa_offset -8
  ret
  .cfi_endproc

.globl mos_cu_call_rbp
mos_cu_call_rbp:
  .cfi_startproc
  pushq %rax
  .cfi_adjust_cfa_offset 8
  movq $0xaaaa, %rbx
  movq $0xbbbb, %rbp
  movq $0xcccc, %r12
  movq $0xdddd, %r13
  movq $0xeeee, %r14
  movq $0xffff, %r15
  call mos_cu_rbp
  popq %rax
  .cfi_adjust_cfa_offset -8
  ret
  .cfi_endproc

.globl mos_cu_call_six
mos_cu_call_six:
  .cfi_startproc
  pushq %rax
  .cfi_adjust_cfa_offset 8
  movq $0x61, %rbx
  movq $0x62, %rbp
  movq $0x63, %r12
  movq $0x64, %r13
  movq $0x65, %r14
  movq $0x66, %r15
  call mos_cu_six
  popq %rax
  .cfi_adjust_cfa_offset -8
  ret
  .cfi_endproc

.globl mos_cu_call_dwarf
mos_cu_call_dwarf:
  .cfi_startproc
  pushq %rax
  .cfi_adjust_cfa_offset 8
  movq $0xd1, %rbx
  movq $0xd2, %rbp
  movq $0xd3, %r12
  movq $0xd4, %r13
  movq $0xd5, %r14
  movq $0xd6, %r15
  call mos_cu_dwarf
  popq %rax
  .cfi_adjust_cfa_offset -8
  ret
  .cfi_endproc

.globl mos_cu_immd
.globl mos_cu_immd_end
mos_cu_immd:
  subq $24, %rsp
  movq %rbx, 8(%rsp)
  movq %r12, 16(%rsp)
  movq $0xdead, %rbx
  movq $0xdead, %r12
  call mos_cu_leaf
  movq 8(%rsp), %rbx
  movq 16(%rsp), %r12
  addq $24, %rsp
  ret
mos_cu_immd_end:

.globl mos_cu_ind
.globl mos_cu_ind_end
mos_cu_ind:
  subq $0x118, %rsp
  movq %r13, 0x108(%rsp)
  movq %r14, 0x110(%rsp)
  movq $0xdead, %r13
  movq $0xdead, %r14
  call mos_cu_leaf
  movq 0x108(%rsp), %r13
  movq 0x110(%rsp), %r14
  addq $0x118, %rsp
  ret
mos_cu_ind_end:

.globl mos_cu_rbp
.globl mos_cu_rbp_end
mos_cu_rbp:
  pushq %rbp
  movq %rsp, %rbp
  pushq %r15
  pushq %rbx
  movq $0xdead, %rbx
  movq $0xdead, %r15
  call mos_cu_leaf
  popq %rbx
  popq %r15
  popq %rbp
  ret
mos_cu_rbp_end:

.globl mos_cu_six
.globl mos_cu_six_end
mos_cu_six:
  subq $56, %rsp
  movq %rbx, 8(%rsp)
  movq %r12, 16(%rsp)
  movq %r13, 24(%rsp)
  movq %r14, 32(%rsp)
  movq %r15, 40(%rsp)
  movq %rbp, 48(%rsp)
  movq $0xdead, %rbx
  movq $0xdead, %r12
  movq $0xdead, %r13
  movq $0xdead, %r14
  movq $0xdead, %r15
  movq $0xdead, %rbp
  call mos_cu_leaf
  movq 8(%rsp), %rbx
  movq 16(%rsp), %r12
  movq 24(%rsp), %r13
  movq 32(%rsp), %r14
  movq 40(%rsp), %r15
  movq 48(%rsp), %rbp
  addq $56, %rsp
  ret
mos_cu_six_end:

.globl mos_cu_dwarf
.globl mos_cu_dwarf_end
mos_cu_dwarf:
  .cfi_startproc
  subq $24, %rsp
  .cfi_adjust_cfa_offset 24
  movq %rbx, 8(%rsp)
  .cfi_offset %rbx, -24
  movq %r12, 16(%rsp)
  .cfi_offset %r12, -16
  movq $0xdead, %rbx
  movq $0xdead, %r12
  call mos_cu_leaf
  movq 8(%rsp), %rbx
  movq 16(%rsp), %r12
  addq $24, %rsp
  .cfi_adjust_cfa_offset -24
  ret
  .cfi_endproc
mos_cu_dwarf_end:

.section .note.GNU-stack,"",@progbits
