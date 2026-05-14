.code

; memset implementation in assembly
memset proc
    ; RCX = dest, RDX = value, R8 = count
    mov rax, rcx        ; return dest
    mov r9, rcx         ; save dest
    mov rcx, r8         ; count
    mov al, dl          ; value
    rep stosb           ; fill memory
    mov rax, r9         ; restore dest for return
    ret
memset endp

; __chkstk implementation - stub for kernel mode
__chkstk proc
    ret
__chkstk endp

end
