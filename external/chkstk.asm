.code

; __chkstk implementation for x64
; This function is called by the compiler when a function needs to allocate
; more than 4KB of stack space to ensure all stack pages are committed

PUBLIC __chkstk

__chkstk PROC
    ; On entry, RAX contains the number of bytes to allocate
    ; We need to probe the stack by touching each 4KB page
    
    push    rcx         ; Save RCX (it will be modified)
    push    rax         ; Save the original allocation size
    
    ; Calculate number of pages to probe (RAX / 4096)
    mov     rcx, rax
    shr     rcx, 12     ; Divide by 4096 (2^12)
    
    ; If less than one page, no probing needed
    test    rcx, rcx
    jz      done
    
probe_loop:
    ; Touch the stack page
    sub     rsp, 1000h  ; Move down 4KB (4096 bytes)
    mov     [rsp], al   ; Touch the page to commit it
    loop    probe_loop  ; Decrement RCX and loop if not zero
    
done:
    pop     rax         ; Restore original allocation size
    pop     rcx         ; Restore RCX
    ret

__chkstk ENDP

END
