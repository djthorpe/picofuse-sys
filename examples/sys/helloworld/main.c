#include <picofuse/sys.h>

int main(void) {
  sys_init();
  sys_printf("hello, world from picofuse\n");
  sys_exit();
  return 0;
}