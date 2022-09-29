/*
 *	DirEntry.h
 *
 *	Description:
 *		Class definition for DirEntry
 *
 *	History:
 *      rogerc      04/11/91    Created
 */

#ifndef _Dir_Entry_
#define _Dir_Entry_

#include <time.h>
#include "Block.h"

const FFLAG_EXIST = 0x01;
const FFLAG_DIRECTORY = 0x02;
const FFLAG_ASSOC = 0x04;
const FFLAG_RECORD = 0x08;
const FFLAG_PROTECT = 0x10;
const FFLAG_MULTI = 0x80;

const MAXNAMELEN = 100;

#define TMYEAR 0
#define TMMONTH 1
#define TMDAY 2
#define TMHOUR 3
#define TMMIN 4
#define TMSEC 5
#define TMOFF 6

#define XTMYEAR 0
#define XTMMONTH 4
#define XTMDAY 6
#define XTMHOUR 8
#define XTMMIN 10
#define XTMSEC 12
#define XTMOFF 16

typedef enum cdtype { ISO, HSFS } CDTYPE;

typedef struct rawdirent {
	unsigned char	de_lendir;
	unsigned char	de_xattr_len;
	unsigned char	de_loc_lsb[4];
	unsigned char	de_loc[4];
	unsigned char	de_len_lsb[4];
	unsigned char	de_len[4];
	unsigned char	de_date[7];
	unsigned char	de_flags;
	unsigned char	de_fu;
	unsigned char	de_gap;
	unsigned char	de_volseq[4];
	unsigned char	de_namelen;
	char        	de_name[1];
}	RAWDIRENT;

typedef struct hsfs_rawdirent {
	unsigned char	de_lendir;
	unsigned char	de_xattr_len;
	unsigned char	de_loc_lsb[4];
	unsigned char	de_loc[4];
	unsigned char	de_len_lsb[4];
	unsigned char	de_len[4];
	unsigned char	de_date[6];
	unsigned char	de_flags;
	unsigned char	de_res;
	unsigned char	de_fu;
	unsigned char	de_gap;
	unsigned char	de_volseq[4];
	unsigned char	de_namelen;
	char        	de_name[1];
}	HSFS_RAWDIRENT;

class DirEntry {
public:
	DirEntry( RAWDIRENT *raw, int notranslate = 0 );
	DirEntry( HSFS_RAWDIRENT *raw, int notranslate = 0 );
	~DirEntry( );
	char *name( );
	unsigned long location( ) { return loc; }
	unsigned long length( ) { return len; }
	unsigned char flags( ) { return fflags; }
	unsigned char fileUnitSize( ) { return fu; }
	unsigned char interleaveGap( ) { return gap; }
	unsigned char extAttribLen( ) { return xattrLen; }
	CDTYPE getType( ) { return type; }
	int &count( ) { return refCount; }
	void dump( );
	int check( );
	time_t time( ) { return recTime; }
private:
	unsigned char xattrLen;
	unsigned char fflags;
	unsigned char fu;
	unsigned char gap;
	unsigned long loc;
	unsigned long len;
	char *filename;
	CDTYPE type;
	time_t recTime; // recording time
	int refCount;	// number of references to this entry
};
	
#endif
