#!/bin/sh
set -e

KERNEL_SRC="$(cd "$(dirname "$0")/../linux-6.18.38" && pwd)"
SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$SRCDIR"
INITRAMFS_TMP=$(mktemp -d)
KVER=$(make -C "$KERNEL_SRC" -s kernelrelease 2>/dev/null)

trap "rm -rf $INITRAMFS_TMP" EXIT

echo "nOS initramfs builder"
echo "Kernel: $KVER"

mkdir -p "$INITRAMFS_TMP"/{bin,dev,etc/ninit/services,root/.ssh,proc,sys,mnt,run}

cp /usr/bin/busybox "$INITRAMFS_TMP/bin/busybox"

for applet in mount umount cat grep cut seq sleep mkdir mdev sh ls \
              switch_root findfs blkid date echo clear dmesg \
              modprobe insmod rmmod lsmod readlink ln \
              ps pidof kill lsusb lspci free df uname; do
    ln -s /bin/busybox "$INITRAMFS_TMP/bin/$applet"
done

for applet in mount switch_root findfs blkid modprobe insmod rmmod lsmod; do
    mkdir -p "$(dirname "$INITRAMFS_TMP/sbin/$applet")"
    ln -s /bin/busybox "$INITRAMFS_TMP/sbin/$applet" 2>/dev/null || true
done

mkdir -p "$INITRAMFS_TMP/etc"
cp "$SRCDIR/etc/os-release" "$INITRAMFS_TMP/etc/os-release"
cp "$SRCDIR/etc/passwd" "$INITRAMFS_TMP/etc/"
cp "$SRCDIR/etc/shadow" "$INITRAMFS_TMP/etc/"
cp "$SRCDIR/ninit/stage1/init.sh" "$INITRAMFS_TMP/init"
cp "$SRCDIR/ninit/stage2/ninit" "$INITRAMFS_TMP/ninit"
cp "$SRCDIR/ninit/stage1/n.sh" "$INITRAMFS_TMP/"
cp "$SRCDIR/bin/pfetch" "$INITRAMFS_TMP/bin/pfetch"

chmod +x "$INITRAMFS_TMP/init" "$INITRAMFS_TMP/n.sh"

INITRAMFS_OUT="$OUTDIR/nos-initramfs-${KVER}.img"
(
    cd "$INITRAMFS_TMP"
    find . -print0 | cpio --null -o --format=newc 2>/dev/null | gzip -9 > "$INITRAMFS_OUT"
)

echo "Done: nos-initramfs-${KVER}.img ($(du -h "$INITRAMFS_OUT" | cut -f1))"
