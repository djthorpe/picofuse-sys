#include <stdint.h>
#include <test.h>

bool test_main(void) {
  sys_mem_debug_reset();

  uint8_t *bytes = sys_malloc(8);
  TestAssert(bytes != NULL, "sys_malloc returned NULL");

  for (size_t index = 0; index < 8; index++) {
    bytes[index] = (uint8_t)(index + 1);
  }

  uint32_t *words = sys_calloc(4, sizeof(uint32_t));
  TestAssert(words != NULL, "sys_calloc returned NULL");
  for (size_t index = 0; index < 4; index++) {
    TestAssert(words[index] == 0, "sys_calloc should zero-initialize word %zu",
               index);
  }

  uint8_t *resized = sys_realloc(bytes, 16);
  TestAssert(resized != NULL, "sys_realloc grow returned NULL");
  for (size_t index = 0; index < 8; index++) {
    TestAssert(resized[index] == (uint8_t)(index + 1),
               "sys_realloc should preserve byte %zu", index);
  }

  uint8_t *allocated_via_realloc = sys_realloc(NULL, 4);
  TestAssert(allocated_via_realloc != NULL,
             "sys_realloc(NULL, size) should allocate memory");

  void *overflow = sys_calloc((size_t)-1, 2);
  TestAssert(overflow == NULL,
             "sys_calloc should reject count * size overflow");

  sys_free(allocated_via_realloc);
  sys_free(resized);
  sys_free(words);
  sys_free(NULL);

  sys_mem_stats_t stats = {0};
  sys_mem_debug_stats(&stats);
  TestAssert(stats.malloc_calls == 1, "expected 1 malloc call, got %zu",
             stats.malloc_calls);
  TestAssert(stats.calloc_calls == 2, "expected 2 calloc calls, got %zu",
             stats.calloc_calls);
  TestAssert(stats.realloc_calls == 2, "expected 2 realloc calls, got %zu",
             stats.realloc_calls);
  TestAssert(stats.free_calls == 4, "expected 4 free calls, got %zu",
             stats.free_calls);
  TestAssert(stats.failed_allocations == 1,
             "expected 1 failed allocation, got %zu", stats.failed_allocations);
  TestAssert(stats.requested_bytes == 44,
             "expected 44 requested bytes, got %zu", stats.requested_bytes);

  sys_mem_event_t events[SYS_MEM_EVENT_CAPACITY] = {{0}};
  size_t event_count = sys_mem_debug_events(events, SYS_MEM_EVENT_CAPACITY);
  TestAssert(event_count == 9, "expected 9 allocator events, got %zu",
             event_count);

  TestAssert(events[0].type == sys_mem_event_malloc,
             "first event should be malloc");
  TestAssert(events[1].type == sys_mem_event_calloc,
             "second event should be calloc");
  TestAssert(events[2].type == sys_mem_event_realloc,
             "third event should be realloc");
  TestAssert(events[3].type == sys_mem_event_realloc,
             "fourth event should be realloc");
  TestAssert(events[4].type == sys_mem_event_calloc,
             "fifth event should be failed calloc");
  TestAssert(events[4].new_ptr == (uintptr_t)NULL,
             "failed calloc should record NULL result");
  TestAssert(events[5].type == sys_mem_event_free,
             "sixth event should be free");
  TestAssert(events[6].type == sys_mem_event_free,
             "seventh event should be free");
  TestAssert(events[7].type == sys_mem_event_free,
             "eighth event should be free");
  TestAssert(events[8].type == sys_mem_event_free,
             "ninth event should be free");

  return true;
}

TestMain(test_main)