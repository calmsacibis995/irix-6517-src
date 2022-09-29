/*
 *  cdda.h
 *
 *  Description:
 *	Header file for cdda.c
 *
 *  History:
 *      pw     8-Nov-95:     Created
 */

#ifndef __CDDA_H__
#define __CDDA_H__
#include "nfs_prot.h"
#include "cdaudio.h"
#include "iso_types.h"

#define MAX_NFS_BUFFER_SIZE 8192

CDPLAYER *cd;

void cdda_init(int num_blocks);
int cdda_numblocks(unsigned int *blocks);
int cdda_openfs(char *dev, char *mntpnt, fhandle_t *root);
void cdda_removefs();
int cdda_getattr(fhandle_t *fh, fattr *fa);
int cdda_lookup(fhandle_t *fh, char *name, fhandle_t *fh_ret);
int cdda_read(fhandle_t *fh, int offset, int count, char *data,
	     unsigned int *amountRead);
int cdda_readdir(fhandle_t *fh, entry *entries, int count, int cookie,
		int *eof, bool_t *nread);
int cdda_readraw(unsigned long offset, void *buf, unsigned long count);
void cdda_disable_name_translations(void);
void cdda_setx(void);
int cdda_getfs(char *path, fhandle_t *fh, struct svc_req *rq);
int cdda_getblksize(void);
int cdda_readlink(fhandle_t *fh, char **link);

#endif /* ! __CDDA_H__ */

