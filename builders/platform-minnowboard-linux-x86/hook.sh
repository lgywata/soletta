#!/bin/bash

set -x

pacman --root=$ROOTFS --noconfirm -S \
       icu

cp -r soletta-target/build/soletta_sysroot/* $ROOTFS/

export http_proxy=http://proxy.jf.intel.com:911
export no_proxy=intel.com

wget http://ab01.bz.intel.com/drones/arch/linux-minnow-drone-4.1.4-1-x86_64.pkg.tar.xz
wget http://ab01.bz.intel.com/drones/arch/low-speed-spidev-minnow-drone-git-r15.2d52b1d-3-x86_64.pkg.tar.xz
pacman --root=$ROOTFS --noconfirm -U linux-minnow-drone-4.1.4-1-x86_64.pkg.tar.xz
pacman --root=$ROOTFS --noconfirm -U low-speed-spidev-minnow-drone-git-r15.2d52b1d-3-x86_64.pkg.tar.xz

cat > $ROOTFS/boot/loader/loader.conf <<EOF
default minnow-drone*
EOF

cat > $ROOTFS/boot/loader/entries/minnow-drone.conf <<EOF
title      Arch Linux
options    console=ttyS0,115200 console=tty0 rw quiet
linux      /vmlinuz-linux-minnow-drone
initrd     /initramfs-linux-minnow-drone.img
EOF

cat > $ROOTFS/etc/systemd/system/app.service <<EOF
[Unit]
Description=app

[Service]
ExecStart=/usr/bin/app

[Install]
WantedBy=multi-user.target
EOF

systemctl --root=$ROOTFS enable app.service
