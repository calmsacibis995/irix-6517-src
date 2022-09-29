/*-
 * lst.h --
 *	Header for using the list library
 *
 * Copyright (c) 1988 by University of California Regents
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appears in all copies.  Neither the University of California nor
 * Adam de Boor makes any representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Id: lst.h,v 1.2 1989/01/29 23:09:29 arc Exp $ SPRITE (Berkeley)
 */
#ifndef _LST_H_
#define _LST_H_

#include	<sprite.h>

/*
 * basic typedef. This is what the Lst_ functions handle
 */

typedef	struct	Lst	*Lst;
typedef	struct	LstNode	*LstNode;

#define	NILLST		((Lst) NIL)
#define	NILLNODE	((LstNode) NIL)

/*
 * NOFREE can be used as the freeProc to Lst_Destroy when the elements are
 *	not to be freed.
 * NOCOPY performs similarly when given as the copyProc to Lst_Duplicate.
 */
#define NOFREE		((void (*)()) 0)
#define NOCOPY		((ClientData (*)()) 0)

#define LST_CONCNEW	0   /* create new LstNode's when using Lst_Concat */
#define LST_CONCLINK	1   /* relink LstNode's when using Lst_Concat */

/*
 * Creation/destruction functions
 */
Lst		  Lst_Init();	    	/* Create a new list */
Lst	    	  Lst_Duplicate();  	/* Duplicate an existing list */
void		  Lst_Destroy();	/* Destroy an old one */

int	    	  Lst_Length();	    	/* Find the length of a list */
Boolean		  Lst_IsEmpty();	/* True if list is empty */

/*
 * Functions to modify a list
 */
ReturnStatus	  Lst_Insert();	    	/* Insert an element before another */
ReturnStatus	  Lst_Append();	    	/* Insert an element after another */
ReturnStatus	  Lst_AtFront();    	/* Place an element at the front of
					 * a lst. */
ReturnStatus	  Lst_AtEnd();	    	/* Place an element at the end of a
					 * lst. */
ReturnStatus	  Lst_Remove();	    	/* Remove an element */
ReturnStatus	  Lst_Replace();	/* Replace a node with a new value */
ReturnStatus	  Lst_Move();	    	/* Move an element to another place */
ReturnStatus	  Lst_Concat();	    	/* Concatenate two lists */

/*
 * Node-specific functions
 */
LstNode		  Lst_First();	    	/* Return first element in list */
LstNode		  Lst_Last();	    	/* Return last element in list */
LstNode		  Lst_Succ();	    	/* Return successor to given element */
LstNode		  Lst_Pred();	    	/* Return predecessor to given
					 * element */
ClientData	  Lst_Datum();	    	/* Get datum from LstNode */

/*
 * Functions for entire lists
 */
LstNode		  Lst_Find();	    	/* Find an element in a list */
LstNode		  Lst_FindFrom();	/* Find an element starting from
					 * somewhere */
LstNode	    	  Lst_Member();	    	/* See if the given datum is on the
					 * list. Returns the LstNode containing
					 * the datum */
int	    	  Lst_Index();	    	/* Returns the index of a datum in the
					 * list, starting from 0 */
void		  Lst_ForEach();	/* Apply a function to all elements of
					 * a lst */
void	    	  Lst_ForEachFrom();  	/* Apply a function to all elements of
					 * a lst starting from a certain point.
					 * If the list is circular, the
					 * application will wrap around to the
					 * beginning of the list again. */
/*
 * these functions are for dealing with a list as a table, of sorts.
 * An idea of the "current element" is kept and used by all the functions
 * between Lst_Open() and Lst_Close().
 */
ReturnStatus	  Lst_Open();	    	/* Open the list */
LstNode		  Lst_Prev();	    	/* Previous element */
LstNode		  Lst_Cur();	    	/* The current element, please */
LstNode		  Lst_Next();	    	/* Next element please */
Boolean		  Lst_IsAtEnd();	/* Done yet? */
void		  Lst_Close();	    	/* Finish table access */

/*
 * for using the list as a queue
 */
ReturnStatus	  Lst_EnQueue();	/* Place an element at tail of queue */
ClientData	  Lst_DeQueue();	/* Remove an element from head of
					 * queue */

#endif _LST_H_
