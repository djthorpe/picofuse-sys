#include <picofuse/net.h>
#include <picofuse/sys.h>

///////////////////////////////////////////////////////////////////////////////
// TYPES

struct net_ntp_t {};

///////////////////////////////////////////////////////////////////////////////
// LIFECYCLE

net_ntp_t *net_ntp_init(net_ntp_callback_t callback, void *user_data) {
  sys_debugf("net",
      "ntp_init: callback=%p userdata=%p unsupported on this platform",
      (void *)callback, user_data);
  (void)callback;
  (void)user_data;
  return NULL; // No-op stub implementation for unsupported platforms.
}

void net_ntp_deinit(net_ntp_t *ntp) {
  if (ntp == NULL) {
    return;
  }
  sys_debugf("net", "ntp_deinit: ntp=%p unsupported on this platform", ntp);
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

bool net_ntp_sync(net_ntp_t *ntp, const char *hostname, uint16_t port) {
  (void)ntp;
  (void)hostname;
  (void)port;
  return false; // No-op stub implementation for unsupported platforms.
}

void net_ntp_set_tzoffset(net_ntp_t *ntp, int32_t tzoffset) {
  (void)ntp;
  (void)tzoffset;
}
