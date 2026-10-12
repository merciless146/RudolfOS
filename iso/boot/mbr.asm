; Rudolf OS hybrid MBR: lets a USB stick or hard disk holding the ISO
; image start Rudolf Boot. GPL-2.0-or-later.
bits 16
org 0x0600
        cli
        xor ax, ax
        mov ss, ax
        mov sp, 0x7C00
        mov ds, ax
        mov es, ax
        sti
        cld
        mov si, 0x7C00
        mov di, 0x0600
        mov cx, 256
        rep movsw
        jmp 0:reloc
reloc:  mov [drv], dl
        mov ah, 0x41              ; LBA extensions present?
        mov bx, 0x55AA
        int 0x13
        jc noext
        mov si, dap
        mov ah, 0x42
        mov dl, [drv]
        int 0x13
        jc ioerr
        cmp dword [0x7C00+8], 'RBTB'
        jne ioerr
        mov dl, [drv]
        mov bx, 'HD'
        jmp 0:0x7C00
noext:  mov si, m_noext
        jmp short pr
ioerr:  mov si, m_err
pr:     lodsb
        or al, al
        jz $
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp pr
m_noext: db 'Rudolf OS: this BIOS cannot read large disks.', 0
m_err:   db 'Rudolf OS: could not load the boot loader.', 0
drv:    db 0
        times 0x194-($-$$) db 0
        db 'RBMB'                 ; patched by the ISO builder
dap:    db 0x10, 0
        dw 32                     ; sectors (16 KiB)
        dw 0x7C00, 0x0000
lba:    dd 0, 0                   ; 512-byte LBA of RBOOT.BIN (patched)
