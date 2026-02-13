#include "lib.h"
#include "process.h"

extern uint8_t end[];

// These global variables go on the data page.
uint8_t *heap_top;
uint8_t *stack_bottom;

void process_main(void) {
  pid_t p = sys_getpid();
  srand(p);

  // The heap starts on the page right after the 'end' symbol,
  // whose address is the first address not allocated to process code
  // or data.
  heap_top = ROUNDUP((uint8_t *)end, PAGESIZE);

  // The bottom of the stack is the first address on the current
  // stack page (this process never needs more than one stack page).
  stack_bottom = ROUNDDOWN((uint8_t *)read_rsp() - 1, PAGESIZE);

  int init = 1;
  int i = 0;
  while (1) {
    console[CPOS(0, 72)] = ('0' + i) | (0x07 << 8);
    if (init) {
      app_printf(p, "%d: buongiorno\n", p);
      init = 0;
    }
    sys_yield();
    i = (i + 1) % 10;
  }
}
