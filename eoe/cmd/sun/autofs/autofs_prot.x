/*
 *	autofs_prot.x
 *
 *	Copyright (c) 1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

/*
 * Autofs protocol
 */

%#define xdr_dev_t xdr_u_long

const A_MAXNAME	= 255;
const A_MAXOPTS	= 255;
const A_MAXPATH	= 1024;

struct mntrequest {
	string	name<A_MAXNAME>;	/* name to be looked up */
	string	map<A_MAXNAME>;		/* map to use */
	string  opts<A_MAXOPTS>;	/* default options */
	string  path<A_MAXPATH>;	/* mountpoint to use */
};


struct mntres {
	int status;	/* 0=OK, otherwise an errno from <sys/errno.h> */
};

struct umntrequest {
	u_int	isdirect;		/* direct mount */
	dev_t	devid;			/* device id */
	dev_t	rdevid;			/* unique device id for lofs */
	struct umntrequest *next;	/* next unmount */
};

struct umntres {
	int status;
};

program AUTOFS_PROG {
	version AUTOFS_VERS {
		/*
		 * Do mount(s)
		 */
		mntres
		AUTOFS_MOUNT(mntrequest) = 1;

		/*
		 * Do unmount(s)
		 */
		umntres
		AUTOFS_UNMOUNT(umntrequest) = 2;
	} = 1;
} = 100099;
