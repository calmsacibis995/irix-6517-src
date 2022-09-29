/*
 * inode.h
 *
 * Include file for inode lookup interface
 */

extern void InvalidateInodeTable(void);

extern char * InodeLookup(int rdev, int inode);

extern int InodeInit(void);

extern int FindInode(char *str, dev_t *rdev, ino_t *ino);
