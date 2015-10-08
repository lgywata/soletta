#!/bin/bash

if [ -z "$1" -o -z "$2" ]; then
    exit 1
fi

if [ ! -d "$2" ]; then
    exit 1
fi

DEV=$(losetup -f --show "$1")

echo $DEV

trap '
    ret=$?;
    set +e;
    umount $ROOTFS
    losetup -d $DEV
    if [[ $ret -ne 0 ]]; then
        echo FAILED TO COPY >&2
    fi
    exit $ret;
    ' EXIT

# clean up after ourselves no matter how we die.
trap 'exit 1;' SIGINT

ROOT=${DEV##*/}

SYSTEM_PART=/dev/${ROOT}p2

ROOTFS="/run/installer-$ROOT/system"
mkdir -p $ROOTFS

mount $SYSTEM_PART $ROOTFS

cp -r $2/* $ROOTFS/

