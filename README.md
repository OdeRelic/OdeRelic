# OdeRelic

OdeRelic – A lightning-fast, ultra-modern, cross-platform utility to manage, convert, and explore your Optical Drive Emulators for Retro game consoles.

## 📖 About

OdeRelic is an open-source Native C++ and Qt QML desktop application dedicated to managing your ODEs for Playstation 1 (PSIO and XStation), PlayStation 2 Open PS2 Loader (OPL), Dreamcast GDEMU and others structure gracefully.

This software is intented for managing your ODEs and not for distribution of copyrighted content. You must use this software only with games you own.


## ✨ Features

### ✅ Current

- **High-Performance**: Built natively on C++ . Extremely slim memory footprint.
- **Cross-Platform Support**: Natively compiles and functions on Windows, macOS, and Linux out-of-the-box.
- **Mass Library Managment**: Instant read speeds of massive collections across local and external media.
- **Automated BIN to ISO**: Automatic conversion of BIN/CUE files to ISO format.
- **Intelligent Art Scraper**: Automatically identifies GameIDs within your binaries and download game ART


### 🚧 Roadmap 

- PS1 Game Native support.
- Dreamcast GDEMU support.
- Saturn SAROO support.
- Cheats support.  
- Direct VMC (Virtual Memory Card) header parsing and previewing.
- Expanded localization integrations.

## 💻 Building from Source

Because OdeRelic targets native execution architectures, you must compile it via CMake.

### Dependencies
- **CMake** (3.16+)
- **Qt 6.5+ Core Framework** (including `QtQuick`, `QtNetwork`, `QtCore`, etc)
- A standard C++17 compatible compiler (e.g. `g++`, `clang`, or `MSVC`)

### Steps

1. Clone the repository.
2. Initialize inside the build directory:
```bash
mkdir build && cd build
cmake ..
```
3. Compile using hardware threads:
```bash
make -j$(sysctl -n hw.ncpu) # MacOS/Linux
# OR
cmake --build . --config Release # Windows
```
4. Find your native executable ready inside your platform's mapped output stream. (e.g., `OdeRelic.app` on macOS).


## 🚀 Getting Started

1. Launch OdeRelic.
2. Direct the file overlay layer toward your targeted ODE root storage (Usually a USB or Internal Disk).
3. Jump into the 'Imports' tab, feed in disparate `.bin`/`.cue`/`.iso` packages from the filesystem via **"Add Games"**, queue them securely, and click process.
4. Enjoy natively curated OPL files!

## 📜 License

This project retains its initial protections via the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for further legal matrix details.

## 🤝 Contributing

Native optimizations and pull requests are overwhelmingly welcome here. Run the local `ctest` native array tests thoroughly on your branches prior to issuing PR hooks to ensure strict C++ data boundaries maintain their integrity!
