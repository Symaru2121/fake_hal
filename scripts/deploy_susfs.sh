#!/system/bin/sh


set -e

OVERLAY_SRC="/data/adb/modules/fake_hal/vendor_overlay"
LOG="/data/adb/modules/fake_hal/fake_hal.log"

log() { echo "[$(date +'%H:%M:%S')] $*" >> "$LOG"; }

log "=== FakeHAL deploy starting ==="


if [ ! -f "$OVERLAY_SRC/lib64/hw/android.hardware.camera.provider-fake" ]; then
    log "ERROR: HAL binary not found at $OVERLAY_SRC"
    exit 1
fi


log "Hiding original camera HAL..."


setprop ctl.stop "vendor.camera-provider-2-7" 2>/dev/null || true
setprop ctl.stop "vendor.camera-provider-2-5" 2>/dev/null || true


sleep 1


FAKE_BIN="$OVERLAY_SRC/lib64/hw/android.hardware.camera.provider-fake"
TARGET_DIR="/vendor/bin/hw"

log "Installing FakeHAL binary..."

mount --bind "$FAKE_BIN" "$TARGET_DIR/android.hardware.camera.provider-fake" 2>/dev/null || \
    cp "$FAKE_BIN" "$TARGET_DIR/android.hardware.camera.provider-fake"

chmod 755 "$TARGET_DIR/android.hardware.camera.provider-fake"
chown root:shell "$TARGET_DIR/android.hardware.camera.provider-fake"


log "Installing VINTF fragment..."
VINTF_SRC="$OVERLAY_SRC/etc/vintf/fake_camera_hal.xml"
VINTF_DST="/vendor/etc/vintf/manifest/fake_camera_hal.xml"

mount --bind "$VINTF_SRC" "$VINTF_DST" 2>/dev/null || \
    cp "$VINTF_SRC" "$VINTF_DST"


log "Installing init.rc..."
RC_SRC="$OVERLAY_SRC/etc/init/fake_camera_hal.rc"
RC_DST="/vendor/etc/init/fake_camera_hal.rc"

if [ -f "$RC_SRC" ]; then
    mount --bind "$RC_SRC" "$RC_DST" 2>/dev/null || \
        cp "$RC_SRC" "$RC_DST"
fi


if command -v susfs >/dev/null 2>&1; then
    log "Applying SUSFS mounts hiding..."


    susfs hide_mounts \
        "$TARGET_DIR/android.hardware.camera.provider-fake" \
        "$VINTF_DST"


    susfs add_sus_path "$OVERLAY_SRC"

    log "SUSFS hiding applied"
else
    log "WARNING: susfs binary not found, mounts will be visible in /proc/mounts"
fi


log "Starting FakeHAL service..."


VIDEO_PATH="${FAKE_HAL_VIDEO:-/data/local/tmp/fake_video.mp4}"

if [ ! -f "$VIDEO_PATH" ]; then
    log "WARNING: video file not found: $VIDEO_PATH"
    log "Place your MP4 at: $VIDEO_PATH"
fi


nohup "$TARGET_DIR/android.hardware.camera.provider-fake" \
    "$VIDEO_PATH" \
    >> "$LOG" 2>&1 &

HAL_PID=$!
log "FakeHAL started with PID $HAL_PID"


sleep 2


if kill -0 $HAL_PID 2>/dev/null; then
    log "FakeHAL running OK (PID $HAL_PID)"
else
    log "ERROR: FakeHAL crashed on startup, check log"
fi


log "Restarting cameraserver..."
setprop ctl.restart cameraserver
sleep 1
setprop ctl.restart mediaserver

log "=== Deploy complete ==="
echo "FakeHAL deployed. Video: $VIDEO_PATH"
echo "Logs: $LOG"
