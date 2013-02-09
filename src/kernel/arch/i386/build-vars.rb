{
    cc: {
        cmd: 'gcc',
        flags: "-m32 -masm=intel -O3 -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4 -mno-sse4.1 -mno-sse4.2 -mno-sse4a -mno-avx -mno-avx2 -mno-3dnow -ffreestanding -nostartfiles -nostdinc -nodefaultlibs -g2 #{$inc_dirs.map { |d| "'-I#{d}'" } * ' '} -Wall -Wextra -pedantic -std=gnu1x -funsigned-char -fno-stack-protector -DKERNEL -DX86 -DI386 -DLITTLE_ENDIAN -DHAS_VMEM -Wdouble-promotion -Wformat=2 -Winit-self -Wmissing-include-dirs -Wswitch-enum -Wsync-nand -Wunused -Wtrampolines -Wundef -Wno-endif-labels -Wshadow -Wwrite-strings -Wlogical-op -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Wnormalized=nfc -Wnested-externs -Winvalid-pch -Wdisabled-optimization -Woverlength-strings -fno-omit-frame-pointer"
    },
    asm: {
        cmd: 'fasm',
        flags: '',
        out: '',
        quiet: true
    },
    objcp: {
        cmd: 'objcopy',
        bin2elf: '-I binary -O elf32-i386 -B i386'
    },
    ld: {
        cmd: 'ld',
        flags: "-melf_i386 -e 0x00100000 -T arch/i386/link.ld '-L#{File.dirname(`gcc -m32 -print-libgcc-file-name`)}'",
        libs: '-lgcc'
    }
}

