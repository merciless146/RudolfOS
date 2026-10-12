ENTRY(_start)
SECTIONS { . = 0x037F8000; .text : { *(.text.start) *(.text*) *(.rodata*) *(.data*) *(.bss*) *(COMMON) } }
