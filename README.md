# OdeRelic

A cross-platform utility to manage, convert, and organize your library for Optical Drive Emulators (ODE) on retro game consoles.

## About

OdeRelic is an open-source C++ and Qt QML desktop application designed to manage ODE structures for PlayStation 1 (PSIO, XStation, POPSTARTER), PlayStation 2 (Open PS2 Loader / OPL), and Dreamcast (GDEMU).

This software is intended for managing your ODE for Retro Consoles and is not for the distribution of copyrighted content. You must use this software only with games you own.

## Features

### Current

- **OPL Implementation**: Built natively in C++ targeting cross-platform compilation (Windows, macOS, Linux).
- **Library Management**: Concurrent file reading of large collections across local and external filesystems.
- **Automated BIN to ISO/VCD**: Fast conversion of BIN/CUE files to ISO (PS2) or VCD (PS1) formats.
- **PS1 / POPSTARTER Integration**: Automated PS1 Game ID detection, multi-track sizing, prerequisite validation (`POPSTARTER.ELF`, `POPS_IOX.PAK`), and automated `/POPS` structural hierarchy enforcement.
- **Automated Art Scraper**: Evaluates GameIDs within binary payloads and downloads corresponding UI game art for PlayStation 1 and 2 platforms.

## Getting Started

1. Launch OdeRelic.
2. Select the root Storage media that you will use on your console (e.g. USB, SD Card, External HDD).
3. Open the **Imports** tab and click **Add Games** or **Add Folder** to select raw `.bin`/`.cue`/`.iso` files.
4. Finalize selection and click **Process** to trigger automated format extraction, Game ID fetching, and directory deployments.

## 💻 Installation

Grab the latest release from the Releases page. [Releases](https://github.com/OdeRelic/OdeRelic/releases) page.

### 🐧 Linux

1. Download the **Linux `.deb`** file .
2. Extract it.
3. Run the `OdeRelic` file.

---

### 🍏 macOS

1. Download the `.dmg` file:
   - **arm64** (Apple Silicon - _only arm macs supported for now_)
2. Open the `.dmg` file.
3. Drag **OdeRelic** to your **Applications** folder.
4. Execute 'xattr -dr com.apple.quarantine ~/Applications/OdeRelic.app' in your **terminal** (App is currently **unsigned** but **100% safe**)
5. **Run** the **app**.

---

### 🪟 Windows

1. Download the `.exe`.
2. **Run it**.
3. If **SmartScreen** appears, click the **"Run anyway"** button (The app is 100% safe)

---

### Roadmap

- PS1 Xstation and PSIO support
- Dreamcast GDEMU support.
- Saturn SAROO support.
- Cheats manager.
- Direct VMC (Virtual Memory Card) header parsing and previewing.
- Expanded localization integrations.

## Building from Source

OdeRelic uses CMake as its primary build system.

### Dependencies
- **CMake** (3.16+)
- **Qt 6.5+ Framework** (`QtQuick`, `QtNetwork`, `QtCore`)
- C++17 compatible compiler (`g++`, `clang`, or `MSVC`)

### Build Steps

1. Clone the repository.
2. Initialize the build directory:
```bash
mkdir build && cd build
cmake ..
```
3. Compile using hardware threads:
```bash
make -j$(sysctl -n hw.ncpu) # macOS/Linux
# OR
cmake --build . --config Release # Windows
```
4. Output executes natively within the build directory.

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for further legal information.

## Contributing

Pull requests are welcome. Execute the local `ctest` regression suite on your branch prior to issuing a pull request to ensure backend data parsers remain stable.
