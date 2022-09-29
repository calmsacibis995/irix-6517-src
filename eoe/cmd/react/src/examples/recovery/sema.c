/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "main.h"

/*
 * Static global shared arena
 */

static usptr_t* sema_shared_arena;

/*
 * Arena file
 */

static char* arena_file = "/usr/tmp/semaXXXXXX";

void
sema_init()
{
	if (usconfig(CONF_INITUSERS, MAXPROCS) == -1) {
		perror("usconfig");
		exit(1);
	}
	sema_shared_arena = usinit(mktemp(arena_file));
	if (sema_shared_arena == NULL) {
		perror("usinit");
		exit(1);
	}
}

void
sema_destroy()
{
        unlink(arena_file);
}

usema_t*
sema_create()
{
	usema_t* sema;

	sema = usnewsema(sema_shared_arena, 0);
	if (sema == NULL) {
		perror("usnewsema");
		exit(1);
	}

	return (sema);
}

void
sema_p(usema_t* sema)
{
	uspsema(sema);
}

void
sema_v(usema_t* sema)
{
	usvsema(sema);
}

barrier_t*
barrier_create()
{
        barrier_t* barrier;

        barrier = new_barrier(sema_shared_arena);
        if (barrier == NULL) {
                perror("new_barrier");
                exit(1);
        }

        return (barrier);
}

	
