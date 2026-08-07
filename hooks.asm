
EXTERN after_Perfect_OG : QWORD
EXTERN after_Perfect_NGE : QWORD

EXTERN after_Shadows_OG : QWORD
EXTERN after_Shadows_NGE : QWORD

EXTERN impl_Shadows : PROTO C


.data


.code

Perfect_OG PROC
    ; stolen
    lea r11,[rsp+2B0h]
    mov rbx,[r11+48h]

    ; save
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push rsi
    push rdi

    sub rsp, 144


    ; CacheCameraData()
    mov rcx,[rsi+660h]
    mov rax,[rcx]
    call qword ptr [rax+158h]


    add rsp, 144

    ; restore
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq


    jmp after_Perfect_OG

Perfect_OG ENDP

Perfect_NGE PROC
    ; stolen
    mov rcx,[rsi+1F0h]
    mov r15,[rsp+588h]

    ; save
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push rsi
    push rdi

    sub rsp, 144


    ; CacheCameraData()
    mov rcx,[rsi+660h]
    mov rax,[rcx]
    call qword ptr [rax+158h]


    add rsp, 144

    ; restore
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq


    jmp after_Perfect_NGE

Perfect_NGE ENDP

Shadows_OG PROC
    ; stolen
    mov rdx,rbx
    mov rcx,rdi
    mov rbx,[rsp+30h]
    add rsp,20h
    pop rdi

    ; save
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push rsi
    push rdi

    sub rsp, 40


    ; cpp
    ; this already in rcx

    mov rax, impl_Shadows
    call rax


    add rsp, 40

    ; restore
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq

    jmp after_Shadows_OG

Shadows_OG ENDP


Shadows_NGE PROC
    ; stolen
    mov rdx,rsi
    mov rcx,rbx
    mov rbx,[rsp+40h]
    add rsp,20h
    pop rsi

    ; save
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push rsi
    push rdi

    sub rsp, 40


    ; cpp
    ; this already in rcx

    mov rax, impl_Shadows
    call rax


    add rsp, 40

    ; restore
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq   

    mov r11, after_Shadows_NGE
    jmp r11

Shadows_NGE ENDP

END
