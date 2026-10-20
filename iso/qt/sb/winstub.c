/* A stand-in for Windows Boot Manager in the boot-menu test (sb_t.py part G, t125b_t.py): an EFI program that
   prints RUDOLF-WINDOWS-STANDIN on the console (OVMF copies it to the serial port), waits 3 s and restarts the
   computer. Built as \EFI\Microsoft\Boot\bootmgfw.efi on a test disk:
     clang --target=x86_64-pc-win32-coff -ffreestanding -fno-stack-protector -mno-red-zone -O2 -c winstub.c
     lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib /out:bootmgfw.efi winstub.o
   GPL-2.0-or-later. */
typedef unsigned long long u64; typedef unsigned short u16;
typedef u64 (__attribute__((ms_abi)) *fn)();
#define AT(p, off) (*(void **)((char *)(p) + (off)))
u64 __attribute__((ms_abi)) efi_main(void *image, void *st)
{
	static u16 msg[] = u"\r\nRUDOLF-WINDOWS-STANDIN\r\n";
	void *out = AT(st, 64), *bs = AT(st, 96), *rt = AT(st, 88);
	((fn)AT(out, 8))(out, msg);              /* ConOut->OutputString */
	((fn)AT(bs, 248))((u64)3000000);         /* BootServices->Stall, microseconds */
	((fn)AT(rt, 104))(0, 0, 0, 0);           /* RuntimeServices->ResetSystem(EfiResetCold) */
	return 0;
}
