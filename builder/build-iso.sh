#!/bin/sh
set -e

KERNEL_SRC="$(cd "$(dirname "$0")/../linux-6.18.38" && pwd)"
SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$SRCDIR"
ISODIR_TMP=$(mktemp -d)
KVER=$(make -C "$KERNEL_SRC" -s kernelrelease 2>/dev/null)
INITRAMFS_FILE="$OUTDIR/nos-initramfs-${KVER}.img"
KERNEL_IMAGE="$KERNEL_SRC/arch/x86/boot/bzImage"

trap "rm -rf $ISODIR_TMP" EXIT

echo "nOS ISO builder"
echo "Kernel: $KVER"

if [ ! -f "$INITRAMFS_FILE" ]; then
    echo "ERROR: $INITRAMFS_FILE not found."
    echo "Run build-initramfs.sh first."
    exit 1
fi

if [ ! -f "$KERNEL_IMAGE" ]; then
    echo "ERROR: $KERNEL_IMAGE not found."
    echo "Build the kernel first (make -C $KERNEL_SRC bzImage)."
    exit 1
fi

mkdir -p "$ISODIR_TMP/boot/grub"

cp "$KERNEL_IMAGE" "$ISODIR_TMP/boot/vmlinuz-nos-${KVER}"
cp "$INITRAMFS_FILE" "$ISODIR_TMP/boot/"

cat > "$ISODIR_TMP/boot/grub/grub.cfg" << GRUB
set timeout=5
set default=0

menuentry "nOS" {
    linux /boot/vmlinuz-nos-${KVER} console=ttyS0 console=tty1 loglevel=0 hostname=nOS
    initrd /boot/nos-initramfs-${KVER}.img
}
GRUB

ISO_OUT="$OUTDIR/nos-${KVER}.iso"
grub-mkrescue -o "$ISO_OUT" "$ISODIR_TMP"

echo "Done: nos-${KVER}.iso ($(du -h "$ISO_OUT" | cut -f1))"
