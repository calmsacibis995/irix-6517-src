/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILES
 *
 *	typedefs.h    user defined type definitions
 *
 *  SCCS
 *
 *	@(#)typedefs.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "typedefs.h"
 *
 *  DESCRIPTION
 *
 *	Miscellaneous typedefs to make things clearer.
 *
 *	Also, since not all C compilers support all mixes of
 *	types (such as unsigned char for example), the
 *	more exotic types are typedef'd also.
 *
 */


typedef unsigned int UINT;	/* Unsigned integer */
typedef unsigned long ULONG;	/* Unsigned long */

#ifndef EXEC_TYPES_H		/* Found in amiga's types.h */
typedef unsigned short USHORT;	/* Unsigned short */
#endif

typedef unsigned char UCHAR;	/* Unsigned char */

#ifndef VOID
#define VOID void		/* Void type (typedef bug!??) */
#endif

typedef int BOOLEAN;		/* Has value of TRUE or FALSE */
typedef long S32BIT;		/* Signed 32 bit word */
typedef long long S64BIT;		/* Signed 64 bit word */

typedef S32BIT LBA;		/* Logical block address */
typedef S32BIT CHKSUM;		/* Checksums are 32 bits */

/*
 *	Nobody can seem to agree on whether signal() returns a
 *	pointer to a function that returns int or a pointer to
 *	a function that returns void.  I know for a fact that
 *	various ports of BSD define these both ways, and the same
 *	for USG ports.  Some ports don't even define them compatibly
 *	between the actual library code, the <signal.h> file, and the
 *	lint libraries!  Grrrrr!!!!
 *
 */

#if !SIGTYPEINT && !SIGTYPEVOID
#  define SIGTYPEINT 1		/* Pick a default, any default... */
#endif

#if SIGTYPEINT
typedef int (*SIGTYPE)();	/* Pointer to function returning int */
#endif

#if SIGTYPEVOID
typedef VOID (*SIGTYPE)();	/* Pointer to function return void */
#endif

/* prototypes, created from cproto.  */
/* sys.c */
int s_fprintf(FILE *stream, char *format, ...);
int s_vfprintf(FILE *stream, char *format, va_list arg);
int s_sprintf(char *s, char *format, ...);
void s_fflush(FILE *stream);
int s_open(char *path, int oflag, int mode);
int s_close(int fildes);
int s_access(char *path, int amode);
int s_read(int fildes, char *buf, UINT nbyte);
int s_chown(char *path, int owner, int group);
void s_sleep(UINT seconds);
int s_uname(struct my_utsname *name);
int s_umask(int cmask);
int s_utime(char *path, struct time_t *times);
int s_chmod(char *path, int mode);
int s_write(int fildes, char *buf, UINT nbyte);
int s_unlink(char *path);
long s_time(long *tloc);
int s_getuid(void);
int s_getgid(void);
char *s_ctime(long *clock);
struct tm *s_localtime(long *clock);
char *s_asctime(struct tm *tm);
FILE *s_fopen(char *file_name, char *type);
int s_mknod(char *path, int mode, int dev);
int s_stat(char *path, struct rmtstat64 *buf);
char *s_malloc(UINT size);
void s_exit(int status);
int s_getopt(int argc, char **argv, char *optstring);
void s_free(char *ptr);
struct passwd *s_getpwent(void);
void s_endpwent(void);
struct group *s_getgrent(void);
void s_endgrent(void);
int s_ioctl(int fildes, int request, int arg);
char *s_strcat(char *s1, char *s2);
int s_strcmp(char *s1, char *s2);
char *s_strcpy(char *s1, char *s2);
char *s_strncpy(char *s1, char *s2, int n);
int s_strlen(char *s);
char *s_strchr(char *s, int c);
char *s_strrchr(char *s, int c);
int s_mkdir(char *path, int mode);
S32BIT s_lseek(int fildes, S32BIT offset, int whence);
void s_rewind(FILE *stream);
int s_tolower(int c);
int s_creat(char *path, int mode);
int s_isdigit(int c);
int s_atoi(char *str);
int s_fileno(FILE *stream);
int s_getc(FILE *stream);
char *s_memcpy(char *s1, char *s2, int n);
int s_link(char *path1, char *path2);
int s_putc(int c, FILE *stream);
int s_dup(int fildes);
char *s_getenv(char *name);
int s_fork(void);
int s_wait(int *stat_loc);
int s_execv(char *path, char *argv[]);
int s_fclose(FILE *stream);
char *s_fgets(char *s, int n, FILE *stream);
int s_readlink(char *name, char *buf, int size);
long s_timezone(void);
int s_symlink(char *name1, char *name2);
int s_eject(int fildes);
int s_format(int fildes);
/* sys2.c */
/* sys3.c */
int isrmt(int fd);
void _rmt_abort(int fildes);
int rmtaccess(char *path, int amode);
int rmtclose(int fildes);
int _rmt_command(int fildes, char *buf);
int rmtcreat(char *path, int mode);
int _rmt_dev(register char *path);
int rmtdup(int fildes);
int rmtioctl(int fildes, int request, int arg);
long rmtlseek(int fildes, long offset, int whence);
int rmtlstat(char *path, struct stat64 *buf);
int rmtopen(char *path, int oflag, int mode);
int contact(char *node, char *user, char *cmd);
int rmtread(int fildes, char *buf, unsigned int nbyte);
int rmtstat(char *path, struct stat64 *buf);
int _rmt_status(int fildes);
int rmtwrite(int fildes, char *buf, unsigned int nbyte);


VOID bru_error(int, ...);
VOID tty_printf (char *str, ...);
