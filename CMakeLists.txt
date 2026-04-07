cmake_minimum_required(VERSION 3.16)
project(fake_hal_production)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(DEFINED CMAKE_ANDROID_NDK)
    set(NDK_ROOT "${CMAKE_ANDROID_NDK}")
elseif(DEFINED ENV{ANDROID_NDK})
    set(NDK_ROOT "$ENV{ANDROID_NDK}")
elseif(DEFINED ENV{ANDROID_HOME})

    file(GLOB NDK_VERSIONS "$ENV{ANDROID_HOME}/ndk/*")
    if(NDK_VERSIONS)
        list(SORT NDK_VERSIONS)
        list(GET NDK_VERSIONS -1 NDK_ROOT)
    endif()
endif()

if(NDK_ROOT)

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(NDK_HOST_TAG "darwin-x86_64")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(NDK_HOST_TAG "linux-x86_64")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(NDK_HOST_TAG "windows-x86_64")
    endif()

    set(NDK_SYSROOT "${NDK_ROOT}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/sysroot/usr/include")
    set(NDK_INCLUDE_DIRS
        ${NDK_SYSROOT}
        ${NDK_SYSROOT}/android
        ${NDK_ROOT}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/sysroot/usr/include/c++/v1
    )
    message(STATUS "NDK sysroot: ${NDK_SYSROOT}")
else()
    set(NDK_INCLUDE_DIRS "")
    message(STATUS "No Android NDK found — building without NDK sysroot includes")
endif()


add_library(fake_hal_core STATIC
    src/MetadataRandomizer.cpp
    src/NoiseOverlay.cpp
    src/GyroWarp.cpp
    src/JpegEncoder.cpp
)

target_include_directories(fake_hal_core PUBLIC
    include
    ${NDK_INCLUDE_DIRS}
)

target_link_libraries(fake_hal_core PUBLIC
    android
    log
    camera2ndk
    mediandk
    jpeg
)

target_compile_options(fake_hal_core PRIVATE
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
)


add_library(fake_hal_full STATIC
    src/FakeCameraDevice.cpp
    src/FakeCameraProvider.cpp
    src/VideoFrameReader.cpp
    src/GrallocHelper.cpp
    src/TimestampSync.cpp
)

target_include_directories(fake_hal_full PUBLIC
    include
    ${NDK_INCLUDE_DIRS}
)

target_link_libraries(fake_hal_full PUBLIC
    fake_hal_core
    android
    log
    camera2ndk
    mediandk
    jpeg
    binder_ndk
)

target_compile_options(fake_hal_full PRIVATE
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-missing-field-initializers
)


add_executable(fake_hal_main
    src/main.cpp
)

target_include_directories(fake_hal_main PRIVATE
    include
    ${NDK_INCLUDE_DIRS}
)

target_link_libraries(fake_hal_main
    fake_hal_full
    android
    log
    camera2ndk
    mediandk
    jpeg
    binder_ndk
)

target_compile_options(fake_hal_main PRIVATE
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-missing-field-initializers
)
