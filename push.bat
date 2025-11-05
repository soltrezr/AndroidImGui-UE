@echo off
set DEVICE_PATH=/data/local/tmp/libProject.so
adb push "build\libs\arm64-v8a\libProject.so" %DEVICE_PATH%
adb shell chmod 755 %DEVICE_PATH%
