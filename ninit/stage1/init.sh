#!/bin/sh

# ninit stage 1
# copyleft relya 2026

# repo: https://github.com/nOS-linux/ninit
# docs: https://nos.relya.ru/docs

. ./n.sh

echo '          ____  _____'
echo '   ____  / __ \/ ___/'
echo '  / __ \/ / / /\__ \'
echo ' / / / / /_/ /___/ /'
echo '/_/ /_/\____//____/'
echo ''

echo "--- ninit stage 1 ---"

# TODO: mount -> nstorage mount
log "mounting proc/sys/tmpfs/devtmpfs"
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /run
mount -t devtmpfs devtmpfs /dev

log "scanning devices"
busybox mdev -s # TODO: mdev -> ndev

TARGET_UUID=$(cat /proc/cmdline | grep -o 'root=UUID=[^ ]*' | cut -d= -f3)

log "looking for root device"
ROOT=""
TIMEOUT=5 # 5 * 0.1 = 0.5 sec
for i in $(seq 1 $TIMEOUT); do
    ROOT=$(findfs UUID=$TARGET_UUID 2>/dev/null)
    if [ -n "$ROOT" ]; then
        break
    fi
    sleep 0.1
done

if [ -z "$ROOT" ]; then
    log "root not found, switching to stage 2"
    exec /sbin/ninit
fi

mkdir -p /new_root
mount -o ro "$ROOT" /new_root # TODO: mount -> nstorage mount

log "switching root"
exec busybox switch_root /new_root /sbin/ninit # TODO: switch_root -> nstorage switch_root
