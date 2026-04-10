#!/bin/bash


set -e

MODULE_DIR="${1:-./fake_hal_module}"
VIDEO_FILE="${2:-}"

mkdir -p "$MODULE_DIR"/{META-INF/com/google/android,system/bin,vendor_overlay/{bin/hw,etc/{vintf/manifest,init},lib64/hw}}


cat > "$MODULE_DIR/module.prop" << 'EOF'
id=fake_hal_camera
name=FakeHAL Camera Provider
version=v1.0
versionCode=100
author=YourNick
description=Camera HAL level video injection. No Java hooks.
updateJson=
EOF


echo "#MAGISK" > "$MODULE_DIR/META-INF/com/google/android/updater-script"
echo "#!/sbin/sh" > "$MODULE_DIR/META-INF/com/google/android/update-binary"
chmod +x "$MODULE_DIR/META-INF/com/google/android/update-binary"


cat > "$MODULE_DIR/customize.sh" << 'CUSTOMIZE'


SKIPUNZIP=1


unzip -o "$ZIPFILE" 'vendor_overlay/*' -d "$MODPATH" >&2


set_perm_recursive "$MODPATH/vendor_overlay/bin" root shell 0755 0755
set_perm "$MODPATH/vendor_overlay/bin/hw/android.hardware.camera.provider-fake" \
    root shell 0755


case "$DEVICE" in
    panther|cheetah|lynx|flame|coral)
        ui_print "Device $DEVICE is supported"
        ;;
    *)
        ui_print "WARNING: Device $DEVICE is not in supported list"
        ui_print "Supported: panther/cheetah/lynx/flame/coral"
        ui_print "Installing anyway, manual tuning may be needed"
        ;;
esac

ui_print "FakeHAL installed!"
ui_print "Place your MP4 at: /data/local/tmp/fake_video.mp4"
ui_print "Then reboot."
CUSTOMIZE
chmod +x "$MODULE_DIR/customize.sh"


cat > "$MODULE_DIR/service.sh" << 'SERVICE'


MODDIR="${0%/*}"
VIDEO_PATH="/data/local/tmp/fake_video.mp4"
LOG="$MODDIR/fake_hal.log"

wait_boot() {
    while [ "$(getprop sys.boot_completed)" != "1" ]; do
        sleep 1
    done
}

wait_boot
sleep 3

echo "[$(date)] FakeHAL service starting..." >> "$LOG"


stop vendor.camera-provider-2-7 2>/dev/null || true

sleep 1


FAKE_HAL="$MODDIR/vendor_overlay/bin/hw/android.hardware.camera.provider-fake"

if [ ! -f "$FAKE_HAL" ]; then
    echo "[ERROR] FakeHAL binary not found" >> "$LOG"
    exit 1
fi

if [ ! -f "$VIDEO_PATH" ]; then
    echo "[WARNING] Video not found: $VIDEO_PATH" >> "$LOG"
fi

nohup "$FAKE_HAL" "$VIDEO_PATH" >> "$LOG" 2>&1 &
echo "[$(date)] FakeHAL PID: $!" >> "$LOG"


sleep 2
setprop ctl.restart cameraserver

echo "[$(date)] Done" >> "$LOG"
SERVICE
chmod +x "$MODULE_DIR/service.sh"


touch "$MODULE_DIR/vendor_overlay/bin/hw/android.hardware.camera.provider-fake"
echo "# Replace with compiled binary from build.sh" > "$MODULE_DIR/README_INSTALL.txt"

cat >> "$MODULE_DIR/README_INSTALL.txt" << 'EOF'

INSTALLATION STEPS:
==================

1. BUILD:
   Set AOSP_ROOT and run:
   $ ./scripts/build.sh

   Output: out_fake_hal/android.hardware.camera.provider-fake

2. COPY binary:
   cp out_fake_hal/android.hardware.camera.provider-fake \
      fake_hal_module/vendor_overlay/bin/hw/

3. COPY video:
   adb push your_video.mp4 /data/local/tmp/fake_video.mp4

4. PATCH vbmeta (once):
   adb reboot bootloader
   ./scripts/vbmeta_patch.sh /path/to/avbtool.py
   fastboot reboot

5. INSTALL module via KSUI app:
   zip -r fake_hal_module.zip fake_hal_module/
   Install via KernelSU Manager -> Install from file

6. REBOOT and verify:
   adb logcat -s FakeHAL_Provider FakeHAL_Device FakeHAL_Jpeg

TROUBLESHOOTING:
===============
- "no cameras available": check VINTF manifest installed correctly
  adb shell cat /vendor/etc/vintf/manifest/fake_camera_hal.xml

- Camera app crashes: check logcat for cameraserver errors
  adb logcat -s cameraserver CameraProvider

- Video not playing: check file permissions and path
  adb shell ls -la /data/local/tmp/fake_video.mp4

CHANGING VIDEO AT RUNTIME:
=========================
adb push new_video.mp4 /data/local/tmp/fake_video.mp4
adb shell setprop ctl.restart fake_camera_hal
EOF

echo "[+] Module structure created at: $MODULE_DIR"
ls -la "$MODULE_DIR"
