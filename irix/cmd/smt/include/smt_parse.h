#ifndef _SMT_PARSE_H_
#define _SMT_PARSE_H_
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Definitions for parser.
 *
 *	$Revision: 1.7 $
 */

/***********************************************************
	Copyright 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#define PARSE_TOKEN_LEN		64	/* 64 */
#define PARSE_LABEL_LEN		64	/* 32 */
#define PARSE_OID_DEPTH		32	/* 16 */

/*
 * A linked list of tag-value pairs for enumerated integers.
 */
struct enum_list {
    struct enum_list *next;
    int	value;
    char *label;
};

/*
 * A linked list of nodes.
 */
struct node {
    struct node *next;
    char label[PARSE_LABEL_LEN]; 	/* This node's (unique) textual name */
    int	subid;	    			/* This node's integer subidentifier */
    char parent[PARSE_LABEL_LEN];	/* The parent's textual name */
    int type;	   		 	/* The type of object this represents */
    int access;     			/* Type of access to the node */
    int status;	    			/* Status of this node */
    struct enum_list *enums;		/* enum list (otherwise NULL) */
};

/*
 * A tree in the format of the tree structure of the MIB.
 */
struct tree {
    struct tree *child_list;	/* list of children of this node */
    struct tree *next_peer;	/* Next node in list of peers */
    struct tree *parent;
    char label[PARSE_LABEL_LEN];/* This node's textual name */
    int subid;			/* This node's integer subidentifier */
    int type;			/* This node's object type */
    int access;			/* Type of access to the node */
    int status;			/* Status of this node */
    struct enum_list *enums;	/* (optional) list of enum (otherwise NULL) */
    void (*printer)();     	/* Value printing function */
};

/* non-aggregate types for tree end nodes */
#define TYPE_OTHER		0
#define TYPE_OBJID		1
#define TYPE_OCTETSTR		2
#define TYPE_INTEGER		3
#define TYPE_NETADDR		4
#define TYPE_IPADDR		5
#define TYPE_COUNTER		6
#define TYPE_GAUGE		7
#define TYPE_TIMETICKS		8
#define TYPE_OPAQUE		9
#define TYPE_SEQUENCE		10
#define TYPE_NULL		11

/* access methods */
#define ACCESS_NOACCESS		0
#define ACCESS_READONLY		1
#define ACCESS_WRITEONLY	2
#define ACCESS_READWRITE	3

#define IS_READABLE(a)	((a) & 1)
#define IS_WRITEABLE(a)	((a) & 2)

/* variable status */
#define STATUS_OBSOLETE		0
#define STATUS_MANDATORY	1
#define STATUS_OPTIONAL		2

extern struct tree *read_mib(char *);
extern char * Malloc(unsigned long);
#endif
