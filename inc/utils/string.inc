section .data
  UNSIGNED_LONG_LONG_MAX_STR_SIZE equ 21
  SIGNED_LONG_LONG_MAX_STR_SIZE   equ 20

section .text
; returns if a string starts with a substring
; @param  rdi: string
; @param  rsi: substring
; @return rax: bool
starts_with:
  sub   rsp, 0x10

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi

  ; get the length of the substring
  mov   rdi, [rsp+0x8]
  call  strlen
  cmp   rax, 0
  jl    .error

  ; compare beginning of the string
  mov   rdi, [rsp]
  mov   rsi, [rsp+0x8]
  mov   rcx, rax
  rep   cmpsb
  jne   .error

  mov   rax, TRUE
  jmp   .return

.error:
  mov   rax, FALSE

.return:
  add   rsp, 0x10
  ret

; returns a pointer to a new string which is a duplicate of the string s
; user has to handle freeing the string
; @param  rdi: pointer to the string
; @return rax: pointer to the duplicated string
strdup:
  sub   rsp, 0x8

  cmp   rdi, 0
  jle   .error

  mov   [rsp], rdi

  ; get length of the string
  mov   rdi, [rsp]
  call  strlen

  cmp   rax, 0
  jle   .error

  ; call strndup with same amount of bytes
  mov   rdi, [rsp]
  mov   rsi, rax
  call  strndup

  cmp   rax, 0
  jle   .error

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x8
  ret

; returns a pointer to a new string which is a duplicate of the string s, 
; it copies at most n bytes
; user has to handle freeing the string
; @param  rdi: pointer to the string
; @param  rsi: number of bytes to be copied
; @return rax: pointer to the duplicated string
strndup:
  sub   rsp, 0x18

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   qword [rsp+0x8], rsi

  ; get length of the string
  mov   rdi, [rsp]
  call  strlen

  cmp   rax, 0
  jle   .error

  mov   rbx, qword [rsp+0x8] 
  cmp   rax, rbx
  cmovg rax, rbx

  mov   qword [rsp+0x8], rax

  mov   rdi, rax
  inc   rdi         ; add one for null char
  call  malloc

  cmp   rax, 0
  jl    .error

  mov   [rsp+0x10], rax

  mov   rdi, rax
  mov   rsi, [rsp]
  mov   rcx, qword [rsp+0x8]
  rep   movsb

  mov   rax, NULL_CHAR
  stosb

  mov   rax, [rsp+0x10]
  
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x18
  ret

; returns the number of occurence of a specific character in the string
; @param  rdi: string
; @param  rsi: char
; @return rax: count
count_char:
  sub   rsp, 0x18

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   qword [rsp+0x8], rsi
  mov   qword [rsp+0x10], 0

  mov   rdi, qword [rsp+0x8]
  call  is_ascii

  test  rax, rax
  jz    .error

  mov   rdi, qword [rsp+0x8]
  mov   rsi, [rsp]

  xor   rax, rax

.loop:
  lodsb

  cmp   rax, 0
  je    .end_loop

  cmp   al, dil
  jne   .loop  

  inc   qword [rsp+0x10]
  jmp   .loop

.end_loop:
  mov   rax, qword [rsp+0x10]
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x18
  ret

; returns if RDI contains a ASCII character
; @param  rdi: int
; @return rax: is_ascii
is_ascii:
  movzx rdi, dil

  cmp   rdi, 0
  jl    .is_not

  cmp   rdi, 0xFF
  jg    .is_not

  mov   rax, TRUE
  jmp   .return

.is_not:
  mov   rax, FALSE

.return:
  ret

; compare two strings
; @param  rdi: str
; @param  rsi: substr
; @return rax: rdi == rsi
strcmp:
  sub   rsp, 0x20

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi

  ; get length of the first string
  mov   rdi, [rsp]
  call  strlen

  cmp   rax ,0
  jl    .error

  mov   qword [rsp+0x10], rax

  ; get length of the second string
  mov   rdi, [rsp+0x8]
  call  strlen

  cmp   rax ,0
  jl    .error

  mov   qword [rsp+0x18], rax

  ; if length are not equal, return false
  cmp   rax, qword [rsp+0x10]
  jne   .str_not_equal

  mov   rdi, [rsp]
  mov   rsi, [rsp+0x8]
  mov   rdx, qword [rsp+0x18]
  call  strncmp
  cmp   rax, 0
  jl    .error

  jmp   .return

.str_not_equal:
  mov   rax, FALSE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x20
  ret

; compare first `count` bytes of two strings
; @param  rdi: str
; @param  rsi: substr
; @param  rdx: count
; @return rax: rdi == rsi
strncmp:
  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  cmp   rdx, 0
  jl    .error

  ; rdi and rsi are already populated
  mov   rcx, rdx
  rep   cmpsb
  jne   .str_not_equal

  mov   rax, TRUE
  jmp   .return

.str_not_equal:
  mov   rax, FALSE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; copy the string pointed to by src, into a string at the buffer pointed to by dst
; @param  rdi: dst
; @param  rsi: src
; @return rax: pointer to dst after copy
strcpy:
  sub   rsp, 0x10

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi

  mov   rdi, rsi
  call  strlen
  mov   rcx, rax

  mov   rdi, [rsp]
  mov   rsi, [rsp+0x8]
  rep   movsb

  mov   rax, rdi

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; converts int in rdi into string
; string pointer needs to be passed
; @param  rdi: int
; @param  rsi: string
; @param  rdx: size
; @return rax: pointer to str(int)
to_string:
  sub   rsp, 0x18

  cmp   rsi, 0
  jle   .error

  cmp   rdx, 0
  jl    .error

  ; save parameters onto the stack
  mov   qword [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   qword [rsp+0x10], rdx

  ; go to the end of the buf (print backward)
  mov   rdi, [rsp+0x8]
  add   rdi, qword [rsp+0x10]

  ; go back to let an extra null char at the end
  dec   rdi
  mov   byte [rdi], NULL_CHAR

  dec   rdi
  
  mov   rax, qword [rsp]
  cmp   rax, 0
  jg    .loop

  neg   rax

.loop:
  xor   rdx, rdx
  mov   rcx, 0xA              ; divisor, dividend in rax
  div   rcx                   ; quotient in rax and remainder in edx

  add   rdx, ZERO_CHAR

  mov   byte [rdi], dl

  dec   rdi

  cmp   rax, 0
  jne   .loop

.end_loop:
  cmp   qword [rsp], 0
  jge   .skip_hyphen

  mov   byte [rdi], HYPHEN
  dec   rdi

.skip_hyphen:
  inc   rdi
 
  mov   rax, rdi

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x18
  ret

; returns the index of the next occurence of the char
; relative to current address
; @param  rdi: string
; @param  rsi: char
; @return rax: index
find_next_char:
  sub   rsp, 0x8
  mov   qword [rsp], 0

  cmp   rdi, 0
  jle   .not_found

  mov   rdx, rsi
  mov   rsi, rdi

.loop:
  mov   al, byte [rsi]
  cmp   al, dl
  je    .found

  cmp   al, NULL_CHAR
  je   .not_found 

  inc   qword [rsp]

  inc   rsi
  jmp   .loop

.not_found:
  mov   rax, FAILURE_CODE
  jmp   .return

.found:
  mov   rax, qword [rsp]

.return:
  add   rsp, 0x8
  ret

; returns the position of n bytes from a substring inside a string
; @param  rdi: string
; @param  rsi: substring
; @param  rdx: n bytes of substring to find
; @return rax: position of the string
strnpos:
  sub   rsp, 0x28

  cmp   rdi, 0
  jle   .does_not_contain

  cmp   rsi, 0
  jle   .does_not_contain

  cmp   rdx, 0
  jle   .does_not_contain

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   qword [rsp+0x18], rdx

  ; get string length
  call  strlen
  mov   qword [rsp+0x10], rax

  cmp   rax, 0
  je    .does_not_contain

  mov   rdi, [rsp]
  mov   rsi, [rsp+0x8]

  ; keep track of cursor index
  mov   qword [rsp+0x20], 0

.loop:
  mov   rcx, qword [rsp+0x18]
  rep   cmpsb

  je    .contains

  mov   rbx, qword [rsp+0x18]
  add   qword [rsp+0x20], rbx
  sub   qword [rsp+0x20], rcx

  mov   rbx, qword [rsp+0x20]
  cmp   rbx, qword [rsp+0x10]
  jge   .does_not_contain

  add   rsi, rcx
  sub   rsi, qword [rsp+0x18]

  jmp   .loop

.does_not_contain:
  mov   rax, -1
  jmp   .return

.contains:
  mov   rax, qword [rsp+0x20]

.return:
  add   rsp, 0x28
  ret


; returns the position of a substring inside a string
; @param  rdi: string
; @param  rsi: substring
; @return rax: position of the string
strpos:
  sub   rsp, 0x10

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jle   .error

  ; *** STACK USAGE *** ;
  ; [rsp]     -> pointer to the string
  ; [rsp+0x8] -> pointer to the substring

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi

  mov   rdi, rsi
  call  strlen
  cmp   rax, 0
  jle   .error

  mov   rdi, [rsp]
  mov   rsi, [rsp+0x8]
  mov   rdx, rax
  call  strnpos
  cmp   rax, 0
  jl    .error

  jmp   .return
  
.error:
  mov   rax, -1

.return:
  add   rsp, 0x10
  ret

; returns the length of the given string
; @param  rdi: string
; @return rax: length of the string
strlen:
  cmp   rdi, 0
  jle   .error

  lea   rsi, [rdi]
  mov   rcx, 0

  cmp   rsi, 0
  je    .return

.loop:
  xor   rax, rax
  mov   al, NULL_CHAR
  cmp   byte [rsi], al
  je    .return

  inc   rsi
  inc   rcx

  jmp   .loop

.error:
  mov   rax, FAILURE_CODE
  ret

.return:
  mov   rax, rcx
  ret

; prints to the standard output the string in rdi
; @param  rdi: pointer to the string
; @return rax: return value
print:
  sub   rsp, 0x8

  cmp   rdi, 0
  jle   .error

  mov   [rsp], rdi

  mov   rdi, [rsp]
  call  strlen
  cmp   rax, 0
  jl    .error

  mov   rdi, STDOUT_FILENO
  mov   rsi, [rsp]
  mov   rdx, rax
  call  write_file

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x8
  ret

; prints to the standard output the string followed by a new line character
; @param  rdi: string
; @return rax: return code
println:
  sub   rsp, 0x8

  cmp   rdi, 0
  jle   .error

  call  print
  cmp   rax, 0
  jl    .return

  mov   rdi, LINE_FEED
  call  print_char

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x8
  ret

; prints n bytes from the string to the standard output
; it does not check buffer overflow, TODO ?
; @param  rdi: string
; @param  rsi: number of bytes to be printed
; @return rax: return code
nprint:
  sub   rsp, 0x10

  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jl    .error

  mov   [rsp], rdi
  mov   qword [rsp+0x8], rsi

  mov   rdi, STDOUT_FILENO
  mov   rsi, [rsp]
  mov   rdx, qword [rsp+0x8]
  call  write_file

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; prints n bytes from the string followed by a new line character
; @param  rdi: string
; @param  rsi: number of bytes to be printed
; @return rax: return code
nprintln:
  cmp   rdi, 0
  jle   .error

  cmp   rsi, 0
  jl    .error

  call  nprint
  cmp   rax, 0
  jl    .return

  mov   rdi, LINE_FEED
  call  print_char

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

; prints a character to the standard output
; @param  rdi: character
print_char:
  push  rbp
  mov   rbp, rsp

  push  rdi

  xor   rsi, rsi

  mov   rax, SYS_WRITE
  mov   rdi, STDOUT_FILENO
  lea   rsi, [rsp]
  mov   rdx, 1
  syscall

  leave
  ret

; converts string to an integer
; @param  rdi: string
; @return rax: int or -1 if error
stoi:
  sub   rsp, 0x18

  cmp   rdi, 0
  jle   .error

  mov   qword [rsp+0x8], 0
  mov   qword [rsp+0x10], 0

  cmp   byte [rdi], HYPHEN
  jne   .pos_number

  mov   qword [rsp+0x10], 1
  inc   rdi

.pos_number:
  call  strlen
  cmp   rax, 0
  jle   .error

  mov   qword [rsp], rax

.next_char:
  xor   rax, rax
  mov   al, byte [rdi]

  ; make sure char is a digit
  cmp   al, ZERO_CHAR
  jl    .error

  mov   rbx, 0
  cmove rax, rbx
  je    .add

  cmp   al, NINE_CHAR
  jg    .error

  xor   rdx, rdx
  sub   rax, ZERO_CHAR ; get int

  mov   rcx, qword [rsp]
  dec   rcx

  cmp   rcx, 0
  jle   .add

.get_mult:
  mov   rbx, 10
  xor   rdx, rdx
  mul   rbx

  cmp   rcx, 1
  jle   .add

  dec   rcx
  jmp   .get_mult

.add:
  add   qword [rsp+0x8], rax
  mov   r9, qword [rsp+0x8]
  inc   rdi

  dec   qword [rsp]
  cmp   qword [rsp], 0
  je    .return

  jmp   .next_char

.error:
  mov   rax, FAILURE_CODE
  add   rsp, 0x18
  ret

.return:
  mov   rax, qword [rsp+0x8]
  mov   rbx, 0
  sub   rbx, rax

  ; check if it's a neg number
  cmp   qword [rsp+0x10], 1
  cmove rax, rbx

  add   rsp, 0x18
  ret

; returns the int value of ascii character or -1 if invalid
; @param  rdi: ascii
; @return rax: int
ascii_to_int:
  sub   rdi, 0x30

  cmp   rdi, 0x0
  jl    .error

  cmp   rdi, 0x9
  jg    .error

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  mov   rax, rdi
  ret

; prints padded string, if strlen is bigger than padding, add '...' before
; @param  rdi: str
; @param  rsi: padding
; @return rax: void
padding_print:
  sub   rsp, 0x18

  cmp   rdi, 0
  jle   .error

  mov   [rsp], rdi
  mov   qword [rsp+0x8], rsi

  ; get string length
  call  strlen
  mov   qword [rsp+0x10], rax

  cmp   rax, qword [rsp+0x8]
  je    .equal
  jl    .less
  jg    .more

; print the string without padding
.equal:
  mov   rdi, [rsp]
  mov   rsi, qword [rsp+0x8]
  call  nprint
  jmp   .return

.less:
  ; print string
  mov   rdi, [rsp]
  mov   rsi, qword [rsp+0x10]
  call  nprint

  ; get difference
  mov   r9, qword [rsp+0x8]
  sub   r9, qword [rsp+0x10]

  ; print padding
.loop:
  mov   rdi, SPACE
  call  print_char

  dec   r9
  
  cmp   r9, 0
  jg    .loop

  jmp   .return 

.more:
  ; get length and subtract 3 for the dots
  mov   rdi, [rsp]
  mov   rsi, qword [rsp+0x8]
  sub   rsi, 3
  call  nprint

  ; print dots
  mov   r9, 3

.loop_dots:
  mov   rdi, DOT
  call  print_char

  dec   r9

  cmp   r9, 0
  jg    .loop_dots

  jmp   .return

.error:
  mov   rax, FAILURE_CODE
  jle   .error

.return:
  add   rsp, 0x18
  ret

; converts value into a string of its hex value
; @param  rdi: value
; @param  rsi: string
; @param  rdx: size
to_hex_string:
  sub   rsp, 0x18

  cmp   rsi, 0
  jle   .error

  cmp   rdx, 0
  jl    .error

  ; save parameters onto the stack
  mov   qword [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   qword [rsp+0x10], rdx

  ; go to the end of the buf (print backward)
  mov   rdi, [rsp+0x8]
  add   rdi, qword [rsp+0x10]

  ; go back to let an extra null char at the end
  dec   rdi
  mov   byte [rdi], NULL_CHAR

  dec   rdi
  
  mov   rax, qword [rsp]
  cmp   rax, 0
  jg    .loop

  neg   rax

.loop:
  xor   rdx, rdx
  mov   rcx, 0x10             ; divisor, dividend in rax
  div   rcx                   ; quotient in rax and remainder in edx

  add   rdx, ZERO_CHAR

  cmp   rdx, NINE_CHAR
  jle   .no_hex

  sub   rdx, 10
  sub   rdx, ZERO_CHAR 
  add   rdx, LOWERCASE_A

.no_hex:
  mov   byte [rdi], dl

  dec   rdi

  cmp   rax, 0
  jne   .loop

.end_loop:
  std
  mov   rax, LOWERCASE_X
  stosb

  mov   rax, ZERO_CHAR
  stosb
  cld

  cmp   qword [rsp], 0
  jge   .skip_hyphen

  mov   byte [rdi], HYPHEN
  dec   rdi

.skip_hyphen:
  inc   rdi
 
  mov   rax, rdi

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x18

  ret


