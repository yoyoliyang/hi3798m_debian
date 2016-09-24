#ifndef _LINUX_HIBERNATE_H
#define _LINUX_HIBERNATE_H

#define HIBERNATE_STATE_NORMAL       0
#define HIBERNATE_STATE_SUSPEND      1
#define HIBERNATE_STATE_RESUME       2

#define HIBERNATE_SHRINK_NONE        0
#define HIBERNATE_SHRINK_ALL         1
#define HIBERNATE_SHRINK_LIMIT1      2
#define HIBERNATE_SHRINK_LIMIT2      3

#ifndef __ASSEMBLY__

extern int pm_device_down;
extern int hibernate_shrink;
extern int hibernate_swapout_disable;
extern int hibernate_separate_pass;
extern int hibernate_canceled;

int hibernate_set_savearea(u64 start, u64 end);
void hibernate_save_cancel(void);

#ifdef CONFIG_PM_HIBERNATE_DEBUG

int hibernate_printf(const char *fmt, ...);
void hibernate_putc(char c);

#else

#define hibernate_printf(fmt...)
#define hibernate_putc(c)

#endif

#endif  /* __ASSEMBLY__ */

#endif  /* _LINUX_HIBERNATE_H */
