/*
 * Copyright 1998, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */


#ifndef __COMMON_H
#define __COMMON_H


/* std h files*/
#include <stdio.h>
#include <stdlib.h>
#include <sgidefs.h>
#include <sys/types.h>
#include <errno.h>

#define TRUE          1
#define FALSE         0
#define MAX_PATH      256
#define ALL_8BIT      0xFF


typedef char bool_t;
typedef unsigned short __uint16_t ;
typedef __uint32_t KB_t;
typedef __uint16_t MB_t;
typedef __uint32_t MHz_t;
typedef __uint32_t Byte_t;

typedef int (*modInit_f)(bool_t);


extern int MAX_PCP_METRICS ;
extern char *metrics[];

#endif
