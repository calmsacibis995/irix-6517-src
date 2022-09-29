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
#ident "$Revision: 1.17 $"

#include <pfmt.h>		/* Include for internationalization */


/*
 *	Defines for xlv_admin functions
 *	XXX TAC remove the items in here that you do not need.
 */
#define         ADD     1
#define         SUB     2
#define         EDIT    3

/*
 *	Object of choice
 */
#define		CHOICE_SUBVOL		1
#define		CHOICE_PLEX		2
#define		CHOICE_VE		3
#define		CHOICE_VE_PLEX_END	4

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


#if 0
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
#endif


/*
 *	Function definitions for xlv_admin 
 */

extern xlv_mk_context_t *xlv_find_gen (char *name);
extern xlv_mk_context_t *xlv_find_plex (char *name);
extern xlv_mk_context_t *xlv_find_vol (char *name);
extern xlv_mk_context_t *xlv_find_ve (char *name);

extern int xlv_add_plex (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref,
	int       	 sub_volume_type,
	int       	 num);

extern int xlv_chk_ve (xlv_tab_subvol_t *sv, int pnum, int vnum);

extern xlv_rm_plex (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type,
	int		 pnum);

extern int xlv_dettach_plex (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	char		 *name,
	int		 type,
	int		 pnum);

extern int xlv_dettach_ve (
	xlv_mk_context_t *target,
	xlv_mk_context_t *new_oref, 
	int		 type,
	int		 pnum,
	int		 vnum,
	char		 *name);

extern int create_plex (
	int		 val,
	xlv_mk_context_t *new_oref,
	int 		 type, 
	int 		 pnum);

extern int xlv_mod_ve (
	xlv_mk_context_t *xlv_mk_context,
	xlv_mk_context_t *new_oref,
	int		 subvol_type,
	int		 pnum,
	int		 mode,
	int		 vnum);

extern int xlv_free_plex(xlv_mk_context_t *target, int mode);
extern int xlv_free_ve(xlv_mk_context_t *target);

extern void xlv_mk_add_completed_obj (xlv_mk_context_t *target, int initflag);


/*
 * Type of values for which get_val() prompts.
 */
#define	VAL_COMMAND		1
#define	VAL_OBJECT_NAME		2
#define	VAL_SUBVOL_NUMBER	3
#define	VAL_NEW_OBJECT_NAME	4
#define	VAL_PLEX_NUMBER		5
#define	VAL_CREATED_OBJECT_NAME	6
#define	VAL_PART_NAME		7
#define	VAL_VE_NUMBER		8
#define	VAL_ADD_OBJECT		9
#define	VAL_DESTROY_DATA	10
#define	VAL_DELETE_LABELS	11
#define	VAL_NODENAME		12
extern int get_val (char *buffer, int choice);


extern int xlv_del_hashentry (char *name);

extern int xlv_get_vol_state (xlv_mk_context_t *target);

#define		FMT_LONG	1
#define		FMT_SHORT	2
/*
 * If format == FMT_LONG then print the structure,
 * otherwise print only the name and type. 
 */
extern int xlv_prnt_list(int format);

extern int xlv_prnt_name(char *name);

/*
 * Execute the choice number specified by the user.
 */
extern int do_choice(int choice, char *name);

/*
 * Display the command menu.
 */
extern void do_begin(void);

/*
 * Command functions:
 */

extern int xlv_e_add (int choice, char *object);
extern int xlv_delete (char *name);
extern int xlv_dettach (int choice, char *object);
extern int xlv_prnt_blockmap (char *name);
extern int xlv_remove (int choice, char *object);
extern int set_nodename (char *object_list);
extern int set_ve_state (char *name);
