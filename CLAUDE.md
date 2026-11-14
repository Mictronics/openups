# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

`openups` is a C++ CLI (libusb) that reports status of and configures Mini-Box.com USB HID power devices: OpenUPS, OpenUPS2, NUC-UPS and DCDC-USB. Device flash has limited write cycles (<100k), so never write config periodically.

## Build

    cmake . && make          # produces ./openups
    sudo make install
    cmake -D_DEBUG=OFF .     # _DEBUG (debug messages) defaults to ON

Requires libusb (`usb` is linked). There are no tests or linters. New source files must be added to the `SOURCES` list in `CMakeLists.txt` by hand. Build output (`Makefile`, `CMakeCache.txt`, `*.cfg`, the `openups` binary) is gitignored; any `*.cfg` files in the tree are local, untracked config exports. Snap packaging lives in `snap/snapcraft.yaml`.

## Usage

    sudo ./openups -h                          # list devices and per-device commands
    sudo ./openups -t openups -s               # status only (omit -s for status + config)
    sudo ./openups -t openups -o file.cfg [-c] # export config (-c adds descriptions)
    sudo ./openups -t openups -i file.cfg      # import config (restarts the UPS)
    sudo ./openups -t dcdcusb -e set_vout:11.5 # device command, `name:param`

Device names: `dcdcusb`, `openups`, `openups2`, `nucups`. Options are parsed in `src/main.cpp` (`getopt "t:i:o:e:cshrbn:"`; `-n` selects among multiple devices).

## Architecture

- `src/devices.h`: table of supported devices (name, USB vendor/product ID, max transfer size, device-specific `-e` commands). Add new devices here.
- `src/lib/usbhid.*`: `USBHID`, the libusb HID transport.
- `src/lib/HIDInterface.*`: abstract base for all devices. It implements the shared parts (message send/recv, `varsToFile`/`fileToVars` for the `KEY=value` cfg format, `executeCommand` parsing of `cmd:param`). Subclasses supply the device specifics: `GetStatus`, `Read/Write/EraseConfigurationMemory`, `parseMessage`, `printValues`, and value conversion.
- `src/lib/HID{OpenUPS,OpenUPS2,NUCUPS,DCDCUSB}.*`: one subclass per device. Each defines its own `ATXMSG` table (`GetMessages()`), which maps each config variable to an index, length, read mode (1=normal, 2=float, 3=hh:mm:ss), multiplier, limits, unit and description. That table drives status display, cfg export/import and the `-c` comments, so adding or changing a config variable usually means editing only that table.
- `src/utils/`: small helpers (`HArray`, `util`).
