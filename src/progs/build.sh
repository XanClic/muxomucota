#!/bin/bash
function find_dirs
{
    find common -name "*.$1" -exec dirname {} \;
    find arch/$ARCH -name "*.$1" -exec dirname {} \;
    find platform/$PLATFORM -name "*.$1" -exec dirname {} \;
}

source arch/$ARCH/build.vars

if [ "$PROGS" = "0" ]; then
    echo "Architecture '$ARCH' does not allow building programs." >&2
    exit 0
fi

if [ "$TARGET" = "clean" ]; then
    rm -f obj/* progs/*
    exit
fi

objdir="$(pwd)/obj"
progdir="$(pwd)/progs"
subdirs="$(find_dirs c) $(find_dirs asm) $(find_dirs S) $(find_dirs incbin)"
subdirs="$(echo "$subdirs" | tr ' ' '\n' | sort -u)"

mkdir -p "$objdir" "$progdir"

for subdir in $subdirs; do
    if [ -f "$subdir/.archs" ]; then
        if [ "$(grep -ce $ARCH $subdir/.archs)" = "0" ]; then
            continue
        fi
    fi

    echo "<<< $subdir >>>"
    objprefix=$(echo $subdir | sed -e 's/\//__/g')

    for file in $(ls $subdir/*.{c,asm,S,incbin} 2> /dev/null); do
        filename=$(basename $file)
        ext=$(echo $filename | sed -e 's/^.*\.//')
        output="$objdir/${objprefix}__$(echo $filename | tr . _).o"

        if [[ (!( -f $output )) || ( $(stat --printf=%Y $file) > $(stat --print=%Y $output) ) ]]; then
            case "$ext" in
                c)
                    echo "CC      $filename"
                    $CC $CFLAGS -c $file -o $output || exit 1
                    ;;
                asm|S)
                    echo "ASM     $filename"
                    if [ "$ASMQUIET" != "" ]; then
                        $ASM $ASMFLAGS $file $ASM_OUT $output &> /dev/null || exit 1
                    else
                        $ASM $ASMFLAGS $file $ASM_OUT $output || exit 1
                    fi
                    ;;
                incbin)
                    echo "OBJCP   $filename"
                    pushd "$subdir" &> /dev/null # Muss so gemacht werden, damit die Symbolnamen stimmen
                    $OBJCP $OBJCPBIN2ELF $filename $output || exit 1
                    popd &> /dev/null
                    ;;
            esac
        fi
    done

    echo "LD      >$subdir"
    $LD $LDFLAGS $objdir/${objprefix}__*.o ../lib/crt/*.o -L../lib -o "$progdir/$(basename $subdir)" -\( -lc $LIBS -\) || exit 1

    if [ "$STRIP" ]; then
        $STRIP $STRFLAGS $progdir/$(basename subdir)
    fi
done
