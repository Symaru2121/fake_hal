# FakeHAL Camera Provider

Подмена камерного потока на уровне Android Camera HAL (AIDL).
Видео из MP4-файла инъецируется напрямую в `cameraserver` —
ни одно приложение (Camera2, CameraX, стоковая камера, видеозвонки)
не видит разницы с реальной камерой.

Проект реализует полноценный Camera HAL Provider с реалистичной
симуляцией CMOS-сенсора: шум, виньетирование, rolling shutter,
гироскопическая стабилизация, JPEG с EXIF-метаданными и
уникальный цифровой отпечаток (FPN) для каждого устройства.

**Целевые устройства:** Pixel 7 (panther), Pixel 7 Pro (cheetah), Pixel 7a (lynx).

---

## Содержание

1. [Название проекта и описание](#1-название-проекта-и-описание)
2. [Требования](#2-требования)
3. [Установка](#3-установка)
4. [Настройка](#4-настройка)
5. [Виртуальное окружение и среда разработки](#5-виртуальное-окружение-и-среда-разработки)
6. [Запуск](#6-запуск)
7. [Использование](#7-использование)
8. [Структура проекта](#8-структура-проекта)
9. [Возможные ошибки и их решения](#9-возможные-ошибки-и-их-решения)
10. [Лицензия](#10-лицензия)

---

## 1. Название проекта и описание

### Что это за проект

**FakeHAL** — это реализация Android Camera Hardware Abstraction Layer (HAL)
на основе AIDL-интерфейса `ICameraProvider`. Проект перехватывает запросы камеры
на уровне HAL и подставляет видеопоток из заранее подготовленного MP4-файла
вместо данных с реального камерного сенсора.

### Для чего он нужен

- **Подмена камерного потока** — приложения камеры (Camera2 API, CameraX, стоковая
  камера, видеозвонки) получают кадры из MP4-файла вместо реальной камеры
- **Прозрачная работа** — `cameraserver` не модифицируется, подмена происходит
  на уровне провайдера HAL
- **Реалистичная симуляция** — физическая модель CMOS-шума, виньетирование (cos⁴),
  rolling shutter, гироскопическая коррекция, EXIF-метаданные
- **Анти-детект** — уникальный Fixed Pattern Noise (FPN) для каждого устройства
  на основе хеша серийного номера (FNV-1a), горячие/мёртвые пиксели,
  динамический rolling shutter skew, Ornstein-Uhlenbeck дрейф метаданных
- **Образовательные и исследовательские цели** — изучение архитектуры Android
  Camera HAL, AIDL-интерфейсов, gralloc, media pipeline

### Ключевые возможности

| Компонент | Описание |
|-----------|----------|
| VideoFrameReader | Декодирование MP4 через NDK MediaCodec → NV21, автоматический loop |
| MetadataRandomizer | Ornstein-Uhlenbeck дрейф ISO, выдержки, AWB, CCM |
| NoiseOverlay | Физическая модель CMOS-шума с FPN-отпечатком устройства |
| GyroWarp | Гироскопическая коррекция через IIO sysfs |
| LensShading | Виньетирование (cos⁴, сила 0.4) |
| RollingShutter | Построчный сдвиг (33ms readout, до 200px) |
| JpegEncoder | JPEG через libjpeg-turbo с EXIF APP1 |
| GrallocHelper | Автоопределение gralloc4/gralloc3/mmap fallback |
| TimestampSync | Синхронизация timestamps камеры и IMU |

---

## 2. Требования

### Устройство (для деплоя)

| Модель | Кодовое имя | Статус |
|--------|-------------|--------|
| Pixel 7 | panther | Полная поддержка |
| Pixel 7 Pro | cheetah | Полная поддержка |
| Pixel 7a | lynx | Полная поддержка |
| Pixel 6 | oriole | Ожидается совместимость (gralloc4) |
| Pixel 8 | shiba | Требует проверки (обновить AOSP ветку) |

**Обязательные требования к устройству:**
- **Разблокированный bootloader** — `fastboot flashing unlock`
- **KernelSU** — установлен и работает (https://kernelsu.org/)
- **SUSFS** — патч для KernelSU (скрытие bind mounts от `/proc/mounts`)
- **Android 13 или 14** (API 33–34)

### Компьютер (для сборки)

| Компонент | Версия | Назначение |
|-----------|--------|------------|
| ОС | Ubuntu 22.04+ / macOS | Основная среда разработки |
| AOSP source tree | android-14.0.0_r1+ | Полная сборка HAL бинарника |
| Java (JDK) | 17+ | AOSP build system |
| Python | 3.x | avbtool, скрипты |
| CMake | 3.16+ | Standalone / тестовая сборка |
| g++ / clang++ | C++17 | Компиляция |
| ffmpeg | любая | Подготовка видео |
| Android NDK | r25+ (26.1.10909125 рекомендуется) | Standalone/emulator сборка |
| Android SDK Platform Tools | последняя | adb, fastboot |
| repo | последняя | Клонирование AOSP |
| Google Test (gtest) | встроен в тесты | Unit-тесты |

### Зависимости проекта (shared_libs из Android.bp)

Эти библиотеки требуются при сборке через AOSP и автоматически линкуются:

```
android.hardware.camera.provider-V1-ndk    # AIDL Camera HAL interfaces
android.hardware.camera.device-V1-ndk
android.hardware.camera.common-V1-ndk
libbinder_ndk                               # AIDL runtime
libbase, liblog, libutils, libcutils        # Android base libs
libcamera_metadata                          # Camera metadata API
libmediandk                                 # NDK Media (видеодекодер)
libjpeg                                     # libjpeg-turbo (JPEG encode)
libhardware, libnativewindow, libvndksupport # Hardware buffer
android.hardware.graphics.mapper@4.0        # gralloc4 (Pixel 6/7/8)
android.hardware.graphics.mapper@3.0        # gralloc3 (fallback)
libhidlbase                                 # HIDL transport
```

---

## 3. Установка

### Шаг 1: Установка инструментов (Ubuntu/Debian)

```bash
# Обновление пакетов
sudo apt-get update

# Компилятор и инструменты сборки
sudo apt-get install -y cmake g++ build-essential

# Java 17 (для AOSP build system)
sudo apt-get install -y openjdk-17-jdk

# Python 3 (для avbtool)
sudo apt-get install -y python3

# ffmpeg (для подготовки видео)
sudo apt-get install -y ffmpeg

# Android SDK Platform Tools (adb, fastboot)
# Вариант 1: через apt
sudo apt-get install -y android-tools-adb android-tools-fastboot

# Вариант 2: скачать с https://developer.android.com/studio/releases/platform-tools
# wget https://dl.google.com/android/repository/platform-tools-latest-linux.zip
# unzip platform-tools-latest-linux.zip
# export PATH="$PWD/platform-tools:$PATH"

# AOSP repo tool
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH="$HOME/bin:$PATH"
```

### Шаг 2: Клонирование репозитория

```bash
git clone <URL_РЕПОЗИТОРИЯ> fake_hal
cd fake_hal
```

### Шаг 3: Клонирование AOSP (для production-сборки)

> **Внимание:** AOSP source tree занимает ~100–200 ГБ. Нужен сервер с минимум
> 64 ГБ RAM и 500 ГБ SSD.

```bash
# Создаём директорию для AOSP
mkdir -p ~/aosp && cd ~/aosp

# Инициализируем репозиторий
repo init -u https://android.googlesource.com/platform/manifest \
    -b android-14.0.0_r1

# Синхронизируем (может занять несколько часов)
repo sync -c -j$(nproc)

# Возвращаемся в директорию проекта
cd /path/to/fake_hal
```

### Шаг 4: Установка Android NDK (для standalone/emulator сборки)

```bash
# Через Android Studio SDK Manager
# Или вручную:
cd ~
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip commandlinetools-linux-11076708_latest.zip -d android-sdk
mkdir -p android-sdk/cmdline-tools/latest
mv android-sdk/cmdline-tools/{bin,lib,NOTICE.txt,source.properties} \
   android-sdk/cmdline-tools/latest/

# Переменные окружения (добавить в ~/.bashrc)
export ANDROID_HOME=$HOME/android-sdk
export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH

# Принять лицензии и установить компоненты
yes | sdkmanager --licenses
sdkmanager "platform-tools" "platforms;android-34" \
    "ndk;26.1.10909125" "cmake;3.22.1"

export NDK_ROOT=$ANDROID_HOME/ndk/26.1.10909125
```

---

## 4. Настройка

### Переменные окружения

| Переменная | Значение по умолчанию | Описание |
|------------|----------------------|----------|
| `AOSP_ROOT` | `$HOME/aosp` | Путь к AOSP source tree |
| `NDK_ROOT` | `$HOME/Android/Sdk/ndk/26.1.10909125` | Путь к Android NDK |
| `TARGET_DEVICE` | `panther` | Целевое устройство: `panther`, `cheetah`, `lynx` |
| `BUILD_VARIANT` | `userdebug` | Вариант сборки AOSP |
| `OUTPUT_DIR` | `./out_fake_hal` | Директория для результатов сборки |
| `STANDALONE` | `0` | `1` = сборка через NDK (только syntax check) |
| `ANDROID_HOME` | — | Путь к Android SDK |

Пример настройки (добавить в `~/.bashrc` или `~/.zshrc`):

```bash
export AOSP_ROOT=$HOME/aosp
export NDK_ROOT=$HOME/Android/Sdk/ndk/26.1.10909125
export TARGET_DEVICE=panther
export ANDROID_HOME=$HOME/android-sdk
export PATH="$ANDROID_HOME/platform-tools:$HOME/bin:$PATH"
```

### Конфигурационные файлы

| Файл | Описание |
|------|----------|
| `Android.bp` | Сборочный файл AOSP — определяет бинарник и зависимости |
| `CMakeLists.txt` | Standalone CMake сборка (для тестов и эмулятора) |
| `Makefile` | Простая компиляция для проверки синтаксиса |
| `fake_camera_hal.rc` | Android init-скрипт для автозапуска HAL |
| `vendor_overlay/etc/vintf/fake_camera_hal.xml` | VINTF manifest — регистрация ICameraProvider |
| `ksu_module/module.prop` | Метаданные KernelSU модуля |
| `ksu_module/service.sh` | Скрипт запуска при загрузке (KernelSU) |
| `sepolicy/fake_hal.te` | SELinux policy для HAL |

### Подготовка видеофайла

Перед запуском необходимо подготовить видео в совместимом формате:

**Автоматическая конвертация:**
```bash
# Конвертировать любое видео
./scripts/prepare_video.sh input.mp4

# С указанием разрешения и fps
./scripts/prepare_video.sh input.mov output.mp4 1920x1080 30

# Создать тестовый паттерн (color bars + таймер, 10 секунд)
./scripts/prepare_video.sh --test-pattern
```

**Ручная конвертация через ffmpeg:**
```bash
ffmpeg -i input.mp4 \
    -c:v libx264 \
    -profile:v baseline \
    -level 3.1 \
    -pix_fmt yuv420p \
    -vf "scale=1920:1080,fps=30" \
    -b:v 8M \
    -maxrate 10M \
    -bufsize 16M \
    -movflags +faststart \
    -an \
    -y fake_video.mp4
```

**Требования к видео:**

| Параметр | Рекомендация | Примечание |
|----------|-------------|------------|
| Кодек | H.264 Baseline | H.265 тоже работает, но медленнее декод |
| Разрешение | 1920×1080 | Автоматически масштабируется под запрос приложения |
| FPS | 30 | Зацикливается автоматически |
| Длительность | ≥10 секунд | Чем длиннее — тем реалистичнее |
| Аудио | Нет | Удаляется при конвертации |
| moov atom | В начале файла | `ffmpeg -movflags +faststart` |

---

## 5. Виртуальное окружение и среда разработки

### Зачем нужно виртуальное окружение

FakeHAL — проект на C++17, а не на Python, поэтому «виртуальное окружение»
здесь — это изолированная среда сборки. Существует несколько уровней
изоляции:

### 1. AOSP Build Environment (production)

AOSP предоставляет собственное изолированное окружение сборки:

```bash
cd $AOSP_ROOT

# Инициализация окружения сборки
source build/envsetup.sh

# Выбор целевого устройства
lunch aosp_panther-userdebug    # Pixel 7
# lunch aosp_cheetah-userdebug  # Pixel 7 Pro
# lunch aosp_lynx-userdebug     # Pixel 7a

# После этого доступны все утилиты AOSP: m, mm, mmm, adb, fastboot и т.д.
```

### 2. Android Emulator (AVD) — тестирование без реального устройства

Для тестирования без физического устройства можно использовать Android эмулятор:

```bash
# Создание AVD (Android Virtual Device)
avdmanager create avd \
    --name fakehal_test \
    --package "system-images;android-34;google_apis;x86_64" \
    --device "pixel_7" \
    --force

# Запуск эмулятора (headless, для серверов)
emulator -avd fakehal_test \
    -gpu swiftshader_indirect \
    -no-snapshot \
    -no-audio \
    -no-window \
    -memory 4096 \
    -cores 4 \
    -writable-system &

# Ожидание загрузки
adb wait-for-device
adb root && adb remount
```

### 3. Standalone тестовая среда (CMake)

Для unit-тестов без AOSP и без устройства:

```bash
cd tests

# Конфигурация
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Сборка
cmake --build build -j$(nproc)

# Запуск тестов
./build/run_tests
```

### 4. CI/CD Pipeline (GitHub Actions)

Проект использует GitHub Actions для автоматической проверки:

- **ci-tests.yml** — unit-тесты на `ubuntu-latest` (CMake + gtest)
- **main.yml** — полная AOSP сборка на self-hosted runner

---

## 6. Запуск

### Способ 1: Сборка через AOSP (production)

Это основной способ получения работающего бинарника:

```bash
# 1. Установить переменные окружения
export AOSP_ROOT=$HOME/aosp
export TARGET_DEVICE=panther    # panther / cheetah / lynx

# 2. Запустить сборку
./scripts/build.sh

# Результат: out_fake_hal/android.hardware.camera.provider-fake
```

### Способ 2: Облачная сборка (GitHub Actions)

```bash
# 1. Создать GitHub репозиторий и запушить код
git remote add origin https://github.com/<user>/fake_hal.git
git push -u origin main

# 2. Настроить self-hosted runner (Ubuntu 22.04, 64GB RAM, 500GB SSD)
./scripts/setup_runner.sh

# 3. Workflow запустится автоматически при push
# 4. Скачать бинарник: Actions → Artifacts → fake-hal-binary
```

### Способ 3: Standalone NDK (только проверка синтаксиса)

```bash
export NDK_ROOT=$HOME/Android/Sdk/ndk/26.1.10909125
STANDALONE=1 ./scripts/build.sh
```

> Standalone build проверяет только синтаксис. Финальный бинарник для
> устройства можно собрать только через AOSP.

### Способ 4: Тестовая сборка (CMake + mock)

```bash
# Сборка и запуск unit-тестов
cd tests
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/run_tests --gtest_output=xml:test_results.xml
```

### Деплой на устройство

#### Шаг 1: Патч vbmeta (один раз)

Отключает проверку хеш-деревьев для `/vendor`, чтобы избежать bootloop:

```bash
# Перевести устройство в fastboot
adb reboot bootloader

# Патч vbmeta (автоматически создаст backup)
./scripts/vbmeta_patch.sh /path/to/avbtool.py

# Проверить статус
./scripts/vbmeta_patch.sh --status

# Загрузиться
fastboot reboot
```

#### Шаг 2: Загрузка видеофайла

```bash
adb push fake_video.mp4 /data/local/tmp/fake_video.mp4
```

#### Шаг 3: Создание и установка KernelSU модуля

```bash
# Создать структуру модуля
./scripts/module_structure.sh ./fake_hal_module

# Скопировать бинарник
cp out_fake_hal/android.hardware.camera.provider-fake \
   fake_hal_module/vendor_overlay/bin/hw/

# Скопировать VINTF manifest
cp vendor_overlay/etc/vintf/fake_camera_hal.xml \
   fake_hal_module/vendor_overlay/etc/vintf/manifest/

# Упаковать в zip
cd fake_hal_module && zip -r ../fake_hal_module.zip . && cd ..

# Загрузить на устройство
adb push fake_hal_module.zip /sdcard/Download/

# Открыть KernelSU Manager → Модули → Установить из файла
# Выбрать fake_hal_module.zip → Установить
```

#### Шаг 4: Перезагрузка

```bash
adb reboot
```

### Запуск через Docker / docker-compose

> Проект не содержит Dockerfile или docker-compose, так как целевой средой
> исполнения является Android-устройство, а не сервер. Сборка выполняется
> в AOSP source tree или через CI/CD.

---

## 7. Использование

### Проверка работы после установки

```bash
# Все компоненты FakeHAL
adb logcat -s FakeHAL_Provider FakeHAL_Device FakeHAL_Gralloc \
    FakeHAL_Jpeg FakeHAL_GyroWarp FakeHAL_Video FakeHAL_Meta

# Только провайдер и устройство
adb logcat -s FakeHAL_Provider FakeHAL_Device

# Gralloc lock/unlock (verbose)
adb logcat -s FakeHAL_Gralloc:V
```

**Ожидаемый вывод логов:**
```
FakeHAL_Provider: Registering FakeCameraProvider
FakeHAL_Provider: FPN fingerprint set for device XXXX
FakeHAL_Gralloc:  GrallocHelper: using gralloc4 (IMapper 4.0)
FakeHAL_Device:   FakeCameraDevice[0]: open()
FakeHAL_Device:   FakeCameraDeviceSession[0]: configureStreams -> 2 streams, target 1920x1080
FakeHAL_Device:   writeYUVToBuffer: wrote 3110400 bytes NV21 (1920x1080) via gralloc4
FakeHAL_Jpeg:     writeJPEGToBuffer: wrote 245760 bytes JPEG to blob buffer via gralloc4
```

### Проверка через приложение

1. Откройте стоковое приложение **Камера**
2. Должно отображаться видео из MP4 вместо реальной камеры
3. Сделайте фото — JPEG будет содержать кадр из видео с реалистичными EXIF
4. Запишите видео — будет записан поток из MP4

```bash
# Открыть камеру через adb
adb shell am start -a android.media.action.IMAGE_CAPTURE
```

### Проверка VINTF

```bash
# Проверить manifest
adb shell cat /vendor/etc/vintf/manifest/fake_camera_hal.xml

# Проверить что HAL зарегистрирован
adb shell dumpsys media.camera | grep -i fake

# Проверить HAL процесс
adb shell ps -A | grep camera.provider-fake
```

### Смена видео на лету

Можно менять видеофайл без перезагрузки:

```bash
# Заменить файл
adb push new_video.mp4 /data/local/tmp/fake_video.mp4

# Перезапустить HAL
adb shell setprop ctl.restart fake_camera_hal

# Или через убийство процесса (init перезапустит)
adb shell pkill -f "camera.provider-fake"
```

### Автоматическая проверка (20 шагов)

Скрипт `verify_hal.sh` выполняет полную проверку установки:

```bash
./scripts/verify_hal.sh [серийник_устройства]
```

Проверяет: ADB подключение, root, KernelSU, HAL процесс, логи, VINTF,
видеофайл, gralloc версию, метаданные, SUSFS скрытность, Play Integrity,
FPN fingerprint, TimestampSync, Rolling Shutter Skew и др.

### Откат изменений

**Удаление модуля:**
1. Откройте **KernelSU Manager** → Модули
2. Выключите или удалите **FakeHAL Camera Provider**
3. Перезагрузите устройство

**Восстановление vbmeta:**
```bash
adb reboot bootloader
./scripts/vbmeta_patch.sh --restore
fastboot reboot
```

Если backup не сохранился, скачайте factory image:
```bash
# https://developers.google.com/android/images
fastboot flash vbmeta vbmeta.img
fastboot flash vbmeta_a vbmeta.img
fastboot flash vbmeta_b vbmeta.img
```

### Запуск unit-тестов

```bash
cd tests
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/run_tests --gtest_output=xml:test_results.xml
```

**Результаты тестирования: 66/66 PASSED**

| Набор тестов | Кол-во | Статус |
|-------------|--------|--------|
| MetadataRandomizerTest | 7 | ALL PASSED |
| NoiseOverlayTest | 9 | ALL PASSED |
| GyroWarpTest | 7 | ALL PASSED |
| JpegEncoderTest | 7 | ALL PASSED |
| PipelineIntegrationTest | 4 | ALL PASSED |
| HALIntegrationTest | 20 | ALL PASSED |
| VideoReaderFallbackTest | 3 | ALL PASSED |
| HALPerformanceTest | 1 | ALL PASSED |
| TimestampSyncTest | 8 | ALL PASSED |

**Производительность:** ~24ms/кадр, ~42 fps (порог: 100ms).

---

## 8. Структура проекта

```
fake_hal/
├── src/                                  # Исходный код (C++)
│   ├── main.cpp                          # Точка входа HAL-сервиса
│   ├── FakeCameraProvider.cpp            # AIDL ICameraProvider — регистрация HAL
│   ├── FakeCameraDevice.cpp              # ICameraDevice + Session — обработка кадров
│   ├── VideoFrameReader.cpp              # NDK MediaCodec: MP4 → NV21, loop, resize
│   ├── MetadataRandomizer.cpp            # Ornstein-Uhlenbeck дрейф ISO/выдержки/AWB/CCM
│   ├── NoiseOverlay.cpp                  # CMOS-шум: read + shot noise, FPN, hot/dead пиксели
│   ├── GyroWarp.cpp                      # IIO sysfs → complementary filter → warp NV21
│   ├── JpegEncoder.cpp                   # libjpeg-turbo + EXIF APP1 + CameraBlob trailer
│   ├── GrallocHelper.cpp                 # gralloc4/gralloc3/mmap fallback lock/unlock
│   ├── TimestampSync.cpp                 # Синхронизация timestamps камеры и IMU
│   └── VideoFrameReader.cpp              # NDK MediaCodec декодер MP4
│
├── include/                              # Заголовочные файлы (публичный API)
│   ├── FakeCameraProvider.h              # Интерфейс провайдера
│   ├── FakeCameraDevice.h                # Интерфейс устройства и сессии
│   ├── VideoFrameReader.h                # API видеодекодера
│   ├── MetadataRandomizer.h              # API генерации метаданных
│   ├── NoiseOverlay.h                    # API модели шума + FPN
│   ├── GyroWarp.h                        # API гироскопической коррекции
│   ├── JpegEncoder.h                     # API JPEG-кодирования
│   ├── GrallocHelper.h                   # API gralloc-абстракции
│   ├── TimestampSync.h                   # API синхронизации timestamp
│   ├── LensShading.h                     # Виньетирование (header-only)
│   ├── RollingShutter.h                  # Rolling shutter эффект (header-only)
│   └── android/                          # Мок-заголовки Android framework
│
├── tests/                                # Unit и интеграционные тесты
│   ├── CMakeLists.txt                    # CMake конфигурация тестов
│   ├── CMakeLists.android.txt            # CMake для Android-сборки тестов
│   ├── test_hal_integration.cpp          # 20 интеграционных тестов HAL
│   ├── test_pipeline_integration.cpp     # 4 теста полного pipeline
│   ├── test_metadata_randomizer.cpp      # 7 тестов метаданных
│   ├── test_noise_overlay.cpp            # 9 тестов шума и FPN
│   ├── test_gyro_warp.cpp               # 7 тестов гироскопической коррекции
│   ├── test_jpeg_encoder.cpp             # 7 тестов JPEG-кодирования
│   ├── test_timestamp_sync.cpp           # 8 тестов синхронизации timestamp
│   ├── test_gralloc_integration.cpp      # Тесты gralloc интеграции
│   └── mocks/                            # Мок-заголовки Android framework
│       ├── aidl/                          # AIDL интерфейсы (стабы)
│       ├── android/                       # Android API стабы
│       ├── camera/                        # CameraMetadata стабы
│       ├── hardware/                      # hardware стабы
│       ├── media/                         # NDK Media стабы
│       ├── system/                        # system стабы
│       └── ...
│
├── scripts/                              # Скрипты сборки и деплоя
│   ├── build.sh                          # Основной скрипт сборки (AOSP/NDK)
│   ├── prepare_video.sh                  # Конвертация видео для HAL
│   ├── vbmeta_patch.sh                   # Патч/восстановление vbmeta (AVB)
│   ├── module_structure.sh               # Создание структуры KernelSU модуля
│   ├── package_module.sh                 # Упаковка KernelSU модуля в zip
│   ├── deploy.sh                         # Деплой через adb
│   ├── deploy_susfs.sh                   # SUSFS overlay — bind mount + скрытие
│   ├── install_susfs.sh                  # Установка SUSFS
│   ├── verify_hal.sh                     # 20-шаговая автоматическая проверка
│   └── setup_runner.sh                   # Настройка GitHub Actions runner
│
├── ksu_module/                           # KernelSU модуль
│   ├── module.prop                       # Метаданные модуля (ID, версия, автор)
│   └── service.sh                        # Скрипт запуска при загрузке
│
├── vendor_overlay/                       # Файлы для /vendor раздела
│   └── etc/
│       ├── init/
│       │   └── fake_camera_hal.rc        # init.rc — автозапуск HAL как сервис
│       └── vintf/
│           ├── fake_camera_hal.xml       # VINTF manifest fragment (ICameraProvider)
│           └── manifest.xml              # Интеграционный manifest
│
├── sepolicy/                             # SELinux политики
│   └── fake_hal.te                       # Правила для домена hal_camera
│
├── docs/                                 # Документация
│   └── EMULATOR_TESTING.md              # Руководство по тестированию на эмуляторе
│
├── .github/workflows/                    # CI/CD
│   ├── ci-tests.yml                      # Unit-тесты (ubuntu-latest)
│   └── main.yml                          # AOSP сборка (self-hosted runner)
│
├── Android.bp                            # AOSP build definition (cc_binary)
├── CMakeLists.txt                        # Standalone CMake (production + тесты)
├── Makefile                              # Простая компиляция (syntax check)
├── fake_camera_hal.rc                    # init.rc для HAL сервиса
├── test_pattern.mp4                      # Тестовое видео (color bars)
└── .gitignore                            # Исключения Git
```

### Архитектура обработки кадра

```
┌──────────────────────────────────────────────────────────────┐
│                    Любое приложение                          │
│              (Camera2 API / CameraX / стоковая камера)       │
└────────────────────────┬─────────────────────────────────────┘
                         │ AIDL binder
┌────────────────────────▼─────────────────────────────────────┐
│                    cameraserver                              │
│             (не знает о подмене, работает штатно)            │
└────────────────────────┬─────────────────────────────────────┘
                         │ ICameraProvider AIDL
┌────────────────────────▼─────────────────────────────────────┐
│              android.hardware.camera.provider-fake           │
│                                                              │
│  ┌─────────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ VideoFrameReader│  │MetadataRand. │  │   GyroWarp     │  │
│  │  MP4 → NV21     │  │ISO/exp/AWB   │  │  IIO→Warp/Crop │  │
│  └────────┬────────┘  └──────┬───────┘  └───────┬────────┘  │
│           │                  │                   │           │
│  ┌────────▼──────────────────▼───────────────────▼────────┐  │
│  │              FakeCameraDeviceSession                   │  │
│  │    processOneRequest() → fillYUVBuffer() → gralloc    │  │
│  │         + NoiseOverlay + LensShading + RollingShutter  │  │
│  │         + JpegEncoder (libjpeg-turbo + EXIF)          │  │
│  └────────────────────────────────────────────────────────┘  │
│                          │                                   │
│  ┌───────────────────────▼──────────────────────────────┐   │
│  │              GrallocHelper (auto-detect)              │   │
│  │  gralloc4 (IMapper 4.0) → gralloc3 → mmap fallback  │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
                         │ KernelSU + SUSFS overlay
┌────────────────────────▼─────────────────────────────────────┐
│                   /vendor раздел                             │
│  Скрыт через SUSFS: bind mounts не видны в /proc/mounts     │
└──────────────────────────────────────────────────────────────┘
```

### Pipeline обработки каждого `processCaptureRequest()`:

1. **VideoFrameReader** — декодирование следующего кадра из MP4 → NV21 (MediaCodec NDK, loop при достижении конца)
2. **GyroWarp** — сдвиг/поворот на основе гироскопа (IIO sysfs, complementary filter 100Hz)
3. **LensShading** — виньетирование (cos⁴, сила 0.4)
4. **NoiseOverlay** — модель CMOS-шума (`σ = √(σ_read² · gain + σ_shot · L · 255 · gain)` + FPN)
5. **RollingShutter** — построчный сдвиг (33ms readout, до 200px)
6. **GrallocHelper** — `lock()` → `memcpy` NV21 → `unlock()` (YUV буферы)
7. **JpegEncoder** — JPEG + EXIF + CameraBlob trailer (BLOB буферы)
8. **MetadataRandomizer** — O-U дрейф ISO, выдержки, AWB gains, CCM
9. **TimestampSync** — корреляция timestamps камеры и IMU

---

## 9. Возможные ошибки и их решения

### Ошибки при установке

#### Ошибка: `repo` не найден
**Описание:** Команда `repo init` не работает.
**Решение:**
```bash
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH="$HOME/bin:$PATH"
# Добавить export в ~/.bashrc для постоянного эффекта
```

#### Ошибка: Неправильная версия Java
**Описание:** AOSP требует Java 17+, а установлена более старая версия.
**Решение:**
```bash
sudo apt install openjdk-17-jdk
sudo update-alternatives --config java
# Выбрать Java 17
java -version  # Должна показать 17.x.x
```

#### Ошибка: NDK не найден
**Описание:** `NDK не найден в $NDK_ROOT`.
**Решение:**
```bash
# Проверить путь
ls $NDK_ROOT/toolchains/llvm/prebuilt/

# Установить NDK, если отсутствует
sdkmanager "ndk;26.1.10909125"

# Или вручную:
export NDK_ROOT=$HOME/Android/Sdk/ndk/26.1.10909125
```

#### Ошибка: `cmake` версия < 3.16
**Описание:** CMake слишком старый для `cmake_minimum_required(VERSION 3.16)`.
**Решение:**
```bash
# Ubuntu
sudo apt remove cmake
sudo snap install cmake --classic

# Или через pip
pip3 install cmake --upgrade
```

---

### Ошибки при сборке

#### Ошибка: `Android.bp` — отсутствует src файл
**Описание:** `error: module "android.hardware.camera.provider-fake" ... missing source file`.
**Решение:**
```bash
# Проверить что все файлы скопированы в AOSP дерево
ls $AOSP_ROOT/hardware/google/camera/hal/fake/src/

# Должны быть все 9+1 файлов:
# main.cpp, FakeCameraProvider.cpp, FakeCameraDevice.cpp,
# VideoFrameReader.cpp, MetadataRandomizer.cpp, NoiseOverlay.cpp,
# GyroWarp.cpp, JpegEncoder.cpp, GrallocHelper.cpp, TimestampSync.cpp
```

#### Ошибка: `m: command not found`
**Описание:** AOSP build environment не инициализирован.
**Решение:**
```bash
cd $AOSP_ROOT
source build/envsetup.sh
lunch aosp_panther-userdebug
# Теперь m доступна
```

#### Ошибка: `undefined reference to ...` при NDK standalone сборке
**Описание:** AIDL HAL interfaces недоступны в standalone NDK.
**Решение:** Standalone сборка предназначена только для проверки синтаксиса.
Для рабочего бинарника используйте AOSP сборку.

---

### Ошибки при запуске

#### Ошибка: «No cameras available» / камера не определяется
**Описание:** VINTF manifest не установлен или HAL не зарегистрирован.
**Решение:**
```bash
# Проверить manifest
adb shell ls -la /vendor/etc/vintf/manifest/fake_camera_hal.xml

# Проверить что HAL процесс запущен
adb shell ps -A | grep camera.provider-fake

# Проверить логи запуска
adb logcat -s FakeHAL_Provider

# Ручной запуск для отладки
adb shell su -c /vendor/bin/hw/camera.provider-fake /data/local/tmp/fake_video.mp4
```

#### Ошибка: Приложение камеры крашится
**Описание:** `android.hardware.camera2.CameraAccessException` или crash при открытии камеры.
**Решение:**
```bash
# Детальные логи gralloc
adb logcat -s FakeHAL_Gralloc:V FakeHAL_Device:V

# Логи cameraserver
adb logcat -s cameraserver CameraProvider CameraService

# Типичные причины:
# 1. gralloc lock failed — проверить что GrallocHelper определил правильную версию
# 2. JPEG > blob buffer — видео слишком высокого качества, уменьшить разрешение
# 3. VINTF ошибка — проверить: adb shell vintf 2>&1 | grep "error\|fake"

# Перезапуск cameraserver
adb shell su -c "kill $(pidof cameraserver)"
```

#### Ошибка: HAL процесс не запускается
**Описание:** `ps -A | grep camera.provider-fake` возвращает пусто.
**Решение:**
```bash
# Проверить .rc файл
adb shell cat /vendor/etc/init/fake_camera_hal.rc

# Проверить SELinux
adb shell getenforce
adb shell dmesg | grep "avc: denied" | grep fake

# Временный permissive (только для отладки!)
adb shell setenforce 0

# Ручной запуск
adb shell su -c /vendor/bin/hw/camera.provider-fake /data/local/tmp/fake_video.mp4
```

---

### Ошибки с видео

#### Ошибка: Серые кадры вместо видео
**Описание:** MediaCodec не может декодировать видео.
**Решение:**
```bash
# Проверить формат видео
ffprobe /path/to/fake_video.mp4

# Требования: H.264 Baseline, yuv420p, moov atom в начале файла
# Перекодировать:
./scripts/prepare_video.sh input.mp4

# Или вручную:
ffmpeg -i input.mp4 -c:v libx264 -profile:v baseline -pix_fmt yuv420p \
    -r 30 -s 1920x1080 -b:v 8M -movflags +faststart -an -y fake_video.mp4

# Проверить на устройстве:
adb shell ls -la /data/local/tmp/fake_video.mp4
# Файл должен существовать и иметь ненулевой размер
```

#### Ошибка: Высокая задержка кадров (>50ms)
**Описание:** Лаг в preview, fps < 20.
**Решение:**
```bash
# Проверить CPU governor
adb shell cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Переключить на performance
adb shell su -c 'echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'

# Уменьшить разрешение видео (720p вместо 1080p)
./scripts/prepare_video.sh input.mp4 output.mp4 1280x720 30

# Проверить thermal throttling
adb shell cat /sys/class/thermal/thermal_zone*/temp
```

---

### Ошибки с Docker / средой сборки

#### Ошибка: Недостаточно места на диске для AOSP
**Описание:** AOSP source tree требует ~200 ГБ.
**Решение:**
```bash
# Проверить свободное место
df -h

# Использовать --depth=1 для частичной синхронизации
repo init -u https://android.googlesource.com/platform/manifest \
    -b android-14.0.0_r1 --depth=1
repo sync -c -j$(nproc) --no-tags
```

#### Ошибка: GitHub Actions runner — недостаточно ресурсов
**Описание:** AOSP сборка требует мощный сервер.
**Решение:** Минимальные требования для self-hosted runner:
- 64 ГБ RAM
- 500 ГБ SSD
- 8+ ядер CPU

```bash
# Настроить runner
./scripts/setup_runner.sh
```

---

### Ошибки с правами доступа

#### Ошибка: SELinux denial
**Описание:** HAL заблокирован SELinux при попытке чтения видео или IIO.
**Решение:**
```bash
# Проверить denial
adb shell dmesg | grep "avc.*camera"

# Временный permissive (только для отладки!)
adb shell setenforce 0

# Для production — установить правильную SELinux policy:
# Файл sepolicy/fake_hal.te содержит необходимые правила
```

#### Ошибка: `Permission denied` при adb push
**Описание:** Нет прав на запись в `/vendor` или `/data/local/tmp`.
**Решение:**
```bash
# Получить root
adb root

# Для /vendor — сначала remount
adb remount

# Для push в /data/local/tmp — root обычно не нужен
adb push fake_video.mp4 /data/local/tmp/
```

---

### Ошибки с vbmeta / bootloader

#### Ошибка: Bootloop после установки
**Описание:** Устройство зацикливается при загрузке.
**Решение:**
```bash
# 1. Загрузиться в recovery (Vol Up + Power)
# 2. Удалить KernelSU модуль:
#    /data/adb/modules/fake_hal/

# 3. Или в fastboot — восстановить vbmeta:
adb reboot bootloader    # или Vol Down + Power
fastboot flash vbmeta vbmeta_original.img
fastboot reboot

# 4. Если backup потерян — скачать factory image:
#    https://developers.google.com/android/images
fastboot flash vbmeta vbmeta.img
fastboot flash vbmeta_a vbmeta.img
fastboot flash vbmeta_b vbmeta.img
```

#### Ошибка: Play Integrity не проходит
**Описание:** DEVICE level integrity = FAIL.
**Решение:**
```bash
# Проверить SUSFS
adb shell su -c "cat /proc/susfs_version"

# Проверить build props
adb shell getprop ro.build.tags        # Должно быть: release-keys
adb shell getprop ro.build.type        # Должно быть: user

# Проверить что bind mounts скрыты
adb shell cat /proc/mounts | grep fake  # Должно быть ПУСТО
```

---

### Ошибки с портами и подключением

#### Ошибка: `adb devices` не показывает устройство
**Описание:** ADB не видит устройство.
**Решение:**
```bash
# Перезапустить adb сервер
adb kill-server
adb start-server
adb devices

# Проверить USB-отладку на устройстве:
# Настройки → Для разработчиков → USB-отладка (включить)
# Подтвердить доступ при подключении

# Проверить udev rules (Linux)
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666"' | \
    sudo tee /etc/udev/rules.d/51-android.rules
sudo udevadm control --reload-rules
```

#### Ошибка: `fastboot` не видит устройство
**Описание:** Устройство в режиме fastboot, но не определяется.
**Решение:**
```bash
# Проверить
fastboot devices

# Если пусто — проверить bootloader unlock:
# На устройстве: Settings → Developer options → OEM unlocking (включить)
# В fastboot:
fastboot flashing unlock
# ВНИМАНИЕ: это удалит все данные на устройстве!
```

---

### Ошибки с FPN (Fixed Pattern Noise)

#### Ошибка: FPN одинаковый на разных устройствах
**Описание:** Два устройства имеют идентичный noise pattern.
**Решение:**
```bash
# Проверить что серийник читается
adb logcat -d -s FakeHAL_Provider | grep "FPN fingerprint"

# Если "UNKNOWN_SERIAL":
adb shell su -c "cat /dev/urandom | head -c 16 | xxd -p > /data/misc/fake_hal_id"

# Перезапустить HAL
adb shell su -c "kill $(pidof camera.provider-fake)"
```

---

## 10. Лицензия

Код предоставляется «как есть» для образовательных и исследовательских целей.
Использование на реальных устройствах — на ваш страх и риск.

---

## Совместимость

| Устройство | SoC | gralloc | Статус |
|-----------|-----|---------|--------|
| Pixel 7 (panther) | Tensor G2 | 4.0 | Полная поддержка |
| Pixel 7 Pro (cheetah) | Tensor G2 | 4.0 | Полная поддержка |
| Pixel 7a (lynx) | Tensor G2 | 4.0 | Полная поддержка |
| Pixel 6 (oriole) | Tensor G1 | 4.0 | Ожидается совместимость |
| Pixel 8 (shiba) | Tensor G3 | 4.0 | Требует проверки |
| Samsung (Exynos) | — | 4.0 | Требует адаптации (свой gralloc) |
| Qualcomm | — | 4.0 | Требует адаптации (другой provider, ion allocator) |

---

## Скрипты проекта

| Скрипт | Описание |
|--------|----------|
| `scripts/build.sh` | Основной скрипт сборки (AOSP / standalone NDK) |
| `scripts/prepare_video.sh` | Конвертация видео в совместимый формат |
| `scripts/vbmeta_patch.sh` | Патч / восстановление vbmeta для отключения AVB |
| `scripts/module_structure.sh` | Создание структуры KernelSU модуля (.zip) |
| `scripts/package_module.sh` | Упаковка KernelSU модуля |
| `scripts/deploy.sh` | Деплой бинарника через adb |
| `scripts/deploy_susfs.sh` | Установка overlay через SUSFS (bind mounts + скрытие) |
| `scripts/install_susfs.sh` | Установка SUSFS патча |
| `scripts/verify_hal.sh` | 20-шаговая автоматическая проверка установки |
| `scripts/setup_runner.sh` | Настройка GitHub Actions self-hosted runner |

---

## Технические детали

### GrallocHelper — запись в буферы камеры

Главная сложность camera HAL — запись данных в буферы, выделенные
`cameraserver` через gralloc allocator. Буферы приходят как
`buffer_handle_t` (указатель на `native_handle_t` с DMA-BUF fd).

| Версия | API | Устройства |
|--------|-----|-----------|
| gralloc4 | IMapper 4.0 HIDL | Pixel 6/7/8, Android 12+ |
| gralloc3 | IMapper 3.0 HIDL | Android 10–11 |
| mmap fallback | `mmap()` через DMA-BUF fd | Универсальный fallback |

### MetadataRandomizer — Ornstein-Uhlenbeck процесс

Значения метаданных используют mean-reverting случайный процесс
`dX = θ(μ-X)dt + σdW`:

| Параметр | θ (скорость возврата) | σ (волатильность) | Диапазон |
|----------|----------------------|-------------------|----------|
| ISO | 0.05 | 3 DN | 50–3200 |
| Выдержка | 0.03 | 0.3 мс | 0.5–33 мс |
| AWB R/B gains | 0.02 | 0.008 | ~0.9–1.1 |
| AWB Gr/Gb gains | 0.02 | 0.003 | ~0.99–1.01 |
