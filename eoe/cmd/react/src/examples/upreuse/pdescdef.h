/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __PDESCDEF_H__
#define __PDESCDEF_H__

#include <sys/types.h>
#include "sema.h"

typedef struct pdesc {
        pid_t     pid;
        uint      opcode;
        uint      flags;
        void*     args;
        void      (*function)(void*);
        usema_t*  ssema_go;
        usema_t*  ssema_done;
} pdesc_t;


#endif /* __PDESCDEF_H__ */
