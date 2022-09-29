/*
 *	cdrom.h
 *
 *	Description:
 *		header file for cdrom.c
 *
 *	History:
 *      rogerc      12/18/90    Created
 */

#define CDROM_BLKSIZE	2048

typedef struct cdrom CDROM;

typedef struct cdfile {
	int				block;
	int				xattr_len;
	int				int_gap;
	int				fu_size;
}	CD_FILE;

struct stat;

void
cd_init( );

unsigned long
cd_voldesc(CDROM *cd);

int
cd_open( char *dev, int num_blocks, CDROM **cdp );

int
cd_close( CDROM *cd );

int
cd_stat( CDROM *cd, struct stat *sb );

int
cd_is_dsp_fd( CDROM *cd, int fd );

int
cd_read_file( CDROM *cd, CD_FILE *fp, unsigned int offset,
 unsigned int count, void *vbuf );

int
cd_read( CDROM *cd, unsigned long offset, void *vdata,
	 unsigned long count, int use_cache, char *reason, int lineno );

int
cd_media_changed( CDROM *cd, int *changed );

void
cd_flush_cache( CDROM *cd );

int
cd_set_blksize(CDROM *cd, int blksize);

int
cd_get_blksize(CDROM *cd);
