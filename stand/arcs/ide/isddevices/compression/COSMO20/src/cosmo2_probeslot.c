/*
 * which_slot.c  
 *
 * Cosmo2 gio 1 chip hardware definitions
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include "cosmo2_defs.h"

extern __uint64_t *cosmo2brbase;

__uint32_t cos2_ProbeSlot(void ) ;
__uint32_t cosmo2_bridge_port;

mgv_write_ind ()
{
}
mgv_read_ind ()
{
}

#if defined(IP30) 
#else
__uint32_t cos2_ProbeSlot( void )
{
       __uint32_t found = 0, temp;

	int i = 0;

	for ( i = 0; i < 3 ; i++) {

        	cosmobase = gio_slot[i].base_addr;

		 if ( (temp = (__uint32_t) cgi1_read_reg(cosmobase)) 
							!= cgi1_cosmo_id_val) {
			found = 0;
			msg_printf( SUM, 
				"i %d cosmobase %x read %x id %x \n", 
				i, (__paddr_t) cosmobase , temp, cgi1_cosmo_id_val );
		}
		else
		{
			cgi1ptr = (cgi1_reg_t *) cosmobase ;
			found = 1; i = 5; 
			msg_printf( SUM, "cosmo found with base address %x \n",  cosmobase);
			break ; 
		}
	}

	return (found);

}
#endif

#if defined(IP30)

struct giobr_bridge_config_s cosmo2_srv_bridge_config =
{
    GIOBR_ARB_FLAGS | GIOBR_DEV_ATTRS | GIOBR_RRB_MAP | GIOBR_INTR_DEV,
    GIOBR_ARB_RING_BRIDGE,
    /* even devices, 0,2,4,6        odd devices 1,3,5,7 */
     {  DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_DATA,
    DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_DATA,
    DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_CMD,
    DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_CMD},
     {  1,                              3,
        1,                              3,
        3,                              1,
        3,                              1},
    {1,0,0,0,0,0,0,0}
};

 __uint32_t
c2_br_rrb_alloc(gioio_slot_t slot, int more)
{
    int                     oops = 0;
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp, bit;

    if (slot & 1) {
      BR_REG_RD_32(BRIDGE_ODD_RESP,0xffffffff,reg);
    }
    else {
      BR_REG_RD_32(BRIDGE_EVEN_RESP,0xffffffff,reg);
    }

    tmp = (0x88888888 & ~reg) >> 3;
    while (more-- > 0) {
        bit = LSBIT(tmp);
        if (!bit) {
            oops = 1;
            break;
        }
        tmp &= ~bit;
        reg = ((reg & ~(bit * 15)) | (bit * (8 + slot / 2)));
    }
    if (slot & 1) {
      BR_REG_WR_32(BRIDGE_ODD_RESP,0xffffffff,reg);
    }
    else {
      BR_REG_WR_32(BRIDGE_EVEN_RESP,0xffffffff,reg);
    }

    return oops;
}
#endif
int
cosmo2bridge(void)
{
#if defined(IP30)
    int                     rv;
    int                     n;
    bridgereg_t             intr_device = 0;
    bridgereg_t             write_val, sav_val;
    giobr_bridge_config_t   bridge_config = &cosmo2_srv_bridge_config;
	__uint64_t value;

/* Set the cosmo2 board in SR mode */
	value = cgi1_read_reg (cosmobase + cgi1_board_con_reg_o );
	cgi1_write_reg( (cgi1_GPO4 | cgi1_GPO0 | cgi1_SpeedRacer) | value, cosmobase +cgi1_board_con_reg_o );
	value = cgi1_read_reg( cosmobase +cgi1_board_con_reg_o );
	msg_printf(SUM, " BCR of cosmo2 0x%x \n", value);

/*** Initialize bridge for VGI1 DMA ***/
    msg_printf(DBG,"Entering c2_initBridge\n");
    bridge_xbow_port = cosmo2_bridge_port; 
	msg_printf(DBG, " bridge_xbow_port %d \n", bridge_xbow_port);
    /*
     * RRB's are initialized to all zeros
     */
    BR_REG_WR_32(BRIDGE_EVEN_RESP,0xffffffff,0x0);
    msg_printf(DBG,"Wrote in BRIDGE_EVEN_RESP \n");
    BR_REG_WR_32(BRIDGE_ODD_RESP,0xffffffff,0x0);
    msg_printf(DBG,"Wrote in BRIDGE_ODD_RESP \n");

    for (n = 0; n < 8; n++) {
        /*
         * Set up the Device(x) registers
         */
        if (bridge_config->mask & GIOBR_DEV_ATTRS) {
            write_val = GIOBR_DEV_CONFIG(bridge_config->device_attrs[n], n);
            BR_REG_WR_32(BRIDGE_DEVICE(n),0x00ffffff, write_val);
    msg_printf(DBG,"Write to BRIDGE_DEVICE(%d) %x \n",n, write_val);
            BR_REG_RD_32(BRIDGE_DEVICE(n),0x00ffffff, sav_val);
    msg_printf(DBG,"Read from BRIDGE_DEVICE(%d) %0x \n",n,sav_val);
        }
        /*
         * setup the RRB's, assumed to have been all cleared
         */
        if (bridge_config->mask & GIOBR_RRB_MAP) {
            rv = c2_br_rrb_alloc(n, bridge_config->rrb[n]);
            if (rv) return(1); /*** Warn - check return value ***/
        }
        if (bridge_config->mask & GIOBR_INTR_DEV) {
            intr_device &= ~BRIDGE_INT_DEV_MASK(n);
            intr_device |= BRIDGE_INT_DEV_SET(bridge_config->intr_devices[n], n);
        }

     }


    if (bridge_config->mask & GIOBR_ARB_FLAGS) {
        BR_REG_RD_32(BRIDGE_ARB,BRIDGE_ARB_MASK,sav_val);
        write_val = bridge_config->arb_flags | sav_val;
        BR_REG_WR_32(BRIDGE_ARB,BRIDGE_ARB_MASK,write_val);
    }
    if (bridge_config->mask & GIOBR_INTR_DEV) {
        /* BR_REG_WR_32(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,intr_device); */
        BR_REG_WR_32(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,0x184000);
        BR_REG_RD_32(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_INT_DEVICE - %0x\n", sav_val);
    }

        /*** Write widget id number to be 0xb ***/
        BR_REG_RD_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
        write_val = (sav_val & ~0xf) | 0xb;
        BR_REG_WR_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,(0x7f0043f << 4) | (cosmo2_bridge_port & 0xf) );
        BR_REG_RD_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_WID_CONTROL - %0x\n", sav_val);

        /*** Write dir map and int_enable registers ***/
        BR_REG_WR_32(BRIDGE_DIR_MAP,BRIDGE_DIR_MAP_MASK,0x800000);
        BR_REG_RD_32(BRIDGE_DIR_MAP,BRIDGE_DIR_MAP_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_DIR_MAP - %0x\n", sav_val);

        BR_REG_WR_32(BRIDGE_INT_ENABLE,BRIDGE_INT_ENABLE_MASK,0x0);
        BR_REG_RD_32(BRIDGE_INT_ENABLE,BRIDGE_INT_ENABLE_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_INT_ENABLE - %0x\n", sav_val);
        return(0);

#endif
}

void
cosmo2_dumpbridge(void)
{
	msg_printf(SUM, "Bridge Status 0x%x\n", *(cosmo2brbase+0x8) );
	msg_printf(SUM, "Control 0x%x\n", *(cosmo2brbase+0x20) );
	msg_printf(SUM, "DIRMAP  0x%x\n", *(cosmo2brbase+0x80) );
	msg_printf(SUM, "INT Status 0x%x\n", *(cosmo2brbase+0x100)) ;
	msg_printf(SUM, "INT MODE REG 0x%x\n", *(cosmo2brbase+0x118)) ;
	msg_printf(SUM, "DEV Reg 0x%x\n", *(cosmo2brbase+0x120)) ;
}

extern void mgras_probe_all_widgets(void);

__uint32_t cos2_ProbeSlot1(void)
{
#if defined(IP30) 
#if 0
    xbowreg_t   in_port;
#endif
    __uint32_t  found = 0, i, value, j, mfg , part;

	msg_printf(DBG, " calling mgras_probe_all_widgets\n");
	mgras_probe_all_widgets ();
	msg_printf(DBG, " after mgras_probe_all_widgets\n");

	for ( i = 0 ; i < MAX_XTALK_PORT_INFO_INDEX; i++) {
	/* check if cosmo2 is plugged-in */
	/* How do I find this */

		if ((mg_xbow_portinfo_t[i].present) && (mg_xbow_portinfo_t[i].alive)) { 
			mfg = mg_xbow_portinfo_t[i].mfg;
			part = mg_xbow_portinfo_t[i].part;
		if ((mfg == BRIDGE_WIDGET_MFGR_NUM) && (part == BRIDGE_WIDGET_PART_NUM)) {

		msg_printf(DBG, " port %d \n", mg_xbow_portinfo_t[i].port ); 

		*(__paddr_t *)((__paddr_t) mg_xbow_portinfo_t[i].base + 0x20 - 0x4 ) = 
			mg_xbow_portinfo_t[i].port |  (0x4 << 12) | (1 << 9) ;

			cosmobase = gio2xbase((mg_xbow_portinfo_t[i].port), cgi1_slot0_base)  ;
		msg_printf(DBG, " cosmobase %x \n", (__paddr_t) cosmobase ); 
			if  (*(__uint32_t *)((__paddr_t)cosmobase+4)  == cgi1_cosmo_id_val)
				{ 
					cosmobase = (__paddr_t*)((__paddr_t)cosmobase + 0) ; 
					found = 1; 
					cgi1ptr = (cgi1_reg_t *) cosmobase ; 
					cosmo2brbase = (__paddr_t *)K1_MAIN_WIDGET(mg_xbow_portinfo_t[i].port); 
					msg_printf(DBG, " cosmo2brbase %x\n",(__paddr_t) cosmo2brbase ); 
					cosmo2_bridge_port = mg_xbow_portinfo_t[i].port;
					return (1); 
				}
			cosmobase = gio2xbase((mg_xbow_portinfo_t[i].port), cgi1_slot1_base);   
		msg_printf(SUM, " cosmobase %x \n", (__paddr_t) cosmobase );
			if ( *(__uint32_t *)((__paddr_t)cosmobase+0)  == cgi1_cosmo_id_val)
				{ 
					cosmobase = cosmobase + 0 ; 
					cgi1ptr = (cgi1_reg_t *) cosmobase ;
					cosmo2brbase = (__paddr_t *)K1_MAIN_WIDGET(mg_xbow_portinfo_t[i].port);
					msg_printf(DBG,"cosmo2brbase %x\n",(__paddr_t) cosmo2brbase ); 
					cosmo2_bridge_port = mg_xbow_portinfo_t[i].port;
					found = 1; 
					return (1); 
				}
		cosmobase = gio2xbase((mg_xbow_portinfo_t[i].port),cgi1_slot_gfx_base);   
		msg_printf(SUM, " cosmobase %x \n", (__paddr_t) cosmobase ); 
			if ( *(__uint32_t *)((__paddr_t)cosmobase+0) == cgi1_cosmo_id_val)
				{ 
					cosmobase = cosmobase + 0 ; 
					found = 1; 
					cosmo2brbase = (__paddr_t *)K1_MAIN_WIDGET(mg_xbow_portinfo_t[i].port); 
					msg_printf(DBG,"cosmo2brbase %x\n",(__paddr_t) cosmo2brbase ); 
					cosmo2_bridge_port = mg_xbow_portinfo_t[i].port;
					return (1); 
				}
/*
			cosmobase = cosmobase + 0;
		if (*(cosmobase) == cgi1_cosmo_id_val) { found = 1; break; }
*/
#if 0		/* in_port currently unused */
			in_port = mg_xbow_portinfo_t[i].port;
#endif
			
			found = 1;
		}
	  }
	}

    if (found == 0) {
    }

#else
       __uint32_t found = 0;
		volatile unsigned long *probe_addr;
    	unsigned long prod_id;


    int i = 0;


    for ( i = 0; i < 3 ; i++) {
            cosmobase = gio_slot[i].base_addr;

        /*
         * Don't know if the slot is in 32 or 64 bit mode.
         * So do a 32-bit read at the 32-bit address offset of
         * the prodid. This should give back prod id regardless
         * of whether slot is in 32 or 64 bit mode.
         */
		msg_printf(DBG, "Probing with badaddr \n");
        probe_addr = (unsigned long *)cosmobase + 1;
        if (badaddr(probe_addr, sizeof(unsigned long)))
            continue;
		msg_printf(DBG, " checking the prod_id \n");

        prod_id = *probe_addr;
        if (prod_id != cgi1_cosmo_id_val) 
			continue ;

            msg_printf( SUM,
                "i %d cosmobase %x prod_id %x id %x \n",
                i, (__paddr_t) cosmobase , prod_id, cgi1_cosmo_id_val );
		found = 1;
		cgi1ptr = (cgi1_reg_t *) cosmobase ;
		return (found) ;
	}
#endif
		return (found);
}


void c2_peek (int argc, char **argv)
{
#if defined(IP30)

    __uint32_t offset, rcv, mask = ~0x0;
    __uint32_t bad_arg = 0;

    msg_printf(DBG, "Entering c2_peek\n");

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &offset);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &offset);
                    }
                    break;
                default:
            bad_arg++;
                    break;
    }
    argc--; argv++;
    }

    if ( bad_arg || argc ) {
    msg_printf(SUM,
       " Usage: c2_peek -o <reg offset>  \n");
    return;
    }

    msg_printf(SUM, "absolute address 0x%x \n", ((__paddr_t) mg_xbow_portinfo_t[0].base + offset) );

	rcv = *(__uint32_t *)((__paddr_t) cosmobase + offset + 0) ;	

    msg_printf(SUM, "reg offset 0x%x value 0x%x \n", offset, rcv);

    msg_printf(DBG, "Leaving br_peek...\n");
#endif
}

void c2_poke(int argc, char **argv)
{
#if defined(IP30)
    __uint32_t offset, val, mask = ~0x0;
    __uint32_t bad_arg = 0;

    msg_printf(DBG, "Entering c2_poke...\n");

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &offset);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &offset);
                    }
                    break;
                case 'v':
                    if (argv[0][2]=='\0') {
                        atobu(&argv[1][0], &val);
                        argc--; argv++;
                    } else {
                        atobu(&argv[0][2], &val);
                    }
                    break;
                default:
            bad_arg++;
                    break;
    }
    argc--; argv++;
    }

    if ( bad_arg || argc ) {
    msg_printf(SUM,
       " Usage: c2_poke -o <reg offset> -v <value> \n");
    return;
    }

	*(__uint32_t *)((__paddr_t) cosmobase + offset + 0 ) = val;

    msg_printf(DBG, "Leaving c2_poke...\n");

#endif
}

