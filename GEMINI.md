# GEMINI.md

## Project Overview

GXDE Control Center is the system settings panel (control center) for the **GXDE Desktop Environment**. It is a fork of Deepin's `dde-control-center` (GCC V4.x) and **must not be installed on Deepin**.

- **Language:** C++17
- **UI toolkit:** Qt6 (Widgets, DBus, Network, Concurrent, Multimedia, Svg) + DTK (Deepin Tool Kit): `Dtk6Core`, `Dtk2widget`, `Dtk6Log`
- **Backends:** D-Bus (`DFrameworkDBus`), `gsettings-qt6`, `KF6NetworkManagerQt` + `libnm`, `gxde-network-utils-qt6`, `GxdeBernese`, X11/XCB (`xcb-ewmh`, `xrandr`, `mtdev`), Wayland (`wayland-client`, `LayerShellQt`)
- **License:** GPL-3.0
- **Binary:** `/usr/bin/gxde-control-center` (installed via `make install`); module plugins live in `/usr/lib/gxde-control-center/modules/`.

## Architecture

The code is organized around a central **Frame** (a `DBlurEffectWidget`) that hosts a navigation bar, a stack of content pages, and a collection of settings **modules**. Each module follows a **Model / View / Worker** pattern and communicates with system backends over D-Bus or GSettings.

Key layers:

- **`src/frame/`** — the main application
  - `main.cpp` — entry point; parses CLI args, selects X11 vs. Wayland (LayerShell) at runtime, registers the D-Bus service.
  - `frame.{h,cpp}` — the main window (`Frame`), content/page stack, and screen/region handling.
  - `navigation/` — left navigation bar (`navmodel`, `navwidget`, `navdelegate`).
  - `interfaces/` — `ModuleInterface`, `PluginInterface`, `FrameProxyInterface` (the contracts every module implements).
  - `modules/` — one subdirectory per settings area (accounts, bluetooth, datetime, defapp, display, keyboard, mouse, network, personalization, power, sound, sync, systeminfo, update, wacom).
  - `widgets/` — reusable DCC widgets (settings items, sliders, switch, passwd edit, etc.).
  - `quick_control/` — quick-control panel UI (bluetooth, wifi, vpn).
  - `wayland/` — Wayland helpers and generated protocol bindings (`dde-shell`, `kywc-output-v1`, `ukui-shell`).
  - `notify/` — notification center.
- **`src/dialogs/`** — a separate small binary, `reboot-reminder-dialog`.
- **`src/helpers/`** — helper tools (`gxdm-autologin`, `blur-settings`).

### Module Pattern

A settings module is a class implementing `dcc::ModuleInterface` (see `src/frame/interfaces/moduleinterface.h`). Its lifecycle methods are:

- `initialize()` — light-weight setup (no heavy resource use).
- `moduleActive()` / `moduleDeactive()` — fetch/release data and backends.
- `name()`, `moduleWidget()`, `showPage()`, `contentPopped()`, `reset()`.

The module typically owns a `*model`, `*worker`, and `*widget` trio; the worker performs D-Bus/GSettings I/O and feeds the model, which the widget renders.

## Building and Running

Dependencies are enumerated in `README.md` and `debian/control`. A first-time build can install them automatically via the build script.

Manual build:

```bash
mkdir Build && cd Build
cmake ..        # CMake >= 3.7
make
sudo make install
```

DEB build script (recommended for first build / debugging):

```bash
./build-deb -d     # install dependencies and build
./build-deb        # build directly (deps already present)
./build-deb -c     # clean artifacts
```

Run with `gxde-control-center -h` for CLI options. Notable flags: `-s/--show`, `-t/--toggle`, `-m <module>` (open a specific module), `-p <page>` (open a specific page within a module).

## Testing

Tests are registered with **CTest** (`include(CTest)` in the root `CMakeLists.txt`). `tests/` contains CMake-script-based checks:

- `check-qt-public.cmake` — verifies the binary has no direct dependency on Qt private/unsupported APIs.
- `check-no-kwindoweffects.cmake` — verifies no `kwindoweffects` dependency.

Run with `ctest` from the build directory (enabled when `BUILD_TESTING` is on).

## Development Conventions

- **C++17**, with `CMAKE_AUTOMOC` and `CMAKE_AUTORCC` enabled; `-g -Wall` in all builds, `-Ofast` added for non-Debug.
- **`QT_NO_KEYWORDS` is defined globally** (GIO headers define a conflicting `signals` member). Use `Q_SIGNALS` / `Q_SLOTS` / `Q_EMIT` instead of the `signals` / `slots` / `emit` macros.
- Namespace: framework types live under `namespace dcc`.
- Use `DWIDGET_USE_NAMESPACE` / `DCORE_USE_NAMESPACE` where DTK types are used heavily.
- **Model/View/Worker separation** within modules (see above). Keep D-Bus/backend calls inside the worker, not the widget.
- Chinese comments are common throughout the codebase; do not remove or "clean up" them.
- Copyright headers (GPL-3.0, Deepin Technology Co., Ltd.) are retained at the top of source files.

### Feature Flags / Build Macros

`src/frame/CMakeLists.txt` defines a large set of compile-time toggles via `-D` definitions (e.g. `DISABLE_ACCOUNT`, `DISABLE_NETWORK`, `DISABLE_POWER`, `DCC_DISABLE_GRUB`, `DCC_UPDATE_USE_PACKAGEKIT`, `DCC_DISABLE_TESTING_GROUP`). Architecture-specific sets are applied for `x86_64`, `sw_64`, `mips64`, and `aarch64`. When adding a module, register its source list in the corresponding `*_FILES` variable and add it to `SRCS` and `QRC`.

## Translations

- `.ts` files live in `translations/` (one per locale, prefixed `gxde-control-center_` and `dialogs_`).
- `.qm` files are generated in release builds via `translate_generation.sh`.
- Helper scripts: `translate_src2ts.sh`, `translate_desktop2ts.sh`, `translate_ts2desktop.sh`, `translate_generation.sh`.

## Packaging

- **Debian:** `debian/` (control, rules, changelog) plus the `build-deb` script.
- **Arch:** `arch/PKGBUILD`.
- Polkit policies (`.policy` files at repo root) and D-Bus service file (`com.deepin.dde.ControlCenter.service`) are installed by the root `CMakeLists.txt`.

## D-Bus Service

The application registers the session-bus service **`com.deepin.dde.ControlCenter`** at path `/com/deepin/dde/ControlCenter` (see `main.cpp` and `dbuscontrolcenterservice.{h,cpp}`). Repeated launches forward to the running instance via `DDBusSender`.

## Commit Conventions

Changelog is generated from conventional commits via `git-clog` (see `.clog.toml`); `chore` and `refactor` are grouped under "Others". `CHANGELOG.md` is auto-generated. Do not edit generated changelog entries by hand.
