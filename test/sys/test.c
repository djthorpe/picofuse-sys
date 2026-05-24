
#include <picofuse/sys.h>
#include <stdio.h>

int main(void) {
  sys_init();
  printf("Hello, world!\n");
  sys_exit();
  return 0;
}
