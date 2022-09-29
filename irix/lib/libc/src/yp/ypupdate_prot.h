/* @(#)ypupdate_prot.h	1.2 88/03/31 4.0NFSSRC; from 1.4 88/02/08 Copyr 1986, Sun Micro */

/*
 * NIS update service protocol
 */
#define	YPU_PROG	100028
#define	YPU_VERS	1
#define	YPU_CHANGE	1
#define	YPU_INSERT	2
#define	YPU_DELETE	3
#define	YPU_STORE	4

#define MAXMAPNAMELEN	255
#define	MAXYPDATALEN	1023

struct yp_buf {
	int	yp_buf_len;	/* not to exceed MAXYPDATALEN */
	char	*yp_buf_val;
};

struct ypupdate_args {
	char   *mapname;	/* not to exceed MAXMAPNAMELEN */
	struct yp_buf key;
	struct yp_buf datum;
};

struct ypdelete_args {
	char   *mapname;
	struct yp_buf key;
};

int xdr_ypupdate_args();
int xdr_ypdelete_args();
