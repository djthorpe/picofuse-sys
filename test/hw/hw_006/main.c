#include <stdint.h>
#include <test.h>

#ifdef SYSTEM_NAME_PICO
static void fill_pattern(uint8_t *bytes, size_t size, uint8_t seed) {
  for (size_t i = 0; i < size; i++) {
    bytes[i] = (uint8_t)((i * 37u + seed) & 0xFFu);
  }
}

static bool assert_all_value(const uint8_t *bytes, size_t size, uint8_t value,
                             const char *label) {
  for (size_t i = 0; i < size; i++) {
    TestAssert(bytes[i] == value, "%s byte %zu expected 0x%02x got 0x%02x",
               label, i, (unsigned int)value, (unsigned int)bytes[i]);
  }

  return true;
}

static bool assert_pattern(const uint8_t *bytes, size_t size, uint8_t seed,
                           const char *label) {
  for (size_t i = 0; i < size; i++) {
    uint8_t expected = (uint8_t)((i * 37u + seed) & 0xFFu);
    TestAssert(bytes[i] == expected, "%s byte %zu expected 0x%02x got 0x%02x",
               label, i, (unsigned int)expected, (unsigned int)bytes[i]);
  }

  return true;
}
#endif

bool test_main(void) {
  hw_block_deinit(NULL);
  TestAssert(!hw_block_valid(NULL), "NULL block should be invalid");
  TestAssert(hw_block_count(NULL) == 0u, "NULL block count should be 0");
  TestAssert(hw_block_size(NULL) == 0u, "NULL block size should be 0");
  TestAssert(!hw_block_read(NULL, 0u, NULL), "NULL block read should fail");
  TestAssert(!hw_block_erase(NULL, 0u), "NULL block erase should fail");
  TestAssert(!hw_block_write(NULL, 0u, NULL), "NULL block write should fail");

  size_t capacity_before = hw_block_flash_get_capacity();
  sys_printf("hw_006: flash block capacity = %u bytes\n",
             (unsigned int)capacity_before);

#ifdef SYSTEM_NAME_PICO
  TestAssert(capacity_before > 0u,
             "Pico flash block capacity should be > 0, got %u",
             (unsigned int)capacity_before);

  TestAssert(hw_block_flash_init(0u) == NULL,
             "flash init with size 0 should fail");

  size_t request_size = capacity_before >= 8192u ? 8192u : capacity_before;
  hw_block_t *block = hw_block_flash_init(request_size);
  TestAssert(block != NULL, "flash block init should succeed");
  TestAssert(hw_block_valid(block), "flash block should be valid after init");

  TestAssert(hw_block_flash_init(request_size) == NULL,
             "second flash block init should fail while one is active");

  size_t block_size = hw_block_size(block);
  size_t block_count = hw_block_count(block);
  TestAssert(block_size > 0u, "block size should be > 0");
  TestAssert(block_count > 0u, "block count should be > 0");

  size_t allocated_size = block_size * block_count;
  TestAssert(allocated_size > 0u, "allocated block region should be > 0 bytes");

  size_t capacity_after_init = hw_block_flash_get_capacity();
  sys_printf("hw_006: block_size=%u count=%u allocated=%u remaining=%u\n",
             (unsigned int)block_size, (unsigned int)block_count,
             (unsigned int)allocated_size, (unsigned int)capacity_after_init);

  TestAssert(capacity_before >= allocated_size,
             "initial capacity should be >= allocated size");
  TestAssert(capacity_after_init == capacity_before - allocated_size,
             "remaining capacity mismatch: expected %u got %u",
             (unsigned int)(capacity_before - allocated_size),
             (unsigned int)capacity_after_init);

  uint8_t *write_buf = (uint8_t *)sys_malloc(block_size);
  uint8_t *read_buf = (uint8_t *)sys_malloc(block_size);
  TestAssert(write_buf != NULL, "write buffer allocation should succeed");
  TestAssert(read_buf != NULL, "read buffer allocation should succeed");

  fill_pattern(write_buf, block_size, 0x5Au);

  TestAssert(!hw_block_read(block, block_count, read_buf),
             "read out-of-range index should fail");
  TestAssert(!hw_block_write(block, block_count, write_buf),
             "write out-of-range index should fail");
  TestAssert(!hw_block_erase(block, block_count),
             "erase out-of-range index should fail");

  TestAssert(hw_block_erase(block, 0u), "erase block 0 should succeed");
  TestAssert(hw_block_read(block, 0u, read_buf),
             "read block 0 after erase should succeed");
  TestAssert(assert_all_value(read_buf, block_size, 0xFFu, "erased pattern"),
             "erased block should read as all 0xFF");

  TestAssert(hw_block_write(block, 0u, write_buf),
             "write block 0 should succeed");
  TestAssert(hw_block_read(block, 0u, read_buf),
             "read block 0 after write should succeed");
  TestAssert(assert_pattern(read_buf, block_size, 0x5Au, "write/read"),
             "written block should round-trip exactly");

  sys_free(read_buf);
  sys_free(write_buf);

  hw_block_deinit(block);
  TestAssert(!hw_block_valid(block), "block should be invalid after deinit");

  size_t capacity_after_deinit = hw_block_flash_get_capacity();
  TestAssert(capacity_after_deinit == capacity_before,
             "capacity should be restored after deinit: expected %u got %u",
             (unsigned int)capacity_before,
             (unsigned int)capacity_after_deinit);
#else
  TestAssert(capacity_before == 0u, "stub flash capacity should be 0, got %u",
             (unsigned int)capacity_before);
  TestAssert(hw_block_flash_init(4096u) == NULL, "stub flash init should fail");
#endif

  return true;
}

TestMain(test_main)
