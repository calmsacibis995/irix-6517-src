/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.11 $
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
#include <smt_snmp_auth.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>
#include <smt_mib.h>


char *
snmp_parse_var_op(
	register char	*data,  	/* IN - ptr to the start of object */
	oid	*var_name,		/* OUT - object id of variable */
	int	*var_name_len,		/* OUT - length of variable name */
	char  	*var_val_type,		/* OUT - int or octet string, 1 byte */
	int	*var_val_len,		/* OUT - length of variable */
	char  	**var_val,		/* OUT - ASN1 value of var pointer */
	int	*listlength)		/* IN/OUT-valid bytes in var_op_list */
{
	char	var_op_type;
	int	var_op_len = *listlength;
	char	*var_op_start = data;

	data = asn_parse_header(data, &var_op_len, &var_op_type);
	if (data == NULL){
		SNMPDEBUG((LOG_DBGSNMP, 0, "var_op: bad header"));
		return(NULL);
	}
	if (var_op_type != (ASN_SEQUENCE | ASN_CONSTRUCTOR)) {
		SNMPDEBUG((LOG_DBGSNMP, 0,
				"var_op: bad op_type1 = %x", var_op_type));
		return(NULL);
	}
	data = asn_parse_objid(data, &var_op_len,
				&var_op_type, var_name, var_name_len);
	if (data == NULL) {
		SNMPDEBUG((LOG_DBGSNMP, 0, "var_op: bad objids"));
		return(NULL);
	}
	if (var_op_type!=(ASN_UNIVERSAL|ASN_PRIMITIVE|ASN_OBJECT_ID)) {
		SNMPDEBUG((LOG_DBGSNMP, 0,
			"var_op: bad op_type2 = %x", var_op_type));
		return(NULL);
	}

	*var_val = data;	/* save pointer to this object */
	/* find out what type of object this is */
	data = asn_parse_header(data, &var_op_len, var_val_type);
	if (data == NULL) {
		SNMPDEBUG((LOG_DBGSNMP, 0, "var_op: bad obj type"));
		return(NULL);
	}
	*var_val_len = var_op_len;
	data += var_op_len;
	*listlength -= (int)(data - var_op_start);
	return(data);
}

char *
snmp_build_var_op(
	register char *data,		/* IN - ptr to the outbuf */
	oid		*var_name,	/* IN - object id of variable */
	int		*var_name_len,	/* IN - length of object id */
	char		var_val_type,	/* IN - type of variable */
	int		var_val_len,	/* IN - length of variable */
	char		*var_val,	/* IN - value of variable */
	register int *listlength)   	/* IN/OUT - num of bytes in outbuf */
{
	char  buf[PACKET_LENGTH];
	int	len;
	char	*cp;

	len = sizeof(buf);
	cp = asn_build_objid(buf, &len,
	    		ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OBJECT_ID,
	    		var_name, *var_name_len);
	if (cp == NULL) {
		ERROR("");
		return(NULL);
	}

	switch(var_val_type) {
		case ASN_INTEGER:
		case GAUGE:
		case COUNTER:
		case TIMETICKS:
	    		cp = asn_build_int(cp, &len,
				var_val_type, (long *)var_val, var_val_len);
	    		break;
		case ASN_OCTET_STR:
		case IPADDRESS:
		case OPAQUE:
	    		cp = asn_build_string(cp, &len,
					var_val_type, var_val, var_val_len);
	    		break;
		case ASN_OBJECT_ID:
	    		cp = asn_build_objid(cp, &len, var_val_type,
		    		(oid *)var_val, var_val_len / sizeof(oid));
	    		break;
		case ASN_NULL:
	    		cp = asn_build_null(cp, &len, var_val_type);
	    		break;
		default:
	    		ERROR("wrong type");
	    		return(NULL);
	}
	if (cp == NULL) {
		ERROR("");
		return(NULL);
	}

	len = cp - buf;
	data = asn_build_header(data, listlength,
				ASN_SEQUENCE | ASN_CONSTRUCTOR, len);
	if (data == NULL) {
		ERROR("");
		return(NULL);
	}
	if (*listlength < len) {
		ERROR("");
		return(NULL);
	}
	bcopy(buf, data, len);
	data += len;
	*listlength -= len;
	return(data);
}


/*
 *
 */
int
snmp_build_trap(register char *out_data,
		int	length,
		oid	*sysOid,
		int	sysOidLen,
		u_long	myAddr,
		int	trapType,
		int	specificType,
		u_long	time,
		oid	*varName,
		int	varNameLen,
		char	varType,
		int	varLen,
		char	*varVal)
{
    long    version = SNMP_VERSION_1;
    int	    sidLen = strlen("public");
    int	    dummyLen;
    char  *out_auth, *out_header, *out_pdu, *out_varHeader, *out_varlist, *out_end;


    out_auth = out_data;
    out_header = snmp_auth_build(out_data, &length, "public", &sidLen, &version, 90);
    if (out_header == NULL){
	ERROR("auth build failed");
	return 0;
    }
    out_pdu = asn_build_header(out_header, &length, TRP_REQ_MSG, 90);
    if (out_pdu == NULL){
	ERROR("header build failed");
	return 0;
    }
    out_data = asn_build_objid(out_pdu, &length,
			       ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OBJECT_ID,
		(oid *)sysOid, sysOidLen);
    if (out_data == NULL){
	ERROR("build enterprise failed");
	return 0;
    }
    out_data = asn_build_string(out_data, &length,
				ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OCTET_STR,
				(char*)&myAddr, sizeof(myAddr));
    if (out_data == NULL){
	ERROR("build agent_addr failed");
	return 0;
    }
    out_data = asn_build_int(out_data, &length,
			     ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			     (long *)&trapType, sizeof(trapType));
    if (out_data == NULL){
	ERROR("build trap_type failed");
	return 0;
    }
    out_data = asn_build_int(out_data, &length,
			     ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER,
			     (long *)&specificType, sizeof(specificType));
    if (out_data == NULL){
	ERROR("build specificType failed");
	return 0;
    }
    out_varHeader = asn_build_int(out_data, &length,
				  TIMETICKS,
				  (long *)&time, sizeof(time));
    if (out_varHeader == NULL){
	ERROR("build timestampfailed");
	return 0;
    }
    out_varlist = asn_build_header(out_varHeader,  &length,
				   ASN_SEQUENCE | ASN_CONSTRUCTOR, 90);
    out_end = snmp_build_var_op(out_varlist, varName, &varNameLen, varType, varLen, varVal, &length);
    if (out_end == NULL){
	ERROR("build varop failed");
	return 0;
    }
    /* Now rebuild header with the actual lengths */
    dummyLen = out_end - out_varlist;
    if (asn_build_header(out_varHeader, &dummyLen,
		 ASN_SEQUENCE | ASN_CONSTRUCTOR, dummyLen) != out_varlist)
	return 0;
    dummyLen = out_end - out_pdu;
    if (asn_build_header(out_header, &dummyLen, TRP_REQ_MSG, dummyLen) != out_pdu)
	return 0;
    dummyLen = out_end - out_header;
    if (snmp_auth_build(out_auth, &dummyLen, "public", &sidLen,
			&version, dummyLen) != out_header)
	return 0;
    return out_end - out_auth;
}
