/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/cat_init.c	1.4.1.4"

#if defined(__STDC__) && !defined(_LIBU)
	#pragma weak _cat_init = __cat_init
#endif

/*
 * IMPORTANT:
 * This section is needed since this file also resides in the compilers'
 * libcsup/msg (v7.2 and higher). Once the compilers drop support for
 * pre-IRIX 6.5 releases this can be removed. Please build a libu before
 * checking in any changes to this file.
 *
 */

#ifndef _LIBU
#include "synonyms.h"
#else
#define _cat_init __cat_init
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "_locale.h"

typedef	nl_catd_t *sginl_catd;

#ifdef _LIBU
#define _mmp_opened __mmp_opened
#endif
int _mmp_opened = 0;

static int cat_malloc_init(int, sginl_catd);
static int cat_mmp_init(int, const char *, sginl_catd);

/*
 * Read a catalog and init the internal structure
 */
int 
_cat_init(const char *name, nl_catd res)
{
  int fd;
  ulong magic;
  sginl_catd sgires;

  sgires = res;

  /*
   * Read file header
   */
  if((fd=open(name,0)) < 0) {
    /*
     * Need read permission
     */
    return errno;
  }

  if (read(fd, (char *)&magic, sizeof(long)) == sizeof(long)){
    if (magic == CAT_MAGIC)
      return cat_malloc_init(fd,sgires);
    else
      return cat_mmp_init(fd,name,sgires);
  }
  return errno;
}

/*
 * Read a malloc catalog and init the internal structure
 */
static int
cat_malloc_init(int fd, sginl_catd res)
{
  struct cat_hdr hdr;
  char *mem;

  lseek(fd,0L,0);
  if (read(fd, (char *)&hdr, sizeof(struct cat_hdr)) != sizeof(struct cat_hdr))
    return errno;
  if ((mem = malloc((size_t)hdr.hdr_mem)) != (char*)0){

    if (read(fd, mem, (size_t) hdr.hdr_mem) == hdr.hdr_mem){
      res->info.m.sets = (struct _cat_set_hdr*)mem;
      res->info.m.msgs = (struct _cat_msg_hdr*)(mem + hdr.hdr_off_msg_hdr);
      res->info.m.data = mem + hdr.hdr_off_msg;
      res->set_nr = hdr.hdr_set_nr;
      res->type = MALLOC;
      close(fd);
      return 0;
    } else {
      free(mem);
      return errno;
    }
  }

  close(fd);
  return NL_ERR_MALLOC;
}

/*
 * Do the gettxt stuff
 */
static int
cat_mmp_init (int sfd, const char *catname, sginl_catd res)
{
  char message_file[MAXNAMLEN];
  struct stat sb;
  int mfd;

  if (_mmp_opened == NL_MAX_OPENED) {
    close(sfd);
    return NL_ERR_MAXOPEN;
  }

  /*
   * get the number of sets
   * of a set file
   */
  if (fstat(sfd, &sb) == -1) {
    close(sfd);
    return errno;
  }

  res->info.g.sets = (struct _set_info *)mmap(0,(size_t) sb.st_size,
       PROT_READ, MAP_SHARED, sfd, 0);

  if (res->info.g.sets == (struct _set_info *)MAP_FAILED) {
    close(sfd);
    return NL_ERR_MAP;
  }

  res->type = MKMSGS;
  res->info.g.set_size = (int)sb.st_size;
  res->set_nr = *((int*)(res->info.g.sets));
  close(sfd);
  
  if (res->set_nr > NL_SETMAX) {
    munmap(res->info.g.sets, (size_t) sb.st_size);
    return NL_ERR_HEADER;
  }

  /*  Now open the file with the messages in and memory map this
   */
  /* sprintf(message_file, "%s.m", catname); */
  strcpy(message_file, catname);
  strcat(message_file, ".m");

  if ((mfd = open(message_file, O_RDONLY)) < 0) {
    munmap(res->info.g.sets, (size_t) sb.st_size);
    return errno;
  }

  if (fstat(mfd, &sb) == -1) {
    munmap(res->info.g.sets, (size_t) sb.st_size);
    close(mfd);
    return errno;
  }

  if ((res->info.g.msgs = mmap(0, (size_t) sb.st_size, PROT_READ,
       MAP_SHARED, mfd, 0)) == (caddr_t)MAP_FAILED) {
    munmap(res->info.g.sets, (size_t) sb.st_size);
    close(mfd);
    return errno;
  }

  res->info.g.msg_size = (int)sb.st_size;
  close(mfd);
  _mmp_opened++;
  return 0;
}









