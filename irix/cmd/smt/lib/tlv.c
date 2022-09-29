/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_parse.h>
#include <smt_asn1.h>
#include <smt_snmp_api.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smtd_fs.h>
#include <tlv.h>

#ifndef NULL
#define NULL	0
#endif

/*
 * tlv_tl
 *
 *	parse: yank 2byte type into long
 *	       yank 2byte length into long.
 *
 *	build: put long type into 2byte
 *		put long length into 2byte.
 *
 *	Also, check the data length validity.
 */
int
tlv_parse_tl(
    	register char **data,		/* IN - ptr to object */
    	register u_long *length,	/* IN/OUT-num of valid bytes left */
    	register u_long *type,		/* IN - TLV type of object */
    	register u_long *len)		/* IN - length of object */
{
    	if (*length < TLV_HDRSIZE) {
		TLVDEBUG((LOG_DBGTLV, 0, "parse_tl: Bad Length"));
		return(-1);
	}
    
	*type = ntohs(((u_short *)(*data))[0]);
	*len  = ntohs(((u_short *)(*data))[1]);
    	
    	*length -= TLV_HDRSIZE;
	if (*length < *len) {
		TLVDEBUG((LOG_DBGTLV, 0, "parse_tl: underflow"));
		return(-1);
	}
	*data += TLV_HDRSIZE;
	return(0);
}
int
tlv_build_tl(
	register char **data,		/* IN - ptr to object */
	register u_long *length,	/* IN/OUT-num of valid bytes left */
	register u_long type,		/* IN - TLV type of object */
	register u_long len)		/* IN - length of object */
{
    	if (*length < TLV_HDRSIZE) {
		TLVDEBUG((LOG_DBGTLV, 0, "build_tl: Bad Length"));
		return(-1);
	}
    
	((u_short *)(*data))[0] = htons(type);
	((u_short *)(*data))[1] = htons(len);

    	*length -= TLV_HDRSIZE;
	if (*length < len) {
		TLVDEBUG((LOG_DBGTLV, 0, "build_tl: overflow"));
		return(-1);
	}
	*data += TLV_HDRSIZE;
	return(0);
}

/*
 *	tlv_char:
 *		parse: pull out 1byte into integer.
 *		build: put integer into 1bytes.
 */
int
tlv_parse_char(
    	register char	**data,		/* IN - ptr to object */
    	register u_long	*length,	/* IN/OUT -  valid bytes left */
    	register u_long	*val)		/* OUT - ptr to output buffer */
{
	if (*length < sizeof(char))
		return(-1);

	*val = (u_long)(**data);
	*length -= sizeof(char);
    	*data += sizeof(char);
	return(0);
}
int
tlv_build_char(
    	register char **data,		/* IN - ptr to start of output buf */
    	register u_long *length,	/* IN/OUT- valid bytes left */
    	register u_long	val)		/* IN - ptr to start of long integer */
{
	if (*length < sizeof(char))
		return(-1);

	**data = val;
    	*length -= sizeof(char);
    	*data += sizeof(char);
	return(0);
}

/*
 *	tlv_short:
 *		parse: pull out 2bytes into integer.
 *		build: put integer into 2bytes.
 */
int
tlv_parse_short(
    	register char	**data,		/* IN - ptr to object */
    	register u_long	*length,	/* IN/OUT -  valid bytes left */
    	register u_long	*val,		/* IN/OUT - ptr to output buffer */
    	register int	valsize)	/* OUT - asn type of object */
{
	if (*length < sizeof(short))
		return(-1);

	switch (valsize) {
		case 2: /* short */
    			*val = (u_long)ntohs(*(u_short *)(*data));
			break;
		case 1: /* 1byte padded char */
			*val = (u_long)((*data)[1]);
			break;
		default:
			return(-1);
	}
	*length -= sizeof(short);
    	*data += sizeof(short);
	return(0);
}
int
tlv_build_short(
    	register char **data,		/* IN - ptr to start of output buf */
    	register u_long *length,	/* IN/OUT- valid bytes left */
    	register u_long	val,		/* IN - ptr to start of long integer */
    	register int	valsize)   	/* IN - size of *intp */
{

	if (*length < sizeof(short))
		return(-1);

	switch (valsize) {
		case 2: /* short */
			*((u_short *)(*data)) = htons(val);
			break;
		case 1: /* 1byte padded char */
			(*data)[0] = 0;
			(*data)[1] = val;
			break;
		default:
			return(NULL);
	}
    	*length -= sizeof(short);
    	*data += sizeof(short);
	return(0);
}

/*
 *	tlv_int:
 *		parse: pull out 4bytes into integer.
 *		build: put integer to 4bytes.
 */
int
tlv_parse_int(
    	register char	**data,		/* IN - ptr to object */
    	register u_long	*length,	/* IN/OUT -  valid bytes left */
    	register u_long	*val,		/* IN/OUT - ptr to output buffer */
    	register int	valsize)	/* OUT - asn type of object */
{
	if (*length < sizeof(long))
		return(-1);

	switch (valsize) {
		case 4: /* Full long */
    			*val = (u_long)ntohl(*(int *)(*data));
			break;
		case 2: /* 2byte padded short */
    			*val = (u_long)ntohs(((u_short *)(*data))[1]);
			break;
		case 1: /* 3byte padded char */
			*val = (u_long)((*data)[3]);
			break;
		default:
			return(-1);
	}
	*length -= sizeof(long);
    	*data += sizeof(long);
	return(0);
}
int
tlv_build_int(
    	register char **data,		/* IN - ptr to start of output buf */
    	register u_long *length,	/* IN/OUT- valid bytes left */
    	register u_long	val,		/* IN - ptr to start of long integer */
    	register int	valsize)   	/* IN - size of *intp */
{

	if (*length < sizeof(long))
		return(-1);

	switch (valsize) {
		case 4: /* Full long */
			*((u_long *)(*data)) = htonl(val);
			break;
		case 2: /* 2byte padded short */
			((u_short *)(*data))[0] = 0;
			((u_short *)(*data))[1] = htons(val);
			break;
		case 1: /* 3byte padded char */
			(*data)[0] = 0;
			(*data)[1] = 0;
			(*data)[2] = 0;
			(*data)[3] = val;
			break;
		default:
			return(-1);
	}
    	*length -= sizeof(long);
    	*data += sizeof(long);
	return(0);
}


/*
 * tlv_string:
 *
 *	parse/build string.
 */
int
tlv_parse_string(
    	register char **data,		/* IN - ptr to object */
    	register u_long *length,	/* IN/OUT - valid bytes left */
    	register char *val,		/* IN/OUT - ptr output buffer */
    	register int	len,		/* IN/OUT - size of output buffer */
    	register int	align)		/* IN - string alignmant */
{
	register int	off;

	if (*length < ((len + (sizeof(int)-1)) & (sizeof(int)-1)))
		return(-1);

	if (align & TLV_PREALIGN) {
		off = sizeof(int) - (len % sizeof(int));
		*length -= off;
		*data += off;
	}

    	bcopy(*data, val, len);
    	*length -= len;
	*data += len;

	if (align & TLV_POSTALIGN) {
		off = sizeof(int) - (len % sizeof(int));
		*length -= off;
		*data += off;
	}

    	return(0);
}
int
tlv_build_string(
    	register char **data,		/* IN - ptr to object */
    	register u_long *length,	/* IN/OUT- valid bytes left */
    	register char *val,	    	/* IN - pointer input buffer */
    	register int	len,		/* IN - size of input buffer */
    	register int	align)		/* IN - string alignmant */
{
	register int	off;

	if (*length < ((len + (sizeof(int)-1)) & (sizeof(int)-1)))
		return(-1);

	if (align & TLV_PREALIGN) {
		off = sizeof(int) - (len % sizeof(int));
		bzero(*data, off);
		*length -= off;
		*data += off;
	}

    	bcopy(val, *data, len);
    	*length -= len;
	*data += len;

	if (align & TLV_POSTALIGN) {
		off = sizeof(int) - (len % sizeof(int));
		bzero(*data, off);
		*length -= off;
		*data += off;
	}

    	return(0);
}
