
/*****************************************************************************
* mgv_edh.c: description
*
* $Revision: 1.1 $
*
* Copyright 1996 Silicon Graphics, Inc.
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
*****************************************************************************/

#ifdef IP30

#include "mgv_diag.h"

void mgv_select_edh_part(int,int,int,int);

/*============================================================*
 *                  Register access routines                  *
 *                                                            *
 * channel is video channel ( 0 or 1 )                        *
 * part is input or output ( 0 or 1 )                         *
 *============================================================*/

/*
 * Only one of the four EDH parts is addressable
 * at a time. This routine selects the appropriate
 * one to talk to.
 *
 * It also selects passthru and test modes of the part.
 * In passtru, the input anc. data packet is passed through
 * rather than being regenerated. The error flags on the IIC bus
 * are not valid.
 *
 */

#define NUM_EDH_PARTS 4
#define MGV_EDH_PART_INDEX(channel, part)  (channel + 2*part)

typedef struct edh_part {
        int passthru;
        int testmode;
        int is_selected;
} t_edh_part;

t_edh_part mgv_edh_part_state[] =
{
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0}
};

int edh_indirs[] =
{
    GAL_EDH_FLAGS1,
    GAL_EDH_FLAGS2,
    GAL_EDH_FLAGS3,
    GAL_EDH_FLAGS4
};

int
mgv_edh_flag_reg(int part_ix )
{
    int val;
    GAL_IND_R1( mgvbase, edh_indirs[part_ix], val);
    /*** warn check this routine ***/
    /*** what it does ***/
    return(val);
}

/*
 * Connect the requested part to I2C address 1; all others to I2C address 0.
 * Also sets passthru or test mode.  Leaves passthru/testmode of other
 * EDH parts in their current state.
 *
 * Updates mgv_edh_part_state table.
 *
 */
void
mgv_select_edh_part( int channel, int part,
                                        int passthru, int test )
{
    /*
     * Part Index table
     * 0: channel 1 input
     * 1: channel 2 input
     * 2: channel 1 output
     * 3: channel 2 output
     */

    int addr_mask, part_mask, i;
    int part_index = MGV_EDH_PART_INDEX(channel,part);

    addr_mask = 0;

    /* do testmode and passthru */
    for(i=0;i<NUM_EDH_PARTS;i++) {
        if ( i == part_index ) {
            if ( test ) {
                part_mask = 3;
                mgv_edh_part_state[i].testmode = 1;
                mgv_edh_part_state[i].passthru = 0;
            }
            else if ( passthru ) {
                part_mask = 2;
                mgv_edh_part_state[i].testmode = 0;
                mgv_edh_part_state[i].passthru = 1;
            }
            else {
                part_mask = 1;
                mgv_edh_part_state[i].testmode = 0;
                mgv_edh_part_state[i].passthru = 0;
                mgv_edh_part_state[i].is_selected = 1;
            }
        }
        else {
            if ( mgv_edh_part_state[i].testmode )
                part_mask = 3;
            else if ( mgv_edh_part_state[i].passthru )
                part_mask = 2;
            else {
                part_mask = 0;
                mgv_edh_part_state[i].is_selected = 0;
            }
        }
        addr_mask |= ( part_mask << (2*i) );
    }
    GAL_IND_W1( mgvbase, GAL_EDH_DEVADDR, addr_mask);
}


/*
 * Read Error flags register.
 */

int
mgv_edh_flags( int channel, int part )
{
    int flags_l, flags_h;

    flags_l = shadow_edh_iic_read[EDH_ERROR0_FLAGS_READ];
    flags_h = shadow_edh_iic_read[EDH_ERROR1_FLAGS_READ];
    flags_h &= 0x00ff;
    flags_h <<= 8;
    flags_l &= 0x00ff;
    flags_l |= flags_h;

    return flags_l;
}


/*
 * Read error count register
 */
unsigned long
mgv_edh_error_count(int channel, int part)
{
    unsigned long error_count;
    int low,med,hi;

    hi  = shadow_edh_iic_read[EDH_STD_ERR2_CNT_READ];
    med = shadow_edh_iic_read[EDH_ERROR1_CNT_READ];
    low = shadow_edh_iic_read[EDH_ERROR0_CNT_READ];

    error_count = (hi & 0x00F8) << 13
               | ((med & 0x00ff) << 8 )
               | (low & 0x00ff);

    return error_count;
}

#define EDH_TVST_NTSC 0
#define EDH_TVST_PAL  1


#endif

