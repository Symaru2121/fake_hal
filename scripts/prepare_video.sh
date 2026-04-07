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
error() { echo -e "${RED}[!]${NC} $*"; }


check_deps() {
    local missing=0
    for cmd in ffmpeg ffprobe; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            error "$cmd не найден. Установите ffmpeg:"
            echo "  Ubuntu/Debian: sudo apt install ffmpeg"
            echo "  macOS:         brew install ffmpeg"
            echo "  Windows:       choco install ffmpeg"
            missing=1
        fi
    done
    if [ "$missing" -eq 1 ]; then
        exit 1
    fi
}

check_deps


if [ "${1:-}" = "--test-pattern" ]; then
    OUTPUT="${2:-test_pattern.mp4}"
    RESOLUTION="${3:-1920x1080}"
    FPS="${4:-30}"
    DURATION="${5:-10}"

    WIDTH="${RESOLUTION%%x*}"
    HEIGHT="${RESOLUTION##*x}"

    info "Генерация тестового паттерна (color bars + timestamp)..."
    info "  Разрешение: ${WIDTH}x${HEIGHT}"
    info "  FPS: ${FPS}"
    info "  Длительность: ${DURATION}s"
    info "  Выход: ${OUTPUT}"

    ffmpeg -hide_banner -loglevel warning \
        -f lavfi -i "testsrc=size=${WIDTH}x${HEIGHT}:rate=${FPS}:duration=${DURATION}" \
        -f lavfi -i "color=c=black:size=${WIDTH}x32:rate=${FPS}:duration=${DURATION}" \
        -filter_complex "[0:v][1:v]overlay=0:0[out]" \
        -map "[out]" \
        -c:v libx264 \
        -profile:v baseline \
        -level 3.1 \
        -pix_fmt yuv420p \
        -b:v 4M \
        -movflags +faststart \
        -an \
        -y "$OUTPUT"

    ok "Тестовый паттерн создан: $OUTPUT"
    echo ""
    info "Для использования:"
    echo "  adb push $OUTPUT /data/local/tmp/fake_video.mp4"
    exit 0
fi


INPUT="${1:?Использование: $0 input.mp4 [output.mp4] [WxH] [fps]}"
OUTPUT="${2:-fake_video.mp4}"
RESOLUTION="${3:-1920x1080}"
FPS="${4:-30}"


if [ ! -f "$INPUT" ]; then
    error "Файл не найден: $INPUT"
    exit 1
fi

WIDTH="${RESOLUTION%%x*}"
HEIGHT="${RESOLUTION##*x}"


info "Анализ входного файла..."
INPUT_INFO=$(ffprobe -v quiet -print_format json -show_streams -show_format "$INPUT" 2>/dev/null)

INPUT_DURATION=$(echo "$INPUT_INFO" | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    d = data.get('format', {}).get('duration', '?')
    print(f'{float(d):.1f}s' if d != '?' else '?')
except: print('?')
" 2>/dev/null || echo "?")

INPUT_CODEC=$(echo "$INPUT_INFO" | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    for s in data.get('streams', []):
        if s.get('codec_type') == 'video':
            print(f\"{s['codec_name']} {s.get('width','?')}x{s.get('height','?')}\")
            break
except: print('?')
" 2>/dev/null || echo "?")

INPUT_SIZE=$(du -h "$INPUT" 2>/dev/null | awk '{print $1}')

echo ""
info "Входной файл: $INPUT"
info "  Размер:    $INPUT_SIZE"
info "  Кодек:     $INPUT_CODEC"
info "  Длина:     $INPUT_DURATION"
echo ""
info "Параметры конвертации:"
info "  Разрешение: ${WIDTH}x${HEIGHT}"
info "  FPS:        ${FPS}"
info "  Кодек:      H.264 Baseline"
info "  Битрейт:    8 Mbps"
info "  Контейнер:  MP4 (faststart)"
info "  Аудио:      удалено"
echo ""
info "Выходной файл: $OUTPUT"
echo ""


info "Конвертируем..."

ffmpeg -hide_banner -loglevel warning -stats \
    -i "$INPUT" \
    -c:v libx264 \
    -profile:v baseline \
    -level 3.1 \
    -pix_fmt yuv420p \
    -vf "scale=${WIDTH}:${HEIGHT}:force_original_aspect_ratio=decrease,pad=${WIDTH}:${HEIGHT}:(ow-iw)/2:(oh-ih)/2:black,fps=${FPS}" \
    -b:v 8M \
    -maxrate 10M \
    -bufsize 16M \
    -movflags +faststart \
    -an \
    -y "$OUTPUT"

echo ""


info "Проверяем результат..."

OUTPUT_SIZE=$(du -h "$OUTPUT" 2>/dev/null | awk '{print $1}')

VERIFY_RESULT=$(ffprobe -v quiet -print_format json -show_streams "$OUTPUT" | python3 -c "
import json, sys
data = json.load(sys.stdin)
for s in data['streams']:
    if s['codec_type'] == 'video':
        codec   = s['codec_name']
        profile = s.get('profile', '?')
        w       = s['width']
        h       = s['height']
        fps     = s.get('r_frame_rate', '?')
        pix_fmt = s.get('pix_fmt', '?')
        print(f'codec={codec} profile={profile} res={w}x{h} fps={fps} pix_fmt={pix_fmt}')

        ok = True
        if codec != 'h264':
            print(f'WARNING: codec is {codec}, expected h264')
            ok = False
        if 'Baseline' not in profile and 'Constrained' not in profile:
            print(f'WARNING: profile is {profile}, expected Baseline')
        if pix_fmt != 'yuv420p':
            print(f'WARNING: pix_fmt is {pix_fmt}, expected yuv420p')
        if ok:
            print('VALID')
" 2>/dev/null)

echo ""
ok "Конвертация завершена!"
echo ""
echo "  Файл:    $OUTPUT"
echo "  Размер:  $OUTPUT_SIZE"
echo "  $VERIFY_RESULT"
echo ""


MOOV_POS=$(python3 -c "
import struct, sys
with open('$OUTPUT', 'rb') as f:
    pos = 0
    while True:
        hdr = f.read(8)
        if len(hdr) < 8: break
        size = struct.unpack('>I', hdr[:4])[0]
        tag  = hdr[4:8].decode('ascii', errors='replace')
        if tag == 'moov':
            print(f'moov atom at offset {pos} (faststart: {\"YES\" if pos < 1024*1024 else \"NO\"})')
            break
        if size < 8: break
        pos += size
        f.seek(pos)
" 2>/dev/null || echo "  moov position: unknown")
info "$MOOV_POS"

echo ""
info "Следующие шаги:"
echo "  adb push $OUTPUT /data/local/tmp/fake_video.mp4"
echo "  adb shell setprop ctl.restart fake_camera_hal    # если HAL уже запущен"
