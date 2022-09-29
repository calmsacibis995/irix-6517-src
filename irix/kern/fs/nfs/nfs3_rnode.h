/*      @(#)rnode.h     1.4 88/07/15 NFSSRC4.0 from 1.21 88/02/08 SMI      */
/*      Copyright (C) 1988, Sun Microsystems Inc. */
/* $Revision: 1.8 $ */

#ifndef __NFS3_RNODE_H__
#define __NFS3_RNODE_H__

typedef struct access_cache {
	uint32 known;
	uint32 allowed;
	struct cred *cred;
	struct access_cache *next;
} access_cache;

typedef struct rddir_cache {
	off_t nfs3_cookie;	/* cookie used to find this cache entry */
	off_t nfs3_ncookie;	/* cookie used to find the next cache entry */
	char *entries;		/* buffer containing dirent entries */
	int eof;		/* EOF reached after this request */
	int entlen;		/* size of dirent entries in buf */
	int buflen;		/* size of the buffer used to store entries */
	int flags;		/* control flags, see below */
	sv_t cv;		/* cv for blocking */
	int error;		/* error from RPC operation */
	int ref;		/* count of references to the entry */
	struct rddir_cache *next;	/* ptr to next entry in cache */
} rddir_cache;

#define	RDDIR		0x1	/* readdir operation in progress */
#define	RDDIRWAIT	0x2	/* waiting on readdir in progress */
#define	RDDIRREQ	0x4	/* a new readdir is required */

#define	RDONTWRITE	0x0400	/* don't even attempt to write */
#define	RMODINPROGRESS	0x8	/* page modification happening */

#define	BHVTOR(bdp)	(bhvtor((bdp)))
#define bhvtofh3(bdp)      (rtofh3(bhvtor(bdp)))
#define BHVTOFH3(bdp)      (rtofh3(bhvtor(bdp)))
#define rtofh3(rp)      ((nfs_fh3 *)(&(rp)->r_ufh.r_fh3))

#endif /* __NFS3_RNODE_H__ */
