/* Definitions ripped off from linker for relocatable elf in relocate.c */

#ifndef __LD_DEFS_H__
#define __LD_DEFS_H__

/*
#include "ld_config.h"
*/
#include <sys/types.h>
#include <cmplrs/host.h>
#include <memory.h>
#include <string.h>
#include <sys/inst.h>

typedef union mips_instruction   INST, *pINST;

typedef uint64 ADDR64;	    /* generic 64-bit virtual address */

/* all internal merged tables */
typedef enum { 
    TBL_EXT, TBL_FD, TBL_DN, TBL_ELFSYM, TBL_LCL, MAX_LD_TBL 
} LD_TBL_TYPE;


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

/* printf formatting code */
#define _fmt_w        "w64"
#define _fmt_v        "lld"
#define _fmt_a        "llx"
#define _fmt_s        "llx"
#define _fmt_w_a      "016llx"
#define _fmt_w_s      "016llx"

#else

typedef uint32 ADDR;		    /* 32-bit virtual address */
typedef int32 OFFSET;		    /* 32-bit file offset */
typedef int32 FILE_SZ;		    /* 32-bit file size/length */

#define ELF_ADDR	Elf32_Addr
#define ELF_HALF	Elf32_Half
#define ELF_OFF		Elf32_Off
#define ELF_SWORD	Elf32_Sword
#define ELF_WORD	Elf32_Word
#define ELF_SIZE        Elf32_Word  /* 32-bit ELF size/length */
#define ELF_INT		Elf32_Sword  /* the natural host unit size */

typedef longlong_t	ELF_SXWORD; /* These three should never be used in */
typedef unsigned char	ELF_BYTE;   /* 32-bit mode? */
typedef unsigned short	ELF_SECTION;	
#define ELF_FLAGS	Elf32_Word

#define ELFCLASS	ELFCLASS32

/* printf formatting code */

#define _fmt_w        "w32"
#define _fmt_v        "d"
#define _fmt_a        "x"
#define _fmt_s        "x"
#define _fmt_w_a      "08x"
#define _fmt_w_s      "08x"

#endif /* _64BIT_OBJECTS */


#if (_MIPS_SZPTR == 32)

#define SIGNED	int32		    /* used for casting pointer to integer */
#define UNSIGNED uint32

#else

#define SIGNED	int64
#define UNSIGNED uint64

#endif /* _MIPS_SZPTR == 32 */



/* Common definitions */
#define LD_BAD_ADDRESS ((ELF_ADDR) -1)

#define HASH_NIL   (-1)

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define LD_ASSERT(EX, p, q) \
    if (!(EX)) error(ER_FATAL, ERN_INTERNAL, p, q)

#define FATAL(p, q) \
    error(ER_FATAL, ERN_INTERNAL, p, q)

#define MALLOC_ASSERT(addr) \
    if (addr == 0) error (ER_FATAL, ERN_MALLOC)

/* system and library calls */

#define FDOPEN(fid, type) \
    fdopen((int)(fid), (const char *)(type))

#define OPEN(path, oflag, mode) \
    open((char *)(path), (int)(oflag), (int)(mode))

#define LSEEK(fid, offset, whence) \
    lseek((int)(fid), (off_t)(offset), (int)(whence))

#define READ(fid, buf, nbyte) \
    read((int)(fid), (void *)(buf), (unsigned)(nbyte))

#define WRITE(fid, buf, nbyte) \
    write((int)(fid), (const void *)(buf), (unsigned)(nbyte))

#define CLOSE(fid) \
    close((int)(fid))

#define FCHMOD(fid, mode) \
    fchmod((int)(fid), (mode_t)(mode))

#define UNLINK(path) \
    unlink((const char *)(path))

#define STAT(path, buf) \
    stat((const char *)(path), (struct stat *) (buf))

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

#define IS_UNDEF(x)    ((x) == SHN_UNDEF || (x) == SHN_MIPS_SUNDEFINED)

#define IS_COMMON(x)    ((x) == SHN_COMMON || (x) == SHN_MIPS_SCOMMON)

#define DEMANGLE(name) \
    (option[OPT_DEMANGLE].flag ? demangle (name) : (name))

#endif /* __LD_DEFS_H__ */

