#define _GNU_SOURCE

#include <picofuse/sys.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS

/**
 * @brief Returns a stable serial-like identifier for the current Linux host.
 */
const char *sys_env_serial(void) {
  static char serial[128];

  if (serial[0] != '\0') {
    return serial;
  }

  FILE *file = fopen("/etc/machine-id", "r");
  if (file != NULL) {
    if (fgets(serial, sizeof(serial), file) != NULL) {
      size_t length = strlen(serial);
      if (length > 0 && serial[length - 1] == '\n') {
        serial[length - 1] = '\0';
      }
    }
    fclose(file);
  }

  if (serial[0] == '\0') {
    memcpy(serial, "unknown", sizeof("unknown"));
  }

  return serial;
}

/**
 * @brief Returns the name of the current environment.
 */
const char *sys_env_name(void) {
  const char *name = program_invocation_short_name;
  return (name && *name) ? name : "unknown";
}

/**
 * @brief Returns the current system identifier.
 */
const char *sys_env_system(void) { return "linux"; }

/**
 * @brief Returns the version of the current environment.
 */
const char *sys_env_version(void) {
#ifdef PROGRAM_VERSION
  return PROGRAM_VERSION;
#else
  return "unknown";
#endif
}
