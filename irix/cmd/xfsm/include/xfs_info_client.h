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

#ident "xfs_info_client.h: $Revision: 1.1 $"

#define HOST_PATTERN_KEY        "HOST_PATTERN"

/* To temporarily bypass internationalization */
#define gettxt(str_line_num,str_default)        str_default

/* Function prototypes */
extern int authenticate_user(void);
