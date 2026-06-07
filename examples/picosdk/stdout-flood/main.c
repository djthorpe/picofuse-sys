/**
 * @file
 * @brief Pico SDK-only multicore USB CDC stress reproducer.
 */

#include <inttypes.h>
#include <stdio.h>

#include <pico/multicore.h>
#include <pico/stdlib.h>

static volatile uint64_t core1_tx_count;

static void tx_flood_core1(void) {
  uint64_t seq = 0u;

  while (true) {
    printf("tx core=1 seq=%" PRIu64
           " payload=abcdefghijklmnopqrstuvwxyz0123456789\n",
           seq);
    core1_tx_count = ++seq;
  }
}

int main(void) {
  uint64_t core0_rx_count = 0u;

  stdio_init_all();
  sleep_ms(1000);

  setvbuf(stdout, NULL, _IONBF, 0);

  printf("usb cdc stress start\n");
  printf("mode: core0 blocks on getchar, core1 floods tx\n");
  printf("host can run: cat /dev/zero > /dev/ttyACM0\n");

  multicore_launch_core1(tx_flood_core1);

  while (true) {
    int ch = getchar();
    (void)ch;
    core0_rx_count++;

    if ((core0_rx_count & 0x3FFu) == 0u) {
      printf("stats tx1=%" PRIu64 " rx0=%" PRIu64 "\n", core1_tx_count,
             core0_rx_count);
    }
  }
}