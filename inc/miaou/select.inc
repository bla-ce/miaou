section .bss
  fd_set_struct:
    fds_bits  resd bits_len
  fd_set_struct_end:

section .data
  FD_SETSIZE equ 1024

  bits_len equ FD_SETSIZE / 8 / LONG_SIZE

  FD_SET_STRUCT_LEN equ fd_set_struct_end - fd_set_struct

section .text

; removes all file descriptors set
; @param  rdi: pointer to file descriptors set
; @return rax: return code
FD_ZERO:
  cmp   rdi, 0
  jl    .error

  mov   rsi, 0
  mov   rdx, FD_SET_STRUCT_LEN
  call  memset
  
  mov   rax, SUCCESS_CODE

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; adds a file descriptor to the set
; @param  rdi: file descriptor to add
; @param  rsi: pointer to file descriptors set
; @param  rax: return code
FD_SET:
  sub   rsp, 0x28

  cmp   rdi, 0
  jl    .error

  cmp   rsi, 0
  jl    .error

  ; *** STACK USAGE *** ;
  ; [rsp]       -> file descriptor to add
  ; [rsp+0x8]   -> pointer to file descriptors set
  ; [rsp+0x10]  -> 8 * sizeof(long)
  ; [rsp+0x18]  -> index that needs to be updated
  ; [rsp+0x20]  -> shifted value

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi

  ; get index
  xor   rdx, rdx
  mov   rax, 8
  mov   rbx, LONG_SIZE
  mul   rbx

  mov   [rsp+0x10], rax

  xor   rdx, rdx
  mov   rax, [rsp]
  div   rbx

  mov   [rsp+0x18], rax

  ; get unshifted value
  xor   rdx, rdx
  mov   rax, [rsp]
  mov   rbx, [rsp+0x10]
  div   rbx

  ; remainder in rdx
  mov   rcx, rdx
  mov   rax, 1

  ; shift value
  shl   rax, cl

  mov   [rsp+0x20], rax

  ; get prev index value
  xor   rdx, rdx
  mov   rax, [rsp+0x10]
  mov   rbx, LONG_SIZE
  mul   rbx

  mov   rdi, [rsp+0x8]
  add   rdi, rax

  ; or value with prev
  mov   rsi, [rdi]
  or    rsi, [rsp+0x20]
  mov   [rdi], rsi

  mov   rax, [rsp+0x20]

.b1:

  mov   rax, SUCCESS_CODE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x28
  ret

; remove a file descriptor from the set
; @param  rdi: file descriptor to add
; @param  rsi: pointer to file descriptors set
; @param  rax: return code
FD_CLR:
  ret

; returns if a file descriptor is still present in the set
; @param  rdi: file descriptor to add
; @param  rsi: pointer to file descriptors set
; @param  rax: return code
FD_ISSET:
  ret

