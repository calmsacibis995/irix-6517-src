/* posix tar definitions
 */
#ident "$Revision: 1.2 $"

#define TARBSIZE	512
#define NAMESIZE	100
#define PREFSIZE	155

struct posix_tar_header {
	char pt_name[NAMESIZE];
	char pt_mode[8];
	char pt_uid[8];
	char pt_gid[8];
	char pt_size[12];
	char pt_mtime[12];
	char pt_chksum[8];
	char pt_typeflag;
	char pt_linkname[NAMESIZE];
	char pt_magic[6];
	char pt_version[2];
	char pt_uname[32];
	char pt_gname[32];
	char pt_devmajor[8];
	char pt_devminor[8];
	char pt_prefix[PREFSIZE];
};

/* for standard tar */
#define THSZ		269

typedef struct posix_tar_header pth_t;
#define PTHSZ sizeof(pth_t)

#define TMAGIC		"ustar"		/* ustar and a null */
#define TMAGLEN		6
#define TVERSION	"00"		/* 00 and no null */
#define TVERSLEN	2

/* Values used in typeflag field */
#define REGTYPE		'0'		/* Regular file */
#define AREGTYPE	'\0'		/* Regular file */
#define LNKTYPE		'1'		/* Link */
#define SYMTYPE		'2'		/* Reserved */
#define CHRTYPE		'3'		/* Character special */
#define BLKTYPE		'4'		/* Block special */
#define DIRTYPE		'5'		/* Directory */
#define FIFOTYPE	'6'		/* FIFO special */
#define CONTTYPE	'7'		/* Reserved */

/* Bits used in the mode field - values in octal */
#define TSUID		04000		/* Set UID on execution */
#define TSGID		02000		/* Set GID on execution */
#define TSVTX		01000		/* Reserved */
					/* File permissions */
#define TUREAD		00400		/* Read by owner */
#define TUWRITE		00200		/* Write by owner */
#define TUEXEC		00100		/* Execute/Search by owner */
#define TGREAD		00040		/* Read by group */
#define TGWRITE		00020		/* Write by group */
#define TGEXEC		00010		/* Execute/Search by group */
#define TOREAD		00004		/* Read by other */
#define TOWRITE		00002		/* Write by other */
#define TOEXEC		00001		/* Execute/Search by other */
