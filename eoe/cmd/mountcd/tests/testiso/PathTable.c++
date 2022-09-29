/*
 *  PathTable.c++
 *
 *  Description:
 *	Implementation of the PathTable class
 *
 *  History:
 *      rogerc      04/15/91    Created
 */

#include <string.h>
#include <stdio.h>
#include <stream.h>
#include "Block.h"
#include "PathTable.h"
#include "util.h"

#define VHBLOCK	16

#define ISOID 1
#define HSFSID 9

#define ISO_PTSIZE	136
#define ISO_PTLOC	148
#define ISO_BLKSIZE     130

#define HSFS_PTSIZE 144
#define HSFS_PTLOC 164
#define HSFS_BLKSIZE 138
#define MAXNAMELEN	300;

/*
 * This is for the byte layout of the path table as it comes off
 * the disc
 */
typedef struct iso_rawpt {
    char	pt_len;
    char	pt_xattr_len;
    char	pt_ext_loc[4];
    char	pt_parent[2];
    char	pt_name[1];
} ISO_RAWPT;

typedef struct hsfs_rawpt {
    char	pt_ext_loc[4];
    char	pt_xattr_len;
    char	pt_len;
    char	pt_parent[2];
    char	pt_name[1];
} HSFS_RAWPT;

PathTable::PathTable(char *devscsi, int notranslate)
{
    Block vh(VHBLOCK, devscsi);

    if (strncmp(vh + ISOID, "CD001", 5) == 0) {
	disctype = ISO;
	int ptLoc = CHARSTOLONG(vh + ISO_PTLOC);
	int ptSize = CHARSTOLONG(vh + ISO_PTSIZE);
	blockSize = CHARSTOSHORT(vh + ISO_BLKSIZE);
	Block ptBlock(ptLoc, (ptSize + blockSize - 1) / blockSize,
		      devscsi);

	/*
	 * Figure out the maximum number of path table records possible, and
	 * allocate that many from the free store
	 */
	int maxRecords = ptSize / sizeof (ISO_RAWPT);
	pt = new PT[maxRecords];
	pathCount = 0;
	int bytesSoFar = 0;
	int bytesThisTime;

	ISO_RAWPT *rawpt = (ISO_RAWPT *)(char *)ptBlock;

	while (bytesSoFar < ptSize) {
	    PT *ptp = pt + pathCount;

	    ptp->pt_parent = CHARSTOSHORT(rawpt->pt_parent) - 1;
	    int stlen;
	    char *str;
	    str = to_unixname(rawpt->pt_name, rawpt->pt_len, notranslate);
	    stlen = strlen(str);
	    ptp->pt_name = new char[stlen + 1];
	    strncpy(ptp->pt_name, str, stlen);
	    ptp->pt_name[stlen] = '\0';
	    ptp->pt_loc = CHARSTOLONG(rawpt->pt_ext_loc);
	    ptp->pt_dirp = new Directory(ptp->pt_loc + rawpt->pt_xattr_len,
					 devscsi, notranslate, disctype);

	    bytesThisTime = sizeof (*rawpt) + rawpt->pt_len - 1;
	    if (bytesThisTime & 1)
		bytesThisTime++;

	    pathCount++;
	    bytesSoFar += bytesThisTime;
	    rawpt = (ISO_RAWPT *)((char *)rawpt + bytesThisTime);
	}
    }
    else if (strncmp(vh + HSFSID, "CDROM", 5) == 0) {
	disctype = HSFS;
	int ptLoc = CHARSTOLONG(vh + HSFS_PTLOC);
	int ptSize = CHARSTOLONG(vh + HSFS_PTSIZE);
	blockSize = CHARSTOSHORT(vh + HSFS_BLKSIZE);
	Block ptBlock(ptLoc, (ptSize + blockSize - 1) / blockSize,
		      devscsi);

	/*
	 * Figure out the maximum number of path table records possible, and
	 * allocate that many from the free store
	 */
	int maxRecords = ptSize / sizeof (HSFS_RAWPT);
	pt = new PT[maxRecords];
	pathCount = 0;
	int bytesSoFar = 0;
	int bytesThisTime;

	HSFS_RAWPT *rawpt = (HSFS_RAWPT *)(char *)ptBlock;

	while (bytesSoFar < ptSize) {
	    PT *ptp = pt + pathCount;

	    ptp->pt_parent = CHARSTOSHORT(rawpt->pt_parent) - 1;
	    int stlen;
	    char *str;
	    str = to_unixname(rawpt->pt_name, rawpt->pt_len, notranslate);
	    stlen = strlen(str);
	    ptp->pt_name = new char[stlen + 1];
	    strncpy(ptp->pt_name, str, stlen);
	    ptp->pt_name[stlen] = '\0';
	    ptp->pt_loc = CHARSTOLONG(rawpt->pt_ext_loc);
	    ptp->pt_dirp = new Directory(ptp->pt_loc, devscsi, notranslate,
					 disctype);

	    bytesThisTime = sizeof (*rawpt) + rawpt->pt_len - 1;
	    if (bytesThisTime & 1)
		bytesThisTime++;

	    pathCount++;
	    bytesSoFar += bytesThisTime;
	    rawpt = (HSFS_RAWPT *)((char *)rawpt + bytesThisTime);
	}
    }
    else {
	cerr << progname << ": invalid disc\n";
	exit (1);
    }
}

void
PathTable::dump()
{
    for (int i = 0; i < pathCount; i++) {
	printf("name: %s\tcount: %d\tloc: %d\tparent: %d\n",
	       pt[i].pt_name, pt[i].pt_count, pt[i].pt_loc, pt[i].pt_parent);
	pt[i].pt_dirp->dump();
    }
}

int
PathTable::check()
{
    int err = 0;
    for (int i = 0; i < pathCount; i++)
	if (pt[i].pt_dirp->check())
	    err = 1;

    return (err);
}

PathTable::~PathTable()
{
    delete pt;
}

Directory *
PathTable::Dir(char *path)
{
    char *p = strdup(path);
    char *comp;
    int parent = 0;

    for (comp = strtok(p, "/"); comp && parent != -1;
	 comp = strtok(0, "/"))
	parent = findDir(comp, parent);

    free(p);
    return parent == -1 ? 0 : pt[parent].pt_dirp;
}


int
PathTable::findDir(char *name, int parent)
{
    for (int i = parent + 1; i < pathCount && parent >= pt[i].pt_parent; i++)
	if (strcmp(name, pt[i].pt_name) == 0 &&
	    pt[i].pt_parent == parent)
	    return i;
    
    return -1;
}
