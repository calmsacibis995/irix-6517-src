/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.8 87/08/12 
 */

#ifdef __STDC__
	#pragma weak ypprot_err = _ypprot_err
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include "yp_prot.h"
#include "ypclnt.h"

/*
 * Maps a NIS protocol error code (as defined in
 * yp_prot.h) to a NIS client interface error code (as defined in
 * ypclnt.h).
 */
int
ypprot_err(unsigned int yp_protocol_error)
{
	int reason;

	switch ((long)yp_protocol_error) {
	case YP_TRUE: 
		reason = 0;
		break;
 	case YP_NOMORE: 
		reason = YPERR_NOMORE;
		break;
 	case YP_NOMAP: 
		reason = YPERR_MAP;
		break;
 	case YP_NODOM: 
		reason = YPERR_DOMAIN;
		break;
 	case YP_NOKEY: 
		reason = YPERR_KEY;
		break;
 	case YP_BADARGS:
		reason = YPERR_BADARGS;
		break;
 	case YP_BADDB:
		reason = YPERR_BADDB;
		break;
 	case YP_VERS:
		reason = YPERR_VERS;
		break;
	default:
		reason = YPERR_YPERR;
		break;
	}
	
  	return(reason);
}
