# usbhidproxy
  usbhidproxy is a USB HID proxy program running on a RP2040 based board to swap control/caps of keyboard or left/right button of mouse like HID remapper.

  You can swap keys without OS support (like OS installer or game console machine).

## Proxy Hardware
- Picossci USB host : https://www.switch-science.com/products/9158

## Checked USB devices
- Keyboard
  - Realforce 87U : https://www.realforce.co.jp/en/products/discontinued/87U_SE07T0/
  - Realforce TKL S : https://www.realforce.co.jp/en/products/discontinued/R2TLS-US-BK/USV.html
- Mouse
  - No-brand mouse

## Behavior
  Proxy hardware receives HID data from a USB device via PIO-USB + tinyusb(host).

  It sends modified data to PC(host) via RP2040 USB + tinyusb(device).

  Both USBs use low-speed mode.

  Current code swaps control/caps or left/right mouse buttons.

  Unlike HID remapper, proxy uses a descriptor USB HID device.  From OS, proxy hardware looks like a connected USB HID device.  I don't know whether it complaints with USB standard, so use where you can take responsibility by yourself.

## To customize swap keys/buttons
  Change code.  No configuration file or method.

## Build
- Setup Raspberry Pi Pico development environment.
- `git clone` or download source tree.
- `cd usbhidproxy`
- `git submodule update --init`
- `mkdir build`
- `cd build`
- `cmake ..`
- `make -j` or `make -j VERBOSE=1`
- Copy `usbhidproxy.uf2` to the board in USB mass storage mode.
- Connect a USB HID device.
- Reset

## Notice
- There is no USB hub function.  Connect one device to one proxy hardware.
- When unplug, unplug proxy hardware at first.  Next, unplug a USB device from proxy hardware.
  - The code to reset a USB device is there, but not checked strictly whether it works or not.
- By memory constraint, there are some restrictions
  - Only one language of USB descriptor is supported.
  - USB descriptor report and HID report size is limited.
  - If a USB HID device has a big descriptor or report size, it may not work.
  - One USB HID device may have some instances of HID. Currently, a maximum number of instance is 8(`HID_INSTANCE_MAX`).
- This does not parse descriptor report and has an assumption that a USB device has a typical(normal) layout.
  - Some devices may not work correctly.
- WinUSB is not supported.
- This works in low-speed mode.  If full or hi speed is required, it does not work.
- Gamepad or other HID device is not supported.

## Furthermore
- It is easy to remap whole keycode like Dvorak or etc if you want.
  - To debug, UART must be wired for `debugPrintf()`.
- To use another proxy hardware (including Raspberry Pi Pico + USB A receptacle cable), add a header file to `board_include/` and check whether `tusb_config.h` is correct for the board.
  - RP2350 is not tested. (ex. Pico 2 or https://www.waveshare.com/wiki/RP2350-USB-A )

## Reference
- HID remapper : https://github.com/jfedor2/hid-remapper
- Pico-PIO-USB : https://github.com/sekigon-gonnoc/Pico-PIO-USB
- Raspberry Pi Pico SDK : https://github.com/raspberrypi/pico-sdk

## License Notice
  Submodules are licensed under their own respective licenses.
