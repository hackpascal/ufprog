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

## Supported controllers
* WCH CH341 (100KB/s read)
* WCH CH347 (4.9MB/s read)
* FTDI MPSSE chips (FT232H/FT2232H/FT4232H) (3MB/s read)
* FTDI FT4222H (6MB/s read using QSPI)

## Supported interfaces
* SPI (Single/Dual/Quad/QPI using SPI-MEM)
* TBD: I2C/Parallel NAND

## Supported flash types
* SPI-NOR
* TBD: SPI-NAND/Parallel NAND/24 EEPROM

## Supported SPI-NOR flashes
* Winbond
* Microchip/SST
* GigaDevice
* ISSI
* Intel
* ESMT
* EON
* TBD: Macronix/XMC/Micron/Spansion/...

## Build Prerequisites
* libusb
* hidapi-libusb
* json-c

## Supported compilers
* MSVC (Windows only. Suggested to use with vcpkg)
* GCC (Windows (mingw) and Linux)

## License
* For executable programs, GPL-2.0-only
* For libraries, LGPL-2.1-only