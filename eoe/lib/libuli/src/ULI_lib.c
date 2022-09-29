#include <sys/types.h>
#include <sys/lock.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#define _BSD_SIGNALS
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/uli.h>
#include <sys/syssgi.h>
#include <sys/ei.h>
#include <errno.h>
#include <sys/PCI/pciba.h>
#include <sys/vme/usrvme.h>

#define STKSZ 1024
struct callback {
    void (*func)(void*);
    void *arg;
    int lock;
    int intrid;
};
#define CALLBACK(vp) ((struct callback*)(vp))

#ifdef DEBUG
int real_intrs, deferred_intrs, spintime, no_point, deferred_intrs;
#endif

#ifdef ULI_TSTAMP
int firstu_ts;
#endif

#define LOCK_INTR_PENDING	(1<<0)
#define LOCK_HELD		(1<<1)

extern int ULIatomic_tao(int*, int); /* atomic test and or */
extern int ULIatomic_tas(int*, int); /* atomic test and set */
extern int ULIatomic_taadd(int*, int); /* atomic test and add */

static void
ULI_callback(struct callback *arg)
{
#ifdef ULI_TSTAMP
    extern int get_timestamp(void);
    firstu_ts = get_timestamp();
#endif
    /* Try to acquire the interrupt lock for this intr. This code runs at
     * interrupt level, so we can't spin waiting for the lock. If we get the
     * lock, we can just process the interrupt and return. Otherwise we do
     * nothing, but we do leave the INTR_PENDING bit set in the lock to
     * indicate that an interrupt occurred while blocked and it should be
     * processed later when the top half unblocks the interrupt.
     */
    if (ULIatomic_tao(&arg->lock, LOCK_INTR_PENDING) == 0) {
	/* we have the lock */
	arg->func(arg->arg);
#ifdef DEBUG
	real_intrs++;
#endif
	arg->lock = 0;
    }
    syssgi(SGI_ULI, ULI_RETURN);
}

void
ULI_block_intr(void *intr)
{
    /* acquire the interrupt lock in the top half. Since this is called
     * from schedulable user code, it's ok to spin
     */
    while(ULIatomic_tao(&CALLBACK(intr)->lock, LOCK_HELD)) {
#ifdef DEBUG
	spintime++;
#endif
    }
}

/* Unblocking the interrupt is not as simple as it may seem. There are 3
 * events each of which must preceed at least one of the other two:
 * checking for a deferred interrupt, processing that deferred interrupt,
 * and unblocking the interrupt. We must check for the deferred interrupt
 * before processing it, we must process it before unblocking the
 * interrupt, and we must unblock the interrupt before checking for a
 * deferred interrupt.  Since this ordering is impossible, we'll do the
 * next best thing which is to just try something that we think will work
 * most of the time, and when it doesn't work loop back and try it again
 * until it does.
 */
void
ULI_unblock_intr(void *intr)
{
    int lock;

    /* unblock the interrupt. Check if there is a deferred interrupt */
    while(ULIatomic_tas(&CALLBACK(intr)->lock, 0) & LOCK_INTR_PENDING) {
	
	/* there is a deferred interrupt, so we must reacquire the lock
	 * in order to process it. The possible contenders here are
	 * the interrupt routine or another thread. Another thread will
	 * have set LOCK_HELD, so we must spin waiting if that is the
	 * case. If the value is 0, we now have the lock and can call
	 * the interrupt routine. If LOCK_INTR_PENDING is set, the interrupt
	 * handler is currently running, which means that it would be
	 * redundant to call the handler again, so we'll just return.
	 */
	while((lock =
	       ULIatomic_tao(&CALLBACK(intr)->lock, LOCK_HELD)) & LOCK_HELD);
	if (lock & LOCK_INTR_PENDING) {
#ifdef DEBUG
	    no_point++;
#endif
	    break;
	}

	/* we have the lock again, process the interrupt */
#ifdef DEBUG
	deferred_intrs++;
#endif
	CALLBACK(intr)->func(CALLBACK(intr)->arg);
    }
}

static int
getstack(char *stack, int stacksize, char **stack_top)
{
    char *newstack;

    /* if caller specified a stack */
    if (stack) {

	/* find and align the top of the stack */
	*stack_top = (char*)(((__psint_t)stack + stacksize) & (~7));

	if (*stack_top <= stack)
	    return(EINVAL);

	return(0);
    }

    /* we are expected to allocate a stack */
    if (stacksize == 0)
	stacksize = 1024;
    if (stacksize < 256)
	return(EINVAL);

    if ((newstack = malloc(stacksize)) == 0)
	return(ENOMEM);

    *stack_top = (char*)(((__psint_t)newstack + stacksize) & (~7));
    return(0);
}

void *
ULI_register_vme(int fd,
		 void (*func)(void*),
		 void *arg,
		 int nsemas,
		 char *stack,
		 int stacksize,
		 int ipl,
		 int *vec)
{
    struct callback *newcallback;
    struct uliargs args;
    int err;

    if ((newcallback = malloc(sizeof(struct callback))) == 0) {
#ifdef DEBUG
	fprintf(stderr, "ULI_register_vme: ");
	perror("malloc");
#endif
	return(0);
    }

    if (err = getstack(stack, stacksize, &args.sp)) {
	free(newcallback);
	errno = err;
	return(0);
    }

    args.pc = (caddr_t)ULI_callback;
    args.funcarg = (void*)newcallback;
    args.dd.vme.ipl = ipl;
    args.dd.vme.vec = *vec;
    args.nsemas = nsemas;
    
    newcallback->func = func;
    newcallback->arg = arg;

    /* don't take an interrupt before the intrid is set */
    newcallback->lock = LOCK_HELD;

    if (ioctl(fd, UVIOCREGISTERULI, &args) < 0) {
#ifdef DEBUG
	perror("UVIOCREGISTERULI");
#endif
	return(0);
    }

    newcallback->intrid = args.id;
    newcallback->lock = 0;
    *vec = args.dd.vme.vec;
    return(newcallback);
}

void *
ULI_register_ei(int fd,
		void (*func)(void*),
		void *arg,
		int nsemas,
		char *stack,
		int stacksize)
{
    struct callback *newcallback;
    struct uliargs args;
    int err;

    if ((newcallback = malloc(sizeof(struct callback))) == 0) {
#ifdef DEBUG
	fprintf(stderr, "ULI_register_ei: ");
	perror("malloc");
#endif
	return(0);
    }

    if (err = getstack(stack, stacksize, &args.sp)) {
	free(newcallback);
	errno = err;
	return(0);
    }

    args.pc = (caddr_t)ULI_callback;
    args.funcarg = (void*)newcallback;
    args.nsemas = nsemas;
    
    newcallback->func = func;
    newcallback->arg = arg;

    /* don't take an interrupt before the intrid is set */
    newcallback->lock = LOCK_HELD;

    if (ioctl(fd, EIIOCREGISTERULI, &args) < 0) {
#ifdef DEBUG
	perror("EIIOCREGISTERULI");
#endif
	return(0);
    }

    newcallback->intrid = args.id;
    newcallback->lock = 0;
    return(newcallback);
}

void *
ULI_register_pci(int fd,
		 void (*func)(void*),
		 void *arg,
		 int nsemas,
		 char *stack,
		 int stacksize,
		 int lines)
{
    struct callback *newcallback;
    struct uliargs args;
    int err;

    if ((newcallback = malloc(sizeof(struct callback))) == 0) {
#ifdef DEBUG
	fprintf(stderr, "ULI_register_pci: ");
	perror("malloc");
#endif
	return(0);
    }

    if (err = getstack(stack, stacksize, &args.sp)) {
	free(newcallback);
	errno = err;
	return(0);
    }

    args.pc = (caddr_t)ULI_callback;
    args.funcarg = (void*)newcallback;
    args.nsemas = nsemas;
    
    newcallback->func = func;
    newcallback->arg = arg;

    /* don't take an interrupt before the intrid is set */
    newcallback->lock = LOCK_HELD;

    if (ioctl(fd, PCIIOCSETULI(lines), &args) < 0) {
#ifdef DEBUG
	perror("register pci");
#endif
	return(0);
    }

    newcallback->intrid = args.id;
    newcallback->lock = 0;
    return(newcallback);
}

#ifdef ULI_TSTAMP
void *
ULI_register_ulits(int fd, void (*func)(void*))
{
    struct callback *newcallback;
    struct uliargs args;
    int err;

    if ((newcallback = malloc(sizeof(struct callback))) == 0) {
	perror("malloc");
	return(0);
    }

    if (err = getstack(0, 0, &args.sp)) {
	free(newcallback);
	errno = err;
	return(0);
    }

    args.pc = (caddr_t)ULI_callback;
    args.funcarg = (void*)newcallback;
    args.nsemas = 0;
    
    newcallback->func = func;

    /* don't take an interrupt before the intrid is set */
    newcallback->lock = LOCK_HELD;

    if (ioctl(fd, 1002, &args) < 0) {
	perror("ioctl 1002");
	return(0);
    }

    newcallback->intrid = args.id;
    newcallback->lock = 0;
    return(newcallback);
}
#endif

int
ULI_sleep(void *intr, int sema)
{
    if (syssgi(SGI_ULI, ULI_SLEEP, CALLBACK(intr)->intrid, sema) < 0) {
#ifdef DEBUG
	perror("ULI_sleep");
#endif
	return(-1);
    }
    return(0);
}

int
ULI_wakeup(void *intr, int sema)
{
    return((int)syssgi(SGI_ULI, ULI_WAKEUP, sema, CALLBACK(intr)->intrid));
}
