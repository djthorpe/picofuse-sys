#include <test.h>

bool test_main(void) {
  sys_mutex_t *mutex = sys_mutex_init();
  TestAssert(mutex != NULL, "sys_mutex_init returned NULL");

  TestAssert(sys_mutex_trylock(mutex),
             "sys_mutex_trylock should succeed on a fresh mutex");
  TestAssert(!sys_mutex_trylock(mutex),
             "sys_mutex_trylock should fail when the mutex is already locked");
  TestAssert(sys_mutex_unlock(mutex),
             "sys_mutex_unlock should succeed after trylock");

  TestAssert(sys_mutex_lock(mutex),
             "sys_mutex_lock should succeed on an unlocked mutex");
  TestAssert(sys_mutex_unlock(mutex),
             "sys_mutex_unlock should succeed after lock");

  sys_mutex_deinit(mutex);

  mutex = sys_mutex_init();
  TestAssert(mutex != NULL,
             "sys_mutex_init should reuse a deinitialized mutex slot");
  TestAssert(sys_mutex_lock(mutex),
             "sys_mutex_lock should succeed on a reused mutex");
  TestAssert(sys_mutex_unlock(mutex),
             "sys_mutex_unlock should succeed on a reused mutex");
  sys_mutex_deinit(mutex);

  return true;
}

TestMain(test_main)