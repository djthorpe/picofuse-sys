#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <picofuse/sys.h>
#include <stdlib.h>

const char *sys_env_serial(void) {
  static char serial[64] = {0};
  if (serial[0] != '\0') {
    return serial;
  }

  io_service_t expert = IOServiceGetMatchingService(
      kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
  if (expert) {
    CFStringRef ref = (CFStringRef)IORegistryEntryCreateCFProperty(
        expert, CFSTR("IOPlatformSerialNumber"), kCFAllocatorDefault, 0);
    IOObjectRelease(expert);
    if (ref) {
      CFStringGetCString(ref, serial, sizeof(serial), kCFStringEncodingUTF8);
      CFRelease(ref);
    }
  }

  return (serial[0] != '\0') ? serial : "unknown";
}

const char *sys_env_name(void) {
  const char *name = getprogname();
  return (name && *name) ? name : "unknown";
}

const char *sys_env_system(void) { return "darwin"; }

const char *sys_env_version(void) {
#ifdef PROGRAM_VERSION
  return PROGRAM_VERSION;
#else
  return "unknown";
#endif
}
