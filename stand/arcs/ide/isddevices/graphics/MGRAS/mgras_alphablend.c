/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sgidefs.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#ifdef MGRAS_DIAG_SIM
#include "mgras_sim.h"
#else
#include "ide_msg.h"
#endif

extern mgras_hw *mgbase;
extern __uint32_t numTRAMs;
extern __uint32_t numRE4s;

int
_mg_alphablend(void)
{
__uint32_t rssnum=4, cfifo_val, cfifo_status, height, scanWidth, cmd, pix_cmd;

cfifo_val = 32;

/* start with the triangles here */
hq_HQ_RE_WrReg_1(0x204,0x47401e00,0x47401400);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_1(0x200,0x47401400,0x47401400);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47401400,0x47401400);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000054);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x5990c9c3);
hq_HQ_RE_WrReg_1(0x204,0x47401e00,0x47401400);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401400,0x47401e00);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x262,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x264,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000001);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47401400,0x47401400);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x262,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x264,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000001);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x58840000);
hq_HQ_RE_WrReg_1(0x204,0x47403200,0x47402800);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_1(0x200,0x47401400,0x47401400);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47403200);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47402800,0x47402800);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47403200);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000046);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x78810803);
hq_HQ_RE_WrReg_1(0x204,0x47403200,0x47402800);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401400,0x47403200);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x262,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x264,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000001);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47402800,0x47402800);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47403200);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x262,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x264,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000001);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x58840000);
hq_HQ_RE_WrReg_1(0x204,0x47404600,0x47403c00);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_1(0x200,0x47401400,0x47401400);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47404600);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47403c00,0x47403c00);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47404600);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47405000,0x47405000);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47405a00);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000038);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x788288c1);
hq_HQ_RE_WrReg_1(0x204,0x47405a00,0x47405000);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401400,0x47405a00);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x262,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x264,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000001);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47405000,0x47405000);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47405a00);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x262,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x264,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000001);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x58840000);
hq_HQ_RE_WrReg_1(0x204,0x47406e00,0x47406400);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_1(0x200,0x47401400,0x47401400);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47406e00);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47406400,0x47406400);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47406e00);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00807879);
hq_HQ_RE_WrReg_1(0x260,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0xffe66800,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000027);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x59845311);
hq_HQ_RE_WrReg_1(0x204,0x47406e00,0x47406400);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401400,0x47406e00);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x262,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x264,0x00199800,0x00199800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000001);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x47406400,0x47406400);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47401e00,0x47401e00);
hq_HQ_RE_WrReg_1(0x202,0x47401e00,0x47406e00);
hq_HQ_RE_WrReg_1(0x25c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25e,0x00000000,0x00a09697);
hq_HQ_RE_WrReg_1(0x260,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x262,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x264,0x00333000,0xffe66800);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }


	return 0;
}
