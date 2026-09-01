# picofuse

NOTE This is being migrated to <https://github.com/mutablelogic/picofuse>


Supported `PICO_BOARD` values in this repository include:

| `PICO_BOARD` value | Description | Board Definition |
| --- | --- | --- |
| `pico` | Raspberry Pi Pico (RP2040) | [pico.h](third_party/pico-sdk/src/boards/include/boards/pico.h) |
| `pico_w` | Raspberry Pi Pico W (RP2040 + Wi-Fi) | [pico_w.h](third_party/pico-sdk/src/boards/include/boards/pico_w.h) |
| `pico2` | Raspberry Pi Pico 2 (RP2350) | [pico2.h](third_party/pico-sdk/src/boards/include/boards/pico2.h) |
| `pico2_w` | Raspberry Pi Pico 2 W (RP2350 + wireless) | [pico2_w.h](third_party/pico-sdk/src/boards/include/boards/pico2_w.h) |
| `pimoroni_pico_plus2_rp2350` | Pimoroni Pico Plus 2 (RP2350) | [pimoroni_pico_plus2_rp2350.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_pico_plus2_rp2350.h) |
| `pimoroni_pico_plus2_w_rp2350` | Pimoroni Pico Plus 2 W (RP2350) | [pimoroni_pico_plus2_w_rp2350.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_pico_plus2_w_rp2350.h) |
| `pimoroni_picolipo_16mb` | Pimoroni Pico LiPo with 16MB flash | [pimoroni_picolipo_16mb.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_picolipo_16mb.h) |
| `pimoroni_picolipo_4mb` | Pimoroni Pico LiPo with 4MB flash | [pimoroni_picolipo_4mb.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_picolipo_4mb.h) |
| `pimoroni_tiny2040` | Pimoroni Tiny 2040 | [pimoroni_tiny2040.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_tiny2040.h) |
| `pimoroni_tiny2040_2mb` | Pimoroni Tiny 2040 (2MB flash variant) | [pimoroni_tiny2040_2mb.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_tiny2040_2mb.h) |
| `pimoroni_tiny2350` | Pimoroni Tiny 2350 | [pimoroni_tiny2350.h](third_party/pico-sdk/src/boards/include/boards/pimoroni_tiny2350.h) |
| `pimoroni_pico_lipo2xl_w` | Pimoroni Pico LiPo 2XL W (custom board) | [pimoroni_pico_lipo2xl_w.h](include/boards/pimoroni_pico_lipo2xl_w.h) |
| `presto` | Pimoroni Presto board (custom board) | [presto.h](include/boards/presto.h) |

Other boards can also be used by providing a compatible board definition
header in the include path (for example in `include/boards/`) and selecting
it via `PICO_BOARD=<board_name>`.
