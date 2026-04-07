#!/system/bin/sh


MODDIR="${0%/*}"
VIDEO_PATH="/data/local/tmp/fake_video.mp4"
LOG="$MODDIR/fake_hal.log"


log_msg() {
    echo "[$(date)] $1" >> "$LOG"
}


wait_boot() {
    local timeout=120
    local i=0
    while [ "$(getprop sys.boot_completed)" != "1" ]; do
        sleep 1
        i=$((i + 1))
        if [ "$i" -ge "$timeout" ]; then
            log_msg "ERROR: Boot timeout after ${timeout}s"
            exit 1
        fi
    done
}

wait_boot
sleep 3

log_msg "FakeHAL service starting..."


stop vendor.camera-provider-2-7 2>/dev/null || true

sleep 1


FAKE_HAL="$MODDIR/vendor_overlay/bin/hw/android.hardware.camera.provider-fake"

if [ ! -f "$FAKE_HAL" ]; then
    log_msg "ERROR: FakeHAL binary not found at $FAKE_HAL"
    exit 1
fi

if [ ! -f "$VIDEO_PATH" ]; then
    log_msg "WARNING: Video not found at $VIDEO_PATH"
fi


install_file() {
    local src="$1"
    local dst="$2"
    local perms="$3"

    if [ ! -f "$src" ]; then
        log_msg "WARNING: Source not found: $src"
        return 1
    fi


    if mount -o bind "$src" "$dst" 2>/dev/null; then
        log_msg "Bind-mounted $src -> $dst"
    else

        cp "$src" "$dst" 2>/dev/null
        if [ $? -ne 0 ]; then
            log_msg "ERROR: Failed to install $src -> $dst"
            return 1
        fi
        log_msg "Copied $src -> $dst"
    fi

    chmod "$perms" "$dst" 2>/dev/null
    chcon u:object_r:vendor_file:s0 "$dst" 2>/dev/null
    return 0
}


if [ -f "$MODDIR/vendor_overlay/etc/vintf/manifest/fake_camera_hal.xml" ]; then
    install_file "$MODDIR/vendor_overlay/etc/vintf/manifest/fake_camera_hal.xml" \
                 "/vendor/etc/vintf/manifest/fake_camera_hal.xml" "644"
fi


if [ -f "$MODDIR/vendor_overlay/etc/init/fake_camera_hal.rc" ]; then
    install_file "$MODDIR/vendor_overlay/etc/init/fake_camera_hal.rc" \
                 "/vendor/etc/init/fake_camera_hal.rc" "644"
fi


if [ -f "$MODDIR/fake_video.mp4" ] && [ ! -f "$VIDEO_PATH" ]; then
    cp "$MODDIR/fake_video.mp4" "$VIDEO_PATH"
    chmod 644 "$VIDEO_PATH"
    log_msg "Copied bundled video to $VIDEO_PATH"
fi


nohup "$FAKE_HAL" "$VIDEO_PATH" >> "$LOG" 2>&1 &
log_msg "FakeHAL PID: $!"


sleep 2
setprop ctl.restart cameraserver

log_msg "FakeHAL service started successfully"
