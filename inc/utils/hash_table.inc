section .data

hash_table_struct:
  ht_n_bucket  dq 0
  ht_n_entries dq 0
  ht_table     dq 0  ; table is an array of addresses pointing to an entry
hash_table_struct_end:

ht_entry_struct:
  ht_entry_key   dq 0  
  ht_entry_value dq 0
  ht_entry_next  dq 0
ht_entry_struct_end:

HT_KEY_MAX_LEN  equ 0xFF
HT_STRUCT_LEN   equ hash_table_struct_end - hash_table_struct

HT_OFF_NBUCKET  equ 0
HT_OFF_NENTRY   equ HT_OFF_NBUCKET + 0x8
HT_OFF_TABLE    equ HT_OFF_NENTRY + 0x8

HT_ENTRY_STRUCT_LEN equ ht_entry_struct_end - ht_entry_struct

HT_ENTRY_OFF_KEY    equ 0
HT_ENTRY_OFF_VALUE  equ HT_ENTRY_OFF_KEY + 0x8
HT_ENTRY_OFF_NEXT   equ HT_ENTRY_OFF_VALUE + 0x8

HT_ENTRY_KEY_MAX_LENGTH   equ 0xFF
HT_ENTRY_VALUE_MAX_LENGTH equ 0x1000

section .text
; returns the number of entries in the hash table
; @param  rdi: pointer to the hash table
; @return rax: number of entries
ht_get_length:
  cmp   rdi, 0
  jle   .error

  mov   rax, qword [rdi+HT_OFF_NENTRY]

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; returns an array of keys. Users have to deal with freeing the pointer 
; (might change)
; @param  rdi: pointer to hash table
; @return rax: pointer to array
ht_get_keys:
  sub   rsp, 0x30

  cmp   rdi, 0
  jle   .error

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the hash table
  ; [rsp+0x8]   -> number of entries
  ; [rsp+0x10]  -> pointer to the array of keys
  ; [rsp+0x18]  -> pointer to the array of entries
  ; [rsp+0x20]  -> pointer to the entry
  ; [rsp+0x28]  -> counter

  mov   [rsp], rdi

  ; init values to 0
  mov   qword [rsp+0x10], 0
  mov   qword [rsp+0x28], 0 ; counter

  ; save the number of entries
  mov   rax, qword [rdi+HT_OFF_NENTRY]
  mov   qword [rsp+0x8], rax

  test  rax, rax
  jz    .ht_empty

  ; malloc an array of n_entry keys (array of pointers)
  mov   rdi, qword [rsp+0x8]
  mov   rsi, 8
  call  calloc
  cmp   rax, 0
  jl    .error
  je    .ht_empty

  ; save array of keys
  mov   [rsp+0x10], rax

  xor   r9, r9

  ; load hash table entries
  mov   rsi, [rsp]
  mov   rdi, [rsi+HT_OFF_TABLE]

  mov   [rsp+0x18], rdi

.loop:
  ; load hash table entries
  mov   rdi, [rsp+0x18]

  ; get offset from the counter
  xor   rdx, rdx
  mov   rax, r9
  mov   rbx, 8
  mul   rbx

  ; go to entry position
  add   rdi, rax

  ; load entry address
  mov   rsi, [rdi]

  test  rsi, rsi
  jz    .empty

  ; save position
  mov   [rsp+0x20], rsi

  ; get the key from the entry
  mov   rdi, [rsi+HT_ENTRY_OFF_KEY]

  ; get array
  mov   rsi, [rsp+0x10]

  ; get the offset
  xor   rdx, rdx
  mov   rax, qword [rsp+0x28]
  mov   rbx, 8
  mul   rbx

  add   rsi, rax

  ; save key into the array
  mov   [rsi], rdi

  inc   qword [rsp+0x28]
  
  ; check if the key has next populated
.inner_loop:  ; AHHAHAHAHAHAHA O(n^2) this is not optimized
  ; save position
  mov   rsi, [rsp+0x20]

  cmp   qword [rsi+HT_ENTRY_OFF_NEXT], 0
  je    .inner_loop_end

  xor   r10, r10
  mov   rax, [rsi+HT_ENTRY_OFF_NEXT]

  ; save new position
  mov   [rsp+0x20], rax

  ; save key
  mov   rdi, [rax+HT_ENTRY_OFF_KEY]

  mov   rsi, [rsp+0x10]
  xor   rdx, rdx
  mov   rax, qword [rsp+0x28]
  mov   rbx, 8
  mul   rbx

  add   rsi, rax
  mov   [rsi], rdi

  inc   qword [rsp+0x28]

  jmp   .inner_loop

.inner_loop_end:
  
.empty:
  inc   r9

  ; compare index with number of buckets
  mov   rdi, [rsp]
  mov   rax, qword [rdi+HT_OFF_NBUCKET]

  cmp   r9, rax
  jge   .end_loop

  jmp   .loop
    
.end_loop:
  mov   rax, [rsp+0x10]
  jmp   .return

.ht_empty:
  xor   rax, rax
  jmp   .return

.error:
  cmp   qword [rsp+0x10], 0
  je    .no_free

  mov   rdi, [rsp+0x10]
  call  free

.no_free:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x30
  ret

; hash function to map values to key
; @param  rdi: pointer to hash
hash_string_key:
  sub   rsp, 0x10

  cmp   rdi, 0
  jle   .error

  mov   [rsp], rdi
  mov   qword [rsp+0x8], 0

.loop:
  xor   rax, rax
  mov   al, byte [rdi]
  movzx rax, al

  cmp   rax, 0
  je    .end_loop

  add   qword [rsp+0x8], rax
  inc   rdi

  jmp   .loop
.end_loop:

  mov   rax, qword [rsp+0x8]
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; returns the index of the key given the hash
; @param  rdi: hash value
; @param  rsi: number of buckets
; @return rax: index of the key
get_index_from_hash:
  xor   rdx, rdx
  mov   rax, rdi
  div   rsi
  
  mov   rax, rdx

  ret

; returns the value of the associated key
; @param  rdi: pointer to the hash table
; @param  rsi: pointer to the key
; @return rax: pointer to the value
ht_get:
  sub   rsp, 0x20

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi

  ; hash string key
  mov   rdi, [rsp+0x8]
  call  hash_string_key
  cmp   rax, 0
  jl    .error

  ; get index from hash
  mov   rdi, rax

  mov   rax, [rsp]
  mov   rsi, qword [rax+HT_OFF_NBUCKET]
  call  get_index_from_hash

  ; store index
  mov   qword [rsp+0x10], rax 

  ; get table
  mov   rdi, [rsp]
  mov   rsi, [rdi+HT_OFF_TABLE]

  ; go to index
  xor   rdx, rdx
  mov   rax, qword [rsp+0x10]

  mov   rdi, [rsi+rax*8]

  ; if rdi is 0, this key does not exist
  cmp   rdi, 0
  je    .error

  ; save entry
  mov   [rsp+0x18], rdi

.loop:
  ; get key 
  mov   rsi, [rdi+HT_ENTRY_OFF_KEY]
  mov   rdi, [rsp+0x8]
  ; compare with wanted key
  call  strcmp

  cmp   rax, TRUE
  ; if equal, jmp to the end of the loop
  je    .loop_end

  mov   rdi, [rsp+0x18]
  mov   rsi, [rdi+HT_ENTRY_OFF_NEXT]
  cmp   rsi, 0
  je    .error  ; key does not exist

  ; go to the next node on the linked list
  mov   rdi, rsi

  mov   [rsp+0x18], rdi

  jmp   .loop

.loop_end:
  mov   rdi, [rsp+0x18]

  ; get value from entry
  mov   rax, [rdi+HT_ENTRY_OFF_VALUE]

  jmp   .return  

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x20
  ret

; deletes an entry from the hash table
; @param  rdi: pointer to the hash table
; @param  rsi: pointer to the key
; @param  rax: return value
ht_del:
  sub   rsp, 0x30

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   qword [rsp+0x10], 0   ; prev pointer

  ; get hash from key
  mov   rdi, [rsp+0x8]
  call  hash_string_key
  cmp   rax, 0
  jl    .error

  mov   qword [rsp+0x18], rax

  ; get number of buckets
  mov   rdi, [rsp]
  mov   rsi, [rdi+HT_OFF_NBUCKET]
  mov   rdi, qword [rsp+0x18]
  call  get_index_from_hash

  mov   qword [rsp+0x20], rax

  ; get hash table
  mov   rsi, [rsp]
  mov   rdi, [rsi+HT_OFF_TABLE]
  
  ; go to index
  mov   rsi, [rdi+rax*8]
  mov   [rsp+0x28], rsi

.loop:
  mov   rsi, [rsp+0x28]
  mov   rdi, [rsi+HT_ENTRY_OFF_KEY]
  ; compare keys
  mov   rsi, [rsp+0x8]
  call  strcmp
  cmp   rax, 0
  jl    .error

  cmp   rax, TRUE
  je    .found_key

  mov   rdi, [rsp+0x28]
  cmp   qword [rdi+HT_ENTRY_OFF_NEXT], 0 ; key does not exist
  je    .error

  ; set prev pointer
  mov   [rsp+0x10], rdi

  mov   rsi, [rdi+HT_ENTRY_OFF_NEXT]
  mov   [rsp+0x28], rsi
  jmp   .loop

.found_key:
  ; get prev pointer
  mov   rax, qword [rsp+0x10]
  test  rax, rax
  ; if zero, entry is root of the bucket
  jz    .root

  ; get entry
  mov   rdi, [rsp+0x28]

  ; get next
  mov   rbx, [rdi+HT_ENTRY_OFF_NEXT]

  ; set prev next to next
  mov   [rax+HT_ENTRY_OFF_NEXT], rbx

  ; free entry
  mov   rdi, [rsp+0x28]
  call  free
  cmp   rax, 0
  jl    .error

  mov   rax, SUCCESS_CODE

  jmp   .return

.root:
  ; get the index
  mov   rax, qword [rsp+0x20]

  ; go to bucket
  mov   rsi, [rsp]
  mov   rdi, [rsi+HT_OFF_TABLE]
  
  ; set the bucket to 0
  mov   rsi, [rdi+rax*8]
  mov   qword [rdi+rax*8], 0

  ; free the entry
  mov   rdi, rsi
  call  free
  
  mov   rax, SUCCESS_CODE

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x30
  ret

; inserts an entry into the hash table
; @param  rdi: pointer to the hash table
; @param  rsi: pointer to the key
; @param  rdx: pointer to the value
; @return rax: return value
ht_insert:
  sub   rsp, 0x40

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  cmp   rdx, 0
  jle   .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   [rsp+0x10], rdx

  ; check key length
  mov   rdi, [rsp+0x8]
  call  strlen

  cmp   rax, 0
  jl    .error

  cmp   rax, HT_ENTRY_KEY_MAX_LENGTH
  jg    .error

  ; check value length
  mov   rdi, [rsp+0x10]
  call  strlen

  cmp   rax, 0
  jl    .error

  cmp   rax, HT_ENTRY_VALUE_MAX_LENGTH
  jg    .error

  ; create ht_entry
  ; malloc entry
  mov   rdi, HT_ENTRY_STRUCT_LEN
  call  malloc
  cmp   rax, 0
  jl    .error

  ; save entry
  mov   [rsp+0x18], rax

  ; dup and store key
  mov   rdi, [rsp+0x8] 
  call  strdup

  cmp   rax, 0
  jl    .error

  mov   rdi, [rsp+0x18]
  mov   [rdi+HT_ENTRY_OFF_KEY], rax

  ; dup and store value
  mov   rdi, [rsp+0x10] 
  call  strdup

  cmp   rax, 0
  jl    .error

  mov   rdi, [rsp+0x18]
  mov   [rdi+HT_ENTRY_OFF_VALUE], rax

  ; set next as 0
  mov   qword [rdi+HT_ENTRY_OFF_NEXT], 0

  ; hash string
  mov   rdi, [rsp+0x8]
  call  hash_string_key

  cmp   rax, 0
  jl   .error

  mov   qword [rsp+0x20], rax

  ; get number of buckets
  mov   rax, [rsp]

  ; mod to the number of buckets
  mov   rdi, qword [rsp+0x20]
  mov   rsi, qword [rax+HT_OFF_NBUCKET]
  call  get_index_from_hash

  ; store index
  mov   qword [rsp+0x28], rax

  ; get hash table 
  mov   rsi, [rsp]
  mov   rdi, [rsi+HT_OFF_TABLE]

  ; check if there is an entry at the index
  mov   rbx, qword [rsp+0x28]

  xor   rdx, rdx
  mov   rax, rbx
  mov   rbx, 8
  mul   rbx
  add   rdi, rax

  ; rdi points to the position of the array
  mov   [rsp+0x38], rdi

  ; if key is 0, go to store key
  cmp   qword [rdi], 0
  je    .store_key

  ; check if current key is the same, if yes replace
  mov   rax, [rdi]

  mov   rdi, [rax+HT_ENTRY_OFF_KEY]
  mov   rsi, [rsp+0x8]
  call  strcmp

  cmp   rax, 0
  jl    .error
  jg    .same_key  ; the key is the same, we can replace

  ; get array position
  mov   rdi, [rsp+0x38]

  ; get the entry
  mov   rax, [rdi]

  ; get the entry we want to add
  mov   rdi, [rsp+0x18]

  ; update next to point to the current entry
  mov   [rdi+HT_ENTRY_OFF_NEXT], rax

  ; now the entry we want to add is the head of the linked list and 
  ; next points to the previous head
  jmp   .store_key

.same_key:
  ; decrease number of entries as we are replacing it
  mov   rdi, [rsp]
  dec   qword [rdi+HT_OFF_NENTRY]

.store_key:
  ; store ht entry 
  mov   rdi, [rsp+0x38]
  mov   rax, [rsp+0x18]
  mov   [rdi], rax

  ; increase the number of entry
  mov   rdi, [rsp]
  inc   qword [rdi+HT_OFF_NENTRY]
  
  mov   rax, SUCCESS_CODE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x40
  ret 

; returns a hash table of n buckets
; @param  rdi: number of buckets
; @return rax: pointer to the hash table
ht_create:
  sub   rsp, 0x10

  cmp   rdi,  0
  jl    .error
  je    .ht_empty

  mov   [rsp], rdi

  ; malloc hash table struct
  mov   rdi, HT_STRUCT_LEN
  call  malloc

  cmp   rax, 0
  jl    .error

  mov   [rsp+0x8], rax

  ; alloc array of addresses pointing to entries
  mov   rdi, qword [rsp]   
  mov   rsi, 8  ; addresses of 8 bytes long
  call  calloc
  cmp   rax, 0
  jl    .error
  je    .ht_empty

  mov   rdi, [rsp+0x8]
  mov   qword [rdi+HT_OFF_TABLE], rax

  mov   rbx, qword [rsp]
  mov   qword [rdi+HT_OFF_NBUCKET], rbx

  mov   rbx, qword [rsp]
  mov   qword [rdi+HT_OFF_NENTRY], 0
  
  mov   rax, [rsp+0x8]
  jmp   .return

.ht_empty:  
  xor   rax, rax
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; frees the hash table and allocated resources
; @param  rdi: pointer to the hash table
; @return rax: return code
ht_free:
  sub   rsp, 0x28

  cmp   rdi, 0
  jle   .error

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to the hash table
  ; [rsp+0x8]   -> number of entries
  ; [rsp+0x10]  -> pointer to the array of entries
  ; [rsp+0x18]  -> pointer to the entry
  ; [rsp+0x20]  -> counter

  mov   [rsp], rdi

  ; init values to 0
  mov   qword [rsp+0x20], 0 ; counter

  ; save the number of entries
  mov   rax, qword [rdi+HT_OFF_NENTRY]
  mov   qword [rsp+0x8], rax

  test  rax, rax
  jz    .ht_empty

  xor   r9, r9

  ; load hash table entries
  mov   rsi, [rsp]
  mov   rdi, [rsi+HT_OFF_TABLE]

  mov   [rsp+0x10], rdi

.loop:
  ; load hash table entries
  mov   rdi, [rsp+0x10]

  ; get offset from the counter
  xor   rdx, rdx
  mov   rax, r9
  mov   rbx, 8
  mul   rbx

  ; go to entry position
  add   rdi, rax

  ; load entry address
  mov   rsi, [rdi]

  test  rsi, rsi
  jz    .empty

  ; save position
  mov   [rsp+0x18], rsi

  ; free the key
  mov   rdi, [rsi+HT_ENTRY_OFF_KEY]
  call  free
  cmp   rax, 0
  jl    .error

  ; free the value
  mov   rsi, [rsp+0x18]
  mov   rdi, [rsi+HT_ENTRY_OFF_VALUE]
  call  free
  cmp   rax, 0
  jl    .error

  ; free the entry
  mov   rdi, [rsp+0x18]
  call  free
  cmp   rax, 0
  jl    .error

  inc   qword [rsp+0x20]
  
  ; check if the key has next populated
.inner_loop:  ; AHHAHAHAHAHAHA O(n^2) this is not optimized
  ; save position
  mov   rsi, [rsp+0x18]

  cmp   qword [rsi+HT_ENTRY_OFF_NEXT], 0
  je    .inner_loop_end

  xor   r10, r10
  mov   rax, [rsi+HT_ENTRY_OFF_NEXT]

  ; save new position
  mov   [rsp+0x18], rax

  ; free the key
  mov   rdi, [rax+HT_ENTRY_OFF_KEY]
  call  free
  cmp   rax, 0
  jl    .error

  ; free the value
  mov   rsi, [rsp+0x18]
  mov   rdi, [rsi+HT_ENTRY_OFF_VALUE]
  call  free
  cmp   rax, 0
  jl    .error

  ; free the entry
  mov   rdi, [rsp+0x18]
  call  free
  cmp   rax, 0
  jl    .error

  inc   qword [rsp+0x20]

  jmp   .inner_loop

.inner_loop_end:
  
.empty:
  inc   r9

  ; compare index with number of buckets
  mov   rdi, [rsp]
  mov   rax, qword [rdi+HT_OFF_NBUCKET]

  cmp   r9, rax
  jge   .end_loop

  jmp   .loop
    
.end_loop:
  mov   rsi, [rsp]
  mov   rdi, [rsi+HT_OFF_TABLE]
  call  free
  cmp   rax, 0
  jl    .error

  ; free hash table
  mov   rdi, [rsp]
  call  free
  cmp   rax, 0
  jl    .error

  mov   rax, SUCCESS_CODE
  jmp   .return

.ht_empty:
  xor   rax, rax
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x28
  ret

