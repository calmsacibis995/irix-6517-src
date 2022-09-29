#include <stdio.h>
/*
 * define _KERNEL to get the kernel off_t size
 */
#define _KERNEL 1
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#undef _KERNEL
#include <cachefs/cachefs_fs.h>

char *Progname;

static char *vnode_types[] = {
	"VNON",
	"VREG",
	"VDIR",
	"VBLK",
	"VCHR",
	"VLNK",
	"VFIFO",
	"VXNAM",
	"VBAD",
	"VSOCK",
	0
};

extern int errno;

static void
print_fid( fid_t *fidp )
{
	int i;

	printf( "     fid_len: %d\n", (int)fidp->fid_len );
	printf( "    fid_data: " );
	for ( i = 0; (i < (int)fidp->fid_len) && (i < MAXFIDSZ); i++ ) {
		printf( "0x%2x ", fidp->fid_data[i] );
	}
	printf( "\n" );
}

static void
prtallocmap( allocmap_t *map, u_short entries )
{
	u_short i;

	printf( "\nAllocation map\n--------------\n\n" );
	printf( "slot   offset      size\n----   ------      ----\n" );
	for ( i = 0; i < entries; i++ ) {
		printf( "%4d 0x%x 0x%x\n", i, (int)map[i].am_start_off,
			(int)map[i].am_size );
	}
}

static char *
mdstate_to_str( u_short mdstate )
{
	static char str[49];

	str[0] = 0;
	if (mdstate & MD_MDVALID) {
		strcat( str, "MD_MDVALID " );
	}
	if (mdstate & MD_NOALLOCMAP) {
		strcat( str, "MD_NOALLOCMAP " );
	}
	if (mdstate & MD_POPULATED) {
		strcat( str, "MD_POPULATED " );
	}
	if (mdstate & MD_KEEP) {
		strcat( str, "MD_KEEP " );
	}
	if (mdstate & MD_INIT) {
		strcat( str, "MD_INIT " );
	}
	return(str);
}

static void
print_attrs( vattr_t *vap )
{
	printf( "       va_type: %s\n", vnode_types[vap->va_type] );
	printf( "       va_mode: 0x%x\n", vap->va_mode );
	printf( "        va_uid: %d\n", vap->va_uid );
	printf( "        va_gid: %d\n", vap->va_gid );
	printf( "       va_fsid: %d\n", vap->va_fsid );
	printf( "     va_nodeid: %d\n", vap->va_nodeid );
	printf( "      va_nlink: %d\n", vap->va_nlink );
	printf( "       va_size: %d\n", (int)vap->va_size );
	printf( "      va_atime: %d %d\n", (int)vap->va_atime.tv_sec,
		(int)vap->va_atime.tv_nsec );
	printf( "      va_mtime: %d %d\n", (int)vap->va_mtime.tv_sec,
		(int)vap->va_mtime.tv_nsec );
	printf( "      va_ctime: %d %d\n", (int)vap->va_ctime.tv_sec,
		(int)vap->va_ctime.tv_nsec );
	printf( "       va_rdev: %d\n", vap->va_rdev );
	printf( "    va_blksize: %d\n", vap->va_blksize );
	printf( "    va_nblocks: %d\n", vap->va_nblocks );
	printf( "      va_vcode: %d\n", vap->va_vcode );
}

static void
print_token( ctoken_t *tokenp )
{
	int i;

	for ( i = 0; i < MAXTOKENSIZE; tokenp++, i++ ) {
		printf( "0x%x ", (int)*tokenp );
	}
	printf( "\n" );
}

static void
print_fileheader( fileheader_t *fhp )
{
	int i;
	char namebuf[MAXPATHLEN];

	printf( "File header size is %d\n", sizeof(fileheader_t));
	printf( "Metadata size is %d\n", sizeof(cachefsmeta_t));
	printf( "Allocation map size is %d\n", sizeof(allocmap_t));
	printf( "The allocation map can contain at most %d entries\n",
		ALLOCMAP_SIZE);
	printf( "     md_state: %s (0x%x)\n",
		mdstate_to_str(fhp->fh_metadata.md_state),
		(int)fhp->fh_metadata.md_state );
	printf( " md_allocents: %d\n", fhp->fh_metadata.md_allocents );
	printf( "  md_checksum: %d\n", (int)fhp->fh_metadata.md_checksum );
	printf( "     md_token: " );
	print_token( &fhp->fh_metadata.md_token );
	printf( "\nAttributes\n----------\n\n" );
	print_attrs( &fhp->fh_metadata.md_attributes );
	printf( "\nBack FID\n--------\n\n" );
	print_fid( &fhp->fh_metadata.md_backfid );
	if (fhp->fh_metadata.md_state & MD_NOALLOCMAP) {
		strncpy(namebuf, (char *)fhp->fh_allocmap,
			fhp->fh_metadata.md_allocents);
		printf( "  fh_allocmap: %s\n", (char *)namebuf );
	} else if (fhp->fh_metadata.md_allocents > 0) {
		prtallocmap( fhp->fh_allocmap, fhp->fh_metadata.md_allocents );
	}
}

main(int argc, char **argv)
{
	int fd;
	char *file;
	int status = 0;
	fileheader_t fh;
	int bytes;

	Progname = *argv++;
	argc--;
	for (file = *argv; file && argc; argc--, file = *++argv) {
		fd = open(file, O_RDONLY);
		if (fd == -1) {
			pr_err("open error for %s: %s", file, strerror(errno));
			status = 1;
		} else {
			bytes = read(fd, &fh, FILEHEADER_SIZE);
			switch (bytes) {
				case -1:
					pr_err("read error for %s: %s", file, strerror(errno));
					break;
				case FILEHEADER_SIZE:
					print_fileheader(&fh);
					break;
				case 0:
					pr_err("Empty file: %s", file);
					break;
				default:
					pr_err("Short file header: %s", file);
			}
			close(fd);
		}
	}
	return(status);
}
