section .bss
sockaddr_struct:
  sockaddr_sin_family  resw 1
  sockaddr_sin_port    resw 1
  sockaddr_sin_addr    resd 1
  sockaddr_sa_zero     resq 1
  sockaddr_addrlen     resq 1
sockaddr_struct_end:

section .data
  ; socket constants
  INADDR_ANY          equ 0
  SOCK_STREAM         equ 1
  SOL_SOCKET          equ 1
  SO_REUSEADDR        equ 2
  AF_INET             equ 2
  STATUS_CODE_LEN     equ 3
  MAX_ROUTES_COUNT    equ 20
  SO_REUSEPORT        equ 15
  MSG_TRUNC           equ 0x20

  DEFAULT_BACKLOG equ 10

  enable  dw 1

  BYTE_MAX    equ 255

  SOCKADDR_STRUCT_LEN equ sockaddr_struct_end - sockaddr_struct
  
  addrlen dq sockaddr_addrlen - sockaddr_sin_family

  SOCKADDR_OFF_SIN_FAMILY  equ 0x0
  SOCKADDR_OFF_SIN_PORT    equ SOCKADDR_OFF_SIN_FAMILY + 0x2
  SOCKADDR_OFF_SIN_ADDR    equ SOCKADDR_OFF_SIN_PORT + 0x2
  SOCKADDR_OFF_SA_ZERO     equ SOCKADDR_OFF_SIN_ADDR + 0x4
  SOCKADDR_OFF_ADDRLEN     equ SOCKADDR_OFF_SA_ZERO + 0x8

section .text
; converts a integer from host byte order to network byte order (big-endian).
; @param  rdi: integer
; @return rax: integer to network byte order
htons:
  mov   rax, rdi
  xchg  al, ah ; bswap 16-bit registers
  ret

; initialise the client socket (create, connect)
; @param  rdi: port the socket will be listening to
; @return rax: file descriptor of the socket
client_init:
  sub   rsp, 0x10

  cmp   rdi, 0
  jl    .error

  ; *** STACK USAGE *** ;
  ; [rsp]     -> port the socket will be listening to
  ; [rsp+0x8] -> file descriptor of the client

  mov   [rsp], rdi

  ; create the socket
  mov   rdi, AF_INET
  mov   rsi, SOCK_STREAM
  mov   rdx, 0
  call  create_socket
  cmp   rax, 0
  jl    .error

  mov   qword [rsp+0x8], rax

  ; set socket options
  mov   rdi, qword [rsp+0x8]
  mov   rsi, SOL_SOCKET
  mov   rdx, SO_REUSEPORT
  call  set_socket_option
  cmp   rax, 0
  jl    .error

  mov   rdi, qword [rsp+0x8]
  mov   rsi, SOL_SOCKET
  mov   rdx, SO_REUSEADDR
  call  set_socket_option
  cmp   rax, 0
  jl    .error

  ; connect to the server
  mov   rdi, qword [rsp+0x8]
  mov   rsi, qword [rsp]
  mov   rdx, AF_INET
  call  connect_to_socket
  cmp   rax, 0
  jl    .error

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret

; initialise the server socket (create, bind and listen)
; @param  rdi: port the socket will be listening to
; @return rax: file descriptor of the socket
server_init:
  sub   rsp, 0x10

  cmp   rdi, 0
  jle   .error

  mov   qword [rsp], rdi

  mov   rdi, AF_INET
  mov   rsi, SOCK_STREAM
  mov   rdx, 0
  call  create_socket
  cmp   rax, 0
  jl    .error

  mov   qword [rsp+0x8], rax

  ; set socket options
  mov   rdi, qword [rsp+0x8]
  mov   rsi, SOL_SOCKET
  mov   rdx, SO_REUSEPORT
  call  set_socket_option
  jl    .error

  mov   rdi, qword [rsp+0x8]
  mov   rsi, SOL_SOCKET
  mov   rdx, SO_REUSEADDR
  call  set_socket_option
  cmp   rax, 0
  jl    .error

  mov   rsi, qword [rsp]
  mov   rdi, qword [rsp+0x8]
  mov   rdx, AF_INET
  call  bind_socket
  cmp   rax, 0
  jl    .error

  ; listen socket
  mov   rdi, qword [rsp+0x8]
  mov   rsi, DEFAULT_BACKLOG
  call  listen_socket
  cmp   rax, 0
  jl    .error

  mov   rax, qword [rsp+0x8]

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x10
  ret
  
; creates socket
; @param  rdi: family
; @param  rsi: type
; @param  rdx: protocol
; @return rax: socket fd
create_socket:
  mov   rax, SYS_SOCKET
  syscall

  ret

; sets socket options
; @param  rdi: socket fd
; @param  rsi: level
; @param  rdx: optname
; @return rax: return value
set_socket_option:
  mov   rax, SYS_SETSOCKOPT
  mov   r10, enable
  mov   r8, 4
  syscall

  ret

; binds socket
; @param  rdi: socket fd
; @param  rsi: port
; @param  rdx: family
; @param  rax: return value
bind_socket:
  sub   rsp, 0x18
  sub   rsp, SOCKADDR_STRUCT_LEN

  ; *** STACK USAGE *** ;
  ; [rsp]       -> socket fd
  ; [rsp+0x8]   -> port
  ; [rsp+0x10]  -> family
  ; [rsp+0x18]  -> sockaddr struct

  cmp   rdi, 0
  jl    .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   [rsp+0x10], rdx

  ; convert port to network port
  xor   rax, rax
  mov   rdi, qword [rsp+0x8]
  call  htons

  lea   rdi, [rsp+0x18]
  mov   word [rdi+SOCKADDR_OFF_SIN_PORT], ax

  mov   rax, qword [rsp+0x10]
  mov   word [rdi+SOCKADDR_OFF_SIN_FAMILY], ax

  mov   dword [rdi+SOCKADDR_OFF_SIN_ADDR], INADDR_ANY
  mov   qword [rdi+SOCKADDR_OFF_SA_ZERO], 0

  mov   rax, SYS_BIND
  mov   rsi, rdi 
  mov   rdi, qword [rsp]
  mov   rdx, qword [addrlen]
  syscall

  cmp   rax, 0
  jl    .error

  mov   rax, SUCCESS_CODE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, SOCKADDR_STRUCT_LEN
  add   rsp, 0x18
  ret

; listens to the socket
; @param  rdi: socket fd
; @param  rsi: backlog
; @return rax: return value
listen_socket:
  mov   rax, SYS_LISTEN
  syscall

  ret

; converts the IP address from text to binary form
; @param  rdi: pointer to IP address
; @return rax: binary form of the ip address
inet_addr:
  sub   rsp, 0x28

  cmp   rdi, 0
  jle   .error

  ; *** STACK USAGE *** ;
  ; [rsp]       -> pointer to ip address string
  ; [rsp+0x8]   -> index to go over the digits
  ; [rsp+0x10]  -> binary form
  ; [rsp+0x18]  -> dynamic pointer
  ; [rsp+0x20]  -> tmp buffer

  mov   [rsp], rdi
  mov   qword [rsp+0x8], 0
  mov   qword [rsp+0x10], 0

  mov   [rsp+0x18], rdi

.loop:
  cmp   qword [rsp+0x8], 3
  jg    .end_loop
  je    .check_null

.check_dot:
  mov   rsi, DOT
  jmp   .get_next_char
  
.check_null:
  mov   rsi, NULL_CHAR

.get_next_char:
  mov   rdi, [rsp+0x18]
  call  find_next_char
  cmp   rax, 0
  jl    .error

  lea   rdi, [rsp+0x20]
  mov   rsi, [rsp+0x18]
  mov   rcx, rax
  rep   movsb

  xor   rax, rax
  stosb

  inc   rsi
  mov   [rsp+0x18], rsi

  lea   rdi, [rsp+0x20]
  call  stoi
  cmp   rax, 0
  jl    .error
  
  cmp   rax, BYTE_MAX
  je    .error

  mov   r9, qword [rsp+0x8]
  mov   byte [rsp+0x10+r9], al

  inc   qword [rsp+0x8]
  jmp   .loop

.end_loop:
  xor   rax, rax
  mov   eax, dword [rsp+0x10]

  jmp   .return 

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, 0x28
  ret

; binds socket
; @param  rdi: socket fd
; @param  rsi: port
; @param  rdx: family
; @param  rax: return value
connect_to_socket:
  sub   rsp, 0x18
  sub   rsp, SOCKADDR_STRUCT_LEN

  ; *** STACK USAGE *** ;
  ; [rsp]       -> socket fd
  ; [rsp+0x8]   -> port
  ; [rsp+0x10]  -> family
  ; [rsp+0x18]  -> sockaddr struct

  cmp   rdi, 0
  jl    .error

  mov   [rsp], rdi
  mov   [rsp+0x8], rsi
  mov   [rsp+0x10], rdx

  ; convert port to network port
  xor   rax, rax
  mov   rdi, qword [rsp+0x8]
  call  htons

  lea   rdi, [rsp+0x18]
  mov   word [rdi+SOCKADDR_OFF_SIN_PORT], ax

  mov   rax, qword [rsp+0x10]
  mov   word [rdi+SOCKADDR_OFF_SIN_FAMILY], ax

  mov   dword [rdi+SOCKADDR_OFF_SIN_ADDR], INADDR_ANY
  mov   qword [rdi+SOCKADDR_OFF_SA_ZERO], 0

  mov   rax, SYS_CONNECT
  mov   rsi, rdi 
  mov   rdi, qword [rsp]
  mov   rdx, qword [addrlen]
  syscall

  cmp   rax, 0
  jl    .error

  mov   rax, SUCCESS_CODE
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, SOCKADDR_STRUCT_LEN
  add   rsp, 0x18
  ret


; accepts a connection and returns the client fd
; @param  rdi: server file descriptor
; @return rax: client file descriptor
accept_connection:
  sub   rsp, 0x10
  sub   rsp, SOCKADDR_STRUCT_LEN

  ; *** STACK USAGE *** ;
  ; [rsp]     -> server file descriptor
  ; [rsp+0x8] -> sockaddr struct

  mov   [rsp], rdi

  lea   rax, [rsp+0x8]

  mov   word [rax+SOCKADDR_OFF_SIN_FAMILY], AF_INET
  mov   word [rax+SOCKADDR_OFF_SIN_PORT], 0
  mov   dword [rax+SOCKADDR_OFF_SIN_ADDR], 0
  mov   qword [rax+SOCKADDR_OFF_SA_ZERO], 0

  mov   rbx, qword [addrlen]
  mov   qword [rax+SOCKADDR_OFF_ADDRLEN], rbx

  mov   rdi, [rsp]
  mov   rax, SYS_ACCEPT
  lea   rsi, [rsp+0x8]
  lea   rdx, [addrlen]
  syscall
  cmp   rax, 0
  jl    .error

  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  add   rsp, SOCKADDR_STRUCT_LEN
  add   rsp, 0x10
  ret

