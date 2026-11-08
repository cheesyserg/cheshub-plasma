# cheshub-plasma

A KDE Plasma 6 System Settings module for the devices supported by
[ChesHub](https://github.com/cheesyserg/cheshub), talking to the hardware
directly through `/dev/hidraw`. No browser and no background service.

It appears under **System Settings → Input & Output → ChesHub Devices**, which
lists the supported devices that are connected; click one to open its page.

| Device | Link | What you can change |
|---|---|---|
| **TECKNET TK-GM012** mouse | USB cable, 2.4 GHz receiver | Report rate, six DPI stages with indicator colours, active stage, six button bindings, sleep timeout, move wake-up, factory reset |
| **iLovbee B75** keyboard | USB cable, 2.4 GHz receiver | 17 lighting effects with per-effect brightness, speed and seven-colour palette and a live preview on the drawing, debounce, sleep, base-layer key remapping, knob / End-key layout, factory reset |

The AJAZZ AJ139 is planned next. Exact byte layouts and what is not exposed
(and why) are in [docs/HARDWARE.md](docs/HARDWARE.md).

## Install

Every push builds packages in GitHub Actions
([`.github/workflows/build.yml`](.github/workflows/build.yml)):

| Distribution | Package |
|---|---|
| Debian 13 (built there; Ubuntu 25.04+ should also work) | `.deb` |
| Fedora | `.rpm` |
| Arch Linux, EndeavourOS, Manjaro | `.pkg.tar.zst` |

Download it from the **Releases** page, or from the latest workflow run's
artifacts (Actions → Build packages → run → Artifacts), then:

```bash
sudo apt install ./cheshub-plasma_*.deb
```

```bash
sudo dnf install ./cheshub-plasma-*.rpm
```

```bash
sudo pacman -U cheshub-plasma-*.pkg.tar.zst
```

The package installs a udev rule (`/usr/lib/udev/rules.d/70-cheshub-plasma.rules`)
that lets the logged-in user open the devices, and reloads udev, so nothing
else needs setting up. If a device was plugged in during installation and is
still listed with a permission error, re-plug it (or its receiver).

To publish a release with the packages attached, push a tag:

```bash
git tag v0.1.0 && git push origin v0.1.0
```

## Use

1. Plug in the device, or its 2.4 GHz receiver.
2. Close the vendor utility or a ChesHub browser tab that has the device open.
3. Open System Settings → Input & Output → ChesHub Devices and click the
   device. It is read as soon as it is connected; Read from Device (top right)
   reads it again.
4. Change settings and press **Apply**. **Reset** reads the device again;
   **Defaults** loads the factory values into the page (written on Apply);
   **Factory Reset…** resets the device itself.

`cheshubctl` checks access and dumps what a device reports, without touching
it:

```bash
cheshubctl list
```

```bash
cheshubctl read
```

## Build from source

Needs CMake, Qt ≥ 6.6, KF6 (KCMUtils, CoreAddons, I18n) and
extra-cmake-modules.

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
```

```bash
cmake --build build && ctest --test-dir build
```

```bash
sudo cmake --install build
```

`-DBUILD_KCM=OFF` builds only the protocol library, the tests and
`cheshubctl`, with just Qt 6.4+ (no KDE Frameworks).

## Layout

```
src/protocol/   byte-exact encoders/decoders (plain C++, no Qt)
                  tkgm012.*   TK-GM012 frames, DPI codec, button records
                  b75.*       B75 wired/wireless frames, config block, matrix
                  b75keys.cpp generated from cheshub's keymap.js
src/hid/        hidraw enumeration, report descriptor parser, I/O and ioctls
src/drivers/    blocking sessions with the vendor's ack / retry / timing rules
src/devices/    Qt objects for QML; I/O runs on a worker thread per device
src/kcm/        KQuickConfigModule, kcm_cheshub.json, ui/*.qml (Kirigami):
                  main.qml device list, DevicePage.qml shell,
                  MousePage/MouseArt, KeyboardPage/KeyboardArt (live preview)
src/tools/      cheshubctl
tests/          QtTest checks against the captures in cheshub's docs
data/           udev rule
packaging/      install scripts and the Arch PKGBUILD
```

The module's metadata is `src/kcm/kcm_cheshub.json`, named after the plugin
target as `kcmutils_add_qml_kcm` expects, with
`X-KDE-System-Settings-Parent-Category: input-devices` (Input & Output).

## License

GPL-3.0-or-later, like ChesHub.
