/* 
 * dlsafe.h 
 *
 * These routines are in an include file instead of a libary because they
 * depend on the internal structure of the doload_environment which may or
 * may not be the same between various machine types. In short, it would
 * have required rewriting more code than I have time to in order to do
 * this right. This solution gets source code sharing if nothing else...
 * Also this way, the safe routines are not exposedas externals in doload.
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident $Revision: 1.1 $

static void doload_punt(e, message)
struct doload_environment *e;
int message;
{
  *(e->statusptr) = message;
  printf ("doload_punt: ERROR %d\n", message);
  longjmp(e->errorJump, 1);
  /*NOTREACHED*/
}

static char *safe_malloc(e, size)
struct doload_environment *e;
long size;
{
    char *result;
    
    result = (char *)malloc((size_t)size);
    if (result == NULL)
	doload_punt(e, DLerr_OutOfMem);
    return result;
}

static char *safe_realloc(e, ptr, size)
struct doload_environment *e;
char *ptr;
long size;
{
    char *result;

    if (ptr == NULL)
	result = (char *)malloc((size_t)size);
    else
	result = (char *)realloc(ptr, (size_t)size);
    if (result == NULL)
	doload_punt(e, DLerr_OutOfMem);
    return result;
}

/* Not static since this is used by testing programs. */
void safe_free(thing)
char *thing;
{
    if (thing != NULL)
	free(thing);
    return ;
}

static safe_read(e, thing, size)
struct doload_environment *e;
char *thing;
long size;
{	
    if ((unsigned)read(e->fd, thing, (int)size) < size)
	doload_punt(e, DLerr_BadRead);
}

#ifdef ORIGINAL
/* Not static since this is used by dofix. */
static void safe_write(e, fd, thing, size)
struct doload_environment *e;
int fd;
char *thing;
long size;
{
    int n;

    n = write(fd, thing, (int)size);
    if (n < size) {
	perror("dofix");
	doload_punt(e, "problems writing .do file");
    }
    return;
}
#endif

#ifdef	NOTYET
static safe_lseek(e, offset, how)
struct doload_environment *e;
long offset;
int how;
{	
    if (lseek(e->fd, offset, how) == -1)
	doload_punt(e, DLerr_BadSeek);
}
#endif
