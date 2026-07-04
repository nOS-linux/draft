#!/bin/sh

# ninit stage 1
# copyleft relya 2026

# repo: https://github.com/nOS-linux/ninit
# docs: https://nos.relya.ru/docs

. ./n.sh

echo "--- ninit stage 1 ---"

# TODO: mount -> nstorage mount
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /run
mount -t devtmpfs devtmpfs /dev

busybox mdev -s # TODO: mdev -> ndev

TARGET_UUID=$(cat /proc/cmdline | grep -o 'root=UUID=[^ ]*' | cut -d= -f3)

ROOT=""
TIMEOUT=50 # 50 * 0.1 = 5 sec
for i in $(seq 1 $TIMEOUT); do
    ROOT=$(findfs UUID=$TARGET_UUID 2>/dev/null)
    if [ -n "$ROOT" ]; then
        break
    fi
    sleep 0.1
done

if [ -z "$ROOT" ]; then
    panic "root not found"
fi

mkdir -p /new_root
mount -o ro "$ROOT" /new_root # TODO: mount -> nstorage mount

exec busybox switch_root /new_root /sbin/ninit # TODO: switch_root -> nstorage switch_root
