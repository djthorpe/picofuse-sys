# Debugging on the Raspberry Pi Pico using the debug probe

These are just some notes on how to debug using the [Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html):

* You will need two USB ports available on your debugging host - one for powering the Pico board, and the other for the debug probe.
* There is a Raspberry Pi specific branch of the software (OpenOCD) that is used to communicate with the debug probe. You can find it [here](https://github.com/raspberrypi/openocd)
* You'll need a serial terminal program to connect to the Pico board. On macOS, you can use `minicom`.
* This guide uses Visual Studio Code with the Cortex-Debug extension, but you can use any IDE that supports GDB debugging.

## Download the software

Download the OpenOCD pre-built binary for your platform from the Raspberry Pi (SDK GitHub repository) [https://github.com/raspberrypi/pico-sdk-tools/releases]. For
example, for MacOS, download the `openocd-0.12.0+dev-mac.zip`, and extract it to a folder of your choice. On MacOS specifically, you should also remove the "quarantine"
bit:

```bash
wget https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.3.0-1/openocd-0.12.0+dev-mac.zip
cd /opt && unzip openocd-0.12.0+dev-mac.zip
xattr -d com.apple.quarantine /opt/openocd-0.12.0+dev-mac/*
```

## Setup Visual Studio Code

TODO

## Compiling

TODO

## Debugging

TODO
