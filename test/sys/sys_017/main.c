#include <test.h>

static void fill_bytes(uint8_t *ptr, size_t size, uint8_t base) {
  for (size_t index = 0; index < size; index++) {
    ptr[index] = (uint8_t)(base + index);
  }
}

static bool check_bytes(const uint8_t *ptr, size_t size, uint8_t base) {
  for (size_t index = 0; index < size; index++) {
    if (ptr[index] != (uint8_t)(base + index)) {
      return false;
    }
  }

  return true;
}

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
  sys_mem_arena_stats_t middle_stats = {0};
  sys_mem_arena_stats_t tail_stats = {0};
  sys_mem_arena_stats_t tail2_stats = {0};
  sys_mem_arena_stats_t realloc_stats = {0};

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

  sys_mem_arena_t *middle = sys_mem_arena_init(32, root, NULL, NULL);
  TestAssert(middle != NULL, "middle arena init returned NULL");
  TestAssert(arena_malloc_calls == 2,
             "middle arena init should allocate exactly once");

  sys_mem_arena_t *tail = sys_mem_arena_init(48, middle, NULL, NULL);
  TestAssert(tail != NULL, "tail arena init returned NULL");
  TestAssert(arena_malloc_calls == 3,
             "tail arena init should allocate exactly once");

  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "sys_mem_arena_next should return the linked middle arena");
  TestAssert(stats.size_bytes >= 96,
             "root arena stats should remain tied to the current arena");
  TestAssert(stats.used_bytes == 0,
             "root arena used_bytes should remain zero without allocations");
  TestAssert(stats.allocations == 0,
             "root arena allocations should remain zero without allocations");
  TestAssert(sys_mem_arena_alloc(root, 0) == NULL,
             "sys_mem_arena_alloc should reject zero-byte requests");
  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "zero-byte allocation should not relink the chain");
  TestAssert(stats.used_bytes == 0,
             "zero-byte allocation should not change used_bytes");
  TestAssert(stats.allocations == 0,
             "zero-byte allocation should not change allocation count");

  uint8_t *bytes = sys_mem_arena_alloc(root, 16);
  TestAssert(bytes != NULL, "sys_mem_arena_alloc returned NULL");
  fill_bytes(bytes, 16, 0x20);

  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "root arena should still link to the middle after allocation");
  TestAssert(stats.used_bytes == 16,
             "root arena used_bytes should track active allocation bytes");
  TestAssert(stats.allocations == 1,
             "root arena allocations should track active allocations");
  TestAssert(sys_mem_arena_next(middle, &middle_stats) == tail,
             "middle arena should link to the tail");
  TestAssert(middle_stats.used_bytes == 0,
             "allocating from root should not affect middle stats");
  TestAssert(middle_stats.allocations == 0,
             "allocating from root should not affect middle allocations");
  TestAssert(sys_mem_arena_next(tail, &tail_stats) == NULL,
             "tail arena should remain the end of the chain");
  TestAssert(tail_stats.used_bytes == 0,
             "allocating from root should not affect tail stats");
  TestAssert(tail_stats.allocations == 0,
             "allocating from root should not affect tail allocations");

  sys_mem_arena_t *tail2 = sys_mem_arena_init(24, tail, NULL, NULL);
  TestAssert(tail2 != NULL, "second tail arena init returned NULL");
  TestAssert(arena_malloc_calls == 4,
             "second tail arena init should allocate exactly once");
  TestAssert(sys_mem_arena_next(tail, &tail_stats) == tail2,
             "tail arena should link to the appended second tail");
  TestAssert(sys_mem_arena_next(tail2, &tail2_stats) == NULL,
             "second tail arena should terminate the chain");

  sys_mem_arena_delete(tail2);
  TestAssert(arena_free_calls == 1,
             "deleting a tail arena in-chain should free only that tail");
  TestAssert(sys_mem_arena_next(tail, &tail_stats) == NULL,
             "deleting a tail arena should relink the previous arena to NULL");
  TestAssert(tail_stats.size_bytes >= 48,
             "deleting a tail arena should leave the previous tail intact");

  uint8_t *grown = sys_mem_arena_realloc(root, bytes, 24);
  TestAssert(grown != NULL, "sys_mem_arena_realloc grow returned NULL");
  TestAssert(
      check_bytes(grown, 16, 0x20),
      "sys_mem_arena_realloc should preserve existing bytes when growing");

  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "root arena should remain linked after realloc");
  TestAssert(stats.used_bytes == 24,
             "root arena used_bytes should grow with realloc");
  TestAssert(stats.allocations == 1,
             "realloc should keep allocation count stable");

  uint8_t *shrunk = sys_mem_arena_realloc(root, grown, 8);
  TestAssert(shrunk != NULL, "sys_mem_arena_realloc shrink returned NULL");
  TestAssert(
      check_bytes(shrunk, 8, 0x20),
      "sys_mem_arena_realloc should preserve prefix bytes when shrinking");

  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "root arena should remain linked after shrinking");
  TestAssert(stats.used_bytes == 8,
             "root arena used_bytes should shrink with realloc");
  TestAssert(stats.allocations == 1,
             "shrinking should keep allocation count stable");

  size_t too_large = stats.size_bytes + 1;
  TestAssert(sys_mem_arena_alloc(root, too_large) == NULL,
             "sys_mem_arena_alloc should fail when the request exceeds arena "
             "capacity");
  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "failed oversized allocation should not relink the chain");
  TestAssert(stats.used_bytes == 8,
             "failed oversized allocation should not change used_bytes");
  TestAssert(stats.allocations == 1,
             "failed oversized allocation should not change allocation count");

  uint8_t *foreign_bytes = sys_malloc(8);
  TestAssert(foreign_bytes != NULL,
             "sys_malloc for foreign pointer returned NULL");
  fill_bytes(foreign_bytes, 8, 0x60);
  sys_mem_arena_free(root, foreign_bytes);
  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "freeing a foreign pointer should not relink the chain");
  TestAssert(stats.used_bytes == 8,
             "freeing a foreign pointer should not change used_bytes");
  TestAssert(stats.allocations == 1,
             "freeing a foreign pointer should not change allocation count");
  TestAssert(
      check_bytes(shrunk, 8, 0x20),
      "freeing a foreign pointer should not affect valid arena allocations");

  TestAssert(sys_mem_arena_realloc(root, foreign_bytes, 12) == NULL,
             "sys_mem_arena_realloc should reject foreign pointers");
  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "reallocating a foreign pointer should not relink the chain");
  TestAssert(stats.used_bytes == 8,
             "reallocating a foreign pointer should not change used_bytes");
  TestAssert(
      stats.allocations == 1,
      "reallocating a foreign pointer should not change allocation count");
  TestAssert(check_bytes(shrunk, 8, 0x20),
             "reallocating a foreign pointer should not affect valid arena "
             "allocations");

  TestAssert(sys_mem_arena_realloc(root, shrunk, too_large) == NULL,
             "sys_mem_arena_realloc should fail when the new size exceeds "
             "arena capacity");
  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "failed oversized realloc should not relink the chain");
  TestAssert(stats.used_bytes == 8,
             "failed oversized realloc should preserve used_bytes");
  TestAssert(stats.allocations == 1,
             "failed oversized realloc should preserve allocation count");
  TestAssert(check_bytes(shrunk, 8, 0x20),
             "failed oversized realloc should preserve the original allocation "
             "contents");

  sys_free(foreign_bytes);

  sys_mem_arena_free(root, shrunk);
  TestAssert(sys_mem_arena_next(root, &stats) == middle,
             "root arena should remain linked after free");
  TestAssert(stats.used_bytes == 0,
             "root arena used_bytes should return to zero after free");
  TestAssert(stats.allocations == 0,
             "root arena allocations should return to zero after free");

  sys_mem_arena_t *realloc_root =
      sys_mem_arena_init(192, NULL, sys_malloc, sys_free);
  TestAssert(realloc_root != NULL,
             "realloc regression arena init should succeed");

  uint8_t *gap_first = sys_mem_arena_alloc(realloc_root, 24);
  uint8_t *gap_second = sys_mem_arena_alloc(realloc_root, 16);
  uint8_t *gap_third = sys_mem_arena_alloc(realloc_root, 8);
  TestAssert(gap_first != NULL && gap_second != NULL && gap_third != NULL,
             "arena realloc regression setup should allocate three blocks");
  fill_bytes(gap_second, 16, 0x70);
  fill_bytes(gap_third, 8, 0x90);

  sys_mem_arena_free(realloc_root, gap_first);
  TestAssert(sys_mem_arena_next(realloc_root, &realloc_stats) == NULL,
             "freeing the first regression block should not relink the "
             "regression arena");
  TestAssert(realloc_stats.allocations == 2,
             "freeing the first regression block should leave two allocations");

  uint8_t *gap_grown = sys_mem_arena_realloc(realloc_root, gap_second, 24);
  TestAssert(gap_grown != NULL,
             "sys_mem_arena_realloc should relocate into an earlier free gap");
  TestAssert(check_bytes(gap_grown, 16, 0x70),
             "relocated realloc should preserve the original bytes");
  TestAssert(check_bytes(gap_third, 8, 0x90),
             "relocated realloc should not corrupt later allocations");
  TestAssert(sys_mem_arena_next(realloc_root, &realloc_stats) == NULL,
             "relocated realloc should not relink the regression arena");
  TestAssert(realloc_stats.allocations == 2,
             "relocated realloc should keep the allocation count stable");

  sys_mem_arena_free(realloc_root, gap_third);
  sys_mem_arena_free(realloc_root, gap_grown);
  TestAssert(
      sys_mem_arena_next(realloc_root, &realloc_stats) == NULL,
      "freeing relocated allocations should not relink the regression arena");
  TestAssert(realloc_stats.used_bytes == 0,
             "freeing relocated allocations should restore used_bytes to zero");
  TestAssert(
      realloc_stats.allocations == 0,
      "freeing relocated allocations should restore allocation count to zero");

  sys_mem_arena_delete(realloc_root);

  sys_mem_arena_delete(middle);
  TestAssert(arena_free_calls == 2,
             "deleting the middle arena should free only that arena");
  TestAssert(sys_mem_arena_next(root, &stats) == tail,
             "deleting the middle arena should relink root to tail");
  TestAssert(sys_mem_arena_next(tail, &tail_stats) == NULL,
             "tail arena should remain the end of the chain after relink");
  TestAssert(tail_stats.size_bytes >= 48,
             "tail arena stats should survive middle deletion");
  TestAssert(tail_stats.used_bytes == 0,
             "middle deletion should not affect tail usage");
  TestAssert(tail_stats.allocations == 0,
             "middle deletion should not affect tail allocations");

  sys_mem_arena_delete(root);
  TestAssert(arena_free_calls == 3,
             "deleting the head arena should free only that arena");

  uint8_t *tail_bytes = sys_mem_arena_alloc(tail, 12);
  TestAssert(tail_bytes != NULL,
             "tail arena should remain usable after head deletion");
  fill_bytes(tail_bytes, 12, 0x40);
  TestAssert(sys_mem_arena_next(tail, &stats) == NULL,
             "tail arena should terminate the chain");
  TestAssert(stats.size_bytes >= 48,
             "tail arena stats should report at least requested capacity");
  TestAssert(stats.used_bytes == 12,
             "tail arena should track allocations after head deletion");
  TestAssert(stats.allocations == 1,
             "tail arena should track allocation count after head deletion");
  TestAssert(check_bytes(tail_bytes, 12, 0x40),
             "tail arena allocation should preserve written data");

  sys_mem_arena_free(tail, tail_bytes);
  sys_mem_arena_delete(tail);
  TestAssert(arena_free_calls == 4,
             "deleting the tail should free only the remaining tail arena");

  return true;
}

TestMain(test_main)
