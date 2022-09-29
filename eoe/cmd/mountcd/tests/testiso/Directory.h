/*
 *	Directory.h
 *
 *	Description:
 *		Class definition for Directory
 *
 *	History:
 *      rogerc      04/16/91    Created
 */

#ifndef _Directory_
#define _Directory_

#include "DirEntry.h"

class Directory {
public:
	Directory( unsigned long loc, char *devscsi = "/dev/scsi/sc0d7l0",
	 int notranslate = 0, CDTYPE type = ISO );
	~Directory( );
	DirEntry *DirEnt( char *file );
	void dump( );
	int check( );
private:
	DirEntry **dir;
	int numEntries;
	int index;
};

#endif
