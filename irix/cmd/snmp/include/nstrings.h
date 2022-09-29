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
char	*strndup(const char *, size_t);
int	nsprintf(char *, size_t, const char *, ...);
#ifdef _VA_LIST_
int	vnsprintf(char *, size_t, const char *, va_list);
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
