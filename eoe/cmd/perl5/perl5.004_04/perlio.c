/*    perlio.c
 *
 *    Copyright (c) 1996, Nick Ing-Simmons
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#define VOIDUSED 1
#include "config.h"

#define PERLIO_NOT_STDIO 0 
#if !defined(PERLIO_IS_STDIO) && !defined(USE_SFIO)
#define PerlIO FILE
#endif
/*
 * This file provides those parts of PerlIO abstraction 
 * which are not #defined in perlio.h.
 * Which these are depends on various Configure #ifdef's 
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef PERLIO_IS_STDIO 

void
PerlIO_init()
{
 /* Does nothing (yet) except force this file to be included 
    in perl binary. That allows this file to force inclusion
    of other functions that may be required by loadable 
    extensions e.g. for FileHandle::tmpfile  
 */
}

#undef PerlIO_tmpfile
PerlIO *
PerlIO_tmpfile()
{
 return tmpfile();
}

#else /* PERLIO_IS_STDIO */

#ifdef USE_SFIO

#undef HAS_FSETPOS
#undef HAS_FGETPOS

/* This section is just to make sure these functions 
   get pulled in from libsfio.a
*/

#undef PerlIO_tmpfile
PerlIO *
PerlIO_tmpfile()
{
 return sftmp(0);
}

void
PerlIO_init()
{
 /* Force this file to be included  in perl binary. Which allows 
  *  this file to force inclusion  of other functions that may be 
  *  required by loadable  extensions e.g. for FileHandle::tmpfile  
  */

 /* Hack
  * sfio does its own 'autoflush' on stdout in common cases.
  * Flush results in a lot of lseek()s to regular files and 
  * lot of small writes to pipes.
  */
 sfset(sfstdout,SF_SHARE,0);
}

#else

/* Implement all the PerlIO interface using stdio. 
   - this should be only file to include <stdio.h>
*/

#undef PerlIO_stderr
PerlIO *
PerlIO_stderr()
{
 return (PerlIO *) stderr;
}

#undef PerlIO_stdin
PerlIO *
PerlIO_stdin()
{
 return (PerlIO *) stdin;
}

#undef PerlIO_stdout
PerlIO *
PerlIO_stdout()
{
 return (PerlIO *) stdout;
}

#undef PerlIO_fast_gets
int 
PerlIO_fast_gets(f)
PerlIO *f;
{
#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
 return 1;
#else
 return 0;
#endif
}

#undef PerlIO_has_cntptr
int 
PerlIO_has_cntptr(f)
PerlIO *f;
{
#if defined(USE_STDIO_PTR)
 return 1;
#else
 return 0;
#endif
}

#undef PerlIO_canset_cnt
int 
PerlIO_canset_cnt(f)
PerlIO *f;
{
#if defined(USE_STDIO_PTR) && defined(STDIO_CNT_LVALUE)
 return 1;
#else
 return 0;
#endif
}

#undef PerlIO_set_cnt
void
PerlIO_set_cnt(f,cnt)
PerlIO *f;
int cnt;
{
 if (cnt < -1)
  warn("Setting cnt to %d\n",cnt);
#if defined(USE_STDIO_PTR) && defined(STDIO_CNT_LVALUE)
 FILE_cnt(f) = cnt;
#else
 croak("Cannot set 'cnt' of FILE * on this system");
#endif
}

#undef PerlIO_set_ptrcnt
void
PerlIO_set_ptrcnt(f,ptr,cnt)
PerlIO *f;
STDCHAR *ptr;
int cnt;
{
#ifdef FILE_bufsiz
 STDCHAR *e = FILE_base(f) + FILE_bufsiz(f);
 int ec = e - ptr;
 if (ptr > e + 1)
  warn("Setting ptr %p > end+1 %p\n", ptr, e + 1);
 if (cnt != ec)
  warn("Setting cnt to %d, ptr implies %d\n",cnt,ec);
#endif
#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE)
 FILE_ptr(f) = ptr;
#else
 croak("Cannot set 'ptr' of FILE * on this system");
#endif
#if defined(USE_STDIO_PTR) && defined(STDIO_CNT_LVALUE)
 FILE_cnt(f) = cnt;
#else
 croak("Cannot set 'cnt' of FILE * on this system");
#endif
}

#undef PerlIO_get_cnt
int 
PerlIO_get_cnt(f)
PerlIO *f;
{
#ifdef FILE_cnt
 return FILE_cnt(f);
#else
 croak("Cannot get 'cnt' of FILE * on this system");
 return -1;
#endif
}

#undef PerlIO_get_bufsiz
int 
PerlIO_get_bufsiz(f)
PerlIO *f;
{
#ifdef FILE_bufsiz
 return FILE_bufsiz(f);
#else
 croak("Cannot get 'bufsiz' of FILE * on this system");
 return -1;
#endif
}

#undef PerlIO_get_ptr
STDCHAR *
PerlIO_get_ptr(f)
PerlIO *f;
{
#ifdef FILE_ptr
 return FILE_ptr(f);
#else
 croak("Cannot get 'ptr' of FILE * on this system");
 return NULL;
#endif
}

#undef PerlIO_get_base
STDCHAR *
PerlIO_get_base(f)
PerlIO *f;
{
#ifdef FILE_base
 return FILE_base(f);
#else
 croak("Cannot get 'base' of FILE * on this system");
 return NULL;
#endif
}

#undef PerlIO_has_base 
int 
PerlIO_has_base(f)
PerlIO *f;
{
#ifdef FILE_base
 return 1;
#else
 return 0;
#endif
}

#undef PerlIO_puts
int
PerlIO_puts(f,s)
PerlIO *f;
const char *s;
{
 return fputs(s,f);
}

#undef PerlIO_open 
PerlIO * 
PerlIO_open(path,mode)
const char *path;
const char *mode;
{
 return fopen(path,mode);
}

#undef PerlIO_fdopen
PerlIO * 
PerlIO_fdopen(fd,mode)
int fd;
const char *mode;
{
 return fdopen(fd,mode);
}

#undef PerlIO_reopen
PerlIO * 
PerlIO_reopen(name, mode, f)
const char *name;
const char *mode;
PerlIO *f;
{
 return freopen(name,mode,f);
}

#undef PerlIO_close
int      
PerlIO_close(f)
PerlIO *f;
{
 return fclose(f);
}

#undef PerlIO_eof
int      
PerlIO_eof(f)
PerlIO *f;
{
 return feof(f);
}

#undef PerlIO_getname
char *
PerlIO_getname(f,buf)
PerlIO *f;
char *buf;
{
#ifdef VMS
 return fgetname(f,buf);
#else
 croak("Don't know how to get file name");
 return NULL;
#endif
}

#undef PerlIO_getc
int      
PerlIO_getc(f)
PerlIO *f;
{
 return fgetc(f);
}

#undef PerlIO_error
int      
PerlIO_error(f)
PerlIO *f;
{
 return ferror(f);
}

#undef PerlIO_clearerr
void
PerlIO_clearerr(f)
PerlIO *f;
{
 clearerr(f);
}

#undef PerlIO_flush
int      
PerlIO_flush(f)
PerlIO *f;
{
 return Fflush(f);
}

#undef PerlIO_fileno
int      
PerlIO_fileno(f)
PerlIO *f;
{
 return fileno(f);
}

#undef PerlIO_setlinebuf
void
PerlIO_setlinebuf(f)
PerlIO *f;
{
#ifdef HAS_SETLINEBUF
    setlinebuf(f);
#else
#  ifdef __BORLANDC__ /* Borland doesn't like NULL size for _IOLBF */
    setvbuf(f, Nullch, _IOLBF, BUFSIZ);
#  else
    setvbuf(f, Nullch, _IOLBF, 0);
#  endif
#endif
}

#undef PerlIO_putc
int      
PerlIO_putc(f,ch)
PerlIO *f;
int ch;
{
 return putc(ch,f);
}

#undef PerlIO_ungetc
int      
PerlIO_ungetc(f,ch)
PerlIO *f;
int ch;
{
 return ungetc(ch,f);
}

#undef PerlIO_read
SSize_t
PerlIO_read(f,buf,count)
PerlIO *f;
void *buf;
Size_t count;
{
 return fread(buf,1,count,f);
}

#undef PerlIO_write
SSize_t
PerlIO_write(f,buf,count)
PerlIO *f;
const void *buf;
Size_t count;
{
 return fwrite1(buf,1,count,f);
}

#undef PerlIO_vprintf
int      
PerlIO_vprintf(f,fmt,ap)
PerlIO *f;
const char *fmt;
va_list ap;
{
 return vfprintf(f,fmt,ap);
}


#undef PerlIO_tell
long
PerlIO_tell(f)
PerlIO *f;
{
 return ftell(f);
}

#undef PerlIO_seek
int
PerlIO_seek(f,offset,whence)
PerlIO *f;
off_t offset;
int whence;
{
 return fseek(f,offset,whence);
}

#undef PerlIO_rewind
void
PerlIO_rewind(f)
PerlIO *f;
{
 rewind(f);
}

#undef PerlIO_printf
int      
#ifdef I_STDARG
PerlIO_printf(PerlIO *f,const char *fmt,...)
#else
PerlIO_printf(f,fmt,va_alist)
PerlIO *f;
const char *fmt;
va_dcl
#endif
{
 va_list ap;
 int result;
#ifdef I_STDARG
 va_start(ap,fmt);
#else
 va_start(ap);
#endif
 result = vfprintf(f,fmt,ap);
 va_end(ap);
 return result;
}

#undef PerlIO_stdoutf
int      
#ifdef I_STDARG
PerlIO_stdoutf(const char *fmt,...)
#else
PerlIO_stdoutf(fmt, va_alist)
const char *fmt;
va_dcl
#endif
{
 va_list ap;
 int result;
#ifdef I_STDARG
 va_start(ap,fmt);
#else
 va_start(ap);
#endif
 result = PerlIO_vprintf(PerlIO_stdout(),fmt,ap);
 va_end(ap);
 return result;
}

#undef PerlIO_tmpfile
PerlIO *
PerlIO_tmpfile()
{
 return tmpfile();
}

#undef PerlIO_importFILE
PerlIO *
PerlIO_importFILE(f,fl)
FILE *f;
int fl;
{
 return f;
}

#undef PerlIO_exportFILE
FILE *
PerlIO_exportFILE(f,fl)
PerlIO *f;
int fl;
{
 return f;
}

#undef PerlIO_findFILE
FILE *
PerlIO_findFILE(f)
PerlIO *f;
{
 return f;
}

#undef PerlIO_releaseFILE
void
PerlIO_releaseFILE(p,f)
PerlIO *p;
FILE *f;
{
}

void
PerlIO_init()
{
 /* Does nothing (yet) except force this file to be included 
    in perl binary. That allows this file to force inclusion
    of other functions that may be required by loadable 
    extensions e.g. for FileHandle::tmpfile  
 */
}

#endif /* USE_SFIO */
#endif /* PERLIO_IS_STDIO */

#ifndef HAS_FSETPOS
#undef PerlIO_setpos
int
PerlIO_setpos(f,pos)
PerlIO *f;
const Fpos_t *pos;
{
 return PerlIO_seek(f,*pos,0); 
}
#else
#ifndef PERLIO_IS_STDIO
#undef PerlIO_setpos
int
PerlIO_setpos(f,pos)
PerlIO *f;
const Fpos_t *pos;
{
 return fsetpos(f, pos);
}
#endif
#endif

#ifndef HAS_FGETPOS
#undef PerlIO_getpos
int
PerlIO_getpos(f,pos)
PerlIO *f;
Fpos_t *pos;
{
 *pos = PerlIO_tell(f);
 return 0;
}
#else
#ifndef PERLIO_IS_STDIO
#undef PerlIO_getpos
int
PerlIO_getpos(f,pos)
PerlIO *f;
Fpos_t *pos;
{
 return fgetpos(f, pos);
}
#endif
#endif

#if (defined(PERLIO_IS_STDIO) || !defined(USE_SFIO)) && !defined(HAS_VPRINTF)

int
vprintf(pat, args)
char *pat, *args;
{
    _doprnt(pat, args, stdout);
    return 0;		/* wrong, but perl doesn't use the return value */
}

int
vfprintf(fd, pat, args)
FILE *fd;
char *pat, *args;
{
    _doprnt(pat, args, fd);
    return 0;		/* wrong, but perl doesn't use the return value */
}

#endif

#ifndef PerlIO_vsprintf
int 
PerlIO_vsprintf(s,n,fmt,ap)
char *s;
const char *fmt;
int n;
va_list ap;
{
 int val = vsprintf(s, fmt, ap);
 if (n >= 0)
  {
   if (strlen(s) >= (STRLEN)n)
    {
     PerlIO_puts(PerlIO_stderr(),"panic: sprintf overflow - memory corrupted!\n");
     my_exit(1);
    }
  }
 return val;
}
#endif

#ifndef PerlIO_sprintf
int      
#ifdef I_STDARG
PerlIO_sprintf(char *s, int n, const char *fmt,...)
#else
PerlIO_sprintf(s, n, fmt, va_alist)
char *s;
int n;
const char *fmt;
va_dcl
#endif
{
 va_list ap;
 int result;
#ifdef I_STDARG
 va_start(ap,fmt);
#else
 va_start(ap);
#endif
 result = PerlIO_vsprintf(s, n, fmt, ap);
 va_end(ap);
 return result;
}
#endif

