#include <picofuse/net.h>
#include <stddef.h>

// Weak fallbacks used when picofuse-net is not linked into the final
// binary, so picofuse-app can call net_ntp_init()/net_ntp_deinit() without
// forcing a hard dependency on the net module. The real implementation
// (src/net/<platform>/ntp.c) defines strong symbols of the same names,
// which the linker prefers over these whenever it is present in the same
// link.

__attribute__((weak)) net_ntp_t *net_ntp_init(net_ntp_callback_t callback,
                                              void *user_data) {
  (void)callback;
  (void)user_data;
  return NULL;
}

__attribute__((weak)) void net_ntp_deinit(net_ntp_t *ntp) { (void)ntp; }
