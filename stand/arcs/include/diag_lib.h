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

/* 
 *   File:  "$Id: diag_lib.h,v 1.2 1996/11/02 02:17:52 sprasad Exp $"
 *
 *   Authors: Chris Satterlee and Mohan Natarajan
 */

#ident  "$Revision: 1.2 $"
#ifndef __DIAG_LIB_H
#define __DIAG_LIB_H
typedef struct libc_device_s { 
    void        (*init)(void *init_data);
    int         (*poll)(void);
    int         (*readc)(void);
    int         (*getc)(void);
    int         (*putc)(int);
    int         (*puts)(char *);
    int         (*flush)(void);         /* Flush output */
    void        (*lock)(int);           /* Used by printf and puts */
    int         led_pattern;
    char       *dev_name;
} libc_device_t;

/* Extern variables */
extern libc_device_t      dev_junkuart, dev_ioc3uart; /* from ../../../IP27prom/libc.h */

/* Extern functions */
extern void               libc_device(libc_device_t *dev); /* from ../../../IP27prom/libc.h */

/* Function prototypes */
__psunsigned_t diag_ioc3_base(__psunsigned_t,int);
int            diag_check_ioc3_cfg(int, __psunsigned_t, int, char *);
int            diag_io6confSpace_sanity(int,__psunsigned_t);
int            diag_bridgeSanity(int,__psunsigned_t);
void*          diag_malloc(size_t); 
void           diag_free(void *); 
int            diag_check_pci_scr(int,__psunsigned_t,int, char *);
#endif 



