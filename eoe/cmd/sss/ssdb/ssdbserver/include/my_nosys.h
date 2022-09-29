/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  Header to remove use of my_functions in functions where we need speed and
  where calls to posix functions should work
*/
#ifndef _my_nosys_h
#define _my_nosys_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef __MY_NOSYS__
#define __MY_NOSYS__

#ifdef MSDOS
#include <io.h>			/* Get prototypes for read()... */
#endif
#ifndef HAVE_STDLIB_H
#include <malloc.h>
#endif

#define my_read(a,b,c,d) my_quick_read(a,b,c,d)
#define my_write(a,b,c,d) my_quick_write(a,b,c)
#define my_seek(a,b,c,d)  my_quick_seek(a,b,c)
extern uint my_quick_read(File Filedes,byte *Buffer,uint Count,myf myFlags);
extern uint my_quick_write(File Filedes,const byte *Buffer,uint Count);
extern ulong my_quick_seek(File fd,ulong pos,int whence);

#if !defined(SAFEMALLOC) && defined(USE_HALLOC)
#define my_malloc(a,b) halloc(a,1)
#define my_no_flags_free(a) hfree(a)
#endif

#endif /* __MY_NOSYS__ */

#ifdef	__cplusplus
}
#endif
#endif
