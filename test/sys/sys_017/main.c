#include <test.h>

static size_t arena_malloc_calls = 0;
static size_t arena_free_calls = 0;

static void *test_arena_malloc(size_t size) {
  arena_malloc_calls++;
  return sys_malloc(size);
}

static void test_arena_free(void *ptr) {
  arena_free_calls++;
  sys_free(ptr);
}

bool test_main(void) {
  sys_mem_arena_stats_t stats = {0};

  sys_mem_arena_t *root =
      sys_mem_arena_init(96, NULL, test_arena_malloc, test_arena_free);
  TestAssert(root != NULL, "root arena init returned NULL");
  TestAssert(arena_malloc_calls == 1,
             "root arena init should allocate exactly once");

  TestAssert(sys_mem_arena_next(root, &stats) == NULL,
             "single arena chain should not have a successor");
  TestAssert(stats.size_bytes >= 96,
             "root arena stats should report at least requested capacity");
  TestAssert(stats.used_bytes == 0,
             "root arena used_bytes should start at zero");
  TestAssert(stats.allocations == 0,
             "root arena allocations should start at zero");

  sys_mem_arena_t *tail = sys_mem_arena_init(32, root, NULL, NULL);
  TestAssert(tail != NULL, "tail arena init returned NULL");
  TestAssert(arena_malloc_calls == 2,
             "tail arena init should allocate exactly once");

  TestAssert(sys_mem_arena_next(root, &stats) == tail,
             "sys_mem_arena_next should return the linked tail arena");
  TestAssert(stats.size_bytes >= 96,
             "root arena stats should remain tied to the current arena");
  TestAssert(stats.used_bytes == 0,
             "root arena used_bytes should remain zero without allocations");
  TestAssert(stats.allocations == 0,
             "root arena allocations should remain zero without allocations");

  TestAssert(sys_mem_arena_next(tail, &stats) == NULL,
             "tail arena should terminate the chain");
  TestAssert(stats.size_bytes >= 32,
             "tail arena stats should report at least requested capacity");
  TestAssert(stats.used_bytes == 0,
             "tail arena used_bytes should start at zero");
  TestAssert(stats.allocations == 0,
             "tail arena allocations should start at zero");

  sys_mem_arena_delete(tail);
  TestAssert(arena_free_calls == 2,
             "deleting from the tail should free the full chain");

  return true;
}

TestMain(test_main)