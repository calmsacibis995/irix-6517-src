/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.10 $"

#ifndef __SN0HINV_H__
#define __SN0HINV_H__

/* some useful macros for hinv display */

extern const char 		*board_class[];

#define BOARD_CLASS(t)		((((t>>4)&0xf)<=KLCLASS_MAX) &&\
				 (((t>>4)&0xf)>0) ?\
				board_class[(((t>>4)&0xf)-1)]:"New Class")

extern const char 		*board_name[][8] ;

#define BOARD_NAME(t)   	(((((t>>4)&0xf)<=KLCLASS_MAX) &&\
				  (((t>>4)&0xf)>0) &&\
				  ((t&0xf)>0) &&\
				 ((t&0xf) <= KLTYPE_MAX)) ?\
				 board_name[(((t>>4)&0xf)-1)][((t&0xf)-1)] : \
				 "New Type")

extern const char 		*component_name[] ;
extern int			cname_size ;

#define COMPONENT_NAME(t)       (((t>0) && \
				 (t<cname_size)) ?\
 				 component_name[t] : "ID")

#define CLASS_INDEX(t)		(t>>4)

#define HINV_VERBOSE    0x01
#define HINV_PATHS      0x02
#define HINV_LIST       0x04
#define HINV_DEFAULT    0x10
#define HINV_MFG        0x20
#define HINV_MVV        0x40

void prnic_fmt(char *) ;
#define CHECK_IOC3_NIC(l, p)    ((!IS_MIO_IOC3(l, p)) &&                \
                                 ((l->brd_type == KLTYPE_PCI) ||        \
                                  ((l->brd_type == KLTYPE_BASEIO) &&    \
                                   (SN00) && (p > 3))))
typedef struct cpu_list_s{
        int size;
        int freq[16];
        int count[16];
}cpu_list_t;

#endif /* __SN0HINV_H__ */

