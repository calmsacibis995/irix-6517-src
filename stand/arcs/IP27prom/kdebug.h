#ifndef _IP27PROM_KDEBUG_H_
#define _IP27PROM_KDEBUG_H_

#if _LANGUAGE_C

extern void kdebug_syms_on(void);
extern void kdebug_syms_off(void);
extern void kdebug_syms_toggle(void);
extern void kdebug_alt_regs(__uint64_t, struct flag_struct *);
extern void kdebug_init(__uint64_t);
extern void kdebug_setup(struct flag_struct *);
#endif /* _LANGUAGE_C */

#define KERN_STACK_OFF	0x3fff000
#define KERN_STACK_SIZE	0x10000
#endif /* _IP27PROM_KDEBUG_H_ */
