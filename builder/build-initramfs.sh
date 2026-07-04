#!/bin/sh
set -e

KVER="$(uname -r)"
SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$SRCDIR"
INITRAMFS_TMP=$(mktemp -d)
MODULES_SRC="/lib/modules/$KVER"

trap "rm -rf $INITRAMFS_TMP" EXIT

echo "=== nOS initramfs builder ==="
echo "Kernel: $KVER"

mkdir -p "$INITRAMFS_TMP"/{bin,dev,etc,proc,sys,mnt,run,new_root}

cp /usr/bin/busybox "$INITRAMFS_TMP/bin/busybox"

for applet in mount umount cat grep cut seq sleep mkdir mdev sh ls \
              switch_root findfs blkid date echo clear dmesg \
              modprobe insmod rmmod lsmod readlink ln; do
    ln -s /bin/busybox "$INITRAMFS_TMP/bin/$applet"
done

for applet in mount switch_root findfs blkid modprobe insmod rmmod lsmod; do
    mkdir -p "$(dirname "$INITRAMFS_TMP/sbin/$applet")"
    ln -s /bin/busybox "$INITRAMFS_TMP/sbin/$applet" 2>/dev/null || true
done

cp "$SRCDIR/ninit/stage1/init.sh" "$INITRAMFS_TMP/init"
cp "$SRCDIR/ninit/stage2/ninit" "$INITRAMFS_TMP/sbin/ninit"
cp "$SRCDIR/ninit/stage1/n.sh" "$INITRAMFS_TMP/"
chmod +x "$INITRAMFS_TMP/init" "$INITRAMFS_TMP/n.sh"

if [ -d "$MODULES_SRC/kernel/drivers/nvme" ]; then
    echo "  -> NVMe modules"
    mkdir -p "$INITRAMFS_TMP/lib/modules/$KVER"
    for mod in nvme-keyring hkdf nvme-auth nvme-core nvme; do
        modpath=$(modinfo -k "$KVER" -n "$mod" 2>/dev/null) || continue
        relpath="${modpath#$MODULES_SRC/kernel/}"
        mkdir -p "$INITRAMFS_TMP/lib/modules/$KVER/kernel/$(dirname "$relpath")"
        case "$modpath" in
            *.zst)
                zstd -dfq "$modpath" -o "/tmp/$(basename "${modpath%.zst}")"
                cp "/tmp/$(basename "${modpath%.zst}")" \
                   "$INITRAMFS_TMP/lib/modules/$KVER/kernel/${relpath%.zst}"
                ;;
            *) cp "$modpath" "$INITRAMFS_TMP/lib/modules/$KVER/kernel/$relpath" ;;
        esac
    done
    if [ -f "$MODULES_SRC/modules.dep" ]; then
        cp "$MODULES_SRC/modules.dep" "$INITRAMFS_TMP/lib/modules/$KVER/"
        sed -i 's/\.zst//g' "$INITRAMFS_TMP/lib/modules/$KVER/modules.dep"
    fi
fi

INITRAMFS_OUT="$OUTDIR/nos-initramfs-${KVER}.img"
(
    cd "$INITRAMFS_TMP"
    find . -print0 | cpio --null -o --format=newc 2>/dev/null | gzip -9 > "$INITRAMFS_OUT"
)

echo "=== Done: nos-initramfs-${KVER}.img ($(du -h "$INITRAMFS_OUT" | cut -f1)) ==="
