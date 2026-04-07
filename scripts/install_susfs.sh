#!/system/bin/sh


susfs_mount


cp /data/local/tmp/fake_hal_main /vendor/bin/


cp /data/local/tmp/manifest.xml /vendor/etc/vintf/


cp /data/local/tmp/fake_camera_hal.rc /vendor/etc/init/


chmod 755 /vendor/bin/fake_hal_main
chmod 644 /vendor/etc/vintf/manifest.xml
chmod 644 /vendor/etc/init/fake_camera_hal.rc


killall cameraserver
sleep 2
start cameraserver

echo "Fake HAL installed via SUSFS"
