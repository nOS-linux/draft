#!/bin/sh
set -e

KVER="$(uname -r)"
SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$SRCDIR"
ISODIR_TMP=$(mktemp -d)
INITRAMFS_FILE="$OUTDIR/nos-initramfs-${KVER}.img"

trap "rm -rf $ISODIR_TMP" EXIT

echo "=== nOS ISO builder ==="
echo "Kernel: $KVER"

if [ ! -f "$INITRAMFS_FILE" ]; then
    echo "ERROR: $INITRAMFS_FILE not found."
    echo "Run build-initramfs.sh first."
    exit 1
fi

mkdir -p "$ISODIR_TMP/boot/grub"

cp /boot/vmlinuz-linux-zen "$ISODIR_TMP/boot/vmlinuz-linux-zen"
cp "$INITRAMFS_FILE" "$ISODIR_TMP/boot/"

cat > "$ISODIR_TMP/boot/grub/grub.cfg" << GRUB
set timeout=5
set default=0

menuentry "nOS" {
    linux /boot/vmlinuz-linux-zen console=ttyS0
    initrd /boot/nos-initramfs-${KVER}.img
}
GRUB

ISO_OUT="$OUTDIR/nos-${KVER}.iso"
grub-mkrescue -o "$ISO_OUT" "$ISODIR_TMP"

echo "=== Done: nos-${KVER}.iso ($(du -h "$ISO_OUT" | cut -f1)) ==="
