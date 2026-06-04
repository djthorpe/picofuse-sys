/**
 * @file
 * @brief USB hotplug scan example.
 */

#include <picofuse/hw.h>
#include <picofuse/sys.h>

/**
 * @brief Print USB attach and detach events.
 */
static void usb_callback(hw_usb_t *usb, hw_usb_event_t event,
                         const hw_usb_device_t *device, void *userdata) {
  (void)usb;
  (void)userdata;

  if (device == NULL) {
    if (event == hw_usb_event_attached) {
      sys_printf("USB enumeration complete\n");
    }
    return;
  }

  const char *action = "unknown";
  if (event == hw_usb_event_attached) {
    action = "attached";
  } else if (event == hw_usb_event_detached) {
    action = "detached";
  } else {
    return;
  }

  sys_printf("USB %s: vid=%04x pid=%04x class=%02x/%02x/%02x\n", action,
             (unsigned int)device->vid, (unsigned int)device->pid,
             (unsigned int)device->device_class,
             (unsigned int)device->device_subclass,
             (unsigned int)device->device_protocol);

  if (event == hw_usb_event_attached &&
      (device->manufacturer[0] != '\0' || device->product[0] != '\0' ||
       device->serial[0] != '\0')) {
    sys_printf("  strings: manufacturer=\"%s\" product=\"%s\" serial=\"%s\"\n",
               device->manufacturer, device->product, device->serial);
  }
}

/**
 * @brief Monitor USB devices and report attach/detach events.
 */
int main(void) {
  sys_init();
  hw_init();

  hw_usb_t *usb = hw_usb_init(usb_callback, NULL);
  if (!hw_usb_valid(usb)) {
    sys_printf("USB init failed\n");
    hw_exit();
    sys_exit();
    return 1;
  }

  sys_printf("USB scan running for 30 seconds...\n");

  uint64_t start_time = sys_timestamp_ms();
  while (sys_timestamp_ms() - start_time < 30 * 1000) {
    hw_poll();
    sys_sleep_ms(100);
  }

  sys_printf("Shutting down\n");
  hw_usb_deinit(usb);
  hw_exit();
  sys_exit();
  return 0;
}
