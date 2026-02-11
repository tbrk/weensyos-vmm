#include "kernel.h"
#include "lib.h"

// kernel.c
//
//    This is the kernel.

// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000 // initial state only

static proc processes[NPROC]; // array of process descriptors
                              // Note that `processes[0]` is never used.
proc *current;                // pointer to currently executing proc

#define HZ 100         // timer interrupt frequency (interrupts/sec)
static unsigned ticks; // # timer interrupts so far

void schedule(void);
void run(proc *p) __attribute__((noreturn));

// kernel(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, int program_number);

void kernel(const char *command) {
  hardware_init();
  log_printf("Starting WeensyOS\n");

  console_clear();
  timer_init(HZ);

  // nullptr is inaccessible even to the kernel
  virtual_memory_map(kernel_pagetable, (uintptr_t)0, (uintptr_t)0, PAGESIZE,
                     PTE_P, NULL); // | PTE_W | PTE_U

  // Set up process descriptors
  memset(processes, 0, sizeof(processes));
  for (pid_t i = 0; i < NPROC; i++) {
    processes[i].p_pid = i;
    processes[i].p_state = P_FREE;
  }

  if (command && strcmp(command, "fork") == 0) {
    process_setup(1, 4);
  } else if (command && strcmp(command, "forkexit") == 0) {
    process_setup(1, 5);
  } else {
    for (pid_t i = 1; i <= 4; ++i) {
      process_setup(i, i - 1);
    }
  }

  // Switch to the first process using run()
  run(&processes[1]);
}

// process_setup(pid, program_number)
//    Load application program `program_number` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.

void process_setup(pid_t pid, int program_number) {
  process_init(&processes[pid], 0);
  processes[pid].p_pagetable = kernel_pagetable;
  int r = program_load(&processes[pid], program_number, NULL);
  assert(r >= 0);
  processes[pid].p_registers.reg_rsp = PROC_START_ADDR + PROC_SIZE * pid;
  uintptr_t stack_page = processes[pid].p_registers.reg_rsp - PAGESIZE;
  memset((void *)PAGEADDRESS(PAGENUMBER(stack_page)), 0, PAGESIZE);
  virtual_memory_map(processes[pid].p_pagetable, stack_page, stack_page,
                     PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);
  processes[pid].p_state = P_RUNNABLE;
}

// exception(reg)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `reg`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled whenever the kernel is running.

void exception(x86_64_registers *reg) {
  // Copy the saved registers into the `current` process descriptor
  // and always use the kernel's page table.
  current->p_registers = *reg;
  set_pagetable(kernel_pagetable);

  // It can be useful to log events using `log_printf`.
  // Events logged this way are stored in the host's `log.txt` file.
  /*log_printf("proc %d: exception %d\n", current->p_pid, reg->reg_intno);*/

  // Show the current cursor location and memory state
  // (unless this is a kernel fault).
  console_show_cursor(cursorpos);
  if (reg->reg_intno != INT_PAGEFAULT || (reg->reg_err & PFERR_USER)) {
    // TODO
  }

  // If Control-C was typed, exit the virtual machine.
  check_keyboard();

  // Actually handle the exception.
  switch (reg->reg_intno) {

  case INT_SYS_PANIC:
    panic(NULL);
    break; // will not be reached

  case INT_SYS_GETPID:
    current->p_registers.reg_rax = current->p_pid;
    break;

  case INT_SYS_YIELD:
    schedule();
    break; /* will not be reached */

  case INT_SYS_PAGE_ALLOC:
    panic("INT_SYS_PAGE_ALLOC not implemented"); // next TP!
    break;

  case INT_TIMER:
    ++ticks;
    schedule();
    break; /* will not be reached */

  case INT_PAGEFAULT: {
    // Analyze faulting address and access type.
    uintptr_t addr = rcr2();
    const char *operation = reg->reg_err & PFERR_WRITE ? "write" : "read";
    const char *problem =
        reg->reg_err & PFERR_PRESENT ? "protection problem" : "missing page";

    if (!(reg->reg_err & PFERR_USER)) {
      panic("Kernel page fault for %p (%s %s, rip=%p)!\n", addr, operation,
            problem, reg->reg_rip);
    }
    console_printf(CPOS(24, 0), 0x0C00,
                   "Process %d page fault for %p (%s %s, rip=%p)!\n",
                   current->p_pid, addr, operation, problem, reg->reg_rip);
    current->p_state = P_BROKEN;
    break;
  }

  default:
    panic("Unexpected exception %d!\n", reg->reg_intno);
    break; /* will not be reached */
  }

  // Return to the current process (or run something else).
  if (current->p_state == P_RUNNABLE) {
    run(current);
  } else {
    schedule();
  }
}

// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule(void) {
  pid_t pid = current->p_pid;
  while (1) {
    pid = (pid + 1) % NPROC;
    if (processes[pid].p_state == P_RUNNABLE) {
      run(&processes[pid]);
    }
    // If Control-C was typed, exit the virtual machine.
    check_keyboard();
  }
}

// run(p)
//    Run process `p`. This means reloading all the registers from
//    `p->p_registers` using the `popal`, `popl`, and `iret` instructions.
//
//    As a side effect, sets `current = p`.

void run(proc *p) {
  assert(p->p_state == P_RUNNABLE);
  current = p;

  // Load the process's current pagetable.
  set_pagetable(p->p_pagetable);

  // This function is defined in k-exception.S. It restores the process's
  // registers then jumps back to user mode.
  exception_return(&p->p_registers);

spinloop:
  goto spinloop; // should never get here
}
