/*
 *	PathTable.h
 *
 *	Description:
 *		Class definition for PathTable
 *
 *	History:
 *      rogerc      04/15/91    Created
 */

#ifndef _PathTable_
#define _PathTable_

#include "Directory.h"

typedef struct pt {
	short			pt_parent;
	int				pt_count;
	unsigned long	pt_loc;
	char			*pt_name;
	Directory		*pt_dirp;
}	PT;

class PathTable {
public:
	PathTable( char *devscsi = "/dev/scsi/sc0d7l0", int notranslate = 0 );

	~PathTable( );
	Directory *Dir( char *path );
	int reference( char *path );
	int count( char *path );
	int location( char *path );
	int lenth( char *path );
	int check( );
	CDTYPE type( ) { return disctype; }
	void dump( );
private:
	int findDir( char *name, int parent );
	PT *pt;
	int pathCount;
	CDTYPE disctype;
};

#endif /* _PathTable_ */
