/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.29 $"

#ifndef	_LIBC_H_
#define	_LIBC_H_

#include <stdarg.h>

#include "rtc.h"

#define isspace(c)	((c) == ' ' || (c) == '\t')
#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define islower(c)	((c) >= 'a' && (c) <= 'z')
#define isalpha(c)	(islower(c) || isupper(c))
#define isalnum(c)	(isalpha(c) || isdigit(c))
#define isprint(c)	((c) >= ' ' && (c) <= '~')
#define isxdigit(c)	((c) >= '0' && (c) <= '9' || \
			 (c) >= 'a' && (c) <= 'f' || \
			 (c) >= 'A' && (c) <= 'F')
#define isodigit(c)	((c) >= '0' && (c) <= '7')
#define toupper(c)	(islower(c) ? (c) - 'a' + 'A' : (c))
#define tolower(c)	(isupper(c) ? (c) - 'A' + 'a' : (c))

typedef struct libc_device_s {
    void	(*init)(void *init_data);
    int		(*poll)(void);
    int		(*readc)(void);
    int		(*getc)(void);
    int		(*putc)(int);
    int		(*puts)(char *);
    int		(*flush)(void);		/* Flush output */
    int		led_pattern;
    char       *dev_name;
} libc_device_t;

libc_device_t  *libc_device(libc_device_t *dev);
void		libc_init(void *init_data);

int		poll(void);
int		readc(void);
int		putc(int c);
int		putchar(int c);
int		puts(char *s);
int		flush(void);
int		kbintr(rtc_time_t *next);
int		more(int *lines, int max);
char	       *device(void);

int		getc_timeout(__uint64_t usec);
int		getc(void);
char	       *gets_timeout(char *buf, int max_length,
			     __uint64_t usec, char *defl);
char	       *gets(char *buf, int max_length);
int		puthex(__uint64_t v);
size_t		strlen(const char *s);
char	       *strcpy(char *dst, const char *src);
char	       *strncpy(char *dst, const char *src, size_t n);
int		strncmp(const char *s1, const char *s2, size_t n);
int		strcmp(const char *s1, const char *s2);
char	       *strchr(const char *s, int c);
char	       *strcat(char *dst, const char *src);
char	       *strstr(const char *as1, const char *as2);
char	       *strrepl(char *s, int start, int len, const char *repstr);
void	       *memset(void *base, int value, size_t len);
int		memcmp8(__uint64_t *s1, __uint64_t *s2, size_t n);
void		bzero(void *base, size_t len);
void	       *memcpy(void *dest, const void *source, size_t len);
__uint64_t	memsum(void *base, size_t len);
int		atoi(const char *cp);
__uint64_t	htol(char *cp);
__uint64_t	strtoull(const char *str, char **nptr, int base);
void		sprintnu(char *tgt, __scunsigned_t n, int b, int digits);
void		sprintns(char *tgt, __scint_t n, int b, int digits);
void		prf(int (*putc)(), __uint64_t putc_data,
		    const char *fmt, va_list adx);
int		printf(const char *fmt, ...);
int		vprintf(const char *fmt, va_list ap);
int		db_printf(const char *fmt, ...);	   /* If verbose on */
int		db_vprintf(const char *fmt, va_list ap);   /* If verbose on */
int		sprintf(char *buf, const char *fmt, ...);
int		vsprintf(char *buf, const char *fmt, va_list ap);
#if 0
int		sprintf_length(const char *fmt, ...);
#endif

int		majority(int n, int *votes);

#endif /* _LIBC_H_ */
