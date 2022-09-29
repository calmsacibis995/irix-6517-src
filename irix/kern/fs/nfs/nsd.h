#ifndef __NSD_H__
#define __NSD_H__

#define NSDPROGRAM	((u_long) 391064)
#define NSDVERSION	1

#define NSDPROC1_NULL	0
#define NSDPROC1_CLOSE	1

int nsd_rfscall(struct mntinfo *, int, xdrproc_t, caddr_t, xdrproc_t, caddr_t,
    struct cred *);
int xdr_nfs_nsdfh(XDR *, fhandle_t *);

#endif /* not __NSD_H__ */
