section .text
; fills a contiguous chunk of memory with a specific value
; @param  rdi: address
; @param  rsi: value
; @param  rdx: count
; @return rax: address
memset:
  sub   rsp, 0x8

  mov   [rsp], rdi

  xor   rax, rax
  mov   rax, rsi
  mov   rcx, rdx
  rep   stosb

  mov   rax, [rsp]

  add   rsp, 0x8
  ret

