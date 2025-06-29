section .text
; checks if adjacent chunks can be merged together
; @param  rdi: chunk
; @return rax: bool
can_coalesce:
  cmp   qword [rdi+CHUNK_OFFSET_INUSE], 0
  je    .heap_bound

  mov   rsi, qword [rdi+CHUNK_OFFSET_SIZE]
  add   rdi, rsi

  cmp   rdi, qword [heap_start]
  jge   .heap_bound

  cmp   qword [rdi+CHUNK_OFFSET_INUSE], 0
  sete  al
  movzx rax, al

  jmp   .return

.heap_bound:
  mov   rax, 0

.return:
  ret

; coalesces adjacent chunks
; @param  rdi: chunk
; @return rax: chunk
coalesce:
  sub   rsp, 0x10

  ; save both chunks onto the stack
  mov   [rsp], rdi
  mov   rsi, rdi
  add   rsi, qword [rdi+CHUNK_OFFSET_SIZE]
  mov   [rsp+0x8], rsi
  mov   rdi, rsi

  ; remove second chunk from free list
  call  remove_from_free_list

  mov   rsi, [rsp]
  mov   rdi, [rsp+0x8]

  ; update first chunk size
  mov   rax, qword [rdi+CHUNK_OFFSET_SIZE]
  add   qword [rsi+CHUNK_OFFSET_SIZE], rax

  ; return first chunk
  mov   rax, [rsp]

  add   rsp, 0x10
  ret

; returns the index of the corresponding bin
; @param  rdi: size
; @return rax: bin index
get_bin:
  bsr   rax, rdi
  sub   rax, FIRST_BIN_BSR_INDEX
  
  mov   rbx, N_BINS-1

  cmp   rax, rbx
  cmovge rax, rbx

  mov   rbx, 0
  cmp   rax, rbx
  cmovl rax, rbx

  ret

; removes a chunk of memory from the free list
; @param  rdi: chunk of memory to remove
; @return rax: void
remove_from_free_list:
  sub   rsp, 0x8
  
  mov   [rsp], rdi
  
  ; get bin of data size
  mov   rdi, qword [rdi+CHUNK_OFFSET_SIZE]
  sub   rdi, CHUNK_METADATA_LEN
  call  get_bin

  mov   rdi, qword [seg_free_list+0x8*rax]
  
  ; find chunk from free list
.loop:
  cmp   rdi, [rsp]
  je    .remove

  cmp   rdi, 0
  je    .return

  mov   rsi, qword [rdi+CHUNK_OFFSET_NEXT]
  mov   rdi, rsi

  jmp   .loop

.remove:
  mov   rsi, [rsp]
  cmp   qword [rsi+CHUNK_OFFSET_PREV], 0
  je    .first_chunk

  cmp   qword [rsi+CHUNK_OFFSET_NEXT], 0
  je    .last_chunk

  ; update both prev and next
  mov   rax, qword [rsi+CHUNK_OFFSET_NEXT]
  mov   rbx, qword [rsi+CHUNK_OFFSET_PREV]

  ; update prev
  mov   qword [rbx+CHUNK_OFFSET_NEXT], rax

  ; update next
  mov   qword [rax+CHUNK_OFFSET_PREV], rbx

  jmp   .return

.last_chunk:
  ; remove prev next pointer
  mov   rbx, qword [rsi+CHUNK_OFFSET_PREV]
  mov   qword [rbx+CHUNK_OFFSET_NEXT], 0

  jmp   .return

.first_chunk:
  cmp   qword [rsi+CHUNK_OFFSET_NEXT], 0
  je    .only_chunk

  ; remove from seg free list first
  mov   qword [seg_free_list+0x8*rax], 0

  ; move next one at the beginning of the free list
  mov   rbx, qword [rsi+CHUNK_OFFSET_NEXT]
  mov   qword [rbx+CHUNK_OFFSET_PREV], 0
  mov   qword [seg_free_list+0x8*rax], rbx

  jmp   .return

.only_chunk:
  ; remove from seg free list and return
  mov   qword [seg_free_list+0x8*rax], 0

.return:
  add   rsp, 0x8
  ret

; adds a chunk of memory to the free list
; @param  rdi: chunk of memory to add
; @return rax: void
add_to_free_list:
  sub   rsp, 0x10

  mov   [rsp], rdi

  ; make sure the heap is not corrupted
  cmp   qword [rdi+CHUNK_OFFSET_MAGIC], MAGIC_VAL_HEAP_CORRUPT
  jne   .error

  ; get bin of data size
  mov   rdi, qword [rdi+CHUNK_OFFSET_SIZE]
  sub   rdi, CHUNK_METADATA_LEN
  call  get_bin
  
  ; save bin index
  mov   qword [rsp+0x8], rax
  
  lea   rsi, [seg_free_list+0x8*rax]
  cmp   qword [rsi], 0
  je    .init_free_list
  
  mov   rdi, [rsp]
  mov   rbx, [rsi]
  
  mov   qword [rdi+CHUNK_OFFSET_NEXT], rbx
  mov   qword [rbx+CHUNK_OFFSET_PREV], rdi
  
  ; update free_list top - LIFO
  mov   rax, qword [rsp+0x8] 
  mov   [seg_free_list+0x8*rax], rdi

  xor   rax, rax
  
  jmp   .return
  
.init_free_list:
  mov   rdi, [rsp]
  mov   [seg_free_list+0x8*rax], rdi

  xor   rax, rax

.return:
  add   rsp, 0x10
  ret

.error:
  mov   rax, FAILURE_CODE
  jmp   .return

; frees previously allocated memory
; @param  rdi: pointer to chunk data
; @return rax: return code
free:
  sub   rsp, 0x8

  cmp   rdi, 0
  jl    .error
  
  ; go to the beginning of the struct
  call  get_chunk_header
  cmp   rax, 0
  jl    .error

  mov   rdi, rax

  mov   rbx, qword [rdi+CHUNK_OFFSET_MAGIC]
  cmp   rbx, MAGIC_VAL_HEAP_CORRUPT
  jne   .error

  mov   rbx, qword [rdi+CHUNK_OFFSET_SIZE]
  add   qword [freed], rbx

  ; make sure the chunk needs to be freed
  mov   rax, qword [rdi+CHUNK_OFFSET_INUSE]
  cmp   rax, FALSE
  je    .return

  mov   [rsp], rdi

  ; check if adjacent chunk is free for coalescing
  call  can_coalesce

  cmp   rax, 0
  je    .free_chunk

  mov   rdi, [rsp]
  call  coalesce

  mov   [rsp], rax

.free_chunk:
  mov   rdi, [rsp]

  ; chunk not in use
  mov   qword [rdi+CHUNK_OFFSET_INUSE], 0
  call  add_to_free_list

  mov   rax, SUCCESS_CODE

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x8
  ret

