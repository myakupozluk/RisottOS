; ============================================
; boot.asm - RisottOS Bootloader
; Loaded by BIOS at 0x7C00, runs in 16-bit real mode
; ============================================

[org 0x7C00]
bits 16

start:
    mov [bootdisk], dl    ; BIOS stores boot drive number in dl, save it
                             ; before we overwrite dl later

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

    jmp 0x1000                    ; jump to loaded kernel

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
; Data
; ============================================
bootmsg:         db "Booting RisottOS...", 0
bootdisk:         db 0
disk_error_msg: db "Disk read failed!", 0

; ============================================
; Padding + boot signature
; ============================================
times 510-($-$$) db 0
dw 0xAA55