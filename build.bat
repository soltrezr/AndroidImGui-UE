@echo off

mkdir build
cd /d build

set "NDK=D:\android-ndk-r28-ollvm"
set "ANDROID_ABI=arm64-v8a"

cmake -G "Ninja" -DANDROID_ABI=%ANDROID_ABI% -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%NDK%\build\cmake\android.toolchain.cmake" ..
ninja -j8

cd ..
call push.bat