#!/bin/bash


VIDEO_FILE=${1:-}

MODULE_DIR="ksu_module"
ZIP_NAME="FakeHAL.zip"

echo "Packaging KernelSU module..."


cp build_production/fake_hal_main $MODULE_DIR/
cp vendor_overlay/etc/vintf/manifest.xml $MODULE_DIR/
cp vendor_overlay/etc/init/fake_camera_hal.rc $MODULE_DIR/

if [ -n "$VIDEO_FILE" ] && [ -f "$VIDEO_FILE" ]; then
    cp "$VIDEO_FILE" $MODULE_DIR/fake_video.mp4
fi


cd $MODULE_DIR
zip -r ../$ZIP_NAME ./*
cd ..

echo "Module packaged as $ZIP_NAME"
echo "Install via KernelSU Manager"
