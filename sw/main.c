#include "eselproc_drv.h"

extern void eselproc_solve_loop();

int main(void) {

  while (1) {
    eselproc_solve_loop();
  }
  // <-- never reached
  return 0; // since we are emulating a
            // hardware unit, we do not return.
}