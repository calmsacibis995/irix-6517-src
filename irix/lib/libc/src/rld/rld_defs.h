#ifndef __RLD_DEFS_H__
#define __RLD_DEFS_H__
/*
 * rld_defs.h
 *
 *      This file sets everything up for both 32 and 64 bit builds
 *      from the same source with very few #ifdefs in the source.
 *
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

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <cmplrs/host.h>

#if (_MIPS_SZPTR == 64)
#define _64BIT_OBJECTS
#endif

typedef uint64 ADDR64;	    /* generic 64-bit virtual address */

#define RLD_BAD_ADDRESS (-1)
/* Architecture-specific definitions */
#ifdef _64BIT_OBJECTS

typedef uint64 ADDR;	    /* 64-bit virtual address */
typedef int64 OFFSET;	    /* 64-bit file offset */
typedef int64 FILE_SZ;	    /* 64-bit file size/length */

#define ELF_ADDR	Elf64_Addr
#define ELF_HALF	Elf64_Half
#define ELF_OFF		Elf64_Off
#define ELF_SWORD	Elf64_Sword
#define ELF_SXWORD	Elf64_Sxword
#define ELF_WORD	Elf64_Word
#define ELF_XWORD	Elf64_Xword
#define ELF_BYTE	Elf64_Byte
#define ELF_SECTION	Elf64_Section
#define ELF_FLAGS	Elf64_Xword
#define ELF_SIZE	Elf64_Xword
#define ELF_INT		Elf64_Sxword /* the natural host unit size */

#define ELFCLASS	ELFCLASS64

#define OBJ_INFO	Elf64_Obj_Info
#else

typedef uint32 ADDR;		    /* 32-bit virtual address */
typedef int32 OFFSET;		    /* 32-bit file offset */
typedef int32 FILE_SZ;		    /* 32-bit file size/length */

#define ELF_ADDR	Elf32_Addr
#define ELF_HALF	Elf32_Half
#define ELF_OFF		Elf32_Off
#define ELF_SWORD	Elf32_Sword
#define ELF_WORD	Elf32_Word
#define ELF_XWORD	Elf32_Word
#define ELF_SIZE        Elf32_Word  /* 32-bit ELF size/length */
#define ELF_INT		Elf32_Sword  /* the natural host unit size */

typedef longlong_t	ELF_SXWORD; /* These three should never be used in */
typedef unsigned char	ELF_BYTE;   /* 32-bit mode? */
typedef unsigned short	ELF_SECTION;	
#define ELF_FLAGS	Elf32_Word

#define ELFCLASS	ELFCLASS32

#define OBJ_INFO	Elf32_Obj_Info
#endif /* _64BIT_OBJECTS */


#if (_MIPS_SZPTR == 32)

#define SIGNED	int32		    /* used for casting pointer to integer */
#define UNSIGNED uint32

#else

#define SIGNED	int64
#define UNSIGNED uint64

#endif /* _MIPS_SZPTR == 32 */



/* Common definitions */

#define HASH_NIL   (-1)

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define LD_ASSERT(EX, p, q) \
    if (!(EX)) error(ER_FATAL, ERN_INTERNAL, p, q)

#define MALLOC_ASSERT(addr) \
    if (addr == 0) error (ER_FATAL, ERN_MALLOC)

/* system and library calls */

#define FDOPEN(fid, type) \
    fdopen((int)(fid), (const char *)(type))

#define OPEN(path, oflag, mode) \
    open((char *)(path), (int)(oflag), (mode_t)(mode))

#define OPEN2(path, oflag) \
    open((char *)(path), (int)(oflag))

#define LSEEK(fid, offset, whence) \
    lseek((int)(fid), (off_t)(offset), (int)(whence))

#define READ(fid, buf, nbyte) \
    read((int)(fid), (void *)(buf), (unsigned)(nbyte))

#define WRITE(fid, buf, nbyte) \
    write((int)(fid), (const void *)(buf), (unsigned)(nbyte))

#define CLOSE(fid) \
    close((int)(fid))

#define FFLUSH(stm) \
    fflush((FILE *)stm)

#define FCLOSE(stm) \
    fclose((FILE *)stm)

#define FSTAT(fid, buf) \
    fstat((int)(fid), (buf))

#define FSTATVFS(fid, buf) \
    fstatvfs((int)(fid), (buf))

#define MMAP(addr, len, prot, flags, fd, off) \
    mmap((void *)(addr), (int)(len), (int)(prot), (int)(flags), (int)(fd), \
	 (off_t)(off))

#define PERROR(s) \
    perror((char *) s)

#define MUNMAP(addr, len) \
    munmap((void *)(addr), (int)(len))

#define MALLOC(nbytes) \
    malloc((size_t)(nbytes))

#define FREE(ptr) \
    if (ptr) free((void *) (ptr))

#define REALLOC(ptr, size) \
    realloc((void *)(ptr), (size_t)(size))

#define CALLOC(nelem, elsize) \
    calloc((size_t)(nelem), (size_t)(elsize))

#define MEMCCPY(s1, s2, c, n) \
    memccpy((void *)(s1), (void *)(s2), (int)(c), (size_t)(n))

#define MEMCHR(s, c, n) \
    memchr((void *)(s), (int)(c), (size_t)(n))

#define MEMCPY(s1, s2, n) \
    memcpy((void *)(s1), (void *)(s2), (size_t)(n))

#define MEMSET(s, c, n) \
    memset((void *)(s), (int)(c), (size_t)(n))

#define MEMCMP(s1, s2, n) \
    memcmp((void *)(s1), (void *)(s2), (size_t)(n))

#define SPRINTF(s,fmt) \
    sprintf((char *)s, (char *)fmt)

#define SPRINTF3(s,fmt,a) \
    sprintf((char *)s, (char *)fmt, a)

#define SPRINTF4(s,fmt,a1,a2) \
    sprintf((char *)s, (char *)fmt, a1, a2)

#define VSPRINTF(s,fmt,v) \
    vsprintf((char *)s, (char *)fmt, (va_list)v)

#define  STRCMP(s1,s2) \
    strcmp((char *)(s1), (char *)(s2))

#define STRNCMP(s1,s2,n) \
    strncmp((char *)(s1), (char *)(s2), (size_t)(n))

#define STRDUP(s) \
    strdup((char *)(s))

#define STRLEN(s) \
    strlen((char *)(s))

#define STRCPY(s1, s2) \
    strcpy((char *)(s1), (char *)(s2))

#define STRTOK(s1, s2) \
    strtok((char *)(s1), (char *)(s2))

#define STRPBRK(s1, s2) \
    strpbrk((char *)(s1), (char *)(s2))

#define STRCHR(s, n) \
    strchr((char *)(s), (int)(n))

#define STRRCHR(s, c) \
    strrchr((char *)(s), (int)(c))

#define STRCAT(s1, s2) \
    strcat((char *)(s1), (char *)(s2))

#define GETS(s) \
    gets((char *)(s))

#define GETENV(s) \
    getenv((char *) s)

#define SGINAP(l) \
    sginap((long)(l))

#define TEST_AND_SET(lp,l) \
    test_and_set((unsigned long *)(lp), (long)(l))

#define ATOMIC_SET(lp,l) \
    atomic_set((unsigned long *)(lp), (long)(l))

/* jmp_buf is not a scalar type, so we can't cast into it.
 */

#define SETJMP(e) \
    setjmp(e)

#define LONGJMP(e,v) \
    longjmp(e, (int)v);

#define FPRINTF(file,fmt) \
    fprintf((FILE *)file, (char *)fmt)

#define FPRINTF3(file,fmt,arg) \
    fprintf((FILE *)file, (char *)fmt, arg)

#define FOPEN(f,t) \
    fopen((char *)f, (char *)t)

#define OPENLOG(id, o, f) \
    openlog((char *)id, (int)o, (int)f)

#define SYSLOG(p, m) \
    syslog((int)p, (char *)m)

#define CLOSELOG closelog

#define EXIT(n) \
    exit((int)n)

#define VFPRINTF(file,fmt,a) \
    vfprintf((FILE *)file, (char *)fmt, (va_list)a)

#define SYSSGI4(i,a1,a2,a3) \
    syssgi((int)i, a1, a2, a3)

#define MPROTECT(a,l,prot) \
    mprotect((caddr_t)a, (size_t) l, (int)prot)

#define IS_UNDEF(x)    ((x) == SHN_UNDEF || (x) == SHN_MIPS_SUNDEFINED)

#define IS_COMMON(x)    ((x) == SHN_COMMON || (x) == SHN_MIPS_SCOMMON)

#endif /* __LD_DEFS_H__ */












