/*
 *  iso.h
 *
 *  Description:
 *	Header file for iso.c
 *
 *  History:
 *      rogerc      12/21/90    Created
 */

#ifndef __ISO_H__
#define __ISO_H__
#include "nfs_prot.h"
#include "cdrom.h"
#include "iso_types.h"

void iso_init(int num_blocks);
int iso_numblocks(unsigned int *blocks);
int iso_openfs(char *dev, char *mntpnt, fhandle_t *root);
void iso_removefs();
int iso_getattr(fhandle_t *fh, fattr *fa);
int iso_lookup(fhandle_t *fh, char *name, fhandle_t *fh_ret);
int iso_read(fhandle_t *fh, int offset, int count, char *data,
	     unsigned int *amountRead);
int iso_readdir(fhandle_t *fh, entry *entries, int count, int cookie,
		int *eof, bool_t *nread);
int iso_readraw(unsigned long offset, void *buf, unsigned long count);
void iso_disable_name_translations(char);
void iso_setx(void);
void iso_disable_extensions(void);
int iso_getfs(char *path, fhandle_t *fh, struct svc_req *rq);
int iso_getblksize(void);
int iso_readlink(fhandle_t *fh, char **link);

#endif /* ! __ISO_H__ */

