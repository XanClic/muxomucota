#!/bin/bash
function find_dirs
{
    find common -name "*.$1" -exec dirname {} \;
    find arch/$ARCH -name "*.$1" -exec dirname {} \;
    find platform/$PLATFORM -name "*.$1" -exec dirname {} \;
}

source arch/$ARCH/build.vars

if [ "$TARGET" = "clean" ]; then
    rm -f obj/* crt/*.o libc.a libm.a
    exit
fi

objdir="$(pwd)/obj"
subdirs="$(find_dirs c) $(find_dirs asm) $(find_dirs S) $(find_dirs incbin)"
subdirs="$(echo "$subdirs" | tr ' ' '\n' | sort -u)"

mkdir -p "$objdir" crt

for subdir in $subdirs; do
    echo "<<< $subdir >>>"
    if [ "$(basename $subdir)" = "math" ]; then
        objprefix=m_$(echo $subdir | sed -e 's/\//__/g')
    else
        objprefix=c_$(echo $subdir | sed -e 's/\//__/g')
    fi

    for file in $(ls $subdir/*.{c,asm,S,incbin} 2> /dev/null); do
        filename=$(basename $file)
        ext=$(echo $filename | sed -e 's/^.*\.//')
        if [ "$(basename $subdir)" = "crt" ]; then
            output="$(pwd)/crt/$(basename $filename .$ext).o"
        else
            output=$objdir/${objprefix}__$(echo $filename | tr . _).o
        fi

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
done

echo "AR      >libc.a"
$AR $ARFLAGS libc.a $objdir/c_*.o || exit 1
if ls $objdir/m_*.o &> /dev/null; then
    echo "AR      >libm.a"
    $AR $ARFLAGS libm.a $objdir/m_*.o || exit 1
fi
