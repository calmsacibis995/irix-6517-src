/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.4 $"

#include <ctype.h>
#include <string.h>
#include <sys/types.h>

extern void	progerr(char *, ...);

/* function prototypes */
int	cknull(char *);
int	cklen(char *, int);
int	ckascii(char *);
int	ckalnum(char *);

#define ERR_LEN		"length of parameter <%s> exceeds limit"
#define ERR_ASCII	"parameter <%s> must be ascii"
#define ERR_ALNUM	"parameter <%s> must be alphanumeric"	
#define ERR_CHAR	"parameter <%s> has incorrect first character"
#define ERR_UNDEF	"parameter <%s> cannot be null"

#define MAXLEN 256
#define TOKLEN 16

int
ckparam(char *param, char *value)
{
	char *token, *lasttok;

	if(strcmp(param, "NAME") == 0) {
		if(cknull(value)) {
			progerr(ERR_UNDEF, param);
			return(1);
		}
		if(cklen(value, MAXLEN)) {
			progerr(ERR_LEN, param);
			return(1);
		}
		if(ckascii(value)) {
			progerr(ERR_ASCII, param);
			return(1);
		}
		return(0);
	}
	if(strcmp(param, "ARCH") == 0) {
		if(cknull(value)) {
			progerr(ERR_UNDEF, param);
			return(1);
		}
		lasttok = NULL;
		token = strtok_r(value, ",", &lasttok);
		while(token) { 
			if(cklen(token, TOKLEN)) {
				progerr(ERR_LEN, param);
				return(1);
			}
			if(ckascii(token)) {
				progerr(ERR_ASCII, param);
				return(1);
			}
			token = strtok_r(NULL, ",", &lasttok);
		}
		return(0);
	}
	if(strcmp(param, "VERSION") == 0) {
		if(cknull(value)) {
			progerr(ERR_UNDEF, param);
			return(1);
		}
		if(*value == '(') {
			progerr(ERR_CHAR, param);
			return(1);
		}
		if(cklen(value, MAXLEN)) {
			progerr(ERR_LEN, param);
			return(1);
		}
		if(ckascii(value)) {
			progerr(ERR_ASCII, param);
			return(1);
		}
		return(0);
	}
	if(strcmp(param, "CATEGORY") == 0) {
		if(cknull(value)) {
			progerr(ERR_UNDEF, param);
			return(1);
		}
		lasttok = NULL;
		token = strtok_r(value, ",", &lasttok);
		while(token) { 
			if(cklen(token, TOKLEN)) {
				progerr(ERR_LEN, param);
				return(1);
			}
			if(ckalnum(token)) {
				progerr(ERR_ALNUM, param);
				return(1);
			}
			token = strtok_r(NULL, ",", &lasttok);
		}
		return(0);
	}
	/* param does not match existing parameters */
	return(1);
}

int 
cknull(char *pt)
{
	if(strlen(pt) == 0)
		return(1);
	return(0);
}	

int
cklen(char *pt, int len)
{
	if(strlen(pt) > len)
		return(1);
	return(0);
}

int
ckascii(char *pt)
{
	while(*pt) { 
		if(!(isascii(*pt))) 
			return(1);
		pt++;
	}
	return(0);
}

int
ckalnum(char *pt)
{
	while(*pt) {
		if(!(isalnum(*pt))) 
			return(1);
		pt++;
	}
	
	return(0);
}
