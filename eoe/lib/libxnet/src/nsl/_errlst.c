/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/_errlst.c	1.3"
#ifdef __STDC__
        #pragma weak t_errlist	= __t_errlist
        #pragma weak t_errno	= __t_errno
        #pragma weak t_nerr	= __t_nerr
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#ifdef _BUILDING_LIBXNET
#include <unistd.h>
#include <msgs/uxlibxnet.h>
#endif

/*
 * transport errno
 */
int t_errno = 0;

#ifdef _BUILDING_LIBXNET
int t_nerr = 29;
#else
int t_nerr = 19;
#endif

int _dummy_errno[4] = {0, 0, 0, 0};
int _dummy_nerr[4] = {0, 0, 0, 0};
char **_dummy_errlst[4] = {0, 0, 0, 0};



/* 
 * transport interface error list
 */

#ifndef _BUILDING_LIBXNET
char *t_errlist[] = {
	"No Error",					/*  0 */
	"Incorrect address format",		  	/*  1 */
	"Incorrect options format",			/*  2 */
	"Illegal permissions",				/*  3 */
	"Illegal file descriptor",			/*  4 */
	"Couldn't allocate address",  			/*  5 */
	"Routine will place interface out of state",    /*  6 */
	"Illegal called/calling sequence number",	/*  7 */
	"System error",					/*  8 */
	"An event requires attention",			/*  9 */
	"Illegal amount of data",			/* 10 */
	"Buffer not large enough",			/* 11 */
	"Can't send message - (blocked)",		/* 12 */
	"No message currently available",		/* 13 */
	"Disconnect message not found",			/* 14 */
	"Unitdata error message not found",		/* 15 */
	"Incorrect flags specified",			/* 16 */
	"Orderly release message not found",            /* 17 */
	"Primitive not supported by provider",		/* 18 */
	"State is in process of changing",              /* 19 */
	"Unsupported struct-type requested",		/* TNOSTRUCTYPE 20*/
	"Invalid transport provider name",		/* TBADNAME 21 */
	"Qlen is zero",					/* TBADQLEN 22 */
	"Address in use",				/* TADDRBUSY 23*/
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	""
};
#else
static struct t_errlst {
	const char *msgid;
	const char *themsg;
} t_errlsts[] = {
	{ _SGI_t_strerror_0, _SGI_St_strerror_0 },
	{ _SGI_t_strerror_TBADADDR, _SGI_St_strerror_TBADADDR },
	{ _SGI_t_strerror_TBADOPT, _SGI_St_strerror_TBADOPT },
	{ _SGI_t_strerror_TACCES, _SGI_St_strerror_TACCES },
	{ _SGI_t_strerror_TBADF, _SGI_St_strerror_TBADF },
	{ _SGI_t_strerror_TNOADDR, _SGI_St_strerror_TNOADDR },
	{ _SGI_t_strerror_TOUTSTAT, _SGI_St_strerror_TOUTSTAT },
	{ _SGI_t_strerror_TBADSEQ, _SGI_St_strerror_TBADSEQ },
	{ _SGI_t_strerror_TSYSERR, _SGI_St_strerror_TSYSERR },
	{ _SGI_t_strerror_TLOOK, _SGI_St_strerror_TLOOK },
	{ _SGI_t_strerror_TBADDATA, _SGI_St_strerror_TBADDATA },
	{ _SGI_t_strerror_TBUFOVFLW, _SGI_St_strerror_TBUFOVFLW },
	{ _SGI_t_strerror_TFLOW, _SGI_St_strerror_TFLOW },
	{ _SGI_t_strerror_TNODATA, _SGI_St_strerror_TNODATA },
	{ _SGI_t_strerror_TNODIS, _SGI_St_strerror_TNODIS },
	{ _SGI_t_strerror_TNOUDERR, _SGI_St_strerror_TNOUDERR },
	{ _SGI_t_strerror_TBADFLAG, _SGI_St_strerror_TBADFLAG },
	{ _SGI_t_strerror_TNOREL, _SGI_St_strerror_TNOREL },
	{ _SGI_t_strerror_TNOTSUPPORT, _SGI_St_strerror_TNOTSUPPORT },
	{ _SGI_t_strerror_TSTATECHNG, _SGI_St_strerror_TSTATECHNG },
	{ _SGI_t_strerror_TNOSTRUCTYPE, _SGI_St_strerror_TNOSTRUCTYPE },
	{ _SGI_t_strerror_TBADNAME, _SGI_St_strerror_TBADNAME },
	{ _SGI_t_strerror_TBADQLEN, _SGI_St_strerror_TBADQLEN },
	{ _SGI_t_strerror_TADDRBUSY, _SGI_St_strerror_TADDRBUSY },
	{ _SGI_t_strerror_TINDOUT, _SGI_St_strerror_TINDOUT },
	{ _SGI_t_strerror_TPROVMISMATCH, _SGI_St_strerror_TPROVMISMATCH },
	{ _SGI_t_strerror_TRESADDR_A, _SGI_St_strerror_TRESADDR_A },
	{ _SGI_t_strerror_TRESADDR_B, _SGI_St_strerror_TRESADDR_B },
	{ _SGI_t_strerror_TQFULL, _SGI_St_strerror_TQFULL },
	{ _SGI_t_strerror_TPROTO, _SGI_St_strerror_TPROTO },
	{ _SGI_t_strerror_unknown, _SGI_St_strerror_unknown }
};
#ifdef COMMENT
/*
 *  The below messages are the default tests for English.
 *  The message catalog for uxlibxnet needs to be changed for
 *  other languages other than English.
 */
	"No Error",					/*  0 */
	"incorrect addr format",		  	/*  TBADADDR 1 */
	"incorrect option format",			/*  TBADOPT 2 */
	"incorrect permissions",			/*  TACCES 3 */
	"illegal transport fd",				/*  TBADF 4 */
	"couldn't allocate addr",  			/*  TNOADDR 5 */
	"out of state",    				/*  TOUTSTAT 6 */
	"bad call sequence number",			/*  TBADSEQ 7 */
	"system error",					/*  TSYSERR 8 */
	"event requires attention",			/*  TLOOK 9 */
	"illegal amount of data",			/*  TBADDATA 10 */
	"buffer not large enough",			/*  TBUFOVFLW 11 */
	"flow control",					/*  TFLOW 12 */
	"no data",					/*  TNODATA 13 */
	"discon_ind not found on queue",		/*  TNODIS 14 */
	"unitdata error not found",			/*  TNOUDERR 15 */
	"bad flags",					/*  TBADFLAG 16 */
	"no ord rel found on queue",            	/*  TNOREL 17 */
	"primitive/action not supported",		/*  TNOTSUPPORT 18 */
	"state is in process of changing",              /*  TSTATECHNG 19 */
	"unsupported struct-type requested",		/*  TNOSTRUCTYPE 20 */
	"invalid transport provider name",		/*  TBADNAME 21 */
	"qlen is zero",					/*  TBADQLEN 22 */
	"address in use",				/*  TADDRBUSY 23 */
	"outstanding connection indications",		/*  TINDOUT 24 */
	"transport provider mismatch",			/*  TPROVMISMATCH 25 */
	"resfd specified to accept w/qlen >0",		/*  TRESADDR 26 */
	"resfd not bound to same addr as fd",		/*  TRESADDR 27 */
	"incoming connection queue full",		/*  TQFULL 28 */
	"XTI protocol error",				/*  TPROTO 29 */
        "%d: error unknown"
#endif	/* COMMENT */

char *
t_errlist(int errnum)
{
	if (errnum < 1 || errnum > t_nerr) {
		char tmp[5];
		char *s, *s1;
		int i;
		int fndone = 0;

		s = (char *)gettxt(_SGI_t_strerror_unknown,
			_SGI_St_strerror_unknown);
		/*
		 *  Only allow two positions for the decimal
		 *  format %d.  Replace the %d in the message text
		 *  with two decimal ascii digits from errnum.
		 */
		for(i=0; i<strlen(s); i++) {
			if((s[i] == '%') && (s[i+1] == 'd'))
				fndone++;
		}
		/* If no "%d" found in messages text .  Just return 's' */
		if(!fndone)
			return(s);
		if((s1 = (char *)malloc(strlen(s) + 1)) == NULL)
			return(s);
		strncpy(s1, (const char *)s, (size_t)strlen(s));
		s1[strlen(s)] = 0;
		if(errnum > 99)
			errnum = 99;
		sprintf(tmp, "%2d", errnum);
		for(i=0; i<strlen(s); i++) {
			if((s1[i] == '%') && (s1[i+1] == 'd')) {
				s1[i] = tmp[0];
				s1[i+1] = tmp[1];
				break;
			}
		}
		return(s1);
	}
	return((char *)gettxt(t_errlsts[errnum].msgid,
		t_errlsts[errnum].themsg));
}
#endif
