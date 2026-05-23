#!/bin/bash
# EasyRTC log tools
# Usage:
#   ./log.sh clean        -- clear log on device
#   ./log.sh dump [name]  -- dump log to local file
#   ./log.sh                -- clean then wait for dump (default)

PKG=cn.easyrtc
LOG_PATH="/data/data/$PKG/files/easyrtc_native.log"

clean_log() {
    echo -n "Clearing native log... "
    adb shell "run-as $PKG rm -f $LOG_PATH 2>&1" && echo "OK" || echo "FAILED"
}

dump_log() {
    local OUT="${1:-easyrtc_native_$(date +%Y%m%d_%H%M%S).log}"
    adb shell "run-as $PKG cat $LOG_PATH 2>&1" > "$OUT"
    local sz=$(wc -c < "$OUT" | tr -d ' ')
    echo "Dumped to $OUT (${sz} bytes)"
}

case "${1:-}" in
    clean)
        clean_log
        ;;
    dump)
        dump_log "$2"
        ;;
    *)
        clean_log
        dump_log "$2"
        ;;
esac
