/*
 * Copyright (c) 1984, 1988  Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *      @(#)ns_error.h	7.3 (Berkeley) 1/28/88
 */
#ifdef __cplusplus
extern "C" {
#endif
#ifdef sgi
# ifdef _KERNEL
#  define KERNEL
# endif
#endif

/*
 * Xerox NS error messages
 */

struct ns_errp {
	u_short		ns_err_num;		/* Error Number */
	u_short		ns_err_param;		/* Error Parameter */
	struct idp	ns_err_idp;		/* Initial segment of offending
						   packet */
	u_char		ns_err_lev2[12];	/* at least this much higher
						   level protocol */
};
struct  ns_epidp {
	struct idp ns_ep_idp;
	struct ns_errp ns_ep_errp;
};

#define	NS_ERR_UNSPEC	0	/* Unspecified Error detected at dest. */
#define	NS_ERR_BADSUM	1	/* Bad Checksum detected at dest */
#define	NS_ERR_NOSOCK	2	/* Specified socket does not exist at dest*/
#define	NS_ERR_FULLUP	3	/* Dest. refuses packet due to resource lim.*/
#define	NS_ERR_UNSPEC_T	0x200	/* Unspec. Error occured before reaching dest*/
#define	NS_ERR_BADSUM_T	0x201	/* Bad Checksum detected in transit */
#define	NS_ERR_UNREACH_HOST	0x202	/* Dest cannot be reached from here*/
#define	NS_ERR_TOO_OLD	0x203	/* Packet x'd 15 routers without delivery*/
#define	NS_ERR_TOO_BIG	0x204	/* Packet too large to be forwarded through
				   some intermediate gateway.  The error
				   parameter field contains the max packet
				   size that can be accommodated */
#define NS_ERR_MAX 20

/*
 * Variables related to this implementation
 * of the network systems error message protocol.
 */
struct	ns_errstat {
/* statistics related to ns_err packets generated */
	int	ns_es_error;		/* # of calls to ns_error */
	int	ns_es_oldshort;		/* no error 'cuz old ip too short */
	int	ns_es_oldns_err;	/* no error 'cuz old was ns_err */
	int	ns_es_outhist[NS_ERR_MAX];
/* statistics related to input messages processed */
	int	ns_es_badcode;		/* ns_err_code out of range */
	int	ns_es_tooshort;		/* packet < IDP_MINLEN */
	int	ns_es_checksum;		/* bad checksum */
	int	ns_es_badlen;		/* calculated bound mismatch */
	int	ns_es_reflect;		/* number of responses */
	int	ns_es_inhist[NS_ERR_MAX];
	u_short	ns_es_codes[NS_ERR_MAX];/* which error code for outhist
					   since we might not know all */
};

#ifdef KERNEL
struct	ns_errstat ns_errstat;
#endif
#ifdef __cplusplus
}
#endif
