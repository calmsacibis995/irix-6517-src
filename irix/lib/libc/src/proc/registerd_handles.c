/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#ifdef __STDC__
        #pragma weak atsproc_child = _atsproc_child
        #pragma weak atsproc_parent = _atsproc_parent
        #pragma weak atsproc_pre = _atsproc_pre
        #pragma weak atfork_child = _atfork_child
        #pragma weak atfork_child_prepend = _atfork_child_prepend
        #pragma weak atfork_parent = _atfork_parent
        #pragma weak atfork_pre = _atfork_pre
#endif

#include "synonyms.h"
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "mplib.h"
#include "libc_interpose.h"
#include "errno.h"

#define MAX_FNS	37
static int	max_fns;
static void	(**sproc_child_fns)(void) = (void (**)(void)) 0 ;
static void	(**sproc_parent_fns)(int, int) = (void (**)(int, int)) 0 ;
static void	(**sproc_pre_fns)(unsigned int) = (void (**)(unsigned int)) 0;
static int	num_sproc_child_fns = 0;
static int	num_sproc_parent_fns = 0;
static int	num_sproc_pre_fns = 0;

static void	(**fork_child_fns)(void) = (void (**)(void)) 0 ;
static void	(**fork_parent_fns)(int, int) = (void (**)(int, int)) 0 ;
static void	(**fork_pre_fns)(void) = (void (**)(void)) 0;
static int	num_fork_child_fns = 0;
static int	num_fork_parent_fns = 0;
static int	num_fork_pre_fns = 0;

#define MAP_FILE "/dev/zero"
/* We can't use malloc because it
   shift users virtual addresses. */

static int
at_init(void)
{
    	int fd;
	void (**at_page)(void);

	max_fns = (int)(getpagesize()/(6*sizeof(void (*)(void))));

	if (!max_fns) 
	    return(-1);

	if ((fd = open(MAP_FILE, O_RDWR)) < 0)
	    return(-1);

	/* get our page of function pointers */
	at_page = (void (**)(void)) mmap(0, getpagesize(),
					 PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	
	if ( (void *)at_page == MAP_FAILED) {
	    close(fd);
	    return(-1);
	}
   
	sproc_child_fns  = (void (**)(void))	(at_page + 0 * max_fns )  ;
	sproc_parent_fns = (void (**)(int, int))(at_page + 1 * max_fns )  ;
	sproc_pre_fns    = (void (**)(unsigned int)) (at_page + 2 * max_fns ) ;
	fork_child_fns   = (void (**)(void))  (at_page + 3 * max_fns ) ;
	fork_parent_fns  = (void (**)(int, int))  (at_page + 4 * max_fns ) ;
	fork_pre_fns     = (void (**)(void))  (at_page + 5 * max_fns );
	close(fd);
	return(0);
}

int
atsproc_child(void (*func)(void))
{
	LOCKDECLINIT(l, LOCKMISC);

	if ( !sproc_child_fns ){

		if ( at_init() < 0 ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}
	
	if ((func == (void(*)(void))0) || num_sproc_child_fns >= max_fns) {
		UNLOCKMISC(l);
		return(-1);
	}

	sproc_child_fns[num_sproc_child_fns++] = func;
	UNLOCKMISC(l);
	return(0);
}

void
__sproc_child_handle(void)
{
	void (**fns)(void) = sproc_child_fns;
	int count = num_sproc_child_fns;

	__tp_sproc_child(); /* Leave until SpeedShop converts */

        while (--count >= 0)
                (*fns++)();
}

int
atsproc_parent(void (*func)(int, int))
{
	LOCKDECLINIT(l, LOCKMISC);

	if ( !sproc_parent_fns ){

		if ( at_init() < 0  ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}	

	if ((func == (void(*)(int ,int))0) || num_sproc_parent_fns >= max_fns) {
		UNLOCKMISC(l);
		return(-1);
	}

	sproc_parent_fns[num_sproc_parent_fns++] = func;
	UNLOCKMISC(l);
	return(0);
}

void
__sproc_parent_handle(int a, int b)
{
	void (**fns)(int, int) = sproc_parent_fns;
	int count = num_sproc_parent_fns;

	__tp_sproc_parent(a, b); /* Leave until SpeedShop converts */

        while (--count >= 0)
                (*fns++)(a, b);
}

int
atsproc_pre(void (*func)(unsigned int))
{
	LOCKDECLINIT(l, LOCKMISC);

	if ( !sproc_pre_fns ){

		if ( at_init() < 0  ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}

	if ((func == (void(*)(unsigned int))0) || num_sproc_pre_fns >= max_fns) {
		UNLOCKMISC(l);
		return(-1);
	}	
	sproc_pre_fns[num_sproc_pre_fns++] = func;
	UNLOCKMISC(l);
	return(0);
}

void
__sproc_pre_handle(unsigned int a)
{
	int count = num_sproc_pre_fns;

        while (--count >= 0)
                (*sproc_pre_fns[count])(a);

	__tp_sproc_pre(a); /* Leave until SpeedShop converts */
}


int
atfork_child(void (*func)(void))
{
	LOCKDECLINIT(l, LOCKMISC);

	if ( !fork_child_fns ){
	
		if ( at_init() < 0  ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}

	if ((func == (void(*)(void))0) || num_fork_child_fns >= max_fns) {
		UNLOCKMISC(l);
		return(-1);
	}	

	fork_child_fns[num_fork_child_fns++] = func;
	UNLOCKMISC(l);
	return(0);
}

int
atfork_child_prepend(void (*func)(void))
{
	int i;
	LOCKDECLINIT(l, LOCKMISC);

	if ( !fork_child_fns ){
	
		if ( at_init() < 0  ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}

	if ((func == (void(*)(void))0) || num_fork_child_fns >= max_fns) {
		UNLOCKMISC(l);
		return(-1);
	}	

	for (i=num_fork_child_fns; i>0; i--)
		fork_child_fns[i] = fork_child_fns[i-1];

	fork_child_fns[0] = func;
	num_fork_child_fns++;

	UNLOCKMISC(l);
	return(0);
}

void
__fork_child_handle(void)
{
	void (**fns)(void) = fork_child_fns;
	int count = num_fork_child_fns;

	__tp_fork_child(); /* Leave until SpeedShop converts */

        while (--count >= 0)
                (*fns++)();
}

int
atfork_parent(void (*func)(int, int))
{
	LOCKDECLINIT(l, LOCKMISC);

	if ( !fork_parent_fns ){

		if ( at_init() < 0  ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}
	
	if ((func == (void(*)(int ,int))0) || num_fork_parent_fns >= max_fns) {
		UNLOCKMISC(l);
		return(-1);
	}

	fork_parent_fns[num_fork_parent_fns++] = func;
	UNLOCKMISC(l);
	return(0);
}

void
__fork_parent_handle(int a, int b)
{
	void (**fns)(int, int) = fork_parent_fns;
	int count = num_fork_parent_fns;

	__tp_fork_parent(a, b); /* Leave until SpeedShop converts */

        while (--count >= 0)
                (*fns++)(a, b);
}

int
atfork_pre(void (*func)(void))
{
	LOCKDECLINIT(l, LOCKMISC);
	
	if ( !fork_pre_fns ){

		if ( at_init() < 0  ) {
			UNLOCKMISC(l);
			setoserror(ENOMEM);
			return(-1);
		}
	}	
	
	if ((func == (void(*)(void))0) || num_fork_pre_fns >= max_fns) {
	    	UNLOCKMISC(l);
		return(-1);
	}

	fork_pre_fns[num_fork_pre_fns++] = func;
	UNLOCKMISC(l);
	return(0);
}

void
__fork_pre_handle(void)
{
	int count = num_fork_pre_fns;

        while (--count >= 0)
                (*fork_pre_fns[count])();

	__tp_fork_pre(); /* Leave until SpeedShop converts */
}
