OUTPUT_ARCH("i386:i386")
OUTPUT_FORMAT("elf32-i386")
physical = 0x00100000;
virtual = 0xF0100000;

SECTIONS
{
    .entry virtual : AT(physical)
    {
        entry = virtual;
        *(.entry)
    }
    .multiboot :
    {
        . = ALIGN(4);
        multiboot = .;
        *(.multiboot)
    }
    .text :
    {
        . = ALIGN(4);
        code = .;
        *(.text)
    }
    .data :
    {
        . = ALIGN(4);
        data = .;
        *(.data)
    }
    .bss :
    {
        . = ALIGN(4);
        bss = .;
        *(.bss)
        *(COMMON)
    }
    .largedata :
    {
        . = ALIGN(4096);
        largedata = .;
        *(.largedata)
    }

    __kernel_elf_end = .;
}

