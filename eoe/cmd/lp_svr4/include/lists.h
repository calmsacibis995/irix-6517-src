/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/lists.h,v 1.1 1992/12/14 13:22:33 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ifndef	LISTS_H
#define	LISTS_H
/*==================================================================*/
/*
*/

#include	<sys/types.h>
#include	"boolean.h"


#ifndef	NULL
#define	NULL	0
#endif

#ifndef	UCHAR
typedef	unsigned char	uchar;
#endif

typedef	enum
{
	EmptyList	= 0,
	CharList	= 1,
	IntList		= 2,
	ShortList	= 3,
	LongList	= 4,
	FloatList	= 5,
	DoubleList	= 6,
	StringList	= 7,
	PointerList	= 8,
	StructureList	= 9

}  listType;

struct	_charList
{
	uint	*map;
	uchar	*members;
};

struct	_intList
{
	uint	*map;
	uint	*members;
};

struct	_shortList
{
	uint	*map;
	ushort	*members;
};

struct	_longList
{
	uint	*map;
	ulong	*members;
};

struct	_floatList
{
	uint	*map;
		 float	*members;
};

struct	_doubleList
{
	uint	*map;
	double	*members;
};

struct	_stringList
{
	char	**members;
};

struct	_pointerList
{
	int	*sizeOfMembers;
	void	**members;
};

struct	_structureList
{
	uint	*map;
	uint	sizeOfMembers;
	void	**members;
};

typedef	struct
{
	listType	type;
	uint		flags;  /**/
	uint		size;	/*  Logical size.	*/
	uint		length; /*  N members.		*/

	union {
		struct	_charList	charList;
		struct	_intList	intList;
		struct	_shortList	shortList;
		struct	_longList	longList;
		struct	_floatList	floatList;
		struct	_doubleList	doubleList;
		struct	_stringList	stringList;
		struct	_pointerList	pointerList;
		struct	_structureList	structureList;
	} listUnion;

}  list;


typedef	union
{
	uchar	charMember;
	uint	intMember;
	ushort	shortMember;
	ulong	longMember;
	float	floatMember;
	double	doubleMember;
	char	*stringMember;
	void	*pointerMember;
	void	*structureMember;

} listMemberTypeUnion;

/*----------------------------------------------------------*/
/*
*/
#if (defined(__STDC__) || (__SVR4__STDC))

int	SizeofListMember (list *, int);
list	*NewList (listType, int);
list	*ApplyToList (list *, void *(*) (), listType, int);
void	*ListMember (list *, int);
void	*PopListMember (list *);
void	*RemoveListMember (list *, int);
void	FreeList (list **);
void	FreeListMembers (list *);
boolean	AppendToList (list *, void *, int);
boolean	GrowList (list *, int);

#else

int	SizeofListMember ();
list	*NewList ();
list	*ApplyToList ();
void	*ListMember ();
void	*PopListMember ();
void	*RemoveListMember ();
void	FreeList ();
void	FreeListMembers ();
boolean	AppendToList ();
boolean	GrowList ();

#endif
/*==================================================================*/
#endif
