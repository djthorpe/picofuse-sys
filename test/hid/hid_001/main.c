#include <picofuse/hid.h>
#include <test.h>

bool test_main(void) {
  hid_t *instances[HID_CAPACITY];
  hid_device_t *user_button = NULL;
  sys_event_queue_t *queue;
  uint32_t i;

  TestAssert(hid_init(NULL) == NULL,
             "hid_init should return NULL for a NULL queue");

  queue = sys_event_queue_init(4);
  TestAssert(queue != NULL, "sys_event_queue_init should return a queue");
  TestAssert(sys_event_queue_valid(queue),
             "sys_event_queue_valid should accept initialized queue");

  for (i = 0u; i < HID_CAPACITY; ++i) {
    instances[i] = hid_init(queue);
    TestAssert(instances[i] != NULL, "hid_init should allocate pool slot %u",
               i);
    TestAssert(!hid_poll(instances[i]),
               "stub hid_poll should return false for slot %u", i);
  }

  TestAssert(hid_init(queue) == NULL,
             "hid_init should return NULL when pool is exhausted");

  if (HID_CAPACITY > 0u) {
    hid_deinit(instances[0]);
    instances[0] = hid_init(queue);
    TestAssert(instances[0] != NULL,
               "hid_init should reuse a released pool slot");

    user_button = hid_register_user_button(instances[0], KEYCODE_ENTER);
    if (user_button != NULL) {
      TestAssert(hid_deregister(instances[0], user_button),
                 "hid_deregister should remove user button registration");
    }
  }

  for (i = 0u; i < HID_CAPACITY; ++i) {
    hid_deinit(instances[i]);
  }
  hid_deinit(NULL);

  sys_event_queue_deinit(queue);
  return true;
}

TestMain(test_main)
