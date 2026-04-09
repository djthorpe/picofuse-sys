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

Configure the project into a dedicated `build` directory:

```sh
cmake -S . -B build -DPICO_BOARD=<board>
```

Build from that directory with:

```sh
cmake --build build
```
