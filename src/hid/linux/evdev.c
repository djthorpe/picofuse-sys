#include "../any/private.h"
#include <picofuse/sys.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS

static bool _hid_evdev_read(hid_device_t *device, void *userdata);
static bool _hid_evdev_deinit(hid_device_t *device, void *userdata);
static char *_hid_evdev_dup_name(const char *src);
static bool _hid_evdev_is_event_node(const char *name);

///////////////////////////////////////////////////////////////////////////////
// GLOBALS

static const char *_hid_evdev_name = "evdev";

static const hid_device_callbacks_t _hid_evdev_callbacks = {
    .init = NULL,
    .read = _hid_evdev_read,
    .deinit = _hid_evdev_deinit,
};

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS

/**
 * @brief Drain pending evdev input events and queue keycode HID events.
 */
static bool _hid_evdev_read(hid_device_t *device, void *userdata) {
  (void)userdata;

  if (device == NULL || device->fd < 0) {
    return false;
  }

  bool processed = false;
  struct input_event ev;
  ssize_t n;

  while ((n = read(device->fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
    if (ev.type != EV_KEY) {
      continue;
    }

    // The kernel's KEY_*/BTN_* codes already match picofuse's KEYCODE_*
    // values, so no translation table is needed here.
    hid_state_t state = ev.value != 0 ? hid_state_on : hid_state_off;
    if (hid_event_queue_keycode(device, state, (uint16_t)ev.code)) {
      processed = true;
    }
  }

  return processed;
}

/**
 * @brief Close the backing file descriptor and free the owned name string
 * for an evdev-backed HID device.
 *
 * Closing releases any EVIOCGRAB exclusive grab automatically, so it does
 * not need to be tracked and released separately. @ref device->name is
 * always a heap copy for evdev devices (see hid_register_evdev()), so it is
 * always freed here rather than only conditionally.
 */
static bool _hid_evdev_deinit(hid_device_t *device, void *userdata) {
  (void)userdata;

  if (device == NULL) {
    return true;
  }

  if (device->fd >= 0) {
    close(device->fd);
    device->fd = -1;
  }

  sys_free((void *)device->name);
  device->name = NULL;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE

/**
 * @brief Heap-copy a NULL-terminated string.
 */
static char *_hid_evdev_dup_name(const char *src) {
  size_t len = sys_strlen(src);
  char *dst = (char *)sys_calloc(1u, len + 1u);
  if (dst == NULL) {
    return NULL;
  }
  sys_memcpy(dst, src, len);
  return dst;
}

// Sized to comfortably cover every code this file tests (the highest is
// BTN_TOUCH at 0x14A), not the full EV_KEY range.
#define _HID_EVDEV_BITS_LEN 96u

static bool _hid_evdev_test_bit(const uint8_t *bits, unsigned int bit) {
  size_t byte = bit / 8u;
  if (byte >= _HID_EVDEV_BITS_LEN) {
    return false;
  }
  return ((bits[byte] >> (bit % 8u)) & 1u) != 0u;
}

/**
 * @brief Best-effort classification of an open evdev device's capabilities.
 *
 * Absolute position axes plus multitouch or BTN_TOUCH indicate a
 * touchscreen; absolute axes without a touch signal indicate a joystick;
 * relative motion plus BTN_LEFT indicates a mouse; and a representative
 * alphabetic key (KEY_A) indicates a keyboard. This is a heuristic, not an
 * authoritative classification (real classifiers, e.g. udev's input_id
 * builtin, use considerably more device-specific knowledge).
 */
static hid_class_t _hid_evdev_classify(int fd) {
  uint8_t ev_bits[_HID_EVDEV_BITS_LEN];
  uint8_t key_bits[_HID_EVDEV_BITS_LEN];
  uint8_t abs_bits[_HID_EVDEV_BITS_LEN];
  uint8_t rel_bits[_HID_EVDEV_BITS_LEN];

  sys_memset(ev_bits, 0, sizeof(ev_bits));
  sys_memset(key_bits, 0, sizeof(key_bits));
  sys_memset(abs_bits, 0, sizeof(abs_bits));
  sys_memset(rel_bits, 0, sizeof(rel_bits));

  (void)ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits);

  bool has_abs = _hid_evdev_test_bit(ev_bits, EV_ABS);
  bool has_rel = _hid_evdev_test_bit(ev_bits, EV_REL);
  bool has_key = _hid_evdev_test_bit(ev_bits, EV_KEY);

  if (has_key) {
    (void)ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
  }

  if (has_abs) {
    (void)ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);

    bool has_mt = _hid_evdev_test_bit(abs_bits, ABS_MT_SLOT) ||
                  _hid_evdev_test_bit(abs_bits, ABS_MT_POSITION_X);
    bool has_xy = _hid_evdev_test_bit(abs_bits, ABS_X) &&
                  _hid_evdev_test_bit(abs_bits, ABS_Y);
    bool has_touch = has_key && _hid_evdev_test_bit(key_bits, BTN_TOUCH);

    if (has_mt || (has_xy && has_touch)) {
      return hid_class_touchscreen;
    }
    if (has_xy) {
      return hid_class_joystick;
    }
  }

  if (has_rel) {
    (void)ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits);

    bool has_xy = _hid_evdev_test_bit(rel_bits, REL_X) &&
                  _hid_evdev_test_bit(rel_bits, REL_Y);
    bool has_left = has_key && _hid_evdev_test_bit(key_bits, BTN_LEFT);

    if (has_xy && has_left) {
      return hid_class_mouse;
    }
  }

  if (has_key && _hid_evdev_test_bit(key_bits, KEY_A)) {
    return hid_class_keyboard;
  }

  return hid_class_unknown;
}

/**
 * @brief Report whether a /dev/input entry name is an evdev device node.
 */
static bool _hid_evdev_is_event_node(const char *name) {
  static const char prefix[] = "event";
  size_t i;
  for (i = 0u; prefix[i] != '\0'; i++) {
    if (name[i] != prefix[i]) {
      return false;
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// METHODS

hid_device_t *hid_register_evdev(hid_t *instance, const char *path,
                                 bool exclusive, void *userdata) {
  if (instance == NULL || path == NULL || path[0] == '\0') {
    sys_debugf("[hid] evdev register failed: invalid arguments");
    return NULL;
  }

  int fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    sys_debugf("[hid] evdev register failed: open %s failed", path);
    return NULL;
  }

  // Grab the device if exclusive access is requested. This prevents other
  // processes from receiving events from the device, but may require root
  // privileges.
  if (exclusive && ioctl(fd, EVIOCGRAB, (void *)1) != 0) {
    sys_debugf("[hid] evdev register failed: EVIOCGRAB %s failed", path);
    close(fd);
    return NULL;
  }

  // Prefer the kernel-reported vendor/product as the device id, since it
  // identifies the device model rather than its (hotplug-order-dependent)
  // /dev/input/eventN path. Fall back to a path hash if unavailable.
  struct input_id dev_id;
  sys_memset(&dev_id, 0, sizeof(dev_id));
  uint32_t id = (ioctl(fd, EVIOCGID, &dev_id) == 0)
                    ? (((uint32_t)dev_id.vendor << 16) | (uint32_t)dev_id.product)
                    : (uint32_t)sys_hash_djb2(path);

  char name_buf[64];
  sys_memset(name_buf, 0, sizeof(name_buf));
  (void)ioctl(fd, EVIOCGNAME(sizeof(name_buf) - 1u), name_buf);

  char *name =
      _hid_evdev_dup_name(name_buf[0] != '\0' ? name_buf : _hid_evdev_name);
  if (name == NULL) {
    close(fd);
    return NULL;
  }

  hid_device_t *device = hid_register(instance, name, id, hid_type_evdev, 0u,
                                      userdata, _hid_evdev_callbacks);
  if (device == NULL) {
    sys_debugf("[hid] evdev register failed: hid_register %s", path);
    sys_free(name);
    close(fd);
    return NULL;
  }

  device->fd = fd;
  sys_debugf("[hid] evdev registered: %s (\"%s\") id=%08X exclusive=%d", path,
             device->name, (unsigned int)id, (int)exclusive);
  return device;
}

size_t hid_evdev_list(hid_device_list_callback_t callback, void *userdata) {
  if (callback == NULL) {
    return 0u;
  }

  DIR *dir = opendir("/dev/input");
  if (dir == NULL) {
    sys_debugf("[hid] evdev list failed: opendir /dev/input failed");
    return 0u;
  }

  size_t count = 0u;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (!_hid_evdev_is_event_node(entry->d_name)) {
      continue;
    }

    char path[320];
    sys_memset(path, 0, sizeof(path));
    sys_sprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      continue;
    }

    hid_class_t hid_class = _hid_evdev_classify(fd);
    close(fd);

    callback(path, hid_type_evdev, hid_class, userdata);
    count++;
  }

  closedir(dir);
  return count;
}
