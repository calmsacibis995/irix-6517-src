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


#ifndef _IDE_XBOW_H_

#define _IDE_XBOW_H_
typedef struct _mg_xbow_portinfo {
    __uint32_t mfg;
    __uint32_t part;
    __uint32_t rev;
    void    *base ;
    uchar_t port;
    uchar_t present;
    uchar_t alive;
} _mg_xbow_portinfo_t;

#define gio2xbase(port, giobase) \
    (__paddr_t *)(LARGE_WIDGET((port)) | BRIDGE_GIO_MEM32_BASE | giobase | (__paddr_t) (K1_MAIN_WIDGET(port))) ;

#endif 
