#include <test.h>

#ifndef SYSTEM_NAME_PICO
#include <signal.h>
static volatile sys_env_signal_t _received_signal = SYS_ENV_SIGNAL_NONE;

static void signal_handler(sys_env_signal_t sig) { _received_signal = sig; }
#endif

bool test_main(void) {
  const char *serial = sys_env_serial();
  TestAssert(serial != NULL, "sys_env_serial should not return NULL");
  TestAssert(serial[0] != '\0', "sys_env_serial should not return empty string");

  const char *name = sys_env_name();
  TestAssert(name != NULL, "sys_env_name should not return NULL");
  TestAssert(name[0] != '\0', "sys_env_name should not return empty string");

  const char *version = sys_env_version();
  TestAssert(version != NULL, "sys_env_version should not return NULL");

#ifdef SYSTEM_NAME_PICO
  TestAssert(!sys_env_signalhandler(0, NULL),
             "Pico sys_env_signalhandler should return false");
#else
  TestAssert(sys_env_signalhandler(0, signal_handler),
             "sys_env_signalhandler should succeed");

  _received_signal = SYS_ENV_SIGNAL_NONE;
  raise(SIGTERM);
  TestAssert(_received_signal == SYS_ENV_SIGNAL_TERM,
             "signal handler should receive SIGTERM");

  _received_signal = SYS_ENV_SIGNAL_NONE;
  raise(SIGINT);
  TestAssert(_received_signal == SYS_ENV_SIGNAL_INT,
             "signal handler should receive SIGINT");

  _received_signal = SYS_ENV_SIGNAL_NONE;
  raise(SIGQUIT);
  TestAssert(_received_signal == SYS_ENV_SIGNAL_QUIT,
             "signal handler should receive SIGQUIT");

  TestAssert(sys_env_signalhandler(0, NULL),
             "sys_env_signalhandler should reset handlers");
#endif

  return true;
}

TestMain(test_main)
