#!/bin/bash

usage() {
    echo "Usage: sudo ./$(basename $0) <device to be flashed, e.g., /dev/mmcblk0>" 1>&$1;
}

if [ -z "$1" ]; then
    echo "No device supplied, aborting..."
    usage 1
    exit 1
fi

if [[ -b "$1" ]]; then
    dd if=flash.img of=$1 bs=3M conv=fsync
else
    echo "$1 is not a device! Aborting..."
    exit 1
fi
