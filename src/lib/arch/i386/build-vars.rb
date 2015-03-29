{
    cc: {
        cmd: 'gcc',
        flags: "-m32 -O2 -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -ffreestanding -nostartfiles -nostdinc -nodefaultlibs -g2 #{$inc_dirs.map { |d| "'-I#{d}'" } * ' '} -Wall -Wextra -pedantic -std=gnu1x -funsigned-char -fno-stack-protector -DX86 -DI386 -DLITTLE_ENDIAN -DHAS_VMEM -Wdouble-promotion -Wformat=2 -Winit-self -Wmissing-include-dirs -Wswitch-enum -Wsync-nand -Wunused -Wtrampolines -Wundef -Wno-endif-labels -Wshadow -Wunsafe-loop-optimizations -Wcast-align -Wwrite-strings -Wlogical-op -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Wnormalized=nfc -Wnested-externs -Winvalid-pch -Wdisabled-optimization -Woverlength-strings -fno-omit-frame-pointer -fno-tree-loop-distribute-patterns"
    },
    asm: {
        cmd: 'fasm',
        flags: '',
        out: '',
        quiet: true
    },
    objcp: {
        cmd: 'objcp',
        bin2elf: '-I binary -O elf32-i386 -B i386'
    },
    ar: {
        cmd: 'ar',
        flags: 'rcs'
    }
}
