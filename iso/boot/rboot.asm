; =====================================================================
;  Rudolf Boot - BIOS boot loader for Rudolf OS
;  Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
;
;  Starts from a CD/DVD (El Torito, no emulation) or from a USB stick /
;  hard disk (via the Rudolf MBR). Shows a boot menu, picks a VESA
;  graphics mode (EDID preferred resolution when possible), loads the
;  Linux kernel (bzImage, initramfs built in) using the Linux x86 boot
;  protocol and starts it.
;
;  The ISO builder patches the 'RBTB' table below with the kernel's
;  position (2048-byte block number) and size in bytes.
;
;  Memory map while running:
;    0x00600  chain-load trampoline       0x07C00  this loader (16 KiB)
;    0x0C000  scratch buffers (VBE, EDID, DAP, drive parameters)
;    0x10000  kernel real-mode part, heap, command line (64 KiB)
;    0x20000  disk bounce buffer (32 KiB)    0x100000  kernel proper
; =====================================================================
bits 16
org 0x7C00

%define VBEINFO   0xC000
%define MODEINFO  0xC200
%define EDIDBUF   0xC300
%define MODELIST  0xC400          ; up to 64 entries: mode, w, h, bpp (4 words)
%define DAP       0xC800
%define DRVPARM   0xC900
%define KSEG      0x1000          ; real-mode kernel segment (linear 0x10000)
%define BOUNCE    0x20000
%define CHUNK     32768
%define CMDLINE   0x1E000         ; linear address of the kernel command line
%define HEAPEND   0xE000

start:
        jmp short entry
        nop
        align 8, db 0
tbl:    db 'RBTB'
k_lba:  dd 0                      ; kernel start, 2048-byte blocks
k_size: dd 0                      ; kernel size, bytes
verstr: db 'Rudolf OS 25', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        times 32-($-verstr) db 0

entry:
        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00
        jmp 0:entry2              ; normalise CS (El Torito may use 07C0:0000)
entry2:
        sti
        cld
        mov [drive], dl
        mov [hdmode], bx          ; 'HD' when started by the Rudolf MBR

        ; ---- bytes per sector of the boot device ----
        mov word [secsize], 2048
        cmp word [hdmode], 'HD'
        jne .q
        mov word [secsize], 512
.q:     mov si, DRVPARM
        mov word [si], 0x1E
        mov ah, 0x48
        mov dl, [drive]
        int 0x13
        jc .ssdone
        mov ax, [DRVPARM+0x18]
        cmp ax, 512
        je .ssok
        cmp ax, 2048
        jne .ssdone
.ssok:  mov [secsize], ax
.ssdone:
        call enable_a20
        call probe_video
        mov byte [sel], 0

; ---------------------------------------------------------------- menu
menu:
        call draw_menu
        ; 5 second countdown until a key is pressed
        mov byte [counting], 1
        mov byte [secs], 5
        mov eax, [0x46C]
        mov [tick0], eax
.loop:
        cmp byte [counting], 0
        je .wait
        mov eax, [0x46C]
        sub eax, [tick0]
        cmp eax, 18
        jb .wait
        add dword [tick0], 18
        dec byte [secs]
        call draw_count
        cmp byte [secs], 0
        je go
.wait:
        mov ah, 1
        int 0x16
        jz .loop
        mov ah, 0
        int 0x16
        cmp byte [counting], 0
        je .key
        mov byte [counting], 0
        push ax
        call draw_count
        pop ax
.key:   cmp ah, 0x48              ; up
        jne .k1
        cmp byte [sel], 0
        je .loop
        dec byte [sel]
        call draw_items
        jmp .loop
.k1:    cmp ah, 0x50              ; down
        jne .k2
        cmp byte [sel], NITEMS-1
        je .loop
        inc byte [sel]
        call draw_items
        jmp .loop
.k2:    cmp al, 13                ; Enter
        je go
        cmp al, 'd'               ; d = debug start (serial console)
        jne .k3
        xor byte [debug], 1
        mov si, s_dbg
        mov dx, 0x1408
        mov bl, 0x1C
        cmp byte [debug], 0
        jne .kd
        mov si, s_nodbg
.kd:    call putsat
        jmp .loop
.k3:
        cmp al, '1'
        jb .loop
        cmp al, '0'+NITEMS
        ja .loop
        sub al, '1'
        mov [sel], al
        call draw_items
        jmp go

go:
        mov al, [sel]
        cmp al, 4
        je chain_hdd
        cmp al, 3
        jne .g1
        call pick_mode            ; screen resolution chooser
        jmp menu
.g1:    ; build the command line
        mov di, cmdbuf
        mov si, cl_base
        call strcpy
        cmp byte [sel], 1
        jne .g2
        mov si, cl_recovery
        call strcpy
.g2:    cmp byte [sel], 2
        jne .g3
        mov si, cl_check
        call strcpy
.g3:    cmp byte [debug], 0
        je .g3b
        mov si, cl_debug
        call strcpy
.g3b:   cmp word [hdmode], 'HD'
        jne .g4
        mov si, cl_usb
        call strcpy
.g4:    mov byte [di], 0
        jmp boot_kernel

; ---------------------------------------------------------------- screen
cls:    mov ax, 0x0003            ; 80x25 colour text
        int 0x10
        mov ah, 1                 ; hide cursor
        mov cx, 0x2000
        int 0x10
        mov ax, 0x0600            ; fill with blue
        mov bh, 0x1F
        xor cx, cx
        mov dx, 0x184F
        int 0x10
        ret

; print zero-terminated string at row DH, col DL, attribute BL
putsat: push si
        mov ah, 2
        xor bh, bh
        int 0x10
.n:     lodsb
        or al, al
        jz .e
        mov ah, 9
        mov cx, 1
        xor bh, bh
        int 0x10
        inc dl
        mov ah, 2
        int 0x10
        jmp .n
.e:     pop si
        ret

; fill a row segment: row DH col DL, CX chars, attribute BL
fillrow:
        mov ah, 2
        xor bh, bh
        int 0x10
        mov ax, 0x0920
        int 0x10
        ret

draw_menu:
        call cls
        mov dx, 0x0000
        mov cx, 80
        mov bl, 0x70
        call fillrow
        mov si, s_title
        mov dx, 0x0002
        mov bl, 0x70
        call putsat
        mov si, verstr
        mov dx, 0x0040
        mov bl, 0x70
        call putsat
        ; antler logo
        mov si, s_logo1
        mov dx, 0x0322
        mov bl, 0x1E
        call putsat
        mov si, s_logo2
        mov dx, 0x0422
        call putsat
        mov si, s_logo3
        mov dx, 0x0522
        call putsat
        mov si, s_choose
        mov dx, 0x0708
        mov bl, 0x1F
        call putsat
        call draw_items
        call draw_mode
        mov si, s_help
        mov dx, 0x1508
        mov bl, 0x17
        call putsat
        mov si, s_help2
        mov dx, 0x1608
        call putsat
        mov dx, 0x1800
        mov cx, 80
        mov bl, 0x70
        call fillrow
        mov si, s_foot
        mov dx, 0x1802
        mov bl, 0x70
        call putsat
        ret

draw_items:
        xor cx, cx
.l:     push cx
        mov dh, cl
        shl dh, 1
        add dh, 9
        mov dl, 8
        mov bl, 0x1F
        cmp cl, [sel]
        jne .n
        mov bl, 0x71
.n:     push bx
        push dx
        mov cx, 64
        call fillrow
        pop dx
        pop bx
        pop cx
        push cx
        mov si, cx
        shl si, 1
        mov si, [items+si]
        add dl, 2
        call putsat
        pop cx
        inc cx
        cmp cx, NITEMS
        jb .l
        call draw_mode
        ret

draw_count:
        mov dx, 0x1308
        mov cx, 64
        mov bl, 0x1F
        call fillrow
        cmp byte [counting], 0
        je .e
        mov al, [secs]
        add al, '0'
        mov [s_cnt_n], al
        mov si, s_cnt
        mov dx, 0x1308
        mov bl, 0x1E
        call putsat
.e:     ret

; show the selected graphics mode in the "Screen resolution" item
draw_mode:
        mov di, s_modeval
        cmp word [vmode], 0
        jne .m
        mov si, s_nomode
        call strcpy
        mov byte [di], 0
        jmp .p
.m:     mov ax, [vw]
        call utoa
        mov byte [di], 'x'
        inc di
        mov ax, [vh]
        call utoa
        mov byte [di], 0
.p:     mov dx, 0x0F2C
        mov cx, 28
        mov bl, 0x1F
        cmp byte [sel], 3
        jne .q
        mov bl, 0x71
.q:     push bx
        call fillrow
        pop bx
        mov si, s_modeval
        mov dx, 0x0F2C
        call putsat
        ret

; ---------------------------------------------------------------- strings / numbers
strcpy: lodsb                     ; copy SI -> DI without the terminator
        or al, al
        jz .e
        stosb
        jmp strcpy
.e:     ret

utoa:   ; AX -> decimal at DI
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.d:     xor dx, dx
        div bx
        push dx
        inc cx
        or ax, ax
        jnz .d
.o:     pop ax
        add al, '0'
        stosb
        loop .o
        pop dx
        pop cx
        pop bx
        ret

; ---------------------------------------------------------------- A20
enable_a20:
        mov ax, 0x2401
        int 0x15
        in al, 0x92
        test al, 2
        jnz .ok
        or al, 2
        and al, 0xFE
        out 0x92, al
.ok:    ret

; ---------------------------------------------------------------- unreal mode
unreal:
        cli
        push ds
        push es
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp $+2
        mov bx, 0x08
        mov ds, bx
        mov es, bx
        and al, 0xFE
        mov cr0, eax
        pop es
        pop ds
        sti
        ret

align 8
gdt:    dq 0
        dq 0x00CF92000000FFFF     ; flat 4 GiB data segment
gdt_end:
gdtr:   dw gdt_end - gdt - 1
        dd gdt

; copy ECX bytes from linear ESI to linear EDI (DS = ES = 0, big limits)
copy32: push ecx
        push esi
        push edi
        call unreal
        pop edi
        pop esi
        pop ecx
        a32 rep movsb
        ret

; ---------------------------------------------------------------- disk
; read CX sectors starting at device sector EAX into linear address EDI (<1MiB)
readsec:
        mov si, DAP
        mov word [si], 0x0010
        mov [si+2], cx
        mov bx, di
        and bx, 0x000F
        mov [si+4], bx
        push edi
        shr edi, 4
        mov [si+6], di
        pop edi
        mov [si+8], eax
        mov dword [si+12], 0
        mov byte [retries], 3
.r:     mov si, DAP
        mov ah, 0x42
        mov dl, [drive]
        int 0x13
        jnc .ok
        dec byte [retries]
        jz .bad
        xor ah, ah
        mov dl, [drive]
        int 0x13
        jmp .r
.ok:    ret
.bad:   mov si, s_ioerr
        jmp fatal

; ---------------------------------------------------------------- VBE
probe_video:
        mov word [vmode], 0
        mov word [nmodes], 0
        mov word [prefw], 0
        mov word [prefh], 0
        ; EDID preferred timing
        mov ax, 0x4F15
        mov bl, 1
        xor cx, cx
        xor dx, dx
        mov di, EDIDBUF
        int 0x10
        cmp ax, 0x004F
        jne .noedid
        cmp dword [EDIDBUF], 0xFFFFFF00
        jne .noedid
        movzx ax, byte [EDIDBUF+58]
        shr ax, 4
        shl ax, 8
        mov al, [EDIDBUF+56]
        mov [prefw], ax
        movzx ax, byte [EDIDBUF+61]
        shr ax, 4
        shl ax, 8
        mov al, [EDIDBUF+59]
        mov [prefh], ax
.noedid:
        mov dword [VBEINFO], 'VBE2'
        mov ax, 0x4F00
        mov di, VBEINFO
        int 0x10
        cmp ax, 0x004F
        jne .done
        mov si, [VBEINFO+14]      ; mode list far pointer
        mov fs, [VBEINFO+16]
.next:  mov cx, [fs:si]
        add si, 2
        cmp cx, 0xFFFF
        je .pick
        cmp word [nmodes], 64
        jae .pick
        push si
        mov ax, 0x4F01
        mov di, MODEINFO
        push cx
        int 0x10
        pop cx
        pop si
        cmp ax, 0x004F
        jne .next
        mov ax, [MODEINFO]        ; attributes: supported, colour, graphics, linear
        and ax, 0x0099
        cmp ax, 0x0099
        jne .next
        mov al, [MODEINFO+0x1B]   ; memory model: packed (4) or direct colour (6)
        cmp al, 6
        je .mm
        cmp al, 4
        jne .next
.mm:    mov al, [MODEINFO+0x19]   ; bits per pixel
        cmp al, 32
        je .keep
        cmp al, 24
        je .keep
        cmp al, 16
        jne .next
.keep:  mov bx, [MODEINFO+0x12]
        cmp bx, 640
        jb .next
        mov dx, [MODEINFO+0x14]
        cmp dx, 480
        jb .next
        mov di, [nmodes]
        shl di, 3
        add di, MODELIST
        mov [di], cx
        mov [di+2], bx
        mov [di+4], dx
        movzx ax, byte [MODEINFO+0x19]
        mov [di+6], ax
        inc word [nmodes]
        jmp .next
.pick:  ; best mode: EDID preferred size at 32 bpp, else largest <= 1920x1200 by bpp then area
        xor bp, bp                ; best score high word kept in [bestscore]
        mov dword [bestscore], 0
        xor cx, cx
.p1:    cmp cx, [nmodes]
        jae .done
        mov di, cx
        shl di, 3
        add di, MODELIST
        mov bx, [di+2]
        mov dx, [di+4]
        cmp bx, 1920
        ja .p2
        cmp dx, 1200
        ja .p2
        movzx eax, bx
        movzx ebx, dx
        imul eax, ebx             ; area
        shr eax, 8
        cmp word [di+6], 32
        jne .np
        add eax, 0x01000000
.np:    mov bx, [di+2]
        cmp bx, [prefw]
        jne .sc
        mov bx, [di+4]
        cmp bx, [prefh]
        jne .sc
        cmp word [di+6], 32
        jne .sc
        add eax, 0x40000000
.sc:    cmp eax, [bestscore]
        jbe .p2
        mov [bestscore], eax
        mov ax, [di]
        mov [vmode], ax
        mov ax, [di+2]
        mov [vw], ax
        mov ax, [di+4]
        mov [vh], ax
.p2:    inc cx
        jmp .p1
.done:  ret

; let the user choose a resolution from the list (arrow keys, Enter, Esc)
pick_mode:
        cmp word [nmodes], 0
        je .ret
        mov word [mcur], 0
.draw:  call cls
        mov si, s_restitle
        mov dx, 0x0102
        mov bl, 0x1F
        call putsat
        mov cx, 0
.row:   cmp cx, [nmodes]
        jae .rows
        cmp cx, 20
        jae .rows
        push cx
        mov si, cx
        shl si, 3
        add si, MODELIST
        push si
        mov di, s_tmp
        mov ax, [si+2]
        call utoa
        mov byte [di], 'x'
        inc di
        mov ax, [si+4]
        call utoa
        mov byte [di], ','
        inc di
        mov byte [di], ' '
        inc di
        mov ax, [si+6]
        call utoa
        mov si, s_bit
        call strcpy
        mov byte [di], 0
        pop si
        pop cx
        push cx
        mov dh, cl
        add dh, 3
        mov dl, 6
        mov bl, 0x1F
        cmp cx, [mcur]
        jne .nb
        mov bl, 0x71
.nb:    push bx
        push dx
        push cx
        mov cx, 30
        call fillrow
        pop cx
        pop dx
        pop bx
        add dl, 2
        mov si, s_tmp
        call putsat
        pop cx
        inc cx
        jmp .row
.rows:  mov ah, 0
        int 0x16
        cmp ah, 0x48
        jne .k1
        cmp word [mcur], 0
        je .rows
        dec word [mcur]
        jmp .draw
.k1:    cmp ah, 0x50
        jne .k2
        mov ax, [mcur]
        inc ax
        cmp ax, [nmodes]
        jae .rows
        cmp ax, 20
        jae .rows
        mov [mcur], ax
        jmp .draw
.k2:    cmp al, 27
        je .ret
        cmp al, 13
        jne .rows
        mov di, [mcur]
        shl di, 3
        add di, MODELIST
        mov ax, [di]
        mov [vmode], ax
        mov ax, [di+2]
        mov [vw], ax
        mov ax, [di+4]
        mov [vh], ax
.ret:   ret

; ---------------------------------------------------------------- boot Linux
boot_kernel:
        call cls
        mov si, s_loading
        mov dx, 0x0A1C
        mov bl, 0x1F
        call putsat
        mov dx, 0x0C0F            ; progress bar frame
        mov cx, 50
        mov bl, 0x10
        call fillrow
        cmp dword [k_size], 0
        jne .have
        mov si, s_nokernel
        jmp fatal
.have:
        ; device sector of the kernel and sectors per chunk
        mov eax, [k_lba]
        cmp word [secsize], 512
        jne .s2k
        shl eax, 2
.s2k:   mov [devlba], eax
        mov ax, CHUNK
        xor dx, dx
        div word [secsize]
        mov [chunksec], ax
        mov dword [done], 0
        mov dword [setupsz], 0
.chunk: mov eax, [done]
        cmp eax, [k_size]
        jae .loaded
        mov eax, [devlba]
        mov cx, [chunksec]
        mov edi, BOUNCE
        call readsec
        movzx eax, word [chunksec]
        add [devlba], eax
        ; bytes in this chunk
        mov ecx, [k_size]
        sub ecx, [done]
        cmp ecx, CHUNK
        jbe .n1
        mov ecx, CHUNK
.n1:    mov [chunkn], ecx
        cmp dword [done], 0
        jne .notfirst
        ; first chunk holds the setup header
        push es
        mov ax, BOUNCE >> 4
        mov es, ax
        cmp dword [es:0x202], 'HdrS'
        jne .badk
        cmp word [es:0x206], 0x0206
        jb .badk
        test byte [es:0x211], 1   ; LOADED_HIGH (bzImage)
        jz .badk
        movzx eax, byte [es:0x1F1]
        or eax, eax
        jnz .ss
        mov eax, 4
.ss:    inc eax
        shl eax, 9
        mov [setupsz], eax
        pop es
        jmp .notfirst
.badk:  pop es
        mov si, s_badkernel
        jmp fatal
.notfirst:
        ; copy the part below setupsz to 0x10000, the rest to 0x100000
        mov esi, BOUNCE
        mov ebx, [done]           ; file offset of this chunk
        mov edx, [chunkn]
.cp:    or edx, edx
        jz .cpd
        cmp ebx, [setupsz]
        jae .high
        mov ecx, [setupsz]
        sub ecx, ebx
        cmp ecx, edx
        jbe .lw
        mov ecx, edx
.lw:    mov edi, 0x10000
        add edi, ebx
        jmp .do
.high:  mov ecx, edx
        mov edi, ebx
        sub edi, [setupsz]
        add edi, 0x100000
.do:    add ebx, ecx
        sub edx, ecx
        push ebx
        push edx
        call copy32
        pop edx
        pop ebx
        jmp .cp
.cpd:   mov eax, [chunkn]
        add [done], eax
        call progress
        jmp .chunk

.loaded:
        ; fill in the boot protocol header
        push ds
        mov ax, KSEG
        mov es, ax
        mov byte [es:0x210], 0xFF          ; type_of_loader: undefined
        or byte [es:0x211], 0x80           ; CAN_USE_HEAP
        or byte [es:0x211], 0x20           ; QUIET_FLAG: no decompressor chatter
        mov word [es:0x224], HEAPEND - 0x200
        mov dword [es:0x228], CMDLINE
        mov dword [es:0x218], 0            ; no separate initrd (built in)
        mov dword [es:0x21C], 0
        mov ax, [vmode]
        or ax, ax
        jz .nov
        add ax, 0x0200                     ; VIDEO_FIRST_VESA + VBE mode number
        mov [es:0x1FA], ax
        jmp .cmd
.nov:   mov word [es:0x1FA], 0xFFFF        ; normal VGA
.cmd:   ; copy the command line to KSEG:HEAPEND
        mov si, cmdbuf
        mov di, HEAPEND
.cl:    lodsb
        stosb
        or al, al
        jnz .cl
        pop ds
        ; jump to the kernel's real-mode entry
        cli
        mov ax, KSEG
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov sp, HEAPEND
        jmp KSEG+0x20:0

progress:
        ; blocks = done * 50 / k_size
        mov eax, [done]
        xor edx, edx
        mov ecx, 50
        mul ecx
        mov ecx, [k_size]
        div ecx
        or ax, ax
        jz .e
        mov cx, ax
        mov dx, 0x0C0F
        mov bl, 0x2A
        call fillrow
.e:     ret

; ---------------------------------------------------------------- chain-load hard disk
chain_hdd:
        call cls
        mov si, tramp
        mov di, 0x0600
        mov cx, tramp_end - tramp
        rep movsb
        jmp 0:0x0600
tramp:  ; runs at 0x0600: read sector 0 of disk 0x80 into 0x7C00 and start it
        mov ax, 0x0201
        mov bx, 0x7C00
        mov cx, 0x0001
        mov dx, 0x0080
        int 0x13
        jc .f
        cmp word [0x7DFE], 0xAA55
        jne .f
        mov dl, 0x80
        jmp 0:0x7C00
.f:     int 0x18
tramp_end:

; ---------------------------------------------------------------- errors
fatal:  push si
        call cls
        pop si
        mov dx, 0x0A08
        mov bl, 0x4F
        call putsat
        mov si, s_presskey
        mov dx, 0x0C08
        mov bl, 0x1F
        call putsat
        mov ah, 0
        int 0x16
        int 0x19
        jmp $

; ---------------------------------------------------------------- data
NITEMS  equ 5
items:  dw it1, it2, it3, it4, it5
it1:    db '1  Start Rudolf OS', 0
it2:    db '2  Recovery mode (start with default settings)', 0
it3:    db '3  Check this disc / USB drive for errors', 0
it4:    db '4  Screen resolution:', 0
it5:    db '5  Boot from the first hard disk', 0
s_title: db 'Rudolf Boot', 0
s_logo1: db '\|/     \|/', 0
s_logo2: db ' \_______/', 0
s_logo3: db '  ( o   o )    Rudolf OS', 0
s_choose: db 'Choose how to start:', 0
s_help:  db 'Up/Down to choose, Enter to start, 1-5 for a shortcut.', 0
s_help2: db 'UEFI computers start Rudolf OS directly (no menu).', 0
s_foot:  db 'GPL-2.0-or-later  -  Secure Boot must be off on UEFI computers', 0
s_cnt:   db 'Starting automatically in '
s_cnt_n: db '5 seconds...', 0
s_nomode: db 'text mode (no VESA)', 0
s_modeval: times 24 db 0
s_restitle: db 'Choose a screen resolution (Enter = use, Esc = back):', 0
s_bit:   db ' bit', 0
s_tmp:   times 32 db 0
s_loading: db 'Loading Rudolf OS ...', 0
s_ioerr: db 'Disk read error while loading Rudolf OS.', 0
s_nokernel: db 'This disc has no kernel (image was not finalised).', 0
s_badkernel: db 'The kernel file is damaged or not a bzImage.', 0
s_presskey: db 'Press any key to restart.', 0
cl_base: db 'rudolf.boot=bios', 0
cl_recovery: db ' rudolf.recovery=1', 0
cl_check: db ' rudolf.check=1', 0
cl_usb:  db ' rudolf.media=disk', 0
cl_debug: db ' rudolf.debug=1 console=tty0 console=ttyS0,115200 loglevel=6', 0
s_dbg:   db 'Debug start: serial console on COM1     ', 0
s_nodbg: db '                                         ', 0
debug:   db 0

drive:   db 0
retries: db 0
counting: db 0
secs:    db 0
sel:     db 0
hdmode:  dw 0
secsize: dw 2048
vmode:   dw 0
vw:      dw 0
vh:      dw 0
nmodes:  dw 0
mcur:    dw 0
prefw:   dw 0
prefh:   dw 0
chunksec: dw 0
tick0:   dd 0
bestscore: dd 0
devlba:  dd 0
done:    dd 0
chunkn:  dd 0
setupsz: dd 0
cmdbuf:  times 160 db 0

        times 16384-($-$$) db 0
