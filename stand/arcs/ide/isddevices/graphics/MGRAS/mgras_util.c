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
#include <sys/types.h>
#include "sys/sbd.h"
#include "ide_msg.h"
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include <sys/mgrashw.h>
#include "mgras_diag.h"

#if (defined(MFG_USED) && defined(IP30))
extern	void	_send_bootp_msg(char *msg);
#endif

extern __uint32_t mg_probe(void);
extern __int32_t _xtalk_nic_probe(__int32_t, char *, __int32_t);
void
__write_rss_reg_noprintf(__uint32_t regnum, __uint32_t data, __uint32_t mask)
{
        __uint32_t __regOffset, __cmd;                           
        __regOffset = (RSS_CTX_CMD_BASE) | HQ_RSS_WRITE_REG | (regnum); 
        __cmd = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, __regOffset, 4);
        HQ3_PIO_WR(CFIFO1_ADDR, __cmd, 0xffffffff);              
        HQ3_PIO_WR(CFIFO1_ADDR, data, mask);                    
}

void
__write_rss_reg_printf(__uint32_t regnum, __uint32_t data, __uint32_t mask)
{
        __uint32_t __regOffset, __cmd;                           
        __regOffset = (RSS_CTX_CMD_BASE) | HQ_RSS_WRITE_REG | (regnum); 
        __cmd = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, __regOffset, 4);
	msg_printf(DBG, "cmd 0x%x; __regOffset 0x%x\n");
        HQ3_PIO_WR(CFIFO1_ADDR, __cmd, 0xffffffff);              
        HQ3_PIO_WR(CFIFO1_ADDR, data, mask);                    
}

void
__write_rss_reg_dbl(__uint32_t regnum, __uint32_t data_hi, __uint32_t data_lo)
{
        __uint32_t __regOffset, __cmd;                           
        __regOffset = (RSS_CTX_CMD_BASE) | HQ_RSS_WRITE_REG | HQ_RSS_EXTENDED_REG | (regnum); 
        __cmd = BUILD_CONTROL_COMMAND( 0, 0, 1, 0, __regOffset, 8);
        HQ3_PIO_WR(CFIFO1_ADDR, __cmd, 0xffffffff);              
        HQ3_PIO_WR64(CFIFO1_ADDR, data_hi, data_lo, ~0x0);                    
}

void
_hq3_pio_wr_re(__uint32_t offset, __uint32_t val, __uint32_t mask)
{
	__uint32_t regnum;
	regnum = RSS_HQ_SPACE((offset - RSS_BASE));
	WRITE_RSS_REG(0, regnum, val, mask);
}

void
_hq3_pio_wr_re_ex(__uint32_t offset, __uint32_t val, __uint32_t mask)
{
	__uint32_t regnum;
	regnum = RSS_HQ_SPACE((offset - RSS_BASE));
	WRITE_RSS_REG(0, (regnum | HQ_RSS_LOAD_AND_XCT), val, mask);
}


void
cmap_compare(char *str, int which, int addr, unsigned int exp,
	     unsigned int rcv, unsigned int mask, __uint32_t *errors)
{
        msg_printf(DBG, "%s Cmap %d addr: %x exp: %x, rcv: %x\n" ,
		   str,which,addr,exp & mask,rcv & mask);
        if ((exp & mask)  != (rcv & mask)) {
        	msg_printf(ERR, "%s Cmap %d addr: %x exp: %x, rcv: %x\n" ,
			   str, which, addr, (exp & mask), (rcv & mask) );
		(*errors)++;
        }
}

void
COMPARE64(char *str, __uint64_t addr, __uint64_t exp, __uint64_t rcv,
        __uint64_t mask, __uint32_t *errors)
{

        if ((exp & mask)  != (rcv & mask) ) {
                msg_printf(ERR,"%s failed at addr:0x%x exp: 0x%llx, rcv: 0x%llx\n",
                           str, addr, (exp & mask), (rcv & mask));
                (*errors)++;
        }
}

void
compare(char *str, int addr, unsigned int exp, unsigned int rcv,
	unsigned int mask, __uint32_t *errors)
{
        msg_printf(DBG, "%s addr: %x exp: %x, rcv: %x\n" ,str, addr,
		   (exp & mask) , (rcv & mask) );

        if ((exp & mask)  != (rcv & mask) ) {
                msg_printf(ERR, "%s failed at addr: %x exp: %x, rcv: %x\n",
			   str, addr, (exp & mask), (rcv & mask));
                (*errors)++;
        }
}

void
compare_report6(char *str, int addr, unsigned int exp, unsigned int rcv,
	unsigned int mask, __uint32_t *errors)
{
        msg_printf(6, "%s addr: %x exp: %x, rcv: %x\n" ,str, addr,
		   (exp & mask) , (rcv & mask) );

        if ((exp & mask)  != (rcv & mask) ) {
                msg_printf(ERR, "%s failed at addr: %x exp: %x, rcv: %x\n",
			   str, addr, (exp & mask), (rcv & mask));
                (*errors)++;
        }
}

int
wait_for_cfifo(int count, int time_out_value)
{
	__uint32_t _time_out = time_out_value;
	__uint32_t level;
	int status = 0;

	msg_printf(DBG, "Checking hq cfifo...\n");
	do {
	     mgras_cfifoGetLevel(mgbase, level);
	     _time_out--;
	} while (((CFIFO_MAX_LEVEL - level) < count) && _time_out);

	if (!_time_out)
		status = CFIFO_TIME_OUT;

	return status;
}

int hq3_pio_rd_re(int offset, __uint32_t mask, __uint32_t *actual,
		  __uint32_t exp, int status)
{
	__paddr_t	Addr; 
	__uint32_t 	cntr;

	cntr = 0x0;
	*actual = 0xdeadbeef;

	if (status) {
		Addr = (__paddr_t)mgbase;
		Addr |= (RSS_BASE + (STATUS << 2));
		while ((*actual & 0x700) != 0x100) {
			if (badaddr_val((volatile void *)Addr, sizeof(__uint32_t), actual)) {
#if HQ4
				msg_printf(SUM, "Can't read the RE4's STATUS register.\n");
#else
				msg_printf(SUM, "GIO Bus Timeout. Can't read the RE4's STATUS register.\n");
#endif
				return (1);
			}
			cntr++;
			if (cntr == 500) {
		   	    if (exp != NO_PRINT_OUT) {
				msg_printf(SUM, "RSS busy. RE4 Status reg = 0x%x (",*actual&0x7ff);
				if ((*actual & 0x100) == 0)
	   			    msg_printf(SUM, " CmdFIFO not empty,");
				if (*actual & 0x200) 
			   	    msg_printf(SUM, " RE4 Busy,");
				if (*actual & 0x400)
			   	    msg_printf(SUM, " PP1-A or PP1-B Busy");
				msg_printf(SUM," )\n");
				msg_printf(SUM,"Data read may not be valid\n");
			    }
			    *actual = 0x100;
			}
		}
	}

	Addr = (__paddr_t)mgbase;
	Addr |= offset;
	if (badaddr_val((volatile void *)Addr, sizeof(__uint32_t), actual)) {
#if HQ4
		msg_printf(ERR, "Can't talk to the device at addr: 0x%x\n", Addr);
#else
		msg_printf(ERR, "GIO Bus Timeout. Can't talk to the device at addr: 0x%x\n", Addr);
#endif
		return (1);
	}
	*actual &= mask;
	msg_printf(DBG, "PIO_RD: Addr = 0x%x, mask = 0x%x, Rcv = 0x%x\n", Addr, mask, *actual);

	return (0);
}

void
continuity_check(_Test_Info *Test, int CmapId, __uint32_t Patrn, int errors)
{
	if (errors)
                msg_printf(ERR, "%s Failed on Cmap%d Red= 0x%x Green=0x%x "
			   "Blue=0x%x \n", Test->TestStr, CmapId,
			   (Patrn >> 22) & 0xff, (Patrn >> 12) & 0xff,
			   (Patrn >> 2) & 0xff);
	else
		msg_printf(DBG,"Cmap%d Red =0x%x Green=0x%x Blue=0x%x Passed\n",
			   CmapId, (Patrn >> 22) & 0xff, (Patrn >> 12) & 0xff,
			   (Patrn >> 2) & 0xff);
}

void
mgras_cmapToggleCblank(mgras_hw *mgbase, int OnOff)
{
       	mgras_xmapSetAddr(mgbase, 4);
	if (OnOff) {				/* CBlank is On */
        	mgras_xmapSetDIBdata(mgbase, (0x3ff | (1 << 14)));
	}
	else
        	mgras_xmapSetDIBdata(mgbase, 0x3ff);
}

int 
report_passfail(_Test_Info *Test, int errors)
{
#if (defined(MFG_USED) && defined(IP30))
	char msg[256];

	strcpy(msg, Test->TestStr);
#endif

	if (!errors) {
		msg_printf(SUM, "     %s Test passed.\n" ,Test->TestStr);
#if (defined(MFG_USED) && defined(IP30))
		strcat(msg, ".pass");
		_send_bootp_msg(msg);
#endif
		return 0;
	}
	else {
		msg_printf(ERR, "**** %s Test failed.  ErrCode %s Errors %d\n",
			   Test->TestStr, Test->ErrStr, errors);
#ifdef IP30
		if (numRE4s == 2)
		msg_printf(SUM, "The faulty FRU is %s\n", "GM20");
		else
		msg_printf(SUM, "The faulty FRU is %s\n", "GM10");
#ifdef MFG_USED
		strcat(msg, "_");
		strcat(msg, Test->ErrStr);
		strcat(msg, ".fail");
		_send_bootp_msg(msg);
#endif
#endif

		return -1;
	}
}

/*ARGSUSED1*/
void
mgras_gfx_setup(int index, int numge)
{
	__uint32_t pp1_ctrl , dac_ctrl , cmapall_ctrl , cmap0_ctrl;
	__uint32_t cmap1_ctrl , vc3_ctrl, bdver_ctrl, pcd_ctrl;

#if !HQ4
	unsigned int bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) <<
				mgras_baseindex(mgbase);
        extern u_int _gio64_arb;

	if ((_gio64_arb & bits) != bits)
		msg_printf(ERR,"GIO64_ARB not set correctly!\n");

	*(volatile unsigned int *) (PHYS_TO_K1(CPU_ERR_STAT)) = 0x0;
	*(volatile unsigned int *) (PHYS_TO_K1(GIO_ERR_STAT)) = 0x0;
#endif
	flushbus();

	pp1_ctrl        = 0x318c;
	dac_ctrl        = 0x108c;
    cmapall_ctrl    = 0x108c;
	cmap0_ctrl      = 0x108c;
    cmap1_ctrl      = 0x108c;
	vc3_ctrl        = 0x210A;
    bdver_ctrl      = 0x108c;

	mgbase->dcbctrl_pp1       = pp1_ctrl;
    mgbase->dcbctrl_dac       = dac_ctrl;
    mgbase->dcbctrl_cmapall   = cmapall_ctrl;
    mgbase->dcbctrl_cmap0     = cmap0_ctrl;
    mgbase->dcbctrl_cmap1     = cmap1_ctrl;
    mgbase->dcbctrl_vc3       = vc3_ctrl;
    mgbase->dcbctrl_bdvers    = bdver_ctrl;
	mgbase->hq_config	  = (0x5888 | numge);

        msg_printf(DBG, "dcbctrl_pp1 0x%x  Rd 0x%x, Wr 0x%x\n",
		&(mgbase->dcbctrl_pp1), mgbase->dcbctrl_pp1, pp1_ctrl);
        msg_printf(DBG, "dcbctrl_dac 0x%x  Rd 0x%x, Wr 0x%x\n",
		&(mgbase->dcbctrl_dac), mgbase->dcbctrl_dac, dac_ctrl);
        msg_printf(DBG, "dcbctrl_cmapall 0x%x  Rd 0x%x Wr 0x%x \n",
		&(mgbase->dcbctrl_cmapall), mgbase->dcbctrl_cmapall,
		cmapall_ctrl);
        msg_printf(DBG, "dcbctrl_cmap0 0x%x  Rd 0x%x Wr 0x%x \n",
		&(mgbase->dcbctrl_cmap0), mgbase->dcbctrl_cmap0, cmap0_ctrl);
        msg_printf(DBG, "dcbctrl_cmap1 0x%x  Rd 0x%x Wr 0x%x \n",
		&(mgbase->dcbctrl_cmap1), mgbase->dcbctrl_cmap1, cmap1_ctrl);
        msg_printf(DBG, "dcbctrl_vc3 0x%x  Rd 0x%x Wr 0x%x \n",
		&(mgbase->dcbctrl_vc3), mgbase->dcbctrl_vc3, vc3_ctrl);
        msg_printf(DBG, "dcbctrl_bdvers 0x%x  Rd 0x%x Wr 0x%x \n",
		&(mgbase->dcbctrl_bdvers), mgbase->dcbctrl_bdvers, bdver_ctrl);
#if HQ4
    pcd_ctrl      	= 0x1088; /* added new for GAMERA */
    mgbase->dcbctrl_pcd    	  = pcd_ctrl;
    msg_printf(DBG, "dcbctrl_pcd 0x%x  Rd 0x%x Wr 0x%x \n",
		&(mgbase->dcbctrl_pcd), mgbase->dcbctrl_pcd, pcd_ctrl);

    mgras_CFIFOWAIT(mgbase, 3);
    mgras_BFIFOPOLL(mgbase, 1);
    MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT); 

#endif

}

#ifdef MFG_USED
char mfgtestmod[20];
char mfgcpuvolt[20];
char mfggfxvolt[20];

#define run_volt_at_loop1(volt, h, n, l) {\
	if (volt[0] == 'h')\
	count = 2;\
	if (volt[0] == 'n')\
	count = 1;\
	if (volt[0] == 'l')\
	count = 0;\
}

#define run_volt_at_loop2(volt, h, n, l) {\
        if (volt[4] == 'h')\
           count = 2;\
        if ((volt[4] == 'n') || (mfggfxvolt[5] == 'n'))\
        count = 1;\
        if ((volt[4] == 'l') || (mfggfxvolt[5] == 'l'))\
           count = 0;\
}

#define run_volt_at_loop3(volt, h, n, l) {\
	if (volt[8] == 'h')\
	count = 2;\
	if (volt[9] == 'n')\
	count = 1;\
	if (volt[9] == 'l')\
	count = 0;\
}

__uint32_t
get_volt_level(int argc, char **argv)
{
	__uint32_t count = 8, tmp;
	char module[80];
	static __uint32_t testloop = 0;

    	argc--; argv++; 
	strcpy(module, argv[0]);

	msg_printf(DBG, " module %s \n", module);
	msg_printf(DBG, " mfgtestmod %s \n", mfgtestmod);
	msg_printf(DBG, " mfgcpuvolt %s \n", mfgcpuvolt);
	msg_printf(DBG, " mfggfxvolt %s \n", mfggfxvolt);

	if (strcmp(module,"gfx") == 0) {
		switch (testloop) {
		case 0:
			run_volt_at_loop1(mfggfxvolt, h, n, l); 
		break;
		case 1:
			tmp = strlen(mfggfxvolt);
			if (tmp > 4) {
			 run_volt_at_loop2(mfggfxvolt, h, n, l) 
			}
		break;
		case 2:
			tmp = strlen(mfggfxvolt);
			if (tmp > 9) {
				run_volt_at_loop3(mfggfxvolt, h, n, l) ;
			}
		break;
		}
		testloop++;
		if ((testloop%3) == 0)
			testloop = 0;
  		if ((mfgtestmod[0] == 'g') || (mfgtestmod[5] == 'g') || (mfgtestmod[10] == 'g')) 
	return count;
  else
	return 8;
	}
                switch (testloop) {
                case 0:
                        run_volt_at_loop1(mfgcpuvolt, h,n,l);
                break;
                case 1:
                        tmp = strlen(mfgcpuvolt);       
                        if (tmp > 4) { 
                        run_volt_at_loop2(mfgcpuvolt, h,n,l);
                        }
                break;
                case 2:
                        tmp = strlen(mfgcpuvolt);       
                        if (tmp > 9) {
                        run_volt_at_loop3(mfgcpuvolt, h,n,l);
                        }
                break;
                }
	if (strcmp(module,"ip30") == 0) {
  		if ((mfgtestmod[0] == 'i') || (mfgtestmod[5] == 'i') )
		return count;
  	else
		return 8;
	}
        if (strcmp(module,"pm10") == 0) {
  		if (mfgtestmod[2] == '1')  
        		return count;
  		else
        		return 8;
        }
        if (strcmp(module,"pm20") == 0) {
  		if (mfgtestmod[2] == '2')   
        		return count;
  		else
        		return 8;
        }

}

/* make getenv accessible to ide users */
__int32_t ide_getenv(int argc, char **argv)
{
	char *pvar, env_var[80];
	char *funcIP, *finalIP, *configIP, *raIP;
	__int32_t testnum = -1, i, bad_arg = 1;

	char volt[15][20] = {"low", "high", "nom", "low-high", "low-nom", 
		"high-low", "high-nom", "nom-low", "nom-high", "low-high-nom", 
		"low-nom-high", "high-low-nom", "high-nom-low", "nom-low-high",
		"nom-high-low"};
	char testmod[11][20] = {"pm10", "pm20", "ip30", "gfx", "pm10-ip30", 
		"pm20-ip30", "pm10-gfx", "pm20-gfx", "ip30-gfx","pm10-ip30-gfx",
		"pm20-ip30-gfx"};		
	
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 's':
				if (argv[0][2]=='\0') {
					strcpy(env_var, argv[1]);
					pvar = getenv(env_var);
					argc--; argv++;
				} else {
					msg_printf(SUM, 
						"Need a space after -%c\n",
						argv[0][1]);
					return (0);
				}
				bad_arg = 0;
				break;
			default: 
				bad_arg = 1;
				break;
		}
		argc--; argv++;
	}

	if (bad_arg) {
		msg_printf(SUM, "Usage: ide_getenv -s <var>\n");
		return (0);
	}

	if (pvar) {

	   msg_printf(VRB, "%s\n", pvar);

	   if ((strcmp(env_var, "cpuvolt") == 0) || (strcmp(env_var, "gfxvolt") == 0)) 
	   { 
		for (i = 0; i < 15; i++) {
			if (strcmp(volt[i], pvar) == 0) {
				if (strcmp(env_var, "cpuvolt") == 0)
				strcpy(mfgcpuvolt, volt[i]);
				else
				strcpy(mfggfxvolt, volt[i]);
				testnum = i;
				break;
			}
		}
		if (testnum == -1) {
			msg_printf(SUM, "%s is an unrecognized value for %s\n",
				pvar, env_var);
			return (testnum);
		}
	   }
	   if (strcmp(env_var, "testmod") == 0) {
		for (i = 0; i < 11; i++) {
			if (strcmp(testmod[i], pvar) == 0) {
				strcpy(mfgtestmod, testmod[i]);
				testnum = i;
				break;
			}
		}
		if (testnum == -1) {
			msg_printf(SUM, "%s is an unrecognized value for %s\n",
				pvar, env_var);
			return (testnum);
		}
	   }
	   if (strcmp(env_var, "numloops") == 0) {
		return(atoi(pvar));
	   }

	   /*
	    * look at the dlserver env, variable 
	    * if it is not the same as the func server, then we
	    * assume that the system is in ASRS.
	    */
	   if (strcmp(env_var, "dlserver") == 0) {
#ifdef IP30
		funcIP    = getenv("func_ip_addr");
		finalIP   = getenv("final_ip_addr");
		configIP  = getenv("config_ip_addr");
		raIP      = getenv("ra_ip_addr");
#else
		configIP  = "10.1.100.2";
		finalIP   = "10.1.100.3";
		funcIP    = "10.1.100.4";
		raIP      = "10.1.100.5";
#endif
		if ((configIP == NULL) || (finalIP == NULL) ||
		    (funcIP == NULL) || (raIP == NULL)) {
			msg_printf(SUM, "One or more of the mfg server variables are not set in the PROM\n");
			return(128);
		} else {
		   msg_printf(SUM, "The IP addresses of the mfg servers:-\n");
		   msg_printf(SUM, "config\t\t%s\nfinal\t\t%s\nfunc\t\t%s\nra\t\t%s\n",
			configIP,finalIP,funcIP,raIP);
		}
		if (strcmp(pvar, funcIP) == 0) {
			/* the system is in function test; return 0 */
			return(0);
		}
		if (strcmp(pvar, configIP) == 0) {
			/* the system is in system integration; return 1 */
			return(1);
		}
		if (strcmp(pvar, finalIP) == 0) {
			/* the system is in final test; return 2 */
			return(2);
		}
		if (strcmp(pvar, raIP) == 0) {
			/* the system is in EBRA; return 3 */
			return(3);
		}
		/* the system is in ASRS; return 128 */
		return(128);
	   }

	}
	else {
		msg_printf(DBG, "No value found\n");
	}

	return(testnum);
}

__int32_t
mg_nic_check(void)
{
	__int32_t errors = 0, do_compare = 1;
	__uint32_t port;
	char name[10];

#if HQ4
	if (numRE4s == 1) 
		strcpy(name, "GM10");
	else if (numRE4s == 2)
		strcpy(name, "GM20");
	else
		strcpy(name, "Unknown");

	port = mg_probe();

	if (port)
		errors = _xtalk_nic_probe(port, name, do_compare);

#else
	msg_printf(VRB, "NIC checking is only supported on HQ4 systems\n");
#endif

	return (errors);
}
void
ra_menu ()
{
    msg_printf(SUM, " \n");
    msg_printf(SUM, "                  IMPACT BOARD REPAIR MENU \n");
    msg_printf(SUM, " \n");
    msg_printf(SUM, "================================================================= \n");
    msg_printf(SUM, "    mg_setboard         mg_reset       allsetup        ra_menu \n");
    msg_printf(SUM, "----------------------------------------------------------------- \n");
    msg_printf(SUM, "    dactest         vc3test        cmaptest        bkend \n");
#ifdef IP30
    msg_printf(SUM, "    hqtest          tge_test       ge0_test \n");
    msg_printf(SUM, "    retest          repp_test      rdram_mem       rdram_repair \n");
#else
    msg_printf(SUM, "    hq3test         tge_test       ge0_test \n");
    msg_printf(SUM, "    re4test         repp_test      rdram_mem       rdram_repair \n");
#endif /* IP30 */
    msg_printf(SUM, "    tram_mem        tram_repair    tex_draw_test \n");
    msg_printf(SUM, "----------------------------------------------------------------- \n");
#ifdef IP30
    msg_printf(SUM, "    hq_menu        dcb_menu       cmap_menu       vc3_menu \n");
#else
    msg_printf(SUM, "    hq3_menu        dcb_menu       cmap_menu       vc3_menu \n");
#endif /* IP30 */
    msg_printf(SUM, "    dac_menu        ge_menu        rss_menu        dma_menu \n");
#ifndef IP30
    msg_printf(SUM, "----------------------------------------------------------------- \n");
    msg_printf(SUM, "    rss    rss_quick   \n");
#endif /* IP30 */
    msg_printf(SUM, "================================================================= \n");
}
void
hq_menu ()
{
        msg_printf(SUM, " \n");
        msg_printf(SUM, "=====     HQ3 DIAG CMDS MENU      ===== \n");
#ifdef IP30
        msg_printf(SUM, "mg_hq [n]       mg_hq_initcheck        mg_hq_RegWr mg_hq_RegRd \n");
        msg_printf(SUM, "mg_hq_RegVf       \n");
#else
        msg_printf(SUM, "mg_hq3 [n]       mg_hq3_initcheck        mg_hq3_RegWr mg_hq3_RegRd \n");
        msg_printf(SUM, "mg_hq3_RegVf       \n");
#endif /* IP30 */
}
void
dcb_menu ()
{
    msg_printf(SUM, "=====     DCB MENU      ===== \n");
    msg_printf(SUM, "mg_xmapdcbreg mg_vc3internalreg   mg_cmaprev  mg_dacmodereg \n");
}
void
cmap_menu ()
{
    msg_printf(SUM, "===== CMAP DIAG CMDS MENU ===== \n");
    msg_printf(SUM, " \n");
    msg_printf(SUM, "  CMAP commands require cmapID as an argument, cmapID : 0/1 \n");
    msg_printf(SUM, " \n");
    msg_printf(SUM, "cmaptest - test CMAP  \n");
    msg_printf(SUM, "mg_cmaprev    mg_cmapaddrsbus mg_cmapaddrsuniq \n");
    msg_printf(SUM, "mg_cmappatrn  mg_cmapdatabus  mg_cmapuniqtest \n");
    msg_printf(SUM, "mg_pokecmap       mg_peekcmap cmapcrc \n");
}
void
vc3_menu ()
{
    msg_printf(SUM, "===== vc3 DIAG CMDS MENU  ===== \n");
    msg_printf(SUM, " \n");
    msg_printf(SUM, "vc3test - test vc3 Regs + vc3 Sram -  \n");
    msg_printf(SUM, "mg_vc3internalreg             mg_vc3clearcursor \n");
    msg_printf(SUM, "mg_vc3init                mg_vc3reset \n");
    msg_printf(SUM, "mg_vc3addrsbus                mg_vc3databus \n");
    msg_printf(SUM, "mg_vc3patrn               mg_vc3addrsuniq \n");
    msg_printf(SUM, "mg_vc3disabledsply        mg_vc3enabledsply \n");
    msg_printf(SUM, "mg_vc3cursorenable        mg_vc3cursordisable \n");
    msg_printf(SUM, "mg_vc3cursormode          mg_vc3cursorposition \n");
    msg_printf(SUM, "mg_starttiming            mg_stoptiming mg_pokevc3 \n");
    msg_printf(SUM, "mg_peekvc3    mg_peekvc3ram   mg_pokevc3ram \n");
}

void
dac_menu ()
{
    msg_printf(SUM, " \n");
    msg_printf(SUM, "===== DAC DIAG CMDS MENU  ===== \n");
    msg_printf(SUM, " \n");
    msg_printf(SUM, "dactest - test dac Regs + dac Gamma Tables -  \n");
    msg_printf(SUM, "mg_dacpllinit mg_dacreset     mg_dacctrlreg \n");
    msg_printf(SUM, "mg_dacmodereg     mg_dacaddrreg \n");
    msg_printf(SUM, "mg_clrpaletteaddrUniq mg_clrpalettewalkbit   mg_clrpalettepatrn \n");
    msg_printf(SUM, "mg_peekdacaddr        mg_pokedacaddr mg_peekdacaddr16 \n");
    msg_printf(SUM, "mg_pokedacaddr16      mg_peekdacmode          mg_pokedacmode  \n");
    msg_printf(SUM, "mg_peekdacctrl        mg_pokedacctrl  \n");
    msg_printf(SUM, "mg_peekclrpalette     mg_pokeclrpalette \n ");
}

void
ge_menu ()
{
msg_printf(SUM, "=====     GE11 DIAG CMDS MENU      ===== \n");
msg_printf(SUM, "mg_set_ge (0|1)   mg_ge_ucode_dbus    mg_ge_ucode_abus \n");
msg_printf(SUM, "mg_ge_ucode_a     mg_ge_ucode_m           mg_ge_ucode_w \n");
msg_printf(SUM, "mg_ge_reg_dump    mg_ge_reg   ge0_ucode   ge1_ucode \n");
msg_printf(SUM, "tge_setup     tge_test    ge0_test    ge0_setup \n");
msg_printf(SUM, "ge1_setup     ge1_test \n");
}

void
rss_reg_menu ()
{
msg_printf(SUM, "===== RSS Register Tests MENU ===== \n");
msg_printf(SUM, "mg_re_status_reg  mg_re_rdwr_regs mg_rdram_addrbus \n");
msg_printf(SUM, "mg_rdram_databus mg_rdram_unique  mg_re_internal_ram \n");
}

void
rss_draw_menu ()
{
msg_printf(SUM, "===== RSS Drawing Tests MENU  ===== \n");
msg_printf(SUM, "mg_z_tri          mg_lines    mg_points  \n");
msg_printf(SUM, "mg_stip_tri   mg_xblock       mg_chars  mg_logicop \n");
msg_printf(SUM, "mg_dither   mg_color_tri  mg_notex_poly   mg_notex_line \n");
msg_printf(SUM, "mg_tex_poly mg_tex_load   mg_tex_mddma    mg_tex_lut4d \n");
#if HQ4
msg_printf(SUM, "mg_dispctrl   scene_test  mg_alphablend  re_texlut_test\n");
#else
msg_printf(SUM, "mg_dispctrl   scene_test  alphablend_test  re_texlut_test\n");
#endif
}

void
rss_dma_menu ()
{
msg_printf(SUM, "rdram_mem         tram_mem        mg_tram_rev_nobuff \n");
msg_printf(SUM, "rdram_color_buffer    rdram_z_buffer      rdram_ACC_AUX_buffer \n");
msg_printf(SUM, "rdram_DisplayCD_buffer    rdram_overlay_buffer    rdram_mem_quick \n");
}

void
rss_utils_menu ()
{
msg_printf(SUM,  "===== RSS Utilities MENU  ===== \n");
msg_printf(SUM,  "mg_wr_rss_reg         mg_rd_rss_reg    mg_rss_init     \n");
msg_printf(SUM,  "mg0_rect              mg0_clear_color  mg0_tri         \n");
msg_printf(SUM,  "mg0_point         mg0_line        mg0_bars \n");
msg_printf(SUM,  "mg_xy_to_rdram    mg_sync_pprdram mg_sync_repp    mg_read_fb \n");
msg_printf(SUM,  "mg_rdram_ccsearch     vline           hline \n");
#if HQ4
msg_printf(SUM,  "mg_restore_pps	mg_dac_crc_rss_pp\n");
#endif
}

void
rss_menu ()
{
msg_printf(SUM, "===== RSS DIAG CMDS MENU  =====\n");
msg_printf(SUM, "rss_reg_menu  rss_draw_menu   rss_dma_menu    rss_utils_menu \n");
#ifdef IP30
msg_printf(SUM, "rdram_repair  tram_repair\n");
#else
msg_printf(SUM, "rdram_repair  tram_repair rss rss_quick \n");
#endif /* IP30 */
}

void
dma_menu ()
{
msg_printf(SUM, "===== DMA DIAG CMDS MENU  =====\n");
msg_printf(SUM, "mg_host_hqdma mg_host_hq_cp_dma dmatest \n");
}
#endif /* MFG_USED */
