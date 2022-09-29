#ifndef _ODSY_KERN_SIM_H_
#define _ODSY_KERN_SIM_H_
/*
** Copyright 1997, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
*/


#define PL_ZERO ((pl_t) 0) /* ddi suggests this should exist */

#if defined(ODSY_SIM_KERN)


#define ODSY_SIM_NR_PSEUDO_BOARDS 1
#ifdef ODSY_IS_NOT_TEXTPORT
#define ODSY_SIM_MAX_DELAYED_PIOS (8<<10) /* fairly large, 
					  ** needs to be big enough
					  ** to download microcode 
					  ** and do GFX_INITIALIZE, GFX_START
					  */

#else
#define ODSY_SIM_MAX_DELAYED_PIOS (8<<13)
#endif

/* 
** a little trickery so we can gob up writes. essentially turns a read or write spec'n into
** a tuple of (headnr,offset,width) that can be passed on to the real read/write fns.
*/
   #define ODSY_SIM_PIPEOFFSET(field) ((__uint32_t)((unsigned char*)&(((odsy_hw*)0)->field) \
                                                     - (unsigned char*)(odsy_hw*)0))
   #define ODSY_SIM_PIPEWIDTH(field)  /*(sizeof(hw->pipefield))*/ (sizeof(((odsy_hw*)0)->field))

   extern unsigned __odsy_sim_hw_touch_rc[];
   #define ODSY_CLEAR_HW_TOUCH_ERROR(hw)       __odsy_sim_hw_touch_rc[((__uint64_t)(hw))-1] = 0
   #define ODSY_CHECK_HW_TOUCH_ERROR(hw)       (__odsy_sim_hw_touch_rc[((__uint64_t)(hw))-1])

   #define ODSY_writePipePIO(hw,pipefield,val) __odsy_sim_hw_touch_rc[((__uint64_t)hw)-1] |= \
                                                             (odsySIMwritePipePIO((__uint32_t)(__uint64_t)hw, \
                                                                    ODSY_SIM_PIPEOFFSET(pipefield), \
                                                                    ODSY_SIM_PIPEWIDTH(pipefield),  \
                                                                    (__uint64_t) val,0/*NULL*/))

   #define ODSY_readPipePIO(hw,pipefield,var)  __odsy_sim_hw_touch_rc[((__uint64_t)hw)-1] |= \
							     (odsySIMreadPipePIO((__uint32_t)(__uint64_t)hw,  \
                                                                   ODSY_SIM_PIPEOFFSET(pipefield),  \
                                                                   ODSY_SIM_PIPEWIDTH(pipefield),   \
                                                                   (void*)&(var)))

   #define ODSY_PIO_TIMEOUT_INTERVAL (170*HZ)

   /* prototypes for read/write sim implementors */

     int odsySIMwritePipePIO(__uint32_t headnr,__uint32_t offset, int width, __uint64_t val,odsy_sim_rdwr *rwp);
     int odsySIMreadPipePIO(__uint32_t headnr,__uint32_t offset, int width, void *rval);
     int odsySIMwritePipeString(__uint32_t headnr,char *string, int is_directive);

#define ODSY_DEFINE_DIRECTIVES 1
#include "odsy_sim_wrapper_directives.h"

#else
   #define ODSY_CLEAR_HW_TOUCH_ERROR(hw)       

# if SLOW_PIO_TEST
   #define ODSY_WRITEPIO_DELAY 500
   #define ODSY_writePipePIO(hw,pipefield,val) { DELAY(ODSY_WRITEPIO_DELAY); ((odsy_hw*)hw)->pipefield = val; }
   #define ODSY_READPIO_DELAY 10
   #define ODSY_readPipePIO(hw,pipefield,var)  { DELAY(ODSY_READPIO_DELAY); var = ((odsy_hw*)hw)->pipefield; }
# else
   #define ODSY_writePipePIO(hw,pipefield,val) ((odsy_hw*)hw)->pipefield = val 
   #define ODSY_readPipePIO(hw,pipefield,var)  var = ((odsy_hw*)hw)->pipefield
# endif


#undef ODSY_DEFINE_DIRECTIVES
#include "odsy_sim_wrapper_directives.h"

#endif

#endif




