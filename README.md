# Universal Flash Programmer

This project aims at developing a flash programmer software supports various of flash chips through various controllers on various platforms.

Currently this project is at an early developing stage.

Pull requests are welcome.

## Features
* Controller plugin support. Easy to add new controller support.
* Controller configuration. Supports configuration-specific features (multiple USB interface/drive strength/chip select/...).
* Abstraction Layer: Controller <- -> Interface <- -> Flash device. Easy to add multiple interface support.
* External Flash ID list support. Easy to add new flash without recompiling the project.
* Cross-platform. Supports Windows and Linux.

## SPI-NOR Features
* SFDP parsing for new flashes.
* Supports OTP regions.
* Supports non-volatile register modification.
* Supports write-protection modification.

## Generic NAND Features
* Supports ECC plugin to replace NAND's on-die ECC
* Supports Bad Block Table (BBT) plugin
* Supports simple Flash Translation Layer (FTL) plugin
* Supports OTP regions.
* Supports reading unique ID.

## SPI-NAND Features
* ONFI parameter page parsing for new flashes.

## Supported controllers
* WCH CH341 (100KB/s read)
* WCH CH347 (4.9MB/s read)
* FTDI MPSSE chips (FT232H/FT2232H/FT4232H) (3MB/s read)
* FTDI FT4222H (6MB/s read using QSPI)
* Programmer using serprog protocol (860KB/s read using USB FS)

## Supported interfaces
* SPI (Single/Dual/Quad/QPI using SPI-MEM)
* TBD: I2C/Parallel NAND

## Supported flash types
* SPI-NOR
* SPI-NAND
* TBD: Parallel NAND/24 EEPROM

## Supported SPI-NOR flashes
* Atmel/Adesto/Dialog/Renesas
* EON
* ESMT
* GigaDevice
* Intel
* ISSI
* Macronix
* Microchip/SST
* Micron
* Spansion/Cypress/Infineon
* Winbond
* XMC
* XTX
* TBD: FudanMicro/Puya/Zbit/Zetta...

## Supported SPI-NAND flashes
* Alliance Memory
* ATO Solution
* CoreStorage
* Dosilicon
* ESMT
* Etron
* Fidelix
* FORESEE
* Fudan Microelectronics
* GigaDevice
* HeYangTek
* ISSI
* Macronix
* Micron
* MK Founder
* Paragon
* Toshiba/Kioxia
* Winbond
* XTX
* Zetta

## Build Prerequisites
* libusb
* hidapi-libusb
* json-c

## Supported compilers
* MSVC (Windows only. Suggested to use with vcpkg)
* GCC (Windows (mingw) and Linux)

## Install

### Linux

```bash
git clone https://github.com/hackpascal/ufprog.git

cd ufprog

cmake -DCMAKE_BUILD_TYPE=None \
    -DBUILD_PORTABLE=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -B build \
    -G Ninja

ninja -C build

ninja -C build install
```

### Arch Linux

Installation can be done via [AUR ufprog-git](https://aur.archlinux.org/pkgbase/ufprog-git).

```bash
yay -Syu ufprog
```

## License
* For executable programs, GPL-2.0-only
* For libraries, LGPL-2.1-only
