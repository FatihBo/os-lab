#include <common.h>

static void os_init() {
  pmm->init();
}

#if defined TESTpmm
  static void os_run() {
    printf("test the pmm\n");
    test_pmm();
  }
#else
  static void os_run() {

    printf("Hello World from CPU #%d\n", cpu_current());
    while (1) {;}
  }
#endif

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
