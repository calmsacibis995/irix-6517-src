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

#define NFS_FHSIZE		32
#define NFS3_FHSIZE		32
#define NFS3_COOKIEVERFSIZE	8
#define NFS3_CREATEVERFSIZE	8
#define NFS3_WRITEVERFSIZE	8

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
#define NFS3_VERSION	((u_long)3)

#define NFSPROC3_NULL		0
#define NFSPROC3_GETATTR	1
#define NFSPROC3_SETATTR	2
#define NFSPROC3_LOOKUP		3
#define NFSPROC3_ACCESS		4
#define	NFSPROC3_READLINK	5
#define	NFSPROC3_READ		6
#define	NFSPROC3_WRITE		7
#define	NFSPROC3_CREATE		8
#define	NFSPROC3_MKDIR		9
#define	NFSPROC3_SYMLINK	10
#define	NFSPROC3_MKNOD		11
#define	NFSPROC3_REMOVE		12
#define	NFSPROC3_RMDIR		13
#define	NFSPROC3_RENAME		14
#define	NFSPROC3_LINK		15
#define	NFSPROC3_READDIR	16
#define	NFSPROC3_READDIRPLUS	17
#define	NFSPROC3_FSSTAT		18
#define	NFSPROC3_FSINFO		19
#define	NFSPROC3_PATHCONF	20
#define	NFSPROC3_COMMIT		21

#define NFS3_OK			0
#define NFS3ERR_PERM		1
#define NFS3ERR_NOENT		2
#define NFS3ERR_IO		5
#define NFS3ERR_NXIO		6
#define NFS3ERR_ACCES		13
#define NFS3ERR_EXIST		17
#define NFS3ERR_XDEV		18
#define NFS3ERR_NODEV		19
#define NFS3ERR_NOTDIR		20
#define NFS3ERR_ISDIR		21
#define NFS3ERR_INVAL		22
#define NFS3ERR_FBIG		27
#define NFS3ERR_NOSPC		28
#define NFS3ERR_ROFS		30
#define NFS3ERR_MLINK		31
#define NFS3ERR_NAMETOOLONG	63
#define NFS3ERR_NOTEMPTY	66
#define NFS3ERR_DQUOT		69
#define NFS3ERR_STALE		70
#define NFS3ERR_REMOTE		71
#define NFS3ERR_BADHANDLE	10001
#define NFS3ERR_NOT_SYNC	10002
#define NFS3ERR_BAD_COOKIE	10003
#define NFS3ERR_NOTSUPP		10004
#define NFS3ERR_TOOSMALL	10005
#define NFS3ERR_SERVERFAULT	10006
#define NFS3ERR_BADTYPE		10007
#define NFS3ERR_JUKEBOX		10008

#define NF3NONE		0
#define NF3REG		1
#define NF3DIR		2
#define NF3BLK		3
#define NF3CHR		4
#define NF3LNK		5
#define NF3SOCK		6
#define NF3FIFO		7

#define FSF3_LINK		0x1
#define FSF3_SYMLINK		0x2
#define FSF_HOMOGENEOUS		0x8
#define FSF3_CANSETTIME		0x10

#define ACCESS3_READ	0x1
#define ACCESS3_LOOKUP	0x2
#define ACCESS3_MODIFY	0x4
#define ACCESS3_EXTEND	0x8
#define ACCESS3_DELETE	0x10
#define ACCESS3_EXECUTE	0x20

/*
** From <fs/xattr.h>
*/
#define XATTR_PROGRAM		391063
#define XATTR1_VERSION		0

#define XATTRPROC1_NULL		0
#define XATTRPROC1_GETXATTR	1
#define XATTRPROC1_SETXATTR	2
#define XATTRPROC1_RMXATTR	3
#define XATTRPROC1_LISTXATTR	4

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

#define WORDS(l)	((((l) % 4) != 0) ? (((l) + (4 - ((l) % 4)))/sizeof(uint32_t)) : ((l)/sizeof(uint32_t)))
#define BLOCKS(l, m)	((((l) % (m)) != 0) ? (((l) + ((m) - ((l) % (m))))/(m)) : ((l)/(m)))
#define FILEID(h)	(uint32_t)(h)[0]
#define PARENT(h)	(uint32_t)(h)[1]

/*
** From "nsd.h"
*/
#define NSDPROGRAM	((u_long) 391064)
#define NSDVERSION	1
#define NSDPROC1_NULL	0
#define NSDPROC1_CLOSE	1
