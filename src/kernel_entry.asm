[bits 32]
[extern kmain]      ;kernel.c main function

global _start
_start:
    call kmain      ;calling kernel main function
    jmp $           ;just dont trust :D