section .bss
  heap_start  resq 1

  chunk_metadata_struct:
    chunk_magic_value   resq 1
    chunk_size          resq 1
    chunk_inuse         resq 1
    chunk_next          resq 1
    chunk_prev          resq 1
  chunk_metadata_struct_end:

section .data
  CHUNK_METADATA_LEN  equ chunk_metadata_struct_end - chunk_metadata_struct

  CHUNK_OFFSET_MAGIC  equ 0
  CHUNK_OFFSET_SIZE   equ CHUNK_OFFSET_MAGIC + 0x8
  CHUNK_OFFSET_INUSE  equ CHUNK_OFFSET_SIZE + 0x8
  CHUNK_OFFSET_NEXT   equ CHUNK_OFFSET_INUSE + 0x8 
  CHUNK_OFFSET_PREV   equ CHUNK_OFFSET_NEXT + 0x8
  CHUNK_OFFSET_DATA   equ CHUNK_OFFSET_PREV + 0x8

  ; magic value on top of each header making sure the heap is not
  ; corrupted
  MAGIC_VAL_HEAP_CORRUPT        equ 0xC0FFEE

  ; min size is 8 bytes which gives a result of 3 with 'bsr' 
  ; instruction.
  FIRST_BIN_BSR_INDEX equ 0x3
  N_BINS              equ 0xC

  seg_free_list times N_BINS dq 0

  mallocd dq 0
  freed   dq 0
  mmapd   dq 0

section .text
; returns the pointer to the header of the chunk
; @param  rdi: pointer to the chunk
; @return rax: pointer to the header
get_chunk_header:
  cmp   rdi, 0
  jl    .error

  sub   rdi, CHUNK_METADATA_LEN
  mov   rax, rdi

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; checks if the chunk can be splitted
; @param  rdi: chunk
; @param  rsi: size
; @return rax: bool
can_split:
  xor   rax, rax

  ; needs to remove header size from the chunk
  mov   rbx, qword [rdi+CHUNK_OFFSET_SIZE]
  sub   rbx, CHUNK_METADATA_LEN

  cmp   rbx, rsi
  setg  al
  movzx rax, al  

  ret

; splits the chunk of memory to allocate the smallest size
; @param  rdi: chunk
; @param  rsi: size
; @return rax: pointer to new chunk
_split:
  sub   rsp, 0x18

  mov   [rsp], rdi
  mov   qword [rsp+0x8], rsi

  ; check if it's a new chunk
  cmp   qword [rdi+CHUNK_OFFSET_INUSE], 1
  je    .new_chunk

  ; remove chunk from free list
  call  remove_from_free_list
  
.new_chunk:
  ; save size onto the stack
  mov   rdi, [rsp]
  mov   rsi, qword [rdi+CHUNK_OFFSET_SIZE]
  mov   qword [rsp+0x10], rsi

  ; update first chunk size and add magic value
  mov   rsi, qword [rsp+0x8]
  mov   qword [rdi+CHUNK_OFFSET_SIZE], rsi

  ; init second chunk
  add   rdi, qword [rdi+CHUNK_OFFSET_SIZE]
  mov   qword [rdi+CHUNK_OFFSET_MAGIC], MAGIC_VAL_HEAP_CORRUPT
  mov   qword [rdi+CHUNK_OFFSET_INUSE], 0

  ; get second chunk size
  mov   rbx, qword [rsp+0x10]
  sub   rbx, qword [rsp+0x8]

  mov   qword [rdi+CHUNK_OFFSET_SIZE], rbx

  ; set prev and next to 0 to avoid garbage data from prev malloc
  mov   qword [rdi+CHUNK_OFFSET_NEXT], 0
  mov   qword [rdi+CHUNK_OFFSET_PREV], 0

  ; add second chunk to the free list
  call  add_to_free_list

.return:
  mov   rax, [rsp]
  add   rsp, 0x18
  ret

; gets memory from the mmap syscall
; @param  rdi: size
; @return rax: returns a pointer to the allocated area
request_from_os:
  mov   rsi, PAGE_SIZE
  call  _align

  add   qword [mmapd], rax
  
  mov   rdi, rax
  call  mmap

  ret

; aligns the size in rdi by the value in rsi
; @param  rdi: size
; @param  rsi: alignment
; @return rax: aligned size
_align:
  ; alignment - 1
  dec   rsi

  ; (size + alignment - 1)
  mov   rax, rdi
  add   rax, rsi

  ; aligned = (size+alignment-1) & ~(alignment-1)
  not   rsi
  and   rax, rsi

  ret

; searches a valid previously allocated chunk
; @param  rdi: size
; @return rax: pointer to chunk, 0 if no valid chunk
search_chunk:
  sub   rsp, 0x10

  mov   qword [rsp], rdi
  mov   qword [rsp+0x8], 0

  call  get_chunk_header
  cmp   rax, 0
  jl    .error

  ; get bin of data size
  mov   rdi, rax
  call  get_bin
  
.loop:
  mov   rdi, [seg_free_list+0x8*rax]
  cmp   rdi, 0
  jne   .first_fit

.next_bin:
  inc   rax
  cmp   rax, N_BINS
  jge   .not_found

  jmp   .loop

.first_fit:
  mov   rbx, qword [rsp]
  cmp   qword [rdi+CHUNK_OFFSET_SIZE], rbx
  jge   .found

  cmp   qword [rdi+CHUNK_OFFSET_NEXT], 0
  je    .next_bin

  mov   rsi, qword [rdi+CHUNK_OFFSET_NEXT]
  mov   rdi, rsi
  jmp   .first_fit

.found:
  ; make sure the data is not corrupted
  cmp   qword [rdi+CHUNK_OFFSET_MAGIC], MAGIC_VAL_HEAP_CORRUPT
  jne   .error

  mov   [rsp+0x8], rdi
  ; check if this chunk can be splitted
  mov   rsi, qword [rsp]
  call  can_split

  cmp   rax, 0
  je    .free_chunk

  mov   rdi, [rsp+0x8]
  mov   rsi, qword [rsp]
  call  _split

  mov   rax, [rsp+0x8]

  jmp   .return

.free_chunk:
  ; use this chunk
  mov   rdi, [rsp+0x8]
  call  remove_from_free_list

  mov   rdi, [rsp+0x8]
  mov   qword [rdi+CHUNK_OFFSET_INUSE], 1
  
  ; return chunk
  mov   rax, rdi
  jmp   .return

.not_found:
  xor   rax, rax

.return:
  add   rsp, 0x10
  ret

.error:
  mov   rax, FAILURE_CODE
  jmp   .return

; allocates a chunk of memory of rsi bytes
; @param  rdi: size
; @return rax: pointer to chunk data
malloc:
  sub   rsp, 0x18

  ; make sure the size if greater than 0
  cmp   rdi, 0
  je    .null
  jl    .error 

  ; add chunk_size to the size
  add   rdi, CHUNK_METADATA_LEN

  ; align the size
  mov   rsi, 0x8
  call  _align

  ; save aligned size onto the stack
  mov   qword [rsp], rax

  ; search for an available chunk of data
  mov   rdi, qword [rsp]
  call  search_chunk

  cmp   rax, 0
  je    .request
  jl    .error

  mov   [rsp+0x8], rax

  ; return the chunk
  jmp   .found

.request:
  ; call mmap
  mov   rdi, qword [rsp]
  call  request_from_os

  cmp   rax, 0
  jl    .error

  mov   [rsp+0x8], rax

  ; initialize chunk
  ; store size of the data
  mov   rdi, qword [rsp]

  ; if request from os, size needs to be page aligned
  mov   rsi, PAGE_SIZE
  call  _align 

  mov   rsi, [rsp+0x8]

  mov   qword [rsi+CHUNK_OFFSET_MAGIC], MAGIC_VAL_HEAP_CORRUPT
  mov   qword [rsi+CHUNK_OFFSET_SIZE],  rax
  mov   qword [rsi+CHUNK_OFFSET_NEXT],  0
  mov   qword [rsi+CHUNK_OFFSET_PREV],  0
  mov   qword [rsi+CHUNK_OFFSET_INUSE], 1

  ; check if heap is init
  cmp   qword [heap_start], 0
  jne   .heap_is_init

  mov   rdi, [rsp+0x8]
  add   rdi, rax
  mov   qword [heap_start], rdi

.heap_is_init:
  ; check if this chunk can be splitted
  mov   rdi, [rsp+0x8]
  mov   rsi, qword [rsp]
  call  can_split

  cmp   rax, 0
  je    .found

  mov   rdi, [rsp+0x8]
  mov   rsi, qword [rsp]
  call  _split

  mov   rax, [rsp+0x8]
  jmp   .found

.error:
  mov   rax, FAILURE_CODE
  jmp   .return

.null:
  xor   rax, rax
  jmp   .return

.found:
  ; save chunk pointer onto the stack
  mov   rax, [rsp+0x8]

  ; make sure the heap is not corrupted
  cmp   qword [rax+CHUNK_OFFSET_MAGIC], MAGIC_VAL_HEAP_CORRUPT
  jne   .error

  ; reset next and prev pointers
  mov   qword [rax+CHUNK_OFFSET_NEXT],  0
  mov   qword [rax+CHUNK_OFFSET_PREV],  0
  mov   qword [rax+CHUNK_OFFSET_INUSE], 1

  mov   rbx, qword [rax+CHUNK_OFFSET_SIZE]
  add   qword [mallocd], rbx

  add   rax, CHUNK_OFFSET_DATA

.return:
  add   rsp, 0x18
  ret

