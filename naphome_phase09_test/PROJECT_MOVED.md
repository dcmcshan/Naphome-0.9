# Project Moved to Root

This project has been moved from `esp-skainet/examples/naphome_phase09_test` to the project root at `naphome_phase09_test`.

## Changes Made

1. **Project Location**: Moved from `esp-skainet/examples/naphome_phase09_test` to `naphome_phase09_test/`
2. **CMakeLists.txt**: Updated component paths to use absolute paths relative to project root
3. **README.md**: Updated build instructions to reflect new location
4. **Code Fix**: Fixed `afe_data` scope issue in `app_main()`

## Building

From the project root:

```bash
cd naphome_phase09_test
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Component Paths

The project now references components using absolute paths:
- `${PROJECT_ROOT}/esp-skainet/components/hardware_driver`
- `${PROJECT_ROOT}/esp-skainet/components/player`
- `${PROJECT_ROOT}/esp-skainet/components/sr_ringbuf`

Where `PROJECT_ROOT` is automatically calculated as the parent directory of `naphome_phase09_test`.
