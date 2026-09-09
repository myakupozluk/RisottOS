; ============================================
; boot.asm - RisottOS Bootloader
; Loaded by BIOS at 0x7C00, runs in 16-bit real mode
; ============================================

[org 0x7C00]
bits 16

start:
    mov [bootdisk], dl    ; BIOS stores boot drive number in dl, save it
                            ; before we overwrite dl later

    mov ax, 0x2401          ; enable A20 line so we can access memory
    int 0x15                  ; above 1MB (needed later for kernel growth)

    mov si, bootmsg
    call print              ; print boot message

    ; --- Read kernel from disk (CHS addressing) ---
    mov ah, 0x02             ; BIOS function: read sectors
    mov al, 1                 ; number of sectors to read
    mov ch, 0                  ; cylinder 0
    mov cl, 2                  ; sector 2 (sector 1 is this bootloader)
    mov dh, 0                  ; head 0
    mov dl, [bootdisk]         ; drive to read from
    mov bx, 0x1000              ; load kernel to address 0x1000
    int 0x13
    jc disk_error                ; carry flag set = read failed

    cli                             ; turning interrupts off temporarily
    lgdt [gdt_descriptor]            ; loading GDT

    mov eax, cr0                      ; cr0 can't be written directly,
    or eax, 1                          ; must go through a register
    mov cr0, eax                        ; set bit 0 -> enable protected mode

    jmp 0x08:protected_mode_start        ; far jump: flush pipeline,
                                            ; load code segment 0x08 (gdt_code)

    

; ============================================
; print - prints a null-terminated string
; input: si = address of string
; ============================================
print:
.loop:
    lodsb                ; al = [si], si++
    or al, al             ; check for null terminator
    jz .done

    mov ah, 0x0E            ; BIOS teletype output
    int 0x10

    jmp .loop

.done:
    ret

; ============================================
; disk_error - prints error message and halts
; ============================================
disk_error:
    mov si, disk_error_msg
    call print
    jmp $                    ; halt forever

; ============================================
; GDT (Global Descriptor Table)
;
; Purpose: Protected mode no longer uses segment:offset math like real
; mode did. Instead, segment registers become indexes into this table,
; which tells the CPU where each segment starts, how big it is, and
; what it's allowed to do (executable, writable, etc).
;
; We use a "flat model": both segments start at 0 and cover the full
; 4GB address space, so in practice segmentation doesn't restrict us
; and we rely on paging later instead.
; ============================================
gdt_start:
    dq 0                      ; null descriptor - required by the CPU, unused

gdt_code:                     ; code segment descriptor
    dw 0xFFFF                  ; limit (bits 0-15) - segment size
    dw 0x0000                   ; base (bits 0-15) - segment starts at 0
    db 0x00                      ; base (bits 16-23)
    db 10011010b                  ; access byte: present, ring 0, code, readable
    db 11001111b                   ; flags (4KB granularity, 32-bit) + limit (16-19)
    db 0x00                          ; base (bits 24-31)    

gdt_data:                     ; data segment descriptor
    dw 0xFFFF                  ; limit (bits 0-15)
    dw 0x0000                   ; base (bits 0-15)
    db 0x00                      ; base (bits 16-23)
    db 10010010b                  ; access byte: present, ring 0, data, writable
    db 11001111b                   ; flags (4KB granularity, 32-bit) + limit (16-19)
    db 0x00                          ; base (bits 24-31)

gdt_end:

; This structure tells the CPU where the GDT is and how big it is.
; The lgdt instruction (next step) will load this into the CPU.
gdt_descriptor:
    dw gdt_end - gdt_start - 1    ; size of GDT minus 1 (CPU convention)
    dd gdt_start                    ; address of the GDT itself


[bits 32]
protected_mode_start:
    mov ax, 0x10          ; gdt_data selector
    mov ds, ax               ; point data segments to it
    mov es, ax
    mov ss, ax

    mov esp, 0x90000          ; set up a fresh 32-bit stack

    mov byte [0xB8780], 'P'      ; write near middle of screen instead of top-left
    mov byte [0xB8782], 'M'

    jmp 0x1000                      ; placeholder, kernel jump comes here later

    

; ============================================
; Data
; ============================================
bootmsg:         db 13, 10, "Booting RisottOS...", 13, 10, 0
bootdisk:         db 0
disk_error_msg: db "Disk read failed!", 13, 10, 0

; ============================================
; Padding + boot signature
; ============================================
times 510-($-$$) db 0
dw 0xAA55