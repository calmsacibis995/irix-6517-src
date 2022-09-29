/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986-1996 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#ifndef __SN1_PRIVATE_H__
#define __SN1_PRIVATE_H__

/*
 * This file contains definitions that are private to the ml/SN0
 * directory.  They should not be used outside this directory.  We
 * reserve the right to change the implementation of functions and
 * variables here as well as any interfaces defined in this file.
 * Interfaces defined in sys/SN are considered by others to be
 * fair game.
 */

#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xtalk_private.h>

#include "../sn_private.h"

/* SN1asm.s */
void bootstrap(void);

#endif __SN1_PRIVATE_H__

