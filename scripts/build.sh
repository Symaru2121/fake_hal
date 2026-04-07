#!/bin/bash


set -euo pipefail


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[*]${NC} $*"; }
ok()    { echo -e "${GREEN}[+]${NC} $*"; }
warn()  { echo -e "${YELLOW}[~]${NC} $*"; }
error() { echo -e "${RED}[!]${NC} $*" >&2; }


STANDALONE="${STANDALONE:-0}"
AOSP_ROOT="${AOSP_ROOT:-$HOME/aosp}"
NDK_ROOT="${NDK_ROOT:-$HOME/Android/Sdk/ndk/26.1.10909125}"
TARGET_DEVICE="${TARGET_DEVICE:-panther}"
BUILD_VARIANT="${BUILD_VARIANT:-userdebug}"
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/out_fake_hal}"

FAKE_HAL_SRC="$(cd "$(dirname "$0")/.." && pwd)"
MODULE_NAME="android.hardware.camera.provider-fake"

echo "================================================="
echo " FakeHAL Build Script"
echo " Mode: $([ "$STANDALONE" = "1" ] && echo "STANDALONE (NDK)" || echo "AOSP")"
echo " Target: ${TARGET_DEVICE}"
echo "================================================="
echo ""


check_command() {
    local cmd="$1"
    local hint="${2:-}"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        error "$cmd не найден."
        [ -n "$hint" ] && echo "  $hint"
        return 1
    fi
    return 0
}

check_deps_common() {
    local fail=0
    check_command python3 "Установите: sudo apt install python3" || fail=1
    return $fail
}

check_deps_aosp() {
    local fail=0
    check_deps_common || fail=1
    check_command java "Установите JDK 17: sudo apt install openjdk-17-jdk" || fail=1
    check_command repo "Установите repo: https://source.android.com/docs/setup/download" || fail=1


    if command -v java >/dev/null 2>&1; then
        JAVA_VER=$(java -version 2>&1 | head -1 | awk -F '"' '{print $2}' | cut -d. -f1)
        if [ "$JAVA_VER" -lt 17 ] 2>/dev/null; then
            warn "Java $JAVA_VER обнаружена, рекомендуется 17+"
        fi
    fi

    return $fail
}

check_deps_standalone() {
    local fail=0
    check_deps_common || fail=1

    if [ ! -d "$NDK_ROOT" ]; then
        error "NDK не найден в $NDK_ROOT"
        echo "  Установите через Android Studio SDK Manager или:"
        echo "  export NDK_ROOT=/path/to/android-ndk-r26c"
        fail=1
    fi

    return $fail
}


check_android_bp() {
    local bp="$FAKE_HAL_SRC/Android.bp"
    if [ ! -f "$bp" ]; then
        error "Android.bp не найден: $bp"
        return 1
    fi

    info "Проверяем Android.bp..."
    local missing=0


    for src in FakeCameraProvider.cpp FakeCameraDevice.cpp VideoFrameReader.cpp \
               MetadataRandomizer.cpp NoiseOverlay.cpp GyroWarp.cpp \
               JpegEncoder.cpp GrallocHelper.cpp; do
        if ! grep -q "$src" "$bp"; then
            error "  Отсутствует src: $src"
            missing=1
        fi
    done


    for lib in libbinder_ndk liblog libcamera_metadata libmediandk libjpeg \
               libhardware libnativewindow; do
        if ! grep -q "\"$lib\"" "$bp"; then
            warn "  Возможно отсутствует shared_lib: $lib"
        fi
    done


    for lib in "android.hardware.graphics.mapper@4.0" "android.hardware.graphics.mapper@3.0"; do
        if ! grep -q "$lib" "$bp"; then
            warn "  Отсутствует gralloc lib: $lib (нужен для GrallocHelper)"
        fi
    done

    if [ "$missing" -eq 0 ]; then
        ok "Android.bp: все srcs и shared_libs присутствуют"
    fi
    return $missing
}


find_built_binary() {
    local aosp_root="$1"
    local device="$2"


    local paths=(
        "$aosp_root/out/target/product/${device}/vendor/bin/hw/$MODULE_NAME"
        "$aosp_root/out/target/product/${device}/system/vendor/bin/hw/$MODULE_NAME"
        "$aosp_root/out/target/product/${device}/obj/EXECUTABLES/${MODULE_NAME}_intermediates/$MODULE_NAME"
        "$aosp_root/out/soong/.intermediates/hardware/google/camera/hal/fake/$MODULE_NAME/android_arm64_armv8-a_vendor/$MODULE_NAME"
    )

    for p in "${paths[@]}"; do
        if [ -f "$p" ]; then
            echo "$p"
            return 0
        fi
    done


    info "Ищем бинарник через find (может занять время)..."
    local found
    found=$(find "$aosp_root/out" -name "$MODULE_NAME" -type f -executable 2>/dev/null | head -1)
    if [ -n "$found" ]; then
        echo "$found"
        return 0
    fi

    return 1
}


build_aosp() {
    check_deps_aosp || exit 1


    if [ ! -f "$AOSP_ROOT/build/envsetup.sh" ]; then
        error "AOSP не найден в $AOSP_ROOT"
        echo "  Установите переменную: export AOSP_ROOT=/path/to/aosp"
        exit 1
    fi

    check_android_bp || warn "Android.bp имеет проблемы, сборка может упасть"


    DEST="$AOSP_ROOT/hardware/google/camera/hal/fake"
    info "Копируем исходники в $DEST..."
    mkdir -p "$DEST"
    cp -r "$FAKE_HAL_SRC"/{src,include,Android.bp,fake_camera_hal.rc} "$DEST/"


    if [ -d "$FAKE_HAL_SRC/vendor_overlay" ]; then
        mkdir -p "$DEST/vendor_overlay"
        cp -r "$FAKE_HAL_SRC/vendor_overlay/." "$DEST/vendor_overlay/"
    fi


    info "Инициализация AOSP build environment..."
    cd "$AOSP_ROOT"


    source build/envsetup.sh
    lunch "aosp_${TARGET_DEVICE}-${BUILD_VARIANT}"


    info "Сборка $MODULE_NAME..."
    m "$MODULE_NAME" -j"$(nproc)"


    mkdir -p "$OUTPUT_DIR"

    BUILT_BIN=""
    if BUILT_BIN=$(find_built_binary "$AOSP_ROOT" "$TARGET_DEVICE"); then
        cp "$BUILT_BIN" "$OUTPUT_DIR/"
        ok "Binary: $OUTPUT_DIR/$MODULE_NAME"
        ok "Size: $(ls -lh "$OUTPUT_DIR/$MODULE_NAME" | awk '{print $5}')"
    else
        error "Бинарник не найден! Проверьте лог сборки."
        exit 1
    fi


    if [ -d "$FAKE_HAL_SRC/vendor_overlay" ]; then
        cp -r "$FAKE_HAL_SRC/vendor_overlay" "$OUTPUT_DIR/"
    fi
    cp -r "$FAKE_HAL_SRC/scripts" "$OUTPUT_DIR/"

    mkdir -p "$OUTPUT_DIR/vendor_overlay/etc/init"
    cp "$FAKE_HAL_SRC/fake_camera_hal.rc" "$OUTPUT_DIR/vendor_overlay/etc/init/"

    return 0
}


build_standalone() {
    check_deps_standalone || exit 1

    info "Standalone NDK build (проверка компиляции)..."
    warn "Результат НЕ линкуется с AIDL HAL interfaces — только syntax check"

    TOOLCHAIN="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
    if [ ! -d "$TOOLCHAIN" ]; then

        TOOLCHAIN="$NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64"
    fi
    if [ ! -d "$TOOLCHAIN" ]; then
        error "NDK toolchain не найден в $NDK_ROOT"
        exit 1
    fi

    CXX="$TOOLCHAIN/bin/aarch64-linux-android33-clang++"
    if [ ! -f "$CXX" ]; then
        error "clang++ не найден: $CXX"
        exit 1
    fi

    mkdir -p "$OUTPUT_DIR"


    SRCS=(
        "$FAKE_HAL_SRC/src/MetadataRandomizer.cpp"
        "$FAKE_HAL_SRC/src/NoiseOverlay.cpp"
        "$FAKE_HAL_SRC/src/GyroWarp.cpp"
        "$FAKE_HAL_SRC/src/JpegEncoder.cpp"
        "$FAKE_HAL_SRC/src/GrallocHelper.cpp"
    )

    local success=0
    local total=${#SRCS[@]}

    for src in "${SRCS[@]}"; do
        local name
        name=$(basename "$src" .cpp)
        info "  Компиляция $name.cpp..."

        if "$CXX" -std=c++17 -c -O2 \
            -I "$FAKE_HAL_SRC/include" \
            -I "$FAKE_HAL_SRC/tests/mocks" \
            -DANDROID -D__ANDROID_API__=33 \
            -fsyntax-only \
            "$src" 2>"$OUTPUT_DIR/${name}.errors"; then
            ok "  $name.cpp — OK"
            ((success++)) || true
        else
            error "  $name.cpp — ОШИБКИ:"
            cat "$OUTPUT_DIR/${name}.errors"
        fi
    done

    echo ""
    if [ "$success" -eq "$total" ]; then
        ok "Все $total файлов скомпилированы успешно (syntax check)"
    else
        warn "$success/$total файлов скомпилированы"
    fi

    return 0
}


auto_push() {
    if ! command -v adb >/dev/null 2>&1; then
        return 0
    fi

    if adb devices 2>/dev/null | grep -q "device$"; then
        local bin="$OUTPUT_DIR/$MODULE_NAME"
        if [ -f "$bin" ]; then
            echo ""
            info "Устройство подключено, пушим бинарник..."
            adb push "$bin" /data/local/tmp/
            adb shell chmod 755 "/data/local/tmp/$MODULE_NAME"
            ok "Бинарник на устройстве: /data/local/tmp/$MODULE_NAME"
        fi
    fi
}


if [ "$STANDALONE" = "1" ]; then
    build_standalone
else
    build_aosp
fi

auto_push

echo ""
ok "Build complete!"
echo ""
info "Следующие шаги:"
echo "  1. Подготовьте видео:"
echo "     ./scripts/prepare_video.sh input.mp4"
echo "     adb push fake_video.mp4 /data/local/tmp/fake_video.mp4"
echo ""
echo "  2. Патч vbmeta (один раз, устройство в fastboot):"
echo "     adb reboot bootloader"
echo "     ./scripts/vbmeta_patch.sh /path/to/avbtool.py"
echo ""
echo "  3. Установите KernelSU модуль:"
echo "     ./scripts/module_structure.sh ./fake_hal_module"
echo "     cp $OUTPUT_DIR/$MODULE_NAME fake_hal_module/vendor_overlay/bin/hw/"
echo "     cd fake_hal_module && zip -r ../fake_hal_module.zip ."
echo "     # Установить через KSU Manager"
echo ""
echo "  4. Перезагрузка и проверка:"
echo "     adb reboot"
echo "     adb logcat -s FakeHAL_Provider FakeHAL_Device FakeHAL_Gralloc"
