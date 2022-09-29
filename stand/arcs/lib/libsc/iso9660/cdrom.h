#ifndef __CDROM_H__
#define __CDROM_H__

/*
** cdrom.h
**
** These routines are used by the iso.c (from eoe/cmd/mountcd) and
** iso9660.c functions.
**
** They are basically modified versions of the eoe/cmd/mountcd cdrom.c
** functions.
**
*/





#define CDROM_BLKSIZE 2048

extern int cd_get_blksize(void);

extern int cd_set_blksize(FSBLOCK *fsb, int blksize);

extern LONG cd_voldesc(void);

extern int cd_read(FSBLOCK *fsb, LONG offset, void *buf, LONG count);

#endif
