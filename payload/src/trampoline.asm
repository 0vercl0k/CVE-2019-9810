; Axel '0vercl0k' Souchet - 3 May 2019

public trampoline_begin
public trampoline_data_address_hooked
public trampoline_data_address_original
public trampoline_savedoffbytes_space_start
public trampoline_savedoffbytes_space_end
public trampoline_end

.code
trampoline_begin:
  push rbp
  mov rbp, rsp
  and sp, 0fff0h

  push rcx
  push rdx
  push r8
  push r9

  ; give the callee the home space
  sub rsp, (4 * 8)
  mov rax, trampoline_data_address_hooked
  call rax
  add rsp, (4 * 8)

  pop r9
  pop r8
  pop rdx
  pop rcx

  mov rsp, rbp
  pop rbp

  ; Space for the saved off instructions
  trampoline_savedoffbytes_space_start:
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  trampoline_savedoffbytes_space_end:

  ; Branching back to the original function
  mov rax, trampoline_data_address_original
  jmp rax

  trampoline_data_address_hooked dq 0deadbeefbaadc0deh
  trampoline_data_address_original dq 0baadc0dedeadbeefh

trampoline_end:

end
