#ifndef _QUERY_H
#define	_QUERY_H

/**************************************************************************
 *                                                                        *
 *           Copyright (C) 1993-1994, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.1 $"

/* The types of the objects for defining queries */
#define	DISK_TYPE_QUERY		1
#define	FS_TYPE_QUERY		2
#define	XLV_TYPE_QUERY		3

/* Logical operators to connect query clauses */
#define	EOQ			0	/* End of Query definition */
#define	QUERY_CLAUSE_AND_OPER	1	/* AND */
#define	QUERY_CLAUSE_OR_OPER	2	/* OR */


/*
 *      The following condition operators are defined for querying
 *      the XLV objects.
 */

#define QUERY_OPER_START		0

#define QUERY_EQUAL			(QUERY_OPER_START + 0)
#define QUERY_NE			(QUERY_OPER_START + 1)
#define QUERY_GT			(QUERY_OPER_START + 2)
#define QUERY_GE			(QUERY_OPER_START + 3)
#define QUERY_LT			(QUERY_OPER_START + 4)
#define QUERY_LE			(QUERY_OPER_START + 5)
#define QUERY_STRCMP			(QUERY_OPER_START + 6)
#define QUERY_TRUE			(QUERY_OPER_START + 7)
#define QUERY_FALSE			(QUERY_OPER_START + 8)

#define QUERY_OPER_END			(QUERY_FALSE)

#define XLV_QUERY_MATCH_OBJ_NM_LEN      512

typedef struct xlv_query_set {
        xlv_oref_t *oref;
        char name[XLV_QUERY_MATCH_OBJ_NM_LEN];
        struct xlv_query_set *next;
        short creator;
} xlv_query_set_t;

typedef struct query_set {
	int type; 	/* DISK_TYPE_QUERY/FS_TYPE_QUERY/XLV_TYPE_QUERY */
	union {
		xlv_vh_entry_t *disk;
		xlv_query_set_t *xlv_obj_ref;
		fs_info_entry_t *fs_ref;
	} value;
	struct query_set *next;
} query_set_t;

typedef struct query_clause {

	int query_type;	/* DISK_TYPE_QUERY/FS_TYPE_QUERY/XLV_TYPE_QUERY*/

	int query_sub_type;	/* Subclass/Object to query under the
				   domain query_type */

	short attribute_id;	/* Attribute being compared */
	char* value;		/* Value to compare with */
	short operator;		/* comparision operator */

	short query_clause_operator;	
				/* Logical Operator (QUERY_CLAUSE_AND_OPER/
				   QUERY_CLAUSE_OR_OPER) to connect 
				   with next query clause. If end of
				   query specify EOQ */
	
	struct query_clause *next;

} query_clause_t;

/* Function Prototypes */
extern short xfs_query_parse_regexp(char* regexpn, char* value);
extern query_set_t* create_in_set(void);
extern void create_fs_in_set(query_set_t** in_set);
extern void create_disks_in_set(query_set_t** in_set);
extern void create_xlv_in_set(query_set_t** in_set);
extern void set_xlv_obj_name(query_set_t **element);
extern void delete_in_set(query_set_t **set);
extern void add_obj_to_buffer(char** buffer,char* hostname,char* obj_type,char* obj_name, char* obj_subtype);
extern int xfs_query(char* hostname,const char* query_type,const char* query_defn_str,char** objs,char** msg);
#endif
