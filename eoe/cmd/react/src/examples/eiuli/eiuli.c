/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* This program demonstrates use of the External Interrupt source
 * to drive a User Level Interrupt.
 *
 * The program requires the presence of an external interrupt cable looped
 * back between output number 0 and one of the inputs on the machine on
 * which the program is run.
 *
 * See ei(7) for more information about external interrupts.
 * See uli(3) for more information about user level interrupts
 */

#include <sys/ei.h>
#include <sys/uli.h>
#include <sys/lock.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/utsname.h>

/* The external interrupt device file is used to access the EI hardware */
#define EIDEV "/dev/ei"
static int eifd;

/* The user level interrupt id. This is returned by the ULI registration
 * routine and is used thereafter to refer to that instance of ULI
 */
static void *ULIid;

/* Variables which are shared between the main process thread and the ULI
 * thread may have to be declared as volatile in some situations. For
 * example, if this program were modified to wait for an interrupt with
 * an empty while() statement, e.g.
 *
 * while(!intr);
 *
 * the value of intr would be loaded on the first pass and if intr is
 * false, the while loop will continue forever since only the register
 * value, which never changes, is being examined. Declaring the variable
 * intr as volatile causes it to be reloaded from memory on each iteration.
 * In this code however, the volatile declaration is not necessary since
 * the while() loop contains a function call, e.g.
 *
 * while(!intr)
 *     ULI_sleep(ULIid, 0);
 *
 * The function call forces the variable intr to be reloaded from memory
 * since the compiler cannot determine if the function modified the value
 * of intr. Thus the volatile declaration is not necessary in this case.
 * When in doubt, declare your globals as volatile.
 */
static int intr;

/* This is the actual interrupt service routine. It runs 
 * asynchronously with respect to the remainder of this program, possibly
 * simultaneously on an MP machine. This function must obey the ULI mode
 * restrictions, meaning that it may not use floating point or make
 * any system calls. Try doing so and see what happens. Also, this 
 * function should be written to execute as quickly as possible, since it
 * runs at interrupt level with lower priority interrupts masked.
 * The system imposes a 1 second time limit on this function to prevent
 * the cpu from freezing if an infinite loop is inadvertently programmed
 * in. Try inserting an infinite loop to see what happens.
 */
static void
intrfunc(void *arg)
{
    /* Set the global flag indicating to the main thread that an
     * interrupt has occurred, and wake it up
     */
    intr = 1;
    ULI_wakeup(ULIid, 0);
}

/* This function generates a periodic external interrupt from a
 * separate process
 */
static void
signaler(void)
{
    int pid;

    if ((pid = fork()) < 0) {
	perror("fork");
	exit(1);
    }
    
    if (pid == 0) {
	while(1) {
	    if (ioctl(eifd, EIIOCSTROBE, 1) < 0) {
		perror("EIIOCSTROBE");
		exit(1);
	    }
	    sleep(1);
	}
    }
}

/* The main routine sets everything up, then sleeps waiting for the
 * interrupt to wake it up
 */
int
main()
{
    struct utsname name;

    /* open the external interrupt device */
    if ((eifd = open(EIDEV, O_RDONLY)) < 0) {
	perror(EIDEV);
	exit(1);
    }

    /* Set the target cpu to which the external interrupt will be
     * directed. This is the cpu on which the ULI handler function above
     * will be called. Note that this is entirely optional, but if
     * you do set the interrupt cpu, it must be done before the
     * registration call below. Once a ULI is registered, it is illegal
     * to modify the target cpu for the external interrupt.
     *
     * This ioctl is only valid on Challenge/Onyx. On Origin2000/200
     * we can only set the interrupt cpu at kernel build time (see
     * system(4))
     */

    if (uname(&name) < 0) {
	perror("uname");
	exit(1);
    }

    if (!strcmp(name.machine, "IP19") ||
	!strcmp(name.machine, "IP21") ||
	!strcmp(name.machine, "IP25")) {

	if (ioctl(eifd, EIIOCSETINTRCPU, 1) < 0) {
	    perror("EIIOCSETINTRCPU");
	    exit(1);
	}
    }


    /* Lock the process image into memory. Any text or data accessed
     * by the ULI handler function must be pinned into memory since
     * the ULI handler cannot sleep waiting for paging from secondary
     * storage. This must be done before the first time the ULI handler
     * is called. In the case of this program, that means before the
     * first EIIOCSTROBE is done to generate the interrupt, but in
     * general it is a good idea to do this before ULI registration 
     * since with some devices an interrupt may occur at any time
     * once registration is complete
     */
    if (plock(PROCLOCK) < 0) {
	perror("plock");
	exit(1);
    }

    /* Register the external interrupt as a ULI source. */
    ULIid = ULI_register_ei(eifd,	/* the external interrupt device */
			    intrfunc,	/* the handler function pointer */
			    0,		/* the argument to the handler */
			    1,		/* the number of semaphores needed */
			    NULL,	/* the stack to use */
			    0);		/* the stack size to use */
    if (ULIid == 0) {
	perror("register ei");
	exit(1);
    }

    /* enable the external interrupt */
    if (ioctl(eifd, EIIOCENABLE) < 0) {
	perror("EIIOCENABLE");
	exit(1);
    }

    /* start the incoming interrupts */
    signaler();

    /* wait for the incoming interrupts and report them */
    while (1) {
	intr = 0;
	while(!intr) {
	    if (ULI_sleep(ULIid, 0) < 0) {
		perror("ULI_sleep");
		exit(1);
	    }
	}
	printf("sleeper woke up\n");
    }
}
