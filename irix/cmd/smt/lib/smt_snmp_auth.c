/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <sys/types.h>
#include <netinet/in.h>

#ifndef NULL
#define NULL 0
#endif

#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_auth.h>

char *
snmp_auth_parse(char	*data,
		int	*length,
		char	*sid,
		int	*slen,
		long	*version)
{
	char    type;

	data = asn_parse_header(data, length, &type);
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "bad header");
		return(NULL);
	}
	if (type != (ASN_SEQUENCE | ASN_CONSTRUCTOR)) {
		sm_log(LOG_ERR, 0, "wrong auth header type");
		return(NULL);
	}
	data = asn_parse_int(data, length, &type, version, sizeof(*version));
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "bad parse of version");
		return(NULL);
	}
	data = asn_parse_string(data, length, &type, sid, slen);
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "bad parse of community");
		return(NULL);
	}
	sid[*slen] = '\0';
	return(data);
}

char *
snmp_auth_build(char	*data,
		int	*length,
		char	*sid,
		int	*slen,
		long	*version,
		int	messagelen)
{
	data = asn_build_header(data, length, ASN_SEQUENCE | ASN_CONSTRUCTOR,
				messagelen + *slen + 5);
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "buildheader");
		return(NULL);
	}
	data = asn_build_int(data, length,
			     ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			     (long *)version, sizeof(*version));
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "buildint");
		return(NULL);
	}
	data = asn_build_string(data, length,
				ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OCTET_STR,
				sid, *slen);
	if (data == NULL) {
		sm_log(LOG_ERR, 0, "buildstring");
		return(NULL);
	}
	return(data);
}
