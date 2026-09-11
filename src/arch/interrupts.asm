[bits 32]

global interrupt_handler
global timer_interrupt_handler
extern keyboard_handler
extern timer_handler

interrupt_handler:
    ; Save registers for keyboard IRQ
    pusha
    call keyboard_handler
    popa
    iretd

timer_interrupt_handler:
    ; Save registers for timer IRQ
    pusha
    call timer_handler
    popa
    iretd