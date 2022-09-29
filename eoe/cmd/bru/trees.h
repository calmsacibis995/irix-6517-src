/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	trees.h    tree handling defines
 *
 *  SCCS
 *
 *	@(#)trees.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "trees.h"
 *
 *  DESCRIPTION
 *
 *	Contains defines for external routines to communicate with
 *	routines in the tree handling module.
 *
 *	The routine path_type can return the following types:
 *
 *		STEM	  =>	path is a stem of the tree
 *				(end of name before reaching leaf)
 *
 *		LEAF	  =>	path is a leaf
 *				(exact match, end of name at leaf)
 *
 *		EXTENSION =>	path is an extension of the tree
 *				(exact match to leaf before end of name)
 *
 *		NOMATCH	  =>	path does not match, but more matches
 *				may be possible
 *
 *		FINISHED  =>	no more paths will match
 *
 */


#define		STEM		(1)	/* Path is a stem of the tree */
#define 	LEAF		(2)	/* Path is a leaf of tree */
#define		EXTENSION	(3)	/* Path is extension of tree */
#define		NOMATCH		(4)	/* No match */
#define		FINISHED	(5)	/* No more matches possible */
