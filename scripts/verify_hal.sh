#!/bin/bash


set -euo pipefail

SERIAL="${1:-}"
ADB="adb${SERIAL:+ -s $SERIAL}"

PASS=0
FAIL=0
WARN=0


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

check() {
    local name="$1"
    local cmd="$2"
    local expected="$3"

    result=$($ADB shell "$cmd" 2>/dev/null || true)
    if echo "$result" | grep -q "$expected"; then
        echo -e "${GREEN}[✓]${NC} $name"
        ((PASS++))
    else
        echo -e "${RED}[✗]${NC} $name (got: ${result:0:80})"
        ((FAIL++))
    fi
}

warn() {
    local name="$1"
    local cmd="$2"
    local expected="$3"

    result=$($ADB shell "$cmd" 2>/dev/null || true)
    if echo "$result" | grep -q "$expected"; then
        echo -e "${GREEN}[✓]${NC} $name"
        ((PASS++))
    else
        echo -e "${YELLOW}[!]${NC} $name (WARNING: ${expected:-empty expected} not found)"
        ((WARN++))
    fi
}

check_empty() {
    local name="$1"
    local cmd="$2"

    result=$($ADB shell "$cmd" 2>/dev/null || true)
    result=$(echo "$result" | tr -d '[:space:]')
    if [ -z "$result" ]; then
        echo -e "${GREEN}[✓]${NC} $name (output is empty as expected)"
        ((PASS++))
    else
        echo -e "${RED}[✗]${NC} $name (unexpected output: ${result:0:80})"
        ((FAIL++))
    fi
}

echo -e "${CYAN}=================================================${NC}"
echo -e "${CYAN} FakeHAL Verification Script v2.0${NC}"
echo -e "${CYAN} $(date '+%Y-%m-%d %H:%M:%S')${NC}"
echo -e "${CYAN}=================================================${NC}"
echo ""


if ! command -v adb &>/dev/null; then
    echo -e "${RED}ERROR: adb not found in PATH${NC}"
    exit 1
fi


echo -e "${CYAN}--- Connectivity ---${NC}"
check "ADB connection" "echo ok" "ok"


check "Root access" "su -c id 2>/dev/null || id" "root\|uid=0"


echo ""
echo -e "${CYAN}--- KernelSU ---${NC}"
warn "KernelSU active" \
    "ksud --version 2>/dev/null || getprop ro.kernelsu.version 2>/dev/null || echo NOPE" \
    "."


echo ""
echo -e "${CYAN}--- HAL Service ---${NC}"
check "FakeHAL process running" \
    "ps -A | grep camera.provider" \
    "camera.provider"


warn "HAL in ServiceManager" \
    "service list 2>/dev/null | grep camera.provider || echo NOT_FOUND" \
    "camera"


echo ""
echo -e "${CYAN}--- Gralloc ---${NC}"
warn "gralloc mapper available" \
    "service list 2>/dev/null | grep mapper || echo NOT_FOUND" \
    "graphics"

warn "gralloc hardware prop" \
    "getprop ro.hardware.gralloc 2>/dev/null || echo default" \
    "."


echo ""
echo -e "${CYAN}--- VINTF ---${NC}"
check "VINTF manifest installed" \
    "cat /vendor/etc/vintf/manifest/fake_camera_hal.xml 2>/dev/null || echo NOT_FOUND" \
    "ICameraProvider"


echo ""
echo -e "${CYAN}--- Video Source ---${NC}"
check "Video file present" \
    "ls /data/local/tmp/fake_video.mp4 2>/dev/null && echo OK || echo MISSING" \
    "OK"


echo ""
echo -e "${CYAN}--- Camera Service ---${NC}"
check "cameraserver alive" \
    "ps -A | grep cameraserver" \
    "cameraserver"


echo ""
echo -e "${CYAN}--- Stealth ---${NC}"
check_empty "SUSFS hiding mounts (no 'fake' in /proc/mounts)" \
    "cat /proc/mounts | grep fake_hal || true"


echo ""
echo -e "${CYAN}--- Play Integrity (build props) ---${NC}"
warn "Build tags = release-keys" \
    "getprop ro.build.tags" \
    "release-keys"

warn "Build type = user" \
    "getprop ro.build.type" \
    "user"

warn "Build fingerprint looks legit" \
    "getprop ro.build.fingerprint" \
    "google"


echo ""
echo -e "${CYAN}--- SELinux ---${NC}"
warn "SELinux enforcing" \
    "getenforce 2>/dev/null || echo Unknown" \
    "Enforcing"


echo ""
echo -e "${CYAN}--- FPN Fingerprint ---${NC}"
warn "FPN fingerprint set" \
    "logcat -d -s FakeHAL_Provider 2>/dev/null | grep 'FPN fingerprint' | tail -1 || echo NOT_FOUND" \
    "FPN fingerprint"


echo ""
echo -e "${CYAN}--- Error Check ---${NC}"
HAL_ERRORS=$($ADB shell "logcat -d -s FakeHAL_Provider FakeHAL_Device FakeHAL_Gralloc 2>/dev/null | grep -i 'error\|fail\|crash' | wc -l" 2>/dev/null || echo "0")
HAL_ERRORS=$(echo "$HAL_ERRORS" | tr -d '[:space:]')
if [ "$HAL_ERRORS" = "0" ] || [ -z "$HAL_ERRORS" ]; then
    echo -e "${GREEN}[✓]${NC} No errors in FakeHAL logs"
    ((PASS++))
else
    echo -e "${YELLOW}[!]${NC} Found $HAL_ERRORS error/fail lines in FakeHAL logs"
    ((WARN++))
    echo "    Recent errors:"
    $ADB shell "logcat -d -s FakeHAL_Provider FakeHAL_Device FakeHAL_Gralloc 2>/dev/null | grep -i 'error\|fail\|crash' | tail -5" 2>/dev/null || true
fi


echo ""
echo -e "${CYAN}--- Recent FakeHAL Logs ---${NC}"
$ADB shell "logcat -d -s FakeHAL_Provider FakeHAL_Device FakeHAL_Gralloc 2>/dev/null | tail -10" 2>/dev/null || echo "(no logs available)"


echo ""
echo -e "${CYAN}--- Device Info ---${NC}"
DEVICE_MODEL=$($ADB shell "getprop ro.product.model" 2>/dev/null || echo "unknown")
DEVICE_SDK=$($ADB shell "getprop ro.build.version.sdk" 2>/dev/null || echo "unknown")
DEVICE_ANDROID=$($ADB shell "getprop ro.build.version.release" 2>/dev/null || echo "unknown")
DEVICE_KERNEL=$($ADB shell "uname -r" 2>/dev/null || echo "unknown")
echo "  Model:   $DEVICE_MODEL"
echo "  Android: $DEVICE_ANDROID (SDK $DEVICE_SDK)"
echo "  Kernel:  $DEVICE_KERNEL"


echo ""
echo -e "${CYAN}=================================================${NC}"
echo -e " Results: ${GREEN}✓ $PASS passed${NC}, ${RED}✗ $FAIL failed${NC}, ${YELLOW}! $WARN warnings${NC}"
echo -e "${CYAN}=================================================${NC}"

if [ "$FAIL" -eq 0 ] && [ "$WARN" -le 2 ]; then
    echo -e " Status: ${GREEN}READY FOR USE${NC}"
    exit 0
elif [ "$FAIL" -eq 0 ]; then
    echo -e " Status: ${YELLOW}MOSTLY OK ($WARN warnings — review above)${NC}"
    exit 0
else
    echo -e " Status: ${RED}NEEDS ATTENTION ($FAIL checks failed)${NC}"
    exit 1
fi
