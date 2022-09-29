/* hash.h --
 *
 * 	This file contains definitions used by the hash module,
 * 	which maintains hash tables.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 *
 * Header: hash.h,v 2.0 87/08/11 09:32:12 brent Exp $ SPRITE (Berkeley)
 */


#ifndef	_HASH
#define	_HASH

#include "list.h"
/* 
 * The following defines one entry in the hash table.
 */

typedef struct Hash_Entry {
    List_Links	      links;		/* Used to link together all the
    					 * entries associated with the same
					 * bucket. */
    ClientData	      clientData;	/* Arbitrary piece of data associated
    					 * with key. */
    union {
	Address	 ptr;			/* One-word key value to identify
					 * entry. */
	int words[1];			/* N-word key value.  Note: the actual
					 * size may be longer if necessary to
					 * hold the entire key. */
	char 	 name[4];		/* Text name of this entry.  Note: the
					 * actual size may be longer if
					 * necessary to hold the whole string.
					 * This MUST be the last entry in the
					 * structure!!! */
    } key;
} Hash_Entry;

/* 
 * A hash table consists of an array of pointers to hash
 * lists.  Tables can be organized in either of three ways, depending
 * on the type of comparison keys:
 *
 *	Strings:	  these are NULL-terminated; their address
 *			  is passed to HashFind as a (char *).
 *	Single-word keys: these may be anything, but must be passed
 *			  to Hash_Find as an Address.
 *	Multi-word keys:  these may also be anything; their address
 *			  is passed to HashFind as an Address.
 *
 *	Single-word keys are fastest, but most restrictive.
 */
 
#define HASH_STRING_KEYS	0
#define HASH_ONE_WORD_KEYS	1

typedef struct Hash_Table {
    List_Links 	*bucketPtr;	/* Pointer to array of List_Links, one
    				 * for each bucket in the table. */
    int 	size;		/* Actual size of array. */
    int 	numEntries;	/* Number of entries in the table. */
    int 	downShift;	/* Shift count, used in hashing function. */
    int 	mask;		/* Used to select bits for hashing. */
    int 	keyType;	/* Type of keys used in table:
    				 * HASH_STRING_KEYS, HASH_ONE-WORD_KEYS,
				 * or >1 menas keyType gives number of words
				 * in keys.
				 */
} Hash_Table;

/* 
 * The following structure is used by the searching routines
 * to record where we are in the search.
 */

typedef struct Hash_Search {
    Hash_Table  *tablePtr;	/* Table being searched. */
    int 	nextIndex;	/* Next bucket to check (after current). */
    Hash_Entry 	*hashEntryPtr;	/* Next entry to check in current bucket. */
    List_Links	*hashList;	/* Hash chain currently being checked. */
} Hash_Search;

/*
 * Macros.
 */

/*
 * ClientData Hash_GetValue(h) 
 *     Hash_Entry *h; 
 */

#define Hash_GetValue(h) ((h)->clientData)

/* 
 * Hash_SetValue(h, val); 
 *     HashEntry *h; 
 *     char *val; 
 */

#define Hash_SetValue(h, val) ((h)->clientData = (ClientData) (val))

/* 
 * Hash_Size(n) returns the number of words in an object of n bytes 
 */

#define	Hash_Size(n)	(((n) + sizeof (int) - 1) / sizeof (int))

/*
 * The following procedure declarations and macros
 * are the only things that should be needed outside
 * the implementation code.
 */

extern Hash_Entry *	Hash_CreateEntry();
extern void		Hash_DeleteTable();
extern void		Hash_DeleteEntry();
extern void		Hash_DeleteTable();
extern Hash_Entry *	Hash_EnumFirst();
extern Hash_Entry *	Hash_EnumNext();
extern Hash_Entry *	Hash_FindEntry();
extern void		Hash_InitTable();
extern void		Hash_PrintStats();

#endif _HASH
