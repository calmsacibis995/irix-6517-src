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

#include "multi.h"

/*
 * Realtime loops
 */
volatile double global_res = 0.0;

int
processA_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processB_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processC_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processD_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processE_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processF_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processK1_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}


int
processK2_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}

int
processK3_loop(pdesc_t* pdesc)
{
        double res = 2;
        int i;
        
        for (i = 0; i < pdesc->nloops; i++) {
                res = res * log(res) - res * sqrt(res);
        }

        global_res = res;

        if (++pdesc->ccount >= pdesc->xloops) {
                return (1);
        } else {
                return (0);
        }
}


