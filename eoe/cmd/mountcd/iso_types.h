/*
 *	iso_types.h
 *
 *	Description:
 *		types for iso 9600 file system
 *
 *	History:
 *      rogerc      12/31/90    Created
 *      rogerc      04/02/91    Adding support for High Sierra
 */

#include <rpc/types.h>

typedef struct fhandle fhandle_t;

#include <sys/fs/nfs_clnt.h>

#define	FHPADSIZE	(NFS_FHSIZE - sizeof(struct loopfid) - 4)

/*
 * This lets this compile under cypress as well as sherwood
 */
#ifndef NFSMNT_LOOPBACK
#define	NFSMNT_LOOPBACK	0x4000	/* local mount via loopback interface */
struct loopfid {
        u_short lfid_len;       /* bytes in fid, excluding lfid_len */
        u_short lfid_pad;       /* explicit struct padding */
        u_long  lfid_fno;       /* file number, from nodeid attribute */
};
#endif

struct fhandle {
    dev_t	fh_dev;				/* device number */
    struct loopfid fh_fid;
    char fh_pad[FHPADSIZE];
};

#define fh_fno fh_fid.lfid_fno
#define fh_len fh_fid.lfid_len

/*
 *	Date and Time format
 */
struct date_time {
	u_char		year[4];
	u_char		month[2];
	u_char		day[2];
	u_char		hour[2];
	u_char		minute[2];
	u_char		second[2];
	u_char		centiseconds[2];
	signed char		greenwich;
};

typedef struct hsdate_time {
	u_char		year[4];
	u_char		month[2];
	u_char		day[2];
	u_char		hour[2];
	u_char		minute[2];
	u_char		second[2];
	u_char		centiseconds[2];
} hsdate_time_t;

/*
 *	Recording Date and Time Format
 */
struct rec_date_time {
	u_char		year;
	u_char		month;
	u_char		day;
	u_char		hour;
	u_char		minute;
	u_char		second;
	signed char		greenwich;
};

struct hsrec_date_time {
	u_char		year;
	u_char		month;
	u_char		day;
	u_char		hour;
	u_char		minute;
	u_char		second;
};

/*
 *	Volume Partition Descriptor
 */
struct vol_part_desc {
	u_char				type;
	char				id[5];
	u_char				version;
	u_char				unused;
	char				sys_id[32];
	char				vol_id[32];
	u_char				vol_part_loc[8];
	u_long				vol_space_size_lsb;
	u_long				vol_space_size_msb;
	u_char				system_use[1960];
};

/*
 *	Directory Record - fixed length part
 */
typedef struct dir_rec {
	u_char					len_dir;
	u_char					xattr_len;
	u_char					ext_loc_lsb[4];
	u_char					ext_loc_msb[4];
	u_char					data_len_lsb[4];
	u_char					data_len_msb[4];
	struct rec_date_time	recording;
	u_char					flags;
#define	FFLAG_EXIST		0x01
#define FFLAG_DIRECTORY	0x02
#define FFLAG_ASSOC		0x04
#define FFLAG_RECORD	0x08
#define FFLAG_PROTECT	0x10
#define FFLAG_MULTI		0x80
	u_char					fu_size;
	u_char					int_gap;
	u_char					vol_seq_num[4];
	u_char					len_fi;
	char					file_id[1];
} dir_t;

/*
 *	Path Table Record
 */
struct path_table_rec {
	u_char		len_di;
	u_char		len_xattr;
	u_long		loc_extent;
	u_short		num_parent_dir;
	char		dir_id[1];
};

/*
 *	Extended Attribute Record
 */
typedef struct xattr {
	u_long				owner_id;
	u_long				group_id;
	u_short				perm;	/*	Note: 0 means permission granted,
									1 means not */
#define PERM_SYSREAD	0x0001
#define PERM_SYSEXE		0x0004
#define PERM_OREAD		0x0010
#define PERM_OEXE		0x0040
#define PERM_GREAD		0x0100
#define PERM_GEXE		0x0400
#define PERM_WREAD		0x1000
#define PERM_WEXE		0x4000
	struct date_time	create;
	struct date_time	modify;
	struct date_time	expire;
	struct date_time	effect;
	u_char				rec_format;
	u_char				rec_attrib;
	u_long				rec_len;
	char				sys_id[22];
	u_char				sys_use[4];
	u_char				xattr_rec_vers;
	u_char				len_esc;
	u_char				reserved[64];
	u_long				len_au;
	u_char				au[1];
} xattr_t;

typedef struct hsxattr {
	u_long    		oid;
	u_long    		pid;
	u_short    		perm;
	hsdate_time_t	create;
	hsdate_time_t	modify;
	hsdate_time_t	expire;
	hsdate_time_t	effect;
	u_char			rec_format;
	u_char			rec_attrib;
	u_long			rec_len;
} hsxattr_t;

/*
 *	Primary volume descriptor
 */
struct p_vol_desc {
	u_char				type;
	char				id[5];
	u_char				version;
	u_char				unused1;
	char				sys_id[32];
	char				vol_id[32];
	u_char				unused2[8];
	u_long				vol_space_size_lsb;
	u_long				vol_space_size_msb;
	u_char				unused3[32];
	u_long				vol_set_size;
	u_long				vol_seq_num;
	u_short                         blksize_lsb;
	u_short                         blksize_msb;
	u_long				path_tbl_size_lsb;
	u_long				path_tbl_size_msb;
	u_long				lpath_tbl_loc;
	u_long				opt_lpath_tbl_loc;
	u_long				mpath_tbl_loc;
	u_long				opt_mpath_tbl_loc;
	struct dir_rec		root;
	char				vol_set_id[128];
	char				pub_id[128];
	char				data_prep_id[128];
	char				app_id[128];
	char				copyright_fid[37];
	char				abstract_fid[37];
	char				biblio_fid[37];
	struct date_time	vol_create;
	struct date_time	vol_modify;
	struct date_time	vol_expire;
	struct date_time	vol_effect;
	u_char				fs_version;
	u_char				reserved1;
	u_char				app_use[512];
	u_char				reserved2[653];
};

/*
 *	Supplementary Volume Descriptor
 */
struct s_vol_desc {
	u_char				type;
	char				id[5];
	u_char				version;
	u_char				flags;
	char				sys_id[32];
	char				vol_id[32];
	u_char				unused1[8];
	u_long				vol_space_size_lsb;
	u_long				vol_space_size_msb;
	u_char				esc_seq[32];
	u_long				vol_set_size;
	u_long				vol_seq_num;
	u_long				log_blk_size;
	u_long				path_tbl_size_lsb;
	u_long				path_tbl_size_msb;
	u_long				lpath_tbl_loc;
	u_long				opt_lpath_tbl_loc;
	u_long				mpath_tbl_loc;
	u_long				opt_mpath_tbl_loc;
	struct dir_rec		root;
	char				vol_set_id[27];
	char				pub_id[27];
	char				data_prep_id[27];
	char				app_id[27];
	char				copyright_fid[36];
	char				abstract_fid[36];
	char				biblio_fid[36];
	struct date_time	vol_create;
	struct date_time	vol_modify;
	struct date_time	vol_expire;
	struct date_time	vol_effect;
	u_char				fs_version;
	u_char				reserved1;
	u_char				app_use[511];
	u_char				reserved2[652];
};

/*
 * High Sierra directory record
 */

typedef struct hsdir {
	u_char					len_dir;
	u_char					xattr_len;
	u_char					ext_loc_lsb[4];
	u_char					ext_loc_msb[4];
	u_char					data_len_lsb[4];
	u_char					data_len_msb[4];
	struct hsrec_date_time	recording;
	u_char					flags;
	u_char					res;
	u_char					fu_size;
	u_char					int_gap;
	u_char					vol_seq_num[4];
	u_char					len_fi;
	char					file_id[1];
} hsdir_t;

/*
 * High Sierra volume descriptor
 */

typedef struct hsvol {
	u_char	lbnlsb[4];
	u_char	lbnmsb[4];
	u_char	type[1];
	u_char	ident[5];
	u_char	version[1];
	u_char	res[1];
	u_char	sys_id[32];
	u_char	vol_id[32];
	u_char	res2[8];
	u_char	vol_space_size_lsb[4];
	u_char	vol_space_size_msb[4];
	u_char	res3[32];
	u_char	vol_set_size[4];
	u_char	vol_set_seq_num[4];
	u_char	blksize_lsb[2];
	u_char	blksize_msb[2];
	u_char	ptsize[8];
	u_char	ptloc[32];
	hsdir_t	root;
} hsvol_t;


/*
 * Level 3 (Multi-Extent) support structures
 */
typedef struct one_extent {
    int			oe_fileid;
    int			oe_start_blk;
    long		oe_start_byte;
    long		oe_length;
    struct one_extent	*oe_next;
} one_xtnt_t;

typedef struct multi_extent {
    int			me_fileid;
    long		me_length;
    short		me_num_xtnts;
    one_xtnt_t		*me_xtnts_head;
    one_xtnt_t		*me_xtnts_tail;
    struct multi_extent	*me_next;
} multi_xtnt_t;

