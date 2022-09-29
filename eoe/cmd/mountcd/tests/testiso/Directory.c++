 /*
  *	Directory.c++
  *
  *	Description:
  *		Methods for Directory class
  *
  *	History:
  *      rogerc      04/16/91    Created
  */

#include <string.h>
#include <stdio.h>
#include <stream.h>
#include "Directory.h"
#include "Block.h"
#include "util.h"

Directory::Directory(unsigned long loc, char *devscsi, int notranslate,
		     CDTYPE type)
{
    Block blk(loc, devscsi);
    RAWDIRENT *raw = (RAWDIRENT *)(void *)blk;
    loc = CHARSTOLONG(raw->de_loc);
    int len = CHARSTOLONG(raw->de_len);
    int maxEntries = len / sizeof (RAWDIRENT);

    dir = new DirEntry *[maxEntries];
    numEntries = 0;

    Block contents(loc + raw->de_xattr_len,
		   (len + blockSize - 1) / blockSize, devscsi);
    int bytesSoFar = 0;
    int bytesThisTime;
    raw = (RAWDIRENT *)(void *)contents;

    while (bytesSoFar < len && raw->de_lendir != 0) {
	if (type == ISO)
	    dir[numEntries++] = new DirEntry(raw, notranslate);
	else if (type == HSFS)
	    dir[numEntries++] = new DirEntry((HSFS_RAWDIRENT *)raw,
					     notranslate);
	else {
	    cerr << progname << ": invalid disc\n" << "\n";
	    exit (1);
	}

	//bytesThisTime = sizeof (RAWDIRENT + raw->de_namelen - 1);
	bytesThisTime = raw->de_lendir;

	raw = (RAWDIRENT *)((char *)raw + bytesThisTime);
	bytesSoFar += bytesThisTime;

	if (raw->de_lendir == 0 && bytesSoFar < len) {
	    int extraBytes = blockSize - bytesSoFar % blockSize;
	    bytesSoFar += extraBytes;
	    raw = (RAWDIRENT *)((char *)raw + extraBytes);
	}
    }
    /*
     * Prepare for queries
     */
    index = 0;
}

Directory::~Directory()
{
    while (numEntries)
	delete dir[--numEntries];
    delete dir;
}

void
Directory::dump()
{
    for (int i = 0; i < numEntries; i++)
	dir[i]->dump();
}

int
Directory::check()
{
    int err = 0;

    for (int i = 0; i < numEntries; i++)
	if (dir[i]->check())
	    err = 1;
    
    return err;
}

DirEntry *
Directory::DirEnt(char *file)
{
    if (strcmp(file, dir[index]->name()) == 0
	&& !(dir[index]->flags() & FFLAG_ASSOC)) {
	return dir[index];
    }

    for (index = 0; index < numEntries; index++) {
	if (strcmp(file, dir[index]->name()) == 0
	    && !(dir[index]->flags() & FFLAG_ASSOC)) {
	    return dir[index];
	}
    }
    
    index = 0;
    return 0;
}
