#include <picofuse/net.h>

// net_poll is a no-op on Pico — hw_poll() drives cyw43_arch_poll() which
// services the lwIP stack and fires MQTT callbacks.
void net_poll(void) {}
