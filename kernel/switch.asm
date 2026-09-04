[BITS 32]

[GLOBAL irq0_stub]

[EXTERN irq0_handler_c]
[EXTERN scheduler_tick]

; ---------------------------------------------------------------------------
; IRQ0 Timer Interrupt + Context Switch
;
; When IRQ0 occurs, CPU already pushed:
;
;     EFLAGS
;     CS
;     EIP
;
; PUSHAD then saves the general-purpose registers.
;
; We give the resulting ESP to scheduler_tick().
; scheduler_tick() returns the ESP that should be restored.
; ---------------------------------------------------------------------------

irq0_stub:
    ; Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    pushad

    ; Keep C code running with the normal direction flag.
    cld

    ; Handle timer tick and send PIC EOI.
    call irq0_handler_c

    ; Pass the saved context ESP to:
    ;
    ; uint32_t scheduler_tick(uint32_t current_esp)
    ;
    push esp
    call scheduler_tick

    ; Remove scheduler_tick argument.
    add esp, 4

    ; scheduler_tick returns the selected process ESP in EAX.
    mov esp, eax

    ; Restore registers belonging to selected process.
    popad

    ; Restore EIP, CS and EFLAGS.
    iretd