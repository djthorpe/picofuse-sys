# picofuse-sys

Picofuse System

## Setup

Clone the repository with submodules:

```sh
git clone --recurse-submodules <repo-url>
```

If you already cloned the repository, initialize the Pico SDK submodule with:

```sh
git submodule update --init --recursive
```

## Build

For Pico targets, install the ARM embedded GCC toolchain first. On macOS with
Homebrew:

```sh
brew install --cask gcc-arm-embedded
```

If the toolchain is not on your `PATH`, pass its `bin` directory with
`-DPICO_TOOLCHAIN_PATH=/path/to/toolchain/bin` when configuring.

Configure the project into a dedicated `build` directory:

```sh
cmake -S . -B build -DPICO_BOARD=<board>
```

Build from that directory with:

```sh
cmake --build build
```

## Clean

```sh
cmake --build build --target clean
```

To fully reset the build tree:

```sh
rm -rf build
```
