#ifndef __ASM_ARM_SUSPEND_H
#define __ASM_ARM_SUSPEND_H

extern void cpu_resume(void);
extern int cpu_suspend(unsigned long, int (*)(unsigned long));

static inline int arch_prepare_suspend (void) { return 0; }
static inline void save_processor_state (void) {}
static inline void restore_processor_state (void) {}

#endif
