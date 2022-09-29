/*
** From <rpc/rpc_msg.h>
*/
#define CALL	0
#define REPLY	1

#define SUCCESS		0
#define PROG_UNAVAIL	1
#define PROG_MISMATCH	2
#define PROC_UNAVAIL	3
#define GARBAGE_ARGS	4
#define SYSTEM_ERROR	5

#define RPC_MISMATCH	0

#define MSG_ACCEPTED	0
#define MSG_DENIED	1

#define RPC_MSG_VERSION	((u_long) 2)

/*
** From <sys/fs/nfs.h>
*/
#define NFS_MAXDATA     8192

#define NFS_UID_NOBODY  -2
#define NFS_GID_NOBODY  -2

#define NFS_OK		0
#define NFSERR_PERM	1
#define NFSERR_NOENT	2
#define NFSERR_IO	5
#define NFSERR_NXIO	6
#define NFSERR_ACCES	13
#define NFSERR_EXIST	17
#define NFSERR_NODEV	19
#define NFSERR_NOTDIR	20
#define NFSERR_ISDIR	21
#define NFSERR_FBIG	27
#define NFSERR_NOSPC	28
#define NFSERR_ROFS	30
#define NFSERR_NAMETOOLONG	63
#define NFSERR_NOTEMPTY	66
#define NFSERR_DQUOT	69
#define NFSERR_STALE	70
#define NFSERR_REMOTE	71

#define NFS_FHSIZE	32

#define	RFS_NULL	0
#define	RFS_GETATTR	1
#define	RFS_SETATTR	2
#define	RFS_ROOT	3
#define	RFS_LOOKUP	4
#define	RFS_READLINK	5
#define	RFS_READ	6
#define	RFS_WRITECACHE	7
#define	RFS_WRITE	8
#define	RFS_CREATE	9
#define	RFS_REMOVE	10
#define	RFS_RENAME	11
#define	RFS_LINK	12
#define	RFS_SYMLINK	13
#define	RFS_MKDIR	14
#define	RFS_RMDIR	15
#define	RFS_READDIR	16
#define	RFS_STATFS	17
#define	RFS_NPROC	18

#define	NFS_PROGRAM	((u_long)100003)
#define	NFS_VERSION	((u_long)2)

/*
** From <rpc/auth.h>
*/
#define AUTH_NONE		0
#define AUTH_UNIX		1
#define AUTH_SHORT		2
#define AUTH_DES		3

#define AUTH_OK			0
#define AUTH_BADCRED		1
#define AUTH_REJECTEDCRED	2
#define AUTH_BADVERF		3
#define	AUTH_REJECTEDVERF	4
#define	AUTH_TOOWEAK		5
#define AUTH_INVALIDRESP	6
#define AUTH_FAILED		7

/*
** From <rpc/auth_unix.h>
*/
#define NGRPS 16

/*
** From <rpc/clnt.h>
*/
#define UDPMSGSIZE   8800


#define WORDS(l)	(((l) % 4) != 0) ? (((l) + (4 - ((l) % 4)))/sizeof(u_int32_t)) : ((l)/sizeof(u_int32_t))

/*
** The filehandle for the name server is a list of indexes through the
** structure tree.  This may change when nsswitch.conf is reread so
** we pass a version on the directory header as well.
*/
typedef struct {
	u_int32_t	f_version;			/* hdr->version */
	u_int16_t	f_path[NFS_FHSIZE / 2 - 1];	/* index path */
} fhandle_t;
