#include <stdio.h>
#include <string.h>
#include <rpc/rpc.h>
#include "sgmstructs.h"
#include "sgmrpc.h"


/* Translates sgmclnt_t structure to external/internal representation */

int xdr_sgmclnt (XDR *xdrs, sgmclnt_t *clientdata)
{
    if ( !xdr_uint32(xdrs, &clientdata->flag) ) return(0);
    if ( !xdr_uint32(xdrs, &clientdata->datalen) ) return(0);
    if ( !xdr_bytes(xdrs, (char **) &clientdata->dataptr, 
		    &clientdata->datalen, clientdata->datalen+MAXBUFSIZE) ) 
	return(0);

    return(1);
}

/* Translates sgmclntxfr_t structure to external/internal representation */

int xdr_client_xfr (XDR *xdrs, sgmclntxfr_t *clientdata)
{

    if ( !xdr_uint32(xdrs, &clientdata->bytestotransfer) ) return(0);
    if ( !xdr_uint32(xdrs, &clientdata->fileoffset) ) return(0);
    if ( !xdr_uint32(xdrs, &clientdata->filenamelen) ) return(0);
    if ( !xdr_bytes(xdrs, &clientdata->filename, &clientdata->filenamelen, 
		     MAXBUFSIZE) ) return(0);
    return(1);
}

/* Translates sgmsrvr_t    structure to external/internal representation */

int xdr_sgmsrvr(XDR *xdrs, sgmsrvr_t *serverdata)
{

    if ( !xdr_uint32(xdrs, &serverdata->flag) ) return(0);
    if ( !xdr_uint32(xdrs, &serverdata->errCode) ) return(0);
    if ( !xdr_uint32(xdrs, &serverdata->bytes1) ) return(0);
    if ( !xdr_uint32(xdrs, &serverdata->bytes2) ) return(0);
    if ( !xdr_uint32(xdrs, &serverdata->filenamelen) ) return(0);
    if ( !xdr_bytes(xdrs, &serverdata->filename, &serverdata->filenamelen, 
		     MAXBUFSIZE) ) return(0);
    if ( !xdr_bytes(xdrs, &serverdata->data, 
		     &serverdata->bytes1, serverdata->bytes1+MAXBUFSIZE) ) return(0);

    return(1);
}
