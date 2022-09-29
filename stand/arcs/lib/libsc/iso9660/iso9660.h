#ifndef __ISO9660_H__
#define __ISO9660_H__

/*
** iso9660.h
**
** Small addition to the iso.h stuff.
**
*/





/* we have our primary volume descriptor and a "dinode" equivalent */
typedef struct iso9660_info {
	struct p_vol_desc *iso9660_pvd;
	dir_t *iso9660_dinode;
} iso9660_info_t;

#define fsb_to_inode(fsb) (((iso9660_info_t *)((fsb)->FsPtr))->iso9660_dinode)
#define fsb_to_mount(fsb) (((iso9660_info_t *)((fsb)->FsPtr))->iso9660_pvd)

#endif
