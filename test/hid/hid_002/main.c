#include <picofuse/hid.h>
#include <picofuse/sys.h>
#include <test.h>

#include <string.h>

typedef struct {
  uint32_t init_count;
  uint32_t read_count;
  uint32_t deinit_count;
  bool read_result;
} fake_device_ctx_t;

static bool fake_init(void *userdata) {
  fake_device_ctx_t *ctx = (fake_device_ctx_t *)userdata;
  if (ctx == NULL) {
    return false;
  }
  ctx->init_count += 1u;
  return true;
}

static bool fake_read(hid_device_t *device, void *userdata) {
  fake_device_ctx_t *ctx = (fake_device_ctx_t *)userdata;
  (void)device;
  if (ctx == NULL) {
    return false;
  }
  ctx->read_count += 1u;
  return ctx->read_result;
}

static bool fake_deinit(void *userdata) {
  fake_device_ctx_t *ctx = (fake_device_ctx_t *)userdata;
  if (ctx == NULL) {
    return false;
  }
  ctx->deinit_count += 1u;
  return true;
}

static void wait_for_next_timestamp_tick(void) {
  uint64_t start_ms = sys_timestamp_ms();
  uint32_t spin = 0u;

  while (sys_timestamp_ms() == start_ms && spin < 1000000u) {
    spin += 1u;
  }
}

bool test_main(void) {
  const char *name = NULL;
  uint32_t id = 0u;
  hid_type_t type = hid_type_none;
  sys_event_queue_t *queue = sys_event_queue_init(8u);
  hid_t *instance;
  hid_device_t *device_a;
  hid_device_t *device_b;
  hid_device_callbacks_t callbacks;
  fake_device_ctx_t ctx_a = {0};
  fake_device_ctx_t ctx_b = {0};

  TestAssert(queue != NULL, "queue init should succeed");

  instance = hid_init(queue);
  TestAssert(instance != NULL, "hid_init should succeed");

  callbacks.init = fake_init;
  callbacks.read = fake_read;
  callbacks.deinit = fake_deinit;

  device_a = hid_register(instance, "fake-device-a", 0xABCDu, hid_type_other,
                          0u, &ctx_a, callbacks);
  TestAssert(device_a != NULL, "hid_register should return first device");
  TestAssert(ctx_a.init_count == 1u, "init callback should run for device A");

  device_b = hid_register(instance, "fake-device-b", 0xBCDEu, hid_type_other,
                          0u, &ctx_b, callbacks);
  TestAssert(device_b != NULL, "hid_register should return second device");
  TestAssert(ctx_b.init_count == 1u, "init callback should run for device B");

  TestAssert(hid_device_next(NULL) == device_a,
             "hid_device_next(NULL) should return first device");
  TestAssert(hid_device_next(device_a) == device_b,
             "hid_device_next(device_a) should return second device");
  TestAssert(hid_device_next(device_b) == NULL,
             "hid_device_next(device_b) should reach end of list");

  TestAssert(hid_device_info(device_a, &name, &id, &type),
             "hid_device_info should succeed for device A");
  TestAssert(name != NULL && strcmp(name, "fake-device-a") == 0,
             "hid_device_info should return correct name for device A");
  TestAssert(id == 0xABCDu, "hid_device_info should return correct id for A");
  TestAssert(type == hid_type_other,
             "hid_device_info should return correct type for A");

  TestAssert(hid_device_info(device_b, &name, &id, &type),
             "hid_device_info should succeed for device B");
  TestAssert(name != NULL && strcmp(name, "fake-device-b") == 0,
             "hid_device_info should return correct name for device B");
  TestAssert(id == 0xBCDEu, "hid_device_info should return correct id for B");
  TestAssert(type == hid_type_other,
             "hid_device_info should return correct type for B");

  ctx_a.read_result = true;
  ctx_b.read_result = false;
  TestAssert(hid_poll(instance),
             "hid_poll should return true when any device read is true");
  TestAssert(ctx_a.read_count == 1u,
             "read callback should run on poll for device A");
  TestAssert(ctx_b.read_count == 1u,
             "read callback should run on poll for device B");

  wait_for_next_timestamp_tick();

  ctx_a.read_result = false;
  ctx_b.read_result = false;
  TestAssert(!hid_poll(instance),
             "hid_poll should return false when all reads are false");
  TestAssert(ctx_a.read_count == 2u,
             "read callback should run again on second poll for device A");
  TestAssert(ctx_b.read_count == 2u,
             "read callback should run again on second poll for device B");

  TestAssert(hid_deregister(instance, device_a),
             "hid_deregister should succeed for device A");
  TestAssert(ctx_a.deinit_count == 1u,
             "deinit callback should run once for device A");
  TestAssert(!hid_device_info(device_a, NULL, NULL, NULL),
             "hid_device_info should fail after deregister of device A");

  TestAssert(hid_deregister(instance, device_b),
             "hid_deregister should succeed for device B");
  TestAssert(ctx_b.deinit_count == 1u,
             "deinit callback should run once for device B");
  TestAssert(!hid_device_info(device_b, NULL, NULL, NULL),
             "hid_device_info should fail after deregister of device B");
  TestAssert(hid_device_next(NULL) == NULL,
             "hid_device_next(NULL) should return NULL after all deregister");

  hid_deinit(instance);
  sys_event_queue_deinit(queue);
  return true;
}

TestMain(test_main)
