#!/bin/bash
function echo_help()
{
    echo 'Usage: ./build.sh --arch=<architecture> --platform=<platform> --image=<image> [target]'
    echo '  Architecture may be one of the following:'
    echo "    $(ls --color=none src/kernel/arch)"
    echo '  Platform may be one of the following:'
    echo "    $(ls --color=none src/kernel/platform)"
    echo '  Image may be one of the following:'
    echo "$(ls build/scripts/*-*-*.sh | sed -e 's/build\/scripts\/\(.*\)-\(.*\)\.sh/    \1: \2/')"
    echo '  The target may be either all or clean (all is default).'
    echo
    echo 'Current setting:'
    echo "  Architecture: $ARCH"
    echo "  Platform:     $PLATFORM"
    echo "  Image:        $IMAGE"
}

ARCH="$(uname -m)"

if [ "$ARCH" == "x86" ]; then
    ARCH=i386
elif [ "$ARCH" == "x86_64" ]; then
    if [ -d "src/kernel/arch/x64" ]
    then
        ARCH=x64
    else
        ARCH=i386
    fi
elif [ "$(echo $ARCH | head -c 3)" == "arm" ]; then
    ARCH=arm
fi

case "$ARCH" in
    i386|x64)
        PLATFORM=pc
        ;;
    arm)
        PLATFORM=rpi
        ;;
    *)
        PLATFORM=default
        ;;
esac

IMAGE=default

TARGET=

for var in $@; do
    if [ $var = '-h' -o $var = '-?' -o $var = '--help' ]; then
        echo_help
        exit 0
    elif [ $(echo $var | head -c 7) = '--arch=' ]; then
        ARCH=$(echo $var | tail -c +8)
    elif [ $(echo $var | head -c 11) = '--platform=' ]; then
        PLATFORM=$(echo $var | tail -c +12)
    elif [ $(echo $var | head -c 8) = '--image=' ]; then
        IMAGE=$(echo $var | tail -c +9)
    elif [ -z "$TARGET" ]; then
        TARGET=$var
    else
        echo "Unknown parameter '$var'." >&2
    fi
done

if [ -z "$ARCH" -o -z "$PLATFORM" -o -z "$IMAGE" ]; then
    echo_help
    exit 1
fi

if [ -z "$TARGET" ]; then
    TARGET=all
elif [ $TARGET != all -a $TARGET != clean ]; then
    echo "Unsupported target $TARGET." >&2
    exit 1
fi

IMAGE_SCRIPT=build/scripts/$ARCH-$PLATFORM-$IMAGE.sh

if [ ! -x $IMAGE_SCRIPT ]; then
    echo "The given architecture/platform/image type combination is not supported" >&2
    exit 1
fi


src_subdirs=$(ls src -p | grep / | grep -v include | sed -e 's/\(.*\)\//\1/')

export include_dirs="$(pwd)/src/include/common $(pwd)/src/include/arch/$ARCH $(pwd)/src/include/platform/$PLATFORM"


export ARCH
export PLATFORM
export TARGET

for src_subdir in $src_subdirs; do
    if [ "$TARGET" != "clean" ]; then
        echo "——— $src_subdir ———"
    fi

    pushd src/$src_subdir &> /dev/null
    ./build.sh || exit 1
    popd &> /dev/null
done

mkdir -p build/images

if [ "$TARGET" == "all" ]; then
    #build initrd here
    echo "——— image ———"
    $IMAGE_SCRIPT || exit 1
else
    rm -rf build/images/*
fi
