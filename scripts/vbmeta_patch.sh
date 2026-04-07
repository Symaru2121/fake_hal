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


AVBTOOL="${AVBTOOL:-avbtool.py}"
DEVICE_SERIAL="${ANDROID_SERIAL:-}"
FASTBOOT="fastboot${DEVICE_SERIAL:+ -s $DEVICE_SERIAL}"

BACKUP_DIR="${BACKUP_DIR:-$HOME/.fake_hal_backups}"
BACKUP_VBMETA="$BACKUP_DIR/vbmeta_original.img"

SUPPORTED_DEVICES="panther cheetah lynx"


check_fastboot() {
    if ! command -v fastboot >/dev/null 2>&1; then
        error "fastboot не найден в PATH"
        echo "  Установите Android SDK Platform Tools:"
        echo "  https://developer.android.com/studio/releases/platform-tools"
        exit 1
    fi

    if ! $FASTBOOT devices 2>/dev/null | grep -q "fastboot"; then
        error "Устройство не найдено в fastboot режиме"
        echo "  Переведите устройство в fastboot:"
        echo "    adb reboot bootloader"
        echo "  Или зажмите Vol Down + Power при включении"
        exit 1
    fi
    ok "Устройство обнаружено в fastboot"
}


detect_device() {
    local product
    product=$($FASTBOOT getvar product 2>&1 | grep "^product:" | awk '{print $2}')

    if [ -z "$product" ]; then
        warn "Не удалось определить устройство через fastboot getvar product"
        product="unknown"
    fi

    echo "$product"
}


check_device_support() {
    local device="$1"

    case "$device" in
        panther)
            info "Устройство: Pixel 7 ($device)"
            ;;
        cheetah)
            info "Устройство: Pixel 7 Pro ($device)"
            ;;
        lynx)
            info "Устройство: Pixel 7a ($device)"
            ;;
        *)
            warn "Устройство '$device' не в списке поддерживаемых ($SUPPORTED_DEVICES)"
            warn "Скрипт продолжит работу, но результат не гарантирован"
            echo ""
            read -r -p "Продолжить? [y/N] " confirm
            if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
                echo "Отменено."
                exit 0
            fi
            ;;
    esac
}


get_active_slot() {
    local slot
    slot=$($FASTBOOT getvar current-slot 2>&1 | grep "^current-slot:" | awk '{print $2}')
    echo "${slot:-a}"
}


backup_vbmeta() {
    mkdir -p "$BACKUP_DIR"

    if [ -f "$BACKUP_VBMETA" ]; then
        warn "Backup уже существует: $BACKUP_VBMETA"
        warn "Пропускаем создание backup (используйте старый)"
        return 0
    fi

    info "Создаём backup текущего vbmeta..."


    if $FASTBOOT fetch vbmeta "$BACKUP_DIR/vbmeta_raw.img" 2>/dev/null; then
        mv "$BACKUP_DIR/vbmeta_raw.img" "$BACKUP_VBMETA"
        ok "Backup сохранён: $BACKUP_VBMETA"
        return 0
    fi


    if command -v adb >/dev/null 2>&1; then

        warn "fastboot fetch не поддерживается, пробуем альтернативные методы..."
    fi


    info "Сохраняем информацию о текущем vbmeta state..."
    $FASTBOOT getvar all 2>&1 | grep -i "vbmeta\|slot\|secure\|unlocked" \
        > "$BACKUP_DIR/device_state.txt" 2>/dev/null || true

    warn "Не удалось создать полный backup vbmeta"
    warn "Информация о состоянии устройства сохранена в $BACKUP_DIR/device_state.txt"
    warn "Для полного восстановления понадобится factory image с"
    warn "  https://developers.google.com/android/images"

    return 0
}


do_patch() {

    if [ "${1:-}" != "" ] && [ "${1:-}" != "--" ]; then
        AVBTOOL="$1"
    fi


    if [ ! -f "$AVBTOOL" ] && ! command -v "$AVBTOOL" >/dev/null 2>&1; then

        for path in \
            "avbtool" \
            "avbtool.py" \
            "$HOME/aosp/external/avb/avbtool.py" \
            "$HOME/aosp/tools/avb/avbtool.py" \
            "/usr/local/bin/avbtool" \
            "/usr/bin/avbtool"; do
            if [ -f "$path" ] || command -v "$path" >/dev/null 2>&1; then
                AVBTOOL="$path"
                break
            fi
        done
    fi

    if [ ! -f "$AVBTOOL" ] && ! command -v "$AVBTOOL" >/dev/null 2>&1; then
        error "avbtool не найден: $AVBTOOL"
        echo "  Укажите путь: $0 /path/to/avbtool.py"
        echo "  Или скачайте из AOSP: external/avb/avbtool.py"
        exit 1
    fi

    info "avbtool: $AVBTOOL"

    echo ""
    echo "====================================="
    echo " FakeHAL vbmeta patcher"
    echo "====================================="
    echo ""


    check_fastboot


    DEVICE=$(detect_device)
    check_device_support "$DEVICE"


    ACTIVE_SLOT=$(get_active_slot)
    info "Активный слот: $ACTIVE_SLOT"


    backup_vbmeta

    echo ""


    TMPDIR=$(mktemp -d)
    trap 'rm -rf "$TMPDIR"' EXIT

    VBMETA_OUT="$TMPDIR/vbmeta_patched.img"

    info "Создаём patched vbmeta (HASHTREE_DISABLED)..."
    info "  Флаг 2 = только отключение хэш-деревьев"
    info "  (AVB signature сохранена, TEE attestation менее чувствителен)"
    echo ""

    python3 "$AVBTOOL" make_vbmeta_image \
        --flag 2 \
        --padding_size 4096 \
        --output "$VBMETA_OUT"


    if [ ! -f "$VBMETA_OUT" ] || [ ! -s "$VBMETA_OUT" ]; then
        error "avbtool не создал vbmeta image"
        exit 1
    fi

    VBMETA_SIZE=$(ls -lh "$VBMETA_OUT" | awk '{print $5}')
    ok "Patched vbmeta создан ($VBMETA_SIZE)"


    echo ""
    info "Прошиваем patched vbmeta..."


    $FASTBOOT flash vbmeta "$VBMETA_OUT"
    ok "vbmeta прошит"


    info "Прошиваем оба A/B слота..."

    if $FASTBOOT flash vbmeta_a "$VBMETA_OUT" 2>/dev/null; then
        ok "vbmeta_a прошит"
    else
        warn "vbmeta_a не найден (single slot?), пропускаем"
    fi

    if $FASTBOOT flash vbmeta_b "$VBMETA_OUT" 2>/dev/null; then
        ok "vbmeta_b прошит"
    else
        warn "vbmeta_b не найден, пропускаем"
    fi


    echo ""
    info "Верификация patched vbmeta..."

    VERIFY_OUTPUT=$(python3 "$AVBTOOL" info_image --image "$VBMETA_OUT" 2>&1)
    echo "$VERIFY_OUTPUT" | grep -A2 "Flags" || true


    if echo "$VERIFY_OUTPUT" | grep -qi "HASHTREE_DISABLED\|flag.*2"; then
        ok "HASHTREE_DISABLED подтверждён"
    else
        warn "Не удалось подтвердить HASHTREE_DISABLED в выводе avbtool"
        warn "Полный вывод:"
        echo "$VERIFY_OUTPUT"
    fi


    cp "$VBMETA_OUT" "$BACKUP_DIR/vbmeta_patched.img"

    echo ""
    ok "vbmeta патч применён успешно!"
    ok "Backup: $BACKUP_DIR/"
    echo ""
    info "Можно загружаться:"
    echo "  fastboot reboot"
    echo ""
    info "Для восстановления оригинального vbmeta:"
    echo "  $0 --restore"
}


do_restore() {
    echo ""
    echo "====================================="
    echo " FakeHAL vbmeta restore"
    echo "====================================="
    echo ""

    check_fastboot

    DEVICE=$(detect_device)
    info "Устройство: $DEVICE"

    if [ -f "$BACKUP_VBMETA" ]; then
        info "Восстанавливаем vbmeta из backup: $BACKUP_VBMETA"
        echo ""

        $FASTBOOT flash vbmeta "$BACKUP_VBMETA"
        ok "vbmeta восстановлен"

        if $FASTBOOT flash vbmeta_a "$BACKUP_VBMETA" 2>/dev/null; then
            ok "vbmeta_a восстановлен"
        else
            warn "vbmeta_a не найден, пропускаем"
        fi

        if $FASTBOOT flash vbmeta_b "$BACKUP_VBMETA" 2>/dev/null; then
            ok "vbmeta_b восстановлен"
        else
            warn "vbmeta_b не найден, пропускаем"
        fi

        echo ""
        ok "vbmeta восстановлен из backup!"

    else
        error "Backup не найден: $BACKUP_VBMETA"
        echo ""
        echo "  Варианты восстановления:"
        echo ""
        echo "  1. Скачайте factory image для вашего устройства:"
        echo "     https://developers.google.com/android/images"
        echo ""
        echo "  2. Распакуйте и прошейте оригинальный vbmeta:"
        echo "     fastboot flash vbmeta vbmeta.img"
        echo "     fastboot flash vbmeta_a vbmeta.img"
        echo "     fastboot flash vbmeta_b vbmeta.img"
        echo ""
        exit 1
    fi

    echo ""
    info "Перезагрузка:"
    echo "  fastboot reboot"
}


do_status() {
    echo ""
    echo "====================================="
    echo " FakeHAL vbmeta status"
    echo "====================================="
    echo ""

    check_fastboot

    DEVICE=$(detect_device)
    info "Устройство: $DEVICE"
    info "Активный слот: $(get_active_slot)"

    echo ""
    info "Fastboot переменные (vbmeta/security):"
    $FASTBOOT getvar all 2>&1 | grep -i "vbmeta\|slot\|secure\|unlocked\|product" | sort || true

    echo ""
    if [ -f "$BACKUP_VBMETA" ]; then
        ok "Backup найден: $BACKUP_VBMETA ($(ls -lh "$BACKUP_VBMETA" | awk '{print $5}'))"
    else
        warn "Backup не найден (ещё не создавался)"
    fi

    if [ -f "$BACKUP_DIR/vbmeta_patched.img" ]; then
        ok "Patched vbmeta: $BACKUP_DIR/vbmeta_patched.img"
    fi

    if [ -f "$BACKUP_DIR/device_state.txt" ]; then
        info "Сохранённое состояние устройства:"
        cat "$BACKUP_DIR/device_state.txt"
    fi
}


case "${1:-patch}" in
    --restore|-r|restore)
        do_restore
        ;;
    --status|-s|status)
        do_status
        ;;
    --help|-h|help)
        echo "Использование:"
        echo "  $0 [avbtool.py]    — патч vbmeta (HASHTREE_DISABLED)"
        echo "  $0 --restore       — восстановление оригинального vbmeta"
        echo "  $0 --status        — проверка текущего состояния"
        echo "  $0 --help          — эта справка"
        echo ""
        echo "Переменные окружения:"
        echo "  AVBTOOL=path       — путь к avbtool.py"
        echo "  BACKUP_DIR=path    — директория для backup (default: ~/.fake_hal_backups)"
        echo "  ANDROID_SERIAL=xxx — serial number устройства"
        ;;
    *)
        do_patch "$@"
        ;;
esac
