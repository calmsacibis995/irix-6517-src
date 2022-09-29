/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.4 $"

#ifndef	_xlv_admin_h
#define	_xlv_admin_h

#include <pfmt.h>		/* Include for internationalization */


/*
 *	Defines for xlv_admin functions
 *	XXX TAC remove the items in here that you do not need.
 */
#define         ADD     1
#define         SUB     2
#define         EDIT    3
#define		LONG			1
#define		SHORT			2

/*
 *	XXX TAC Remove this
 */
#define 	MAX_UUID_LEN 36         /* does not include trailing \0 */


/*
 *	Error codes for xlv_admin functions
 */
#define 	_M_SELECT	2
#define 	_M_ADD_HDR	3
#define 	_M_ADD_SV	4
#define 	_M_ADD_PLEX	5
#define 	_M_ADD_VE	6
#define 	_M_DET_HDR	7
#define 	_M_DET_SV	8
#define 	_M_DET_PLEX	9
#define 	_M_DET_VE	10
#define 	_M_RM_HDR	11
#define 	_M_RM_SV	12
#define 	_M_RM_PLEX	13
#define 	_M_RM_VE	14
#define 	_M_DEL_HDR	15
#define 	_M_DEL_VOL	16
#define 	_M_DEL_PLEX	17
#define 	_M_DEL_VE	18
#define 	_M_SHOW_HDR	19
#define 	_M_SHOW_TYPE	20
#define 	_M_SHOW_ALL	21
#define 	_M_SHOW_SELECT	22
#define 	_M_SHOW_BLKMAP	23
#define 	_M_ADD_EXHDR	24
#define 	_M_ADD_EXSV	25
#define 	_M_ADD_EXPLEX	26
#define 	_M_ADD_EXVE	27
#define 	_M_EXIT_HDR	28
#define 	_M_EXIT		29
#define 	_M_SV_NSUPORT	30
#define 	_M_NSUPORT	31
#define 	_M_INVALID_CH	32
#define 	_M_CHOICE	33
#define 	_M_OBJ		34
#define 	_M_SEL_SUBVOL	35
#define 	_M_SEL_NEW_OBJ	36
#define 	_M_SEL_PLEX	37
#define 	_M_SEL_PART	38
#define 	_M_SEL_VE	39
#define 	_M_SEL_EXIST	40
#define 	_E_NOT_FOUND	41
#define 	_E_ADD_TO_VE	42
#define 	_E_BLOCKMAP	43
#define 	_E_PLEX_PLEX	44
#define 	_E_VE_VE	45
#define 	_E_ERROR	46
#define 	_E_NOT_VOL	47
#define 	_E_NOT_PLEX	48
#define 	_E_NO_VE	49
#define 	_E_DEL_HASH	50
#define 	_M_HDR_LIST	51
#define 	_M_VOL		52
#define 	_M_PLEX		53
#define 	_M_VE		54
#define 	_M_HDR_SLIST	55
#define 	_E_DET_SELF	56
#define		_E_CURSOR	57
#define 	_E_ADD_SELF	58
#define		_E_MAX_PLEX	59
#define		_E_NO		60
#define		_E__PLEX	61
#define		_E_TAC		62
#define		RM_SUBVOL_CRTVOL	100
#define		RM_SUBVOL_NOVOL		101
#define		RM_SUBVOL_NOSUBVOL	102
#define		ADD_NOVOL		103
#define		DEL_VOL_BAD_TYPE	104
#define 	DEL_PLEX_BAD_TYPE	105
#define		SUBVOL_NO_EXIST		106
#define		MALLOC_FAILED		107
#define		INVALID_SUBVOL		108
#define		INVALID__OPER_ENTITY	109
#define		PLEX_NO_EXIST		110
#define		LAST_SUBVOL		111
#define		LAST_PLEX		112
#define		INVALID_PLEX		113
#define		INVALID_VE		114
#define		LAST_VE			115
#define		INVALID_OPER_ENTITY	116
#define		ENO_SUCH_ENT		117
#define		VE_FAILED		118
#define		ADD_NOVE		119
#define		E_NO_PCOVER		120
#define		E_OBJ_NOT_PLEX		121
#define		E_NO_OBJECT		122
#define		E_OBJ_NOT_VOL_ELMNT	123
#define		E_VE_SPACE_NOT_CVRD	124
#define		E_VE_END_NO_MATCH	125
#define		E_VE_GAP		126
#define 	E_VE_MTCH_BAD_STATE	127
#define		VE_NO_EXIST		128
#define		E_VOL_STATE_MISS_UNIQUE	129
#define		E_VOL_STATE_MISS_SUBVOL	130
#define		E_C_K_VE_NO_MATCH	131
#define		E_VE_NULL		132
#define		E_PLX_NULL		133
#define		E_C_K_PLX_NO_MATCH	134
#define		E_SUBVOL_BUSY		135
#define		E_HASH_TBL		136

#define		E_VOL_MISS_PIECE	138

#define 	STR_YES			"yes"


/*
 *	External variables required by xlv_admin functions
 */
extern xlv_attr_cursor_t cursor;


/*
 *	Structures for xlv_admin functions
 *	XXX TAC remove these if you do not need...
 */
typedef struct part_array {
        char    name[30];
        int     pos;
}       part_array_t;

typedef struct xlv_part_list {
        int     count;
        part_array_t    part[10];
} xlv_part_list_t;



/*
 *	Function definitions for xlv_admin 
 */
extern int xlv_add_partition (
	xlv_mk_context_t *target,
	int		 type,
	int		 pnum,
	int		 vnum,
	int		 dpnum,
	xlv_part_list_t  *partitions);


extern xlv_mk_context_t *xlv_find_gen (char *name);
extern xlv_mk_context_t *xlv_find_plex (char *name);
extern xlv_mk_context_t *xlv_find_vol (char *name);
extern xlv_mk_context_t *xlv_find_ve (char *name);

extern xlv_del_volume (xlv_mk_context_t *target);

extern xlv_del_ve (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type,
	int		 pnum,
	int		 vnum);

extern xlv_rm_subvol (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type);

extern xlv_rm_plex (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type,
	int		 pnum,
	char		**msg);

extern xlv_rm_ve (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type,
	int		 pnum,
	int		 vnum);

extern xlv_rm_partition (
	xlv_mk_context_t *target,
	int		 type,
	int		 pnum,  
	int		 vnum,
	int		 dpnum);

extern int xlv_dettach_subvol (
	xlv_mk_context_t *target, 
	xlv_mk_context_t *new_oref,
	char		 *name,
	int		 type);

extern xlv_dettach_plex (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	char		 *name,
	int		 type,
	int		 pnum,
	char		**msg);

extern xlv_dettach_ve (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type,
	int		 pnum,
	int		 vnum,
	char		 *name);

extern int create_plex(
	int		 val,
	xlv_mk_context_t *new_oref,
	int 		 type, 
	int 		 pnum);

extern void xlv_mk_add_completed_obj (xlv_mk_context_t * target, int initflag);

extern int get_val (char *buffer, int choice);


#endif	/* _xlv_admin_h */
