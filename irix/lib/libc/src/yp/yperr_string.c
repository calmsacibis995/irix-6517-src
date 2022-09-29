/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.8 87/08/12
 */

#ifdef __STDC__
	#pragma weak yperr_string = _yperr_string
#endif
#include "synonyms.h"

struct dom_binding;			/* for prototyping */
#include <rpcsvc/ypclnt.h>

/*
 * This returns a pointer to an error message string appropriate to an input
 * NIS error code.  An input value of zero will return a success message.
 * In all cases, the message string will start with a lower case chararacter,
 * and will be terminated neither by a period (".") nor a newline.
 */
char *
yperr_string(int code)
{
	char *pmesg;
	
	switch (code) {

	case 0:  {
		pmesg = "NIS operation succeeded";
		break;
	}
		
	case YPERR_BADARGS:  {
		pmesg = "args to NIS function are bad";
		break;
	}
	
	case YPERR_RPC:  {
		pmesg = "RPC failure on NIS operation";
		break;
	}
	
	case YPERR_DOMAIN:  {
		pmesg = "can't bind to a server which serves domain";
		break;
	}
	
	case YPERR_MAP:  {
		pmesg = "no such map in server's domain";
		break;
	}
		
	case YPERR_KEY:  {
		pmesg = "no such key in map";
		break;
	}
	
	case YPERR_YPERR:  {
		pmesg = "internal NIS server or client error";
		break;
	}
	
	case YPERR_RESRC:  {
		pmesg = "local resource allocation failure";
		break;
	}
	
	case YPERR_NOMORE:  {
		pmesg = "no more records in map database";
		break;
	}
	
	case YPERR_PMAP:  {
		pmesg = "can't communicate with portmapper";
		break;
		}
		
	case YPERR_YPBIND:  {
		pmesg = "can't communicate with ypbind";
		break;
		}
		
	case YPERR_YPSERV:  {
		pmesg = "can't communicate with ypserv";
		break;
		}
		
	case YPERR_NODOM:  {
		pmesg = "local domain name not set";
		break;
	}

	case YPERR_BADDB:  {
		pmesg = "NIS map data base is bad";
		break;
	}

	case YPERR_VERS:  {
		pmesg = "NIS client/server version mismatch";
		break;
	}

	case YPERR_ACCESS: {
		pmesg = "permission denied";
		break;
	}

	case YPERR_BUSY: {
		pmesg = "database is busy";
		break;
	}
		
	default:  {
		pmesg = "unknown NIS client error code";
		break;
	}
	
	}

	return(pmesg);
}
