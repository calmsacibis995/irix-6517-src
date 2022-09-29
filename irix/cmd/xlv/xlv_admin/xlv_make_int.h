/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.3 $"

#ifndef YES
#define YES 1
#endif
#ifndef NO
#define NO 0
#endif
#define FORCE 0x1
#define EXPERT 0x2
#define START 0x4

extern int Tclflag;

typedef xlv_oref_t xlv_mk_context_t;


extern int xlv_mk_vol (xlv_mk_context_t    *xlv_mk_context,
            	       xlv_name_t          volume_name,
            	       long                volume_flags,
            	        int                expertflag);

extern int xlv_mk_subvol (xlv_mk_context_t *xlv_mk_context,
               		  short            sub_volume_type,
               		  long             sub_volume_flags);

extern int xlv_mk_plex (xlv_mk_context_t   *xlv_mk_context,
             		xlv_name_t         plex_name,
             		long               plex_flags,
             		 int               expertflag);

extern int xlv_mk_ve	(xlv_mk_context_t	*xlv_mk_context,
			 xlv_tab_vol_elmnt_t	*I_ve,
			 unsigned			flags);

extern void xlv_mk_init (void);

extern int xlv_mk_finish_curr_obj (xlv_mk_context_t *xlv_mk_context);

extern void xlv_mk_print_all (xlv_mk_context_t *xlv_mk_context, int p_mode);

extern int xlv_mk_write_disk_labels (int *write_cnt);

extern int xlv_mk_disk_part_in_use (xlv_mk_context_t *xlv_mk_context,
				    char	     *device_path,
				    dev_t	     dev);

extern int xlv_mk_quit_check(xlv_mk_context_t *current_context);
extern int xlv_mk_exit_check(void);
extern int xlv_mk_update_vh(void);
