panic() {
    TIMESTAMP=$(date +%H:%M:%S) # ?
    echo "panic: $1"
    exec /bin/sh # TODO: sh -> nrecovery
}

log() {
    echo "ninit: $1"
}
