#!/bin/bash


VIDEO_FILE=${1:-fake_video.mp4}

echo "Deploying FakeHAL to device..."


adb push build_production/fake_hal_main /data/local/tmp/


adb push vendor_overlay/etc/vintf/manifest.xml /data/local/tmp/


adb push vendor_overlay/etc/init/fake_camera_hal.rc /data/local/tmp/


if [ -f "$VIDEO_FILE" ]; then
    adb push "$VIDEO_FILE" /data/local/tmp/fake_video.mp4
else
    echo "Warning: $VIDEO_FILE not found, using existing on device"
fi


adb shell chmod 755 /data/local/tmp/fake_hal_main

echo "Files deployed. Run install_susfs.sh on device with root."
