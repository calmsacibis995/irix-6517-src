#ifndef STRINGS_H
#define	STRINGS_H
/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Extended <string.h> for the netman library.
 */
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
long	strtol(const char *, char **, int);
char	*strndup(const char *, int);
int	nsprintf(char *, int, const char *, ...);
#ifdef _VA_LIST_
int	vnsprintf(char *, int, const char *, va_list);
#endif
#ifdef __cplusplus
}
#endif

/*
 * A struct that is sometimes convenient for representing strings as
 * counted arrays of bytes.
 */
struct string {
	char	*s_ptr;		/* pointer to string store */
	int	s_len;		/* length in bytes */
};

#endif
