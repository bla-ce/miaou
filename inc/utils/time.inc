section .bss
  ; timers
  start_tv_sec  resq 1
  start_tv_nsec resq 1

  end_tv_sec  resq 1
  end_tv_nsec resq 1

  ; dt struct
  dt_year    resq 1
  dt_month   resq 1
  dt_day     resq 1
  dt_hour    resq 1
  dt_minute  resq 1
  dt_second  resq 1

  now_tv_sec  resq 1
  now_tv_nsec resq 1

section .data
  ; time constants
  CLOCK_REALTIME  equ 0
  HOURS_IN_DAY    equ 24
  EPOCH_YEAR      equ 1970
  
  SECONDS_IN_MINUTE equ 60
  SECONDS_IN_HOUR   equ 3600
  SECONDS_IN_DAY    equ 86400
  DAYS_IN_YEAR      equ 365
  DAYS_IN_LEAP_YEAR equ 366

  utc_str db "UTC", NULL_CHAR

  days_in_each_month db 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31

section .text
; convert unix to date
; @param  rdi: timestamp
; @return rax: pointer to dt struct
unix_to_date_time:
  sub   rsp, 0x8

  mov   qword [rsp], rdi

  ; second = timestamp % seconds_in_minute
  xor   rdx, rdx
  mov   rax, qword [rsp]
  mov   rbx, SECONDS_IN_MINUTE
  div   rbx

  mov   qword [dt_second], rdx

  ; timestamp /= seconds in minute
  mov   qword [rsp], rax

  ; minute = timestamp % seconds_in_minute
  xor   rdx, rdx
  mov   rbx, SECONDS_IN_MINUTE
  div   rbx

  mov   qword [dt_minute], rdx

  ; timestamp /= seconds_in_minute
  mov   qword [rsp], rax

  ; hour = timestamp % 24
  xor   rdx, rdx
  mov   rbx, HOURS_IN_DAY
  div   rbx

  mov   qword [dt_hour], rdx

  ; timestamp /= 24
  mov   qword [rsp], rax

  mov   qword [dt_year], EPOCH_YEAR

  ; calculate current year
.curr_year:
  ; check if year is leap_year
  mov   rdi, qword [dt_year]
  call  is_leap_year

  ; check if n_days is 366 or 365
  mov   rbx, DAYS_IN_LEAP_YEAR
  mov   rdx, DAYS_IN_YEAR

  cmp   rax, FALSE
  cmove rbx, rdx

  cmp   qword [rsp], rbx
  jl    .curr_month

  ; while loop
  ; sub timestamp 366 or 365 depending on is leap
  sub   qword [rsp], rbx

  ; inc year
  inc   qword [dt_year]

  jmp   .curr_year

.curr_month:
  mov   qword [dt_month], 0

.loop:
  ; calculate current month
  ; get n_days in month
  mov   rdi, qword [dt_month]
  mov   rsi, qword [dt_year]
  call  days_in_month

  cmp   qword [rsp], rax
  jle   .return

  sub   qword [rsp], rax
  inc   qword [dt_month]

  jmp   .loop

.return:
  mov   rax, qword [rsp]
  mov   qword [dt_day], rax

  lea   rax, [dt_year]
  add   rsp, 0x8
  ret

; return the number if days in the month
; @param  rdi: month
; @param  rsi: year
; @return rax: n_days
days_in_month:
  sub   rsp, 0x10

  mov   qword [rsp], rdi
  mov   qword [rsp+0x8], rsi

  ; check if month is two
  cmp   qword [rsp], 2
  jne   .skip_feb

  ; if yes, check if year is leap year
  mov   rdi, qword [rsp+0x8]
  call  is_leap_year

  cmp   rax, FALSE
  je    .skip_feb

  ; if yes, return 29
  mov   rax, 29
  jmp   .return

.skip_feb:
; return days of (month-1)
  mov   rbx, qword [rsp]
  dec   rbx

  xor   rax, rax
  mov   al, byte [days_in_each_month + rbx]

.return:
  ; clean stack
  add   rsp, 0x10
  ret

; return if the year in rdi is a leap year
; @param  rdi: year
; @return rax: (bool)is_leap_year
is_leap_year:
  sub   rsp, 0x8

  ; save year onto the stack
  mov   qword [rsp], rdi

  ; check if year % 4 is equal to 0
  xor   rdx, rdx
  mov   rax, qword [rsp]
  mov   rbx, 0x4
  div   rbx

  cmp   rdx, 0
  je    .check_mod_100

  jmp   .check_mod_400

.check_mod_100:
  ; if yes, check if year % 100 != 0

  xor   rdx, rdx
  mov   rax, qword [rsp]
  mov   rbx, 100
  div   rbx

  cmp   rdx, 0
  je    .is_not_leap

  jmp   .is_leap

.check_mod_400:
  ; check if year % 400 == 0
  xor   rdx, rdx
  mov   rax, qword [rsp]
  mov   rbx, 400
  div   rbx

  cmp   rdx, 0
  jne   .is_not_leap

.is_leap:
  ; if yes, return 1
  mov   rax, 1
  add   rsp, 0x8
  ret

.is_not_leap:
  ; return 0
  mov   rax, 0

  ; clean stack and return
  add   rsp, 0x8
  ret

; return now's unix timestamp TODO: func desc
; @return rax: timestamp
now:
  mov   rax, SYS_CLOCK_GETTIME
  mov   rdi, CLOCK_REALTIME
  lea   rsi, [now_tv_sec]
  syscall

  cmp   rax, 0
  jl    .error

  mov   rax, qword [now_tv_sec] 
  jmp   .return

.error:
  mov   rax, FAILURE_CODE

.return:
  ret

