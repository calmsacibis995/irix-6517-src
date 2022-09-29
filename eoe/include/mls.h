
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _MLS_DOT_H_
#define _MLS_DOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.23 $"

#include <sys/types.h>
#include <sys/mac_label.h>
#include <shadow.h>
#include <clearance.h>

/* constant define for mlsfile type */
#define MAC_LABEL	0	/* labels file */
#define MAC_MSEN_TYPE	1	/* msentypes file */
#define MAC_LEVEL	2	/* levels file */
#define MAC_CATEGORY	3	/* categorys file */
#define MAC_MINT_TYPE 	4	/* minttypes file */
#define MAC_GRADE	5	/* grades file */
#define MAC_DIVISION	6	/* divisions file */
#define MAX_MLSFILES    7	/* total 7 files above */

#define MAC_MIN_ENT	1	/* minumim value for entity database type */
#define MAC_MAX_ENT   	7	/* maximum value for entity database type */
#define MAC_MAX_DEFAULT 26      /* maximum internal msentype and minttype */ 

#define WITHOUT_LEVEL	0	/* msen type don't need level, category */
#define WITHOUT_GRADE	0	/* mint type don't need grade, division */
#define WITHOUT_LVLGRD  0	/* don't need level,grade,category, division */
#define NEED_LEVEL 	1	/* msen type need level, category */
#define NEED_GRADE	1	/* mint type need grade, division */
#define NEED_LVLGRD	1	/* need level, cat or grade, division */
#define INVALID_TYPE   -1	/* no such msen or mint type value */

#define MAX_MLS_NAME_LEN 128    /* max string length for a field name */
#define MAX_MLS_LINE_LEN 512    /* max line length for a entry */
#define INVALID_ENTRY_VALUE 65535

/* binary mls database file path */
#define LABEL_DB_PATH	"/etc/mac_label"
#define LABEL_DB	"lbldb_bin"

/* Error returns from mac_label_devs */
#define MAC_LD_DEVL_FOPEN	-1      /* couldn't open device list file    */
#define MAC_LD_GETPLABEL	-2      /* couldn't get current plabel value */
#define MAC_LD_SETPLABEL	-3      /* couldn't set plabel to system hi  */
#define MAC_LD_RESETPLABEL	-4      /* couldn't restore original plabel  */
#define MAC_LD_SOME_FAIL	-5      /* couldn't relabel some devices     */
#define MAC_LD_ALL_FAIL		-6      /* couldn't relabel any  devices     */
#define MAC_LD_KMEM_ERR		-7      /* error reading /dev/kmem           */


/* constant defines for mac_invalid error code return */
#define VALID_MAC_LABEL 	0  /* valid mac label indication */
#define NULL_MAC_LABEL		1  /* mac label pointer is a NULL */
#define INVALID_MSENTYPE	2  /* msentype value is not valid */
#define INVALID_MINTTYPE	3  /* minttype value is not valid */
#define INVALID_MSEN_ASSOC 	4  /* level and category present when not 
				      allowed, or level, category missing 
				      when it is required */	
#define INVALID_MINT_ASSOC 	5  /* grade and division present when not 
				      allowed, or grade,division missing 
				      when it is required */	
#define INVALID_SET_FORMAT	6  /* category or division set has duplicate 
				      numbers or not in ascending order */
#define INVALID_SET_COUNT 	7  /* category set and division set total is
				      out of range */
		
#define TAB_CHAR		0x09

/* constant define for mac_labeltostr() flag argument */
#define COMP_ONLY	0
#define NAME_OR_COMP	1
#define NAME_ONLY	2

/*
 * structure for sorting category set and division set value
 */
typedef struct valueent {
        unsigned short  value;  /* category or division value */
        struct valueent *next;  /* pointer to next entry */
        struct valueent *prev;  /* pointer to prev entry */
}VALUEENT;

typedef struct valuelist {
	VALUEENT *head;
	VALUEENT *tail;
}VALUELIST;

/*
 * minttype, grade and division entry
 */
typedef struct dbent {
	int	value;		/* data field value */
	char	*name;		/* pointer to data field name */
	struct dbent *next;
	struct dbent *prev;
}DBENT;


/*
 * structure for label/component string entry
 */
typedef struct lbldbent {
	char	*name;		/* pointer to label name string */
	char	*comp;		/* pointer to label component string */
	struct lbldbent *next;
}LBLDBENT;


/*
 * structure to build entry list for label/component string
 */
typedef struct lbldblist {
	struct lbldbent *head;	/* head of entry list */
	struct lbldbent *tail;  /* tail of entry list */
}LBLDBLIST;


/* structure for loadin msentype, level, category, minttype, grade and
 * division entry information into binary file
 */
typedef struct db_bin_ent {
	unsigned short	value;
	unsigned short	strlen;
	char	name[MAX_MLS_NAME_LEN];
}DB_BIN_ENT;

/* structure for loadin msentype, level, category, minttype, grade and
 * division header information into binary file
 */
typedef struct db_bin_hdr {
	int	db_type;	/* database type */
	int	entry_total;	/* total entry count */
	int	entries_bytes;  /* total entry bytes */
}DB_BIN_HDR;	


/* structure for loading label name and label component string header
 * information into binary file
 */
typedef struct lbldb_bin_hdr {
	int	entry_total;	/* total entry count */
	int	entries_bytes;  /* total entry bytes */
}LBLDB_BIN_HDR;	

/*
 * structure for loading label name and label component string into
 * binary file
 */
typedef struct lbldb_bin_ent {
	unsigned short 	strlen;		/* string length */
	unsigned short	lblsize;	/* mac label size */
 	char	name[MAX_MLS_NAME_LEN];		/* label name string */
	mac_label label;	/* mac label */
}LBLDB_BIN_ENT;


/*
 * structure to store the entry information from reading the db binary
 * file
 */
typedef struct dbinfo {
	DB_BIN_ENT *list;
	int	entry_total;
}DBINFO;


/*
 * structure to store the entry information from reading the lbldb binary
 * file
 */
typedef struct lbldbinfo {
	LBLDB_BIN_ENT *list;
	int	entry_total;
}LBLDBINFO;


/*
 * global variables used for Trusted IRIX/B library functions 
 */
extern LBLDB_BIN_HDR __mac_lhead;      /* header for labelname binary file */	
extern DB_BIN_HDR __mac_dhead[];       /* header for entity binary file */	
extern int __mls_entry_total[];	       /* total entry for each db file */
extern int __mac_bad_label_entry;      /* bad label entry in labelnames */
extern int __mac_db_error;	       /* bad entity entry in mlsfiles */	
extern DBINFO __mac_db[];	       /* binary entity database */
extern LBLDBINFO __mac_lbldb;	       /* binary label database */

extern int mac_label_devs (char *, mac_label *, uid_t);

#ifdef __cplusplus
}
#endif

#endif /* _MLS_DOT_H_ */
