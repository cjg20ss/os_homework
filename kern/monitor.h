#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);

int mon_backtrace(int argc, char **argv, struct Trapframe *tf); // [lab1-exercise11,12]

int mon_rainbow(int argc, char **argv, struct Trapframe *tf);   // [lab1-challenge]
int mon_setfgc(int argc, char **argv, struct Trapframe *tf);    // [lab1-challenge]
int mon_resetfgc(int argc, char **argv, struct Trapframe *tf);  // [lab1-challenge]
int mon_setbgc(int argc, char **argv, struct Trapframe *tf);    // [lab1-challenge]
int mon_resetbgc(int argc, char **argv, struct Trapframe *tf);  // [lab1-challenge]
int mon_seecolor(int argc, char **argv, struct Trapframe *tf);  // [lab1-challenge]

#endif	// !JOS_KERN_MONITOR_H
