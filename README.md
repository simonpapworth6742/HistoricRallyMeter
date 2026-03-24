# HistoricRallyMeter

A precision timing and speed measurement application for historic car regulation rallies, built for Raspberry Pi 5 with dual-display support.

## Introduction

In historic car regulation rallies, drivers must maintain set average speeds over multi-segment stages, with timing accuracy measured to within 0.1 seconds over distances ranging from 2 km to hundreds of kilometres. HistoricRallyMeter provides real-time speed, distance, and timing calculations by reading pulse counters connected to the vehicle's gearbox or wheels via I2C, displaying the information across two dedicated screens — one for the driver and one for the co-pilot.

The driver display shows current speed, target speed, total and trip averages, and a semicircular rally gauge indicating how many seconds ahead or behind the target pace. The co-pilot display provides stage setup, segment management, calibration, and detailed timing controls.

## Features

- **Dual 1280x400 displays** — dedicated driver and co-pilot windows, auto-detected and fullscreened on matching monitors
- **Rally gauge** — semicircular ahead/behind timing indicator with auto-scaling (green/yellow/red) and needle
- **Real-time speed calculation** — rolling 10-second average from hardware counters polled at up to 200 Hz
- **Segment management** — multi-segment stages with target speeds, automatic or manual progression
- **Ahead/behind timing** — high-precision calculation with speed adjustment arrows and audible tone feedback
- **Dual counter support** — single gearbox counter or averaged dual wheel counters
- **Calibration** — stored as millimetres per 1000 counts for integer-precision distance calculation
- **Persistent state** — all settings, segments, and window positions saved to JSON

## Hardware Requirements

- **Raspberry Pi 5** (4GB+ recommended)
- **LSI ls7866c 32-bit counters** — two devices on I2C bus 1 at addresses 0x70 and 0x71
- **Displays** — two 1280x400 screens (e.g. Waveshare 7.9" bar displays) connected via DSI/HDMI; the app also works with a single standard monitor during development
- **Audio output** — for speed adjustment tone feedback (ALSA default device)

## Installation on Raspberry Pi 5

### 1. Install build dependencies

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-3-dev libasound2-dev git
```

### 2. Enable I2C

```bash
sudo raspi-config
```

Navigate to **Interface Options > I2C > Enable**, then add your user to the i2c group:

```bash
sudo usermod -aG i2c $USER
```

Reboot for changes to take effect. Verify with:

```bash
ls /dev/i2c-1
```

### 3. Set I2C baud rate to 400 kHz

The default I2C baud rate (100 kHz) is too slow for reliable high-frequency counter polling. Increase it to 400 kHz by editing the boot config:

```bash
sudo nano /boot/firmware/config.txt
```

Find the line:

```
dtparam=i2c_arm=on
```

And change it to (or add if not present):

```
dtparam=i2c_arm=on,i2c_arm_baudrate=400000
```

Reboot for the change to take effect. Verify the baud rate with:

```bash
sudo cat /sys/module/i2c_bcm2835/parameters/baudrate
```

### 4. Clone and build

```bash
git clone https://github.com/simonpapworth6742/HistoricRallyMeter.git
cd HistoricRallyMeter
make all
```

### 5. Prevent inactive window dimming (recommended)

GTK3 dims unfocused windows by default, which makes the second display hard to read. To fix this:

```bash
mkdir -p ~/.config/gtk-3.0
cp gtk-example-gtk.css ~/.config/gtk-3.0/gtk.css
```

### 6. Add desktop shortcut

Copy the included `.desktop` file to your desktop for easy launching:

```bash
cp HistoricRallyMeter.desktop ~/Desktop/
chmod +x ~/Desktop/HistoricRallyMeter.desktop
```

The shortcut assumes the project is cloned to `~/HistoricRallyMeter`. If you cloned to a different location, edit the `Exec=` line in the `.desktop` file to match.

### 7. Run

```bash
./HistoricRallyMeter
```

Run from the project directory — the application reads and writes `rally_config.json` relative to the current working directory. On first run, default values are used and saved on exit. You can also launch from the desktop shortcut created in step 6.

## Build Commands

| Command | Description |
|---------|-------------|
| `make all` | Release build with `-O2` optimisation |
| `make debug` | Debug build with symbols (`-g -O0`), produces `HistoricRallyMeter_debug` |
| `make test` | Build and run unit tests |
| `make clean` | Remove all build artifacts |

## Configuration

Application state is stored in `rally_config.json` (created automatically on first exit):

- **units** — KPH (default) or MPH
- **calibration** — millimetres per 1000 counter increments (set via the co-pilot calibration screen)
- **counters** — single gearbox counter or dual wheel counters
- **segments** — target speed and distance for each stage segment
- **window state** — driver window size and monitor assignment

This file is in `.gitignore` as it contains per-installation state.

## Development

- **Design specification**: see [Design.md](Design.md) for the full application design and requirements
- **IDE support**: VS Code / Cursor debug configurations are provided in `.vscode/` (launch, tasks, IntelliSense)
- **C++ standard**: C++20
- **GUI framework**: GTK3 with Cairo for custom gauge rendering
- **Dependencies**: GTK3, ALSA (libasound), pthreads
