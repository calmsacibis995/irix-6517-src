#include <sys/gr2hw.h>
#include "ide_msg.h"

#include "xmap.h"
#include "sys/param.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "gr2.h"

/* #define MFG_USED 1 */

#define TRUE 1
#define FALSE 0 
#define PIX_MODE(val)    ( (val & 0x7) 		<< 24)
#define PIX_PG(val)     (((val & 0x1f) << 3) 	<< 24)

#define PD_MODE(val)     ((val & 0x3) 		<< 16)
#define ALT_VIDEO(val)  (((val & 0x3) << 2) 	<< 16)
#define OLAYEN(val)     (((val & 0xf) << 4)	<< 16)

#define ULAYEN(val)     ( (val & 0x1)		<< 8)
#define AUX_PG(val)     (((val & 0x1f) << 1)	<< 8)
#define POU_OVER(val)   (((val & 0x1) << 6)	<< 8)
#define POU_PX(val)     (((val & 0x1) << 7)	<< 8)

#define POU_PG(val)      (val & 0x1f)
#define POU_MODE(val)   ((val & 0x3) << 5)
#define POU_MM(val)     ((val & 0x3) << 7)

#define LOADDRSIZE 	256
#define HIADDRSIZE 	32 
#define MISCSIZE	6 
#define MODESIZE	32	
#define CLUTSIZE	8192 

#define GR2_FIFO_EMPTY		0x1
#define GR2_FIFO_HALF_FULL	0x2
#define GR2_FIFO_OVF		0x4
#define GR2_FIFO_DEPTH		512	

#define	HI_FRNTPORCH_START	1059
#define	HI_BCKPORCH_END	34	
extern struct gr2_hw *base;

void GR2_XMAPmenu(void) {
	msg_printf(SUM, "XMAP cmd#(choose one):\n");    
	msg_printf(SUM, "**********************\n");    
        msg_printf(SUM, "               MISC = %d		MODE= %d\n",GR2_XMAP_MISC, GR2_XMAP_MODE);
        msg_printf(SUM, "               CLUT = %d\n",GR2_XMAP_CLUT);

        msg_printf(SUM, "               ADDRLO = %d 		ADDRHI = %d\n",GR2_XMAP_ADDRLO, GR2_XMAP_ADDRHI);
        msg_printf(SUM, "               BYTECNT= %d (read-only)	FIFOSTATUS= %d\n",GR2_XMAP_BYTECNT, GR2_XMAP_FIFOSTATUS);


}
int checkxmap(int, int, int, int, int, int);

int
checkxmap(int cmd, int addr, int wrval, int loop,
	  int xmapnum, int wr_all)
{
	unsigned int i,j,tmp;
	volatile unsigned char rdval;
	unsigned char byte0, byte1, byte2;
	unsigned int lncount,modeval;
	int num_to_load;
	int error = 0;

	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY) 
		DELAY(1);	


	/* Must  write to individual XMAP5 during vertical retrace*/ 
	   if (!wr_all) {
		do {
	  	base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;
	   }	

	if (wr_all) {
        base->xmapall.addrlo = addr & 0xff;
        base->xmapall.addrhi = (addr & 0x1f00) >> 8;
        }
        else if (xmapnum == GR2_XMAP0) {
        base->xmap[0].addrlo = addr & 0xff;
        base->xmap[0].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP1) {
        base->xmap[1].addrlo = addr & 0xff;
        base->xmap[1].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP2) {
        base->xmap[2].addrlo = addr & 0xff;
        base->xmap[2].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP3) {
        base->xmap[3].addrlo = addr & 0xff;
        base->xmap[3].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP4) {
        base->xmap[4].addrlo = addr & 0xff;
        base->xmap[4].addrhi = (addr & 0x1f00) >> 8;
        }

	

	if (cmd == GR2_XMAP_CLUT) {
		/* Writing 3 bytes at a time*/
		if (loop <=  (GR2_FIFO_DEPTH/3)) 
			num_to_load = loop;
		else 
			num_to_load = GR2_FIFO_DEPTH/3;	
	} else { 
		if (loop < GR2_FIFO_DEPTH)
			num_to_load = loop;	
		else
			num_to_load = GR2_FIFO_DEPTH;	
	}
  for (i = addr; i < loop + addr; i+= num_to_load) {

	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY) 
			DELAY(1);	
    for (j = 0; j <  num_to_load; j++) {
		
	if (!wr_all) {
	do {
       		base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
		base->vc1.addrlo = GR2_CUR_YC & 0xff;
		lncount = base->vc1.cmd0;
	} while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;
	}
	switch (cmd) {
		case GR2_XMAP_MISC:
			if (i == 3) /* PGREG  4 bits*/ 
				tmp = wrval & 0x0f;
			else if (i == 4) /* CU_REG  5 bits*/ 
				tmp = wrval & 0x1f;
			else if (i > 6) {
				msg_printf(ERR, "Writing past MISC range (0-0x6)\n");
			}
			else
				tmp = wrval & 0xff;
			if (wr_all) {
			base->xmapall.misc = tmp;
			} else if (xmapnum == GR2_XMAP0) {
			base->xmap[0].misc = tmp;
			} else if (xmapnum == GR2_XMAP1) {
			base->xmap[1].misc = tmp;
			} else if (xmapnum == GR2_XMAP2) {
			base->xmap[2].misc = tmp;
			} else if (xmapnum == GR2_XMAP3) {
			base->xmap[3].misc = tmp;
			} else if (xmapnum == GR2_XMAP4) {
			base->xmap[4].misc = tmp;
			}
			
		break;
		case GR2_XMAP_MODE:
			if (i > 0x7f) {
				msg_printf(ERR, "Writing past MODE range (0-0x7f)\n");	
			}
			if (wr_all) { 
			base->xmapall.mode = wrval; 
			} else if (xmapnum == GR2_XMAP0) {
			base->xmap[0].mode = wrval; 
			} else if (xmapnum == GR2_XMAP1) {
			base->xmap[1].mode = wrval; 
			} else if (xmapnum == GR2_XMAP2) {
                        base->xmap[2].mode = wrval;
			} else if (xmapnum == GR2_XMAP3) {
                        base->xmap[3].mode = wrval;
			} else if (xmapnum == GR2_XMAP4) {
                        base->xmap[4].mode = wrval;
			}
		
			
			break;
		case GR2_XMAP_CLUT:
			if (i > 0x1fff) {
				msg_printf(ERR,"Writing past addr of color lookup table range (0-0x1fff)\n");	
			}
			if (wr_all) {		
			base->xmapall.clut = wrval & 0xff; 
			base->xmapall.clut = wrval & 0xff; 
			base->xmapall.clut = wrval & 0xff; 
			} else if (xmapnum == GR2_XMAP0) { 
			base->xmap[0].clut = wrval & 0xff; 
			base->xmap[0].clut = wrval & 0xff; 
			base->xmap[0].clut = wrval & 0xff; 
			} else if (xmapnum == GR2_XMAP1) { 
			base->xmap[1].clut = wrval & 0xff; 
			base->xmap[1].clut = wrval & 0xff; 
			base->xmap[1].clut = wrval & 0xff; 
			} else if (xmapnum == GR2_XMAP2) { 
			base->xmap[2].clut = wrval & 0xff; 
			base->xmap[2].clut = wrval & 0xff; 
			base->xmap[2].clut = wrval & 0xff; 
			} else if (xmapnum == GR2_XMAP3) { 
			base->xmap[3].clut = wrval & 0xff; 
			base->xmap[3].clut = wrval & 0xff; 
			base->xmap[3].clut = wrval & 0xff; 
			} else if (xmapnum == GR2_XMAP4) { 
			base->xmap[4].clut = wrval & 0xff; 
			base->xmap[4].clut = wrval & 0xff; 
			base->xmap[4].clut = wrval & 0xff; 
			}
			break;
		case GR2_XMAP_ADDRLO:
			if (wr_all) {
			base->xmapall.addrlo = i & 0xff;
			} else if (xmapnum == GR2_XMAP0) {
			base->xmap[0].addrlo = i & 0xff;
			} else if (xmapnum == GR2_XMAP1) {
			base->xmap[1].addrlo = i & 0xff;
			} else if (xmapnum == GR2_XMAP2) {
			base->xmap[2].addrlo = i & 0xff;
			} else if (xmapnum == GR2_XMAP3) {
			base->xmap[3].addrlo = i & 0xff;
			} else if (xmapnum == GR2_XMAP4) {
			base->xmap[4].addrlo = i & 0xff;
			}
        while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY)
                DELAY(1);

			if (xmapnum == GR2_XMAP0) {
                        rdval = base->xmap[0].addrlo;
                        } else if (xmapnum == GR2_XMAP1) {
                        rdval = base->xmap[1].addrlo;
                        } else if (xmapnum == GR2_XMAP2) {
                        rdval = base->xmap[2].addrlo;
                        } else if (xmapnum == GR2_XMAP3) {
                        rdval = base->xmap[3].addrlo;
                        } else if (xmapnum == GR2_XMAP4) {
                        rdval = base->xmap[4].addrlo;
                        }
                        if (rdval != (i & 0xff)) {
				error ++;
                                msg_printf(ERR,"Low addr byte, GR2_XMAP%d: \
expected %#x, returned %#x\n",xmapnum, i & 0xff, rdval);
			print_xmap_loc(xmapnum);
			}
			break;
		case GR2_XMAP_ADDRHI:
			if (wr_all) {
			base->xmapall.addrhi = i & 0x1f;
			} else if (xmapnum == GR2_XMAP0) {
			base->xmap[0].addrhi = i & 0x1f;
			} else if (xmapnum == GR2_XMAP1) {
			base->xmap[1].addrhi = i & 0x1f;
			} else if (xmapnum == GR2_XMAP2) {
			base->xmap[2].addrhi = i & 0x1f;
			} else if (xmapnum == GR2_XMAP3) {
			base->xmap[3].addrhi = i & 0x1f;
			} else if (xmapnum == GR2_XMAP4) {
			base->xmap[4].addrhi = i & 0x1f;
			}
	while ((base->xmap[0].fifostatus & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
			if (xmapnum == GR2_XMAP0) {
                        rdval = base->xmap[0].addrhi;
                        } else if (xmapnum == GR2_XMAP1) {
                        rdval = base->xmap[1].addrhi;
                        } else if (xmapnum == GR2_XMAP2) {
                        rdval = base->xmap[2].addrhi;
                        } else if (xmapnum == GR2_XMAP3) {
                        rdval = base->xmap[3].addrhi;
                        } else if (xmapnum == GR2_XMAP4) {
                        rdval = base->xmap[4].addrhi;
                        }
                        if (rdval != (i & 0x1f)) {
				error ++;
                                msg_printf(ERR,"High addr byte, GR2_XMAP%d: \
expected %#x, returned %#x\n", xmapnum, i & 0x1f, rdval);
			print_xmap_loc(xmapnum);
			}
			break;

		default:
			msg_printf(ERR, "Only allow testing GR2_XMAP addr(lo/high), MISC, MODE and CLUT registers\n");
			return -1;
	} /* end of switch */
	
	} /* j < num_to_load */
    	} /* END num_to_load loop */

	if ((base->xmap[0].fifostatus & GR2_FIFO_OVF) == 0) 
		msg_printf(ERR,"Overflow\n");

	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY) 
		DELAY(1);
	do {
       		base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
       		base->vc1.addrlo = GR2_CUR_YC & 0xff;
		lncount = base->vc1.cmd0;
	} while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;

/*Read back and compare*/
        if (xmapnum == GR2_XMAP0) {
        base->xmap[0].addrlo = addr & 0xff;
        base->xmap[0].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP1) {
        base->xmap[1].addrlo = addr & 0xff;
        base->xmap[1].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP2) {
        base->xmap[2].addrlo = addr & 0xff;
        base->xmap[2].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP3) {
        base->xmap[3].addrlo = addr & 0xff;
        base->xmap[3].addrhi = (addr & 0x1f00) >> 8;
        } else if (xmapnum == GR2_XMAP4) {
        base->xmap[4].addrlo = addr & 0xff;
        base->xmap[4].addrhi = (addr & 0x1f00) >> 8;
        }

	for (i = addr; i < loop + addr; i++) {
		do {
                	base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                	base->vc1.addrlo = GR2_CUR_YC & 0xff;
                	lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;
        switch (cmd) {
		case GR2_XMAP_MISC:
                        if (xmapnum == GR2_XMAP0) {
                        rdval = base->xmap[0].misc ;
                        } else if (xmapnum == GR2_XMAP1) {
                        rdval = base->xmap[1].misc ;
                        } else if (xmapnum == GR2_XMAP2) {
                        rdval = base->xmap[2].misc ;
                        } else if (xmapnum == GR2_XMAP3) {
                        rdval = base->xmap[3].misc ;
                        } else if (xmapnum == GR2_XMAP4) {
                        rdval = base->xmap[4].misc ;
                        }
                        if (i == 3)  {/* PGREG  4 bits*/
                                tmp =  wrval & 0x0f;
				rdval &= 0x0f;
                        }else if (i == 4) { /* CU_REG  5 bits*/
                                tmp = wrval & 0x1f;
				rdval &= 0x1f;
                        }
                        else
                                tmp = wrval & 0xff;
			

			if (rdval != tmp) {
				error ++;
				msg_printf(ERR,"GR2_XMAP%d MISC reg, addr %#x: \
expected %#x, returned %#x\n",xmapnum, i, tmp, rdval);
				print_xmap_loc(xmapnum);
			}
		break;
                case GR2_XMAP_MODE:
			if (xmapnum == GR2_XMAP0) {
                        modeval = base->xmap[0].mode;
			} else if (xmapnum == GR2_XMAP1) { 
                        modeval = base->xmap[1].mode;
			} else if (xmapnum == GR2_XMAP2) {
			modeval = base->xmap[2].mode;
			} else if (xmapnum == GR2_XMAP3) {
			modeval = base->xmap[3].mode;
			} else if (xmapnum == GR2_XMAP4){
			modeval = base->xmap[4].mode;
			}

			
                        if (modeval != wrval) {
				error ++;
                                msg_printf(ERR, "GR2_XMAP%d MODE reg, addr %#x): \
expected %#x, returned %#x\n", xmapnum,i*4, wrval, modeval);
				print_xmap_loc(xmapnum);
				}
			 break;

                case GR2_XMAP_CLUT:
			if (xmapnum == GR2_XMAP0) {
                        byte0 = base->xmap[0].clut;
			byte1 = base->xmap[0].clut;
                        byte2 = base->xmap[0].clut;
			} else if (xmapnum == GR2_XMAP1) {
                        byte0 = base->xmap[1].clut;
			byte1 = base->xmap[1].clut;
                        byte2 = base->xmap[1].clut;
			} else if (xmapnum == GR2_XMAP2) {
                        byte0 = base->xmap[2].clut;
			byte1 = base->xmap[2].clut;
                        byte2 = base->xmap[2].clut;
			} else if (xmapnum == GR2_XMAP3) {
                        byte0 = base->xmap[3].clut;
			byte1 = base->xmap[3].clut;
                        byte2 = base->xmap[3].clut;
			} else if (xmapnum == GR2_XMAP4) {
                        byte0 = base->xmap[4].clut;
			byte1 = base->xmap[4].clut;
                        byte2 = base->xmap[4].clut;
			}

			if (byte0 != wrval) {
				error ++;
                                msg_printf(ERR, "Byte 0 of GR2_XMAP%d color lookup, addr %#x: \
expected %#x, returned %#x\n", xmapnum, i, wrval, byte0);
				print_xmap_loc(xmapnum);
				}

			if (byte1 != wrval) {
				error ++;
                                msg_printf(ERR, "Byte 1 of GR2_XMAP%d color lookup, addr %#x: \
expected %#x, returned %#x\n", xmapnum, i,wrval, byte1);
				print_xmap_loc(xmapnum);
				}

			if (byte2 != wrval) {
				error ++;
                                msg_printf(ERR, "Byte 2 of GR2_XMAP%d color lookup, addr %#x: \
expected %#x, returned %#x\n", xmapnum, i,wrval, byte2);
				print_xmap_loc(xmapnum);
				}
                        break;
		case GR2_XMAP_ADDRLO:
			break;
		case GR2_XMAP_ADDRHI:
			break;
        }
    }

    return error ; 
} 

/*ARGSUSED*/
int
gr2_wrxmap(int argc, char *argv[])
{
#ifdef MFG_USED
        __psunsigned_t  cmd, val, loop, i,j,lncount;
	__psint_t xmapnum,num_to_load,fifostatus;
	
        if (argc < 3)
        {
		GR2_XMAPmenu();
                msg_printf(ERR, "usage: %s cmd# value <count> <GR2_XMAP#(0-4)>\n", argv[0]);
                msg_printf(ERR, "usage: For color lookup table, give a 24 bit value in hex\n");
                return -1;
        }

        argv++;
        atobu_ptr(*(argv), &cmd);
        argv++;
        atobu_ptr(*(argv), &val);

	if (argc >= 4) {
	argv++;
	loop = atoi(*argv); 
	}
	else loop = 1;

	if (argc == 5) {
		argv++;
		xmapnum = atoi(*argv); 
	}
	else {
		xmapnum = GR2_XMAPALL;
	}

   for (i=0; i < loop; i+= num_to_load) {
	if (((fifostatus = base->xmap[0].fifostatus) & GR2_FIFO_OVF) == 0) 
		msg_printf(ERR,"Overflow\n");
	while (((fifostatus = base->xmap[0].fifostatus) & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
	if (cmd == GR2_XMAP_CLUT) {
		/* Writing 3 bytes at a time*/
		if (loop <=  (GR2_FIFO_DEPTH/3)) 
			num_to_load = loop;
		else 
			num_to_load = GR2_FIFO_DEPTH/3;	
	} else if (cmd == GR2_XMAP_MODE) {
		/* Writing 4 bytes at a time*/
		if (loop <=  (GR2_FIFO_DEPTH/4)) 
			num_to_load = loop;
		else 
			num_to_load = GR2_FIFO_DEPTH/4;	
	} else { 
		if (loop < GR2_FIFO_DEPTH)
			num_to_load = loop;	
		else
			num_to_load = GR2_FIFO_DEPTH;	
	}
	for (j=0; j < num_to_load; j++){
	if (xmapnum != GR2_XMAPALL) {
		 do {
                base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;

	}
        switch (cmd) {
		case GR2_XMAP_MISC:
                if (xmapnum == GR2_XMAPALL)
                        base->xmapall.misc = val & 0xff;
                else if ( xmapnum == GR2_XMAP0)
                        base->xmap[0].misc = val & 0xff;
                else if ( xmapnum == GR2_XMAP1)
                        base->xmap[1].misc = val & 0xff;
                else if ( xmapnum == GR2_XMAP2)
                        base->xmap[2].misc = val & 0xff;
                else if ( xmapnum == GR2_XMAP3)
                        base->xmap[3].misc = val & 0xff;
                else if ( xmapnum == GR2_XMAP4)
                        base->xmap[4].misc = val & 0xff;
                break;
		case GR2_XMAP_MODE:
                if (xmapnum == GR2_XMAPALL) {
                        base->xmapall.mode = val;
			
		}
                else if ( xmapnum == GR2_XMAP0) {
                        base->xmap[0].mode = val ;
		}
                else if ( xmapnum == GR2_XMAP1) {
                        base->xmap[1].mode = val;
		}
                else if ( xmapnum == GR2_XMAP2) {
                        base->xmap[2].mode = val;
			
		}
                else if ( xmapnum == GR2_XMAP3) {
                        base->xmap[3].mode = val;
		}
                else if ( xmapnum == GR2_XMAP4) {
                        base->xmap[4].mode = val;
		}
                break;
		case GR2_XMAP_CLUT:
                if (xmapnum == GR2_XMAPALL) {
                        base->xmapall.clut = (val & 0xff0000) >> 16;
                        base->xmapall.clut = (val & 0xff00) >> 8;
                        base->xmapall.clut = val & 0xff;
		}
                else if ( xmapnum == GR2_XMAP0) {
                        base->xmap[0].clut = (val & 0xff0000) >> 16;
                        base->xmap[0].clut = (val & 0xff00) >> 8;
                        base->xmap[0].clut = val & 0xff;
		}
                else if ( xmapnum == GR2_XMAP1) {
                        base->xmap[1].clut =  (val & 0xff0000) >> 16;
                        base->xmap[1].clut = (val & 0xff00) >> 8;
                        base->xmap[1].clut = val & 0xff;
			
		}
                else if ( xmapnum == GR2_XMAP2) {
                        base->xmap[2].clut =  (val & 0xff0000) >> 16;
                        base->xmap[2].clut = (val & 0xff00) >> 8;
                        base->xmap[2].clut = val & 0xff;
		}
                else if ( xmapnum == GR2_XMAP3) {
                        base->xmap[3].clut =  (val & 0xff0000) >> 16;
                        base->xmap[3].clut = (val & 0xff00) >> 8;
                        base->xmap[3].clut = val & 0xff;
		}	
                else if ( xmapnum == GR2_XMAP4) {
                        base->xmap[4].clut =  (val & 0xff0000) >> 16;
                        base->xmap[4].clut = (val & 0xff00) >> 8;
                        base->xmap[4].clut = val & 0xff;
		}
                break;
                case GR2_XMAP_ADDRLO:
		if (xmapnum == GR2_XMAPALL)
                	base->xmapall.addrlo = val & 0xff;
		else if ( xmapnum == GR2_XMAP0) 
                	base->xmap[0].addrlo = val & 0xff;
		else if ( xmapnum == GR2_XMAP1) 
                	base->xmap[1].addrlo = val & 0xff;
		else if ( xmapnum == GR2_XMAP2) 
                	base->xmap[2].addrlo = val & 0xff;
		else if ( xmapnum == GR2_XMAP3)
                	base->xmap[3].addrlo = val & 0xff;
		else if ( xmapnum == GR2_XMAP4) 
                	base->xmap[4].addrlo = val & 0xff;
                break;
		case GR2_XMAP_ADDRHI:
		if (val > 0x1f) {
			msg_printf(ERR,"Writing past hi byte addr range (0-0x1f)\n"); 
		}
                if (xmapnum == GR2_XMAPALL)
                        base->xmapall.addrhi = val & 0x1f;
                else if ( xmapnum == GR2_XMAP0)
                        base->xmap[0].addrhi = val & 0x1f;
                else if ( xmapnum == GR2_XMAP1)
                        base->xmap[1].addrhi = val & 0x1f;
                else if ( xmapnum == GR2_XMAP2)
                        base->xmap[2].addrhi = val & 0x1f;
                else if ( xmapnum == GR2_XMAP3)
                        base->xmap[3].addrhi = val & 0x1f;
                else if ( xmapnum == GR2_XMAP4)
                        base->xmap[4].addrhi = val & 0x1f;
                break;
                case GR2_XMAP_FIFOSTATUS:
#ifdef LATER
		if (xmapnum == GR2_XMAPALL)
                	base->xmapall.fifostatus = val & 0xff;
		else if ( xmapnum == GR2_XMAP0) 
                	base->xmap[0].fifostatus = val & 0xff;
		else if ( xmapnum == GR2_XMAP1) 
                	base->xmap[1].fifostatus = val & 0xff;
		else if ( xmapnum == GR2_XMAP2) 
                	base->xmap[2].fifostatus = val & 0xff;
		else if ( xmapnum == GR2_XMAP3)
                	base->xmap[3].fifostatus = val & 0xff;
		else if ( xmapnum == GR2_XMAP4) 
                	base->xmap[4].fifostatus = val & 0xff;
#endif
			msg_printf(ERR, "FIFOSTATUS  register is  read only\n");
			return -1;
                break;
		default:
			msg_printf(ERR, "bytecount and crc registers are read only\n");
			return -1;
		break;	
   	}
	}
   if ((i%500 == 0) && (i>0))
   	printf(".");
   }
   if (i > 1)
   	printf("\n");
#else /* MFG_USED */
	msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);	
	return 0;
#endif
}

/*ARGSUSED*/
int
gr2_wrxmapRGB(int argc, char *argv[])
{
#ifdef MFG_USED

        __psunsigned_t  red, green,blue, loop, i, start,xmapnum;
	unsigned char rd_value;
	int fifostatus;

        if (argc < 5)
        {
                msg_printf(ERR, "Can write ranges in color map.\n");
		msg_printf(ERR, " usage: %s start_addr(in hex)  r g b  <count> <GR2_XMAP#(0-4)\n", argv[0]);
                return -1;
        }

        argv++;
        atobu_ptr(*argv, &start);
        argv++;
        atobu_ptr(*(argv), &red);
        argv++;
        atobu_ptr(*(argv), &green);
        argv++;
        atobu_ptr(*(argv), &blue);

        if (argc >= 6) {
	        argv++;
        	loop = atoi(*argv);
        }
	else loop = 1;

        if (argc >= 7) {
	        argv++;
        	xmapnum = atoi(*argv);
        }
	else xmapnum = GR2_XMAPALL;

	while (((fifostatus = base->xmap[0].fifostatus) & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
	switch (xmapnum) { 
		case GR2_XMAPALL:
			base->xmapall.addrlo =  start & 0xff; 
			base->xmapall.addrhi =  (start & 0x1f00) >> 8; 
			break;
		case GR2_XMAP0:
			base->xmap[0].addrlo =  start & 0xff; 
			base->xmap[0].addrhi =  (start & 0x1f00) >> 8; 
			break;
		case GR2_XMAP1:
			base->xmap[1].addrlo =  start & 0xff; 
			base->xmap[1].addrhi =  (start & 0x1f00) >> 8; 
			break;
		case GR2_XMAP2:
			base->xmap[2].addrlo =  start & 0xff; 
			base->xmap[2].addrhi =  (start & 0x1f00) >> 8; 
			break;
		case GR2_XMAP3:
			base->xmap[3].addrlo =  start & 0xff; 
			base->xmap[3].addrhi =  (start & 0x1f00) >> 8; 
			break;
		case GR2_XMAP4:
			base->xmap[4].addrlo =  start & 0xff; 
			base->xmap[4].addrhi =  (start & 0x1f00) >> 8; 
			break;
	}
   for (i=0; i < loop; i++) {
	while (((fifostatus = base->xmap[0].fifostatus) & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
	switch(xmapnum) { 
	case GR2_XMAPALL:
		base->xmapall.clut = red & 0xff;
		base->xmapall.clut = green & 0xff;
		base->xmapall.clut = blue & 0xff;
	break;
	case GR2_XMAP0:
		base->xmap[0].clut = red & 0xff;
		base->xmap[0].clut = green & 0xff;
		base->xmap[0].clut = blue & 0xff;
	break;
	case GR2_XMAP1:
		base->xmap[1].clut = red & 0xff;
		base->xmap[1].clut = green & 0xff;
		base->xmap[1].clut = blue & 0xff;
	break;
	case GR2_XMAP2:
		base->xmap[2].clut = red & 0xff;
		base->xmap[2].clut = green & 0xff;
		base->xmap[2].clut = blue & 0xff;
	break;
	case GR2_XMAP3:
		base->xmap[3].clut = red & 0xff;
		base->xmap[3].clut = green & 0xff;
		base->xmap[3].clut = blue & 0xff;
	break;
	case GR2_XMAP4:
		base->xmap[4].clut = red & 0xff;
		base->xmap[4].clut = green & 0xff;
		base->xmap[4].clut = blue & 0xff;
	break;
	}
    }
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif
}

/*ARGSUSED*/
int
gr2_rdxmap(int argc, char *argv[])
{
#ifdef MFG_USED

        unsigned int cmd, loop,i,xmapnum, msg,lncount;
	unsigned char val,addrhi,addrlo;
	unsigned int wordval, rdaddr,fifostatus;
	
        

        if (argc < 4)
        {
		GR2_XMAPmenu();
                msg_printf(ERR, "usage: %s cmd# numloop GR2_XMAP#(0-4)\n", argv[0]);
                msg_printf(ERR, "If numloop > 1, no msgs. will be printed\n");
                return -1;
        }

        argv++;
        atobu_ptr(*(argv), &cmd);

        argv++;
        loop = atoi(*(argv));
	if (loop > 1)
	 	msg = FALSE;
	else msg = TRUE;

        argv++;
        xmapnum = atoi(*argv);

	/* Wait for fifo to empty */
	while (((fifostatus = base->xmap[0].fifostatus) & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
        for (i=0; i< loop; i++) {
		do {
	  	base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;

          switch (cmd) {
		case GR2_XMAP_MISC:
                if ( xmapnum == GR2_XMAP0) {
                        val = base->xmap[0].misc;
		}
                else if ( xmapnum == GR2_XMAP1) {
                        val = base->xmap[1].misc;
                } else if ( xmapnum == GR2_XMAP2) {
                        val = base->xmap[2].misc;
                } else if ( xmapnum == GR2_XMAP3) {
                        val = base->xmap[3].misc;
                } else if ( xmapnum == GR2_XMAP4) {
                        val = base->xmap[4].misc;
		}
		if (msg) {
		   msg_printf(DBG, "Reading GR2_XMAP%d misc register: \
%#x\n",xmapnum,val);
		}
                break;
                case GR2_XMAP_MODE:
                if ( xmapnum == GR2_XMAP0)
		{
                        wordval = base->xmap[0].mode;
		}
                else if ( xmapnum == GR2_XMAP1) {
                        wordval = base->xmap[1].mode;
		}
                else if ( xmapnum == GR2_XMAP2) {
                        wordval = base->xmap[2].mode;
		}
                else if ( xmapnum == GR2_XMAP3) {
			wordval = base->xmap[3].mode;
		}
                else if ( xmapnum == GR2_XMAP4) {
			wordval = base->xmap[4].mode;
		}
		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP%d mode register, WID %d:  %#x\n",xmapnum,i, wordval);
		}
                break;
                case GR2_XMAP_CLUT:
                if ( xmapnum == GR2_XMAP0) {
                        val = base->xmap[0].clut;
                        wordval = (val << 16);
                        val = base->xmap[0].clut;
			wordval |= (val << 8 );
                        val = base->xmap[0].clut;
			wordval |= val;
		}
                else if ( xmapnum == GR2_XMAP1) {
                        val = base->xmap[1].clut;
                        wordval = (val << 16);
                        val = base->xmap[1].clut;
			wordval |= (val << 8 );
                        val = base->xmap[1].clut;
			wordval |= val;
		}
                else if ( xmapnum == GR2_XMAP2) {
                        val = base->xmap[2].clut;
                        wordval = (val << 16);
                        val = base->xmap[2].clut;
			wordval |= (val << 8 );
                        val = base->xmap[2].clut;
			wordval |= val;
		}
                else if ( xmapnum == GR2_XMAP3) {
                        val = base->xmap[3].clut;
                        wordval = (val << 16);
                        val = base->xmap[3].clut;
			wordval |= (val << 8 );
                        val = base->xmap[3].clut;
			wordval |= val;
		}
                else if ( xmapnum == GR2_XMAP4) {
                        val = base->xmap[4].clut;
                        wordval = (val << 16);
                        val = base->xmap[4].clut;
			wordval |= (val << 8 );
                        val = base->xmap[4].clut;
			wordval |= val;
		}

		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP%d color lookup, loop %#x:  %#6x\n",xmapnum,i,wordval);
		}
                break;
                case GR2_XMAP_CRC:
                if ( xmapnum == GR2_XMAP0)
                        val = base->xmap[0].crc;
                else if ( xmapnum == GR2_XMAP1)
                        val = base->xmap[1].crc;
                else if ( xmapnum == GR2_XMAP2)
                        val = base->xmap[2].crc;
                else if ( xmapnum == GR2_XMAP3)
                        val = base->xmap[3].crc;
                else if ( xmapnum == GR2_XMAP4)
                        val = base->xmap[4].crc;
		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP%d crc register or revision, loop %d:  %#x\n",xmapnum,i,val);
		}
                break;
                case GR2_XMAP_ADDRLO:
                if ( xmapnum == GR2_XMAP0)
			val = base->xmap[0].addrlo;
                else if ( xmapnum == GR2_XMAP1)
                        val = base->xmap[1].addrlo;
                else if ( xmapnum == GR2_XMAP2)
                        val = base->xmap[2].addrlo;
                else if ( xmapnum == GR2_XMAP3)
                        val = base->xmap[3].addrlo;
                else if ( xmapnum == GR2_XMAP4)
                        val = base->xmap[4].addrlo;
		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP%d low address, loop %d:  %#x\n",xmapnum,i,val);
		}
                break;
                case GR2_XMAP_ADDRHI:
                if ( xmapnum == GR2_XMAP0)
                        val = base->xmap[0].addrhi;
                else if ( xmapnum == GR2_XMAP1)
                        val = base->xmap[1].addrhi;
                else if ( xmapnum == GR2_XMAP2)
                        val = base->xmap[2].addrhi;
                else if ( xmapnum == GR2_XMAP3)
                        val = base->xmap[3].addrhi;
                else if ( xmapnum == GR2_XMAP4)
                        val = base->xmap[4].addrhi;
		if (val > 0x1f)
			msg_printf(ERR,"Hi byte addr is past range (0-0x1f)\n");
		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP %d high address, loop %d:  %#x\n",xmapnum,i,val);
		}
                break;
                case GR2_XMAP_BYTECNT:
                if ( xmapnum == GR2_XMAP0)
                        val = base->xmap[0].bytecnt;
                else if ( xmapnum == GR2_XMAP1)
                        val = base->xmap[1].bytecnt; 
                else if ( xmapnum == GR2_XMAP2)
                        val = base->xmap[2].bytecnt;
                else if ( xmapnum == GR2_XMAP3)
                        val = base->xmap[3].bytecnt;
                else if ( xmapnum == GR2_XMAP4)
                        val = base->xmap[4].bytecnt;
		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP%d bytecnt(base 10), loop %d:  %d\n",xmapnum,i,val);
		}
		break ;
                case GR2_XMAP_FIFOSTATUS:
                if ( xmapnum == GR2_XMAP0)
                        val = base->xmap[0].fifostatus;
                else if ( xmapnum == GR2_XMAP1)
                        val = base->xmap[1].fifostatus; 
                else if ( xmapnum == GR2_XMAP2)
                        val = base->xmap[2].fifostatus;
                else if ( xmapnum == GR2_XMAP3)
                        val = base->xmap[3].fifostatus;
                else if ( xmapnum == GR2_XMAP4)
                        val = base->xmap[4].fifostatus;
		if (msg) {
			msg_printf(DBG, "Reading GR2_XMAP%d fifo status, loop %d:  0x%x\n",xmapnum,i,val);
		}
                break;
          }
	if ((i%500 == 0) && (i > 0))
		printf(".");
	
     }
   if (i > 1)
   	printf("\n");
#else /* MFG_USED */
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]); 
return 0;
#endif
}

int
addrinc(int cmd, int addr, int loop, int xmapnum, int wr_all)
{
        unsigned int i,j,lncount;
	volatile unsigned char rdval;
	volatile unsigned int mode;
	int error = 0;


	while ((base->xmap[0].fifostatus & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
	
	if (!wr_all) {
	         do {
                base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;
	}
        if (wr_all) {
        base->xmapall.addrlo = addr & 0xff;
        base->xmapall.addrhi = addr >>  8;
        }
        else if (xmapnum == GR2_XMAP0) {
        base->xmap[0].addrlo = addr & 0xff;
        base->xmap[0].addrhi = addr >> 8 ;
        } else if (xmapnum == GR2_XMAP1) {
        base->xmap[1].addrlo = addr & 0xff;
        base->xmap[1].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP2) {
        base->xmap[2].addrlo = addr & 0xff;
        base->xmap[2].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP3) {
        base->xmap[3].addrlo = addr & 0xff;
        base->xmap[3].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP4) {
        base->xmap[4].addrlo = addr & 0xff;
        base->xmap[4].addrhi = addr >> 8;
        }

        for (i = addr; i < loop + addr; i++) {
	while ((base->xmap[0].fifostatus & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	
	if (!wr_all) {
		 do {
                base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;

	}
	switch (cmd) {
		case GR2_XMAP_ADDRLO:
/*Assume writing mode register autoincrements address register 4x*/
                        if (wr_all) {
                        base->xmapall.addrlo = i & 0xff;
                        base->xmapall.mode = i;
                        } else if (xmapnum == GR2_XMAP0) {
                        base->xmap[0].addrlo = i & 0xff;
                        base->xmap[0].mode = i;
                        } else if (xmapnum == GR2_XMAP1) {
                        base->xmap[1].addrlo = i & 0xff;
                        base->xmap[1].mode = i;
                        } else if (xmapnum == GR2_XMAP2) {
                        base->xmap[2].addrlo = i & 0xff;
                        base->xmap[2].mode = i;
                        } else if (xmapnum == GR2_XMAP3) {
                        base->xmap[3].addrlo = i & 0xff;
                        base->xmap[3].mode = i;
                        } else if (xmapnum == GR2_XMAP4) {
                        base->xmap[4].addrlo = i & 0xff;
                        base->xmap[4].mode = i;
                        }
			i+=4;
                        break;
                case GR2_XMAP_ADDRHI:
                        if (wr_all) {
                        base->xmapall.addrhi = i & 0x1f;
			} else if (xmapnum == GR2_XMAP0) {
                        base->xmap[0].addrhi = i & 0x1f;
                        } else if (xmapnum == GR2_XMAP1) {
                        base->xmap[1].addrhi = i & 0x1f;
                        } else if (xmapnum == GR2_XMAP2) {
                        base->xmap[2].addrhi = i & 0x1f;
                        } else if (xmapnum == GR2_XMAP3) {
                        base->xmap[3].addrhi = i & 0x1f;
                        } else if (xmapnum == GR2_XMAP4) {
                        base->xmap[4].addrhi = i & 0x1f;
                        }
			for (j = 0; j < 0x100/4; j++) {
                        base->xmapall.mode = i;
			}
			i++;
                        break;
		}
	}
	while ((base->xmap[0].fifostatus & GR2_FIFO_EMPTY) == 1) 
		DELAY(1);	

	 do {
                base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;

	if (xmapnum == GR2_XMAP0) {
        base->xmap[0].addrlo = addr & 0xff;
        base->xmap[0].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP1) {
        base->xmap[1].addrlo = addr & 0xff;
        base->xmap[1].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP2) {
        base->xmap[2].addrlo = addr & 0xff;
        base->xmap[2].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP3) {
        base->xmap[3].addrlo = addr & 0xff;
        base->xmap[3].addrhi = addr >> 8;
        } else if (xmapnum == GR2_XMAP4) {
        base->xmap[4].addrlo = addr & 0xff;
        base->xmap[4].addrhi = addr >> 8;
        }

        for (i = addr ; i < (loop + addr);) {
		 do {
                base->vc1.addrhi = (GR2_CUR_YC >> 8) & 0xff;
                base->vc1.addrlo = GR2_CUR_YC & 0xff;
                lncount = base->vc1.cmd0;
                } while  ((lncount < HI_FRNTPORCH_START) && (lncount > (HI_BCKPORCH_END-5))) ;

        	switch (cmd) {
		case GR2_XMAP_ADDRLO:
                        if (xmapnum == GR2_XMAP0) {
                        rdval = base->xmap[0].addrlo;
                        mode = base->xmap[0].mode;
                        } else if (xmapnum == GR2_XMAP1) {
                        rdval = base->xmap[1].addrlo;
                        mode = base->xmap[1].mode;
                        } else if (xmapnum == GR2_XMAP2) {
                        rdval = base->xmap[2].addrlo;
                        mode = base->xmap[2].mode;
                        } else if (xmapnum == GR2_XMAP3) {
                        rdval = base->xmap[3].addrlo;
                        mode = base->xmap[3].mode;
                        } else if (xmapnum == GR2_XMAP4) {
                        rdval = base->xmap[4].addrlo;
                        mode = base->xmap[4].mode;
                        }
                        if (rdval != (i & 0xff)) {
				error ++;
                                msg_printf(ERR,"Low addr byte, GR2_XMAP%d: \
expected %#x, returned %#x\n",xmapnum, i  & 0xff, rdval);
			print_xmap_loc(xmapnum);
			}
			i+=4;
                        break;
                case GR2_XMAP_ADDRHI:
                        if (xmapnum == GR2_XMAP0) {
                        rdval = base->xmap[0].addrhi;
			for (j=0; j < 0x100/4; j++) /* Increment high address by
						   * reading low addr 0x100/4 
						   * times*/
                        mode = base->xmap[0].mode;
                        } else if (xmapnum == GR2_XMAP1) {
                        rdval = base->xmap[1].addrhi;
			for (j=0; j < 0x100/4; j++)
                        mode = base->xmap[1].mode;
                        } else if (xmapnum == GR2_XMAP2) {
                        rdval = base->xmap[2].addrhi;
			for (j=0; j < 0x100/4; j++)
                        mode = base->xmap[2].mode;
                        } else if (xmapnum == GR2_XMAP3) {
                        rdval = base->xmap[3].addrhi;
			for (j=0; j < 0x100/4; j++)
                        mode = base->xmap[3].mode;
                        } else if (xmapnum == GR2_XMAP4) {
                        rdval = base->xmap[4].addrhi;
			for (j=0; j < 0x100/4; j++)
                        mode = base->xmap[4].mode;
                        }	
			if (rdval != i & 0x1f) {
				error ++;
                                msg_printf(ERR,"High addr byte, GR2_XMAP%d: \
expected %#x, returned %#x\n", xmapnum, i & 0x1f, rdval);
			print_xmap_loc(xmapnum);
			}
			i++;
                        break;
		}
	}

	return error ;
}

int
gr2_testxmap(int argc, char *argv[])
{

        unsigned int cmd, addr, wrval, pattern;
        unsigned int loop, xmapnum, wr_all;
	int err = 0 ;
        

	if (argc > 1) {
		if (argc < 6) {
			GR2_XMAPmenu();
        		msg_printf(ERR, "usage: %s cmd# start_addr 8-bit_value count RDWRGR2_XMAP#(0-4) \
<1=GR2_XMAPALL used for writing>", argv[0]);
			return -1;
	}

        argv++;
        atobu(*(argv), &cmd);
        argv++;
        atobu(*(argv), &addr);
        argv++;
        atobu(*(argv), &wrval);
        wrval &= 0xff;  /* Masked off low 8 bits*/
        argv++;
        loop = atoi(*argv);
        argv++;
        xmapnum = atoi(*(argv));
	
	if (argc == 7)
                wr_all = TRUE;
        else wr_all = FALSE;

		err += checkxmap(cmd,addr,wrval,loop,xmapnum,wr_all);
	} else {
		pattern = 0x55;
		msg_printf(VRB, "Checking low addr byte\n");		
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP0,FALSE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP1,FALSE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP2,FALSE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP3,FALSE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP4,FALSE);
		msg_printf(VRB, "Now, using GR2_XMAPALL to write\n");		
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP0,TRUE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP1,TRUE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP2,TRUE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP3,TRUE);
			err += checkxmap(GR2_XMAP_ADDRLO,0,pattern,LOADDRSIZE,GR2_XMAP4,TRUE);
		msg_printf(VRB, "Checking high addr byte\n");		
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP0,FALSE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP1,FALSE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP2,FALSE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP3,FALSE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP4,FALSE);
		msg_printf(VRB, "Now, using GR2_XMAPALL to write\n");		
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP0,TRUE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP1,TRUE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP2,TRUE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP3,TRUE);
			err += checkxmap(GR2_XMAP_ADDRHI,0,pattern,HIADDRSIZE,GR2_XMAP4,TRUE);

			for (pattern = 0; pattern < 0x100; pattern +=0x55) {
			msg_printf(VRB, "Checking xmap misc register with pattern %#x\n",pattern); 
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP0,FALSE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP1,FALSE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP2,FALSE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP3,FALSE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP4,FALSE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP0,TRUE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP1,TRUE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP2,TRUE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP3,TRUE);
			err += checkxmap(GR2_XMAP_MISC,0,pattern,MISCSIZE,GR2_XMAP4,TRUE);

			msg_printf(VRB, "Checking xmap mode register with pattern %#x\n",pattern); 
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP0,FALSE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP1,FALSE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP2,FALSE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP3,FALSE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP4,FALSE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP0,TRUE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP1,TRUE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP2,TRUE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP3,TRUE);
			err += checkxmap(GR2_XMAP_MODE,0,pattern,MODESIZE,GR2_XMAP4,TRUE);
			}
		msg_printf(VRB, "\n");		
                msg_printf(VRB, "Autoinc low addr byte\n");
			err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP0,FALSE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP1,FALSE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP2,FALSE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP3,FALSE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP4,FALSE);
                msg_printf(VRB, "Now, use GR2_XMAPALL to write\n");
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP0,TRUE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP1,TRUE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP2,TRUE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP3,TRUE);
                        err += addrinc(GR2_XMAP_ADDRLO,0,LOADDRSIZE,GR2_XMAP4,TRUE);
                msg_printf(VRB, "Autoinc high addr byte\n");
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP0,FALSE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP1,FALSE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP2,FALSE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP3,FALSE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP4,FALSE);
                msg_printf(VRB, "Now, using GR2_XMAPALL to write\n");
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP0,TRUE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP1,TRUE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP2,TRUE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP3,TRUE);
                        err += addrinc(GR2_XMAP_ADDRHI,0,HIADDRSIZE,GR2_XMAP4,TRUE);
	}

	return err ;
}

/* Do only color look up table test */
int
gr2_testxmap_clut(int argc, char *argv[])
{
	unsigned int cmd, addr, wrval,pattern;
        unsigned int loop, xmapnum, wr_all;
	int err = 0;
        

	if (argc > 1) {
		if (argc < 6) {
			/* GR2_XMAPmenu(); */
        		msg_printf(SUM, "               CLUT = %d\n",GR2_XMAP_CLUT);
        		msg_printf(ERR, "usage: %s cmd# start_addr 8-bit_value count RDWRGR2_XMAP#(0-4) \
<1=GR2_XMAPALL used for writing>", argv[0]);
			return -1;
	}

        argv++;
	atobu(*(argv), &cmd);
        argv++;
        atobu(*(argv), &addr);
        argv++;
        atobu(*(argv), &wrval);
        wrval &= 0xff;  /* Masked off low 8 bits*/
        argv++;
        loop = atoi(*argv);
        argv++;
        xmapnum = atoi(*(argv));
	
	if (argc == 7)
                wr_all = TRUE;
        else wr_all = FALSE;

		err += checkxmap(cmd,addr,wrval,loop,xmapnum,wr_all);
	} else {
			for (pattern = 0; pattern <  0x100 ; pattern +=0x55) {
			
		msg_printf(VRB, "Checking xmap clut with pattern %#x\n",pattern);		
			err += checkxmap(GR2_XMAP_CLUT,0,pattern,CLUTSIZE,GR2_XMAP0,TRUE);
			err += checkxmap(GR2_XMAP_CLUT,0,pattern,CLUTSIZE,GR2_XMAP1,TRUE);
			err += checkxmap(GR2_XMAP_CLUT,0,pattern,CLUTSIZE,GR2_XMAP2,TRUE);
			err += checkxmap(GR2_XMAP_CLUT,0,pattern,CLUTSIZE,GR2_XMAP3,TRUE);
			err += checkxmap(GR2_XMAP_CLUT,0,pattern,CLUTSIZE,GR2_XMAP4,TRUE);
			
		
			}
	}

	return err ;
}

/* Note that the tests write to the pixel/aux buffer one cycle before reading
* from the buffer.  Thus the pixel/aux value must be set one cycle before
* did and cursor values.
*/

/* following are test values
*/

int
gr2_xcolortest(int argc, char *argv[])
{
	int pg_reg, pix_pg, cu_reg;

	pg_reg = pix_pg = 0;
  	cu_reg = cuVal;

	if (argc < 2) {
	msg_printf(DBG,"usage: %s <PG_REG> <PIX_PG> <CU_REG> (Args in hex)\n",argv[0]);  
	}
	if (argc >=2) {
		argv++;
		atob(*argv, &pg_reg); 
	}
	if (argc >= 3) {
		argv++;
		atob(*argv, &pix_pg); 
    msg_printf(DBG, "PG_REG = %#x	PIX_PG = %#x	CU_REG= %#x\n", pg_reg, pix_pg, cu_reg);
	}
	if (argc >= 4) {
		argv++;
		atob(*argv, &cu_reg); 
	}
	
    msg_printf(DBG, "PG_REG = %x	PIX_PG = %x	CU_REG= %x\n", pg_reg, pix_pg, cu_reg);

    base->xmapall.addrhi = 0;
    base->xmapall.addrlo = 3;
    base->xmapall.misc = pg_reg;
    base->xmapall.misc = cu_reg;

    base->xmapall.addrhi = 0;
    base->xmapall.addrlo = 0;

    /* (0) 24-bit rgb */
    base->xmapall.mode =  PIX_MODE(pix24)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (1) 16-bit rgb */
    base->xmapall.mode =  PIX_MODE(pix16)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (2) 12-bit rgb 0 */
    base->xmapall.mode =  PIX_MODE(pix12_0)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (3) 12-bit rgb 1 */
    base->xmapall.mode =  PIX_MODE(pix12_1)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (4) 8-bit rgb 0 */
    base->xmapall.mode =  PIX_MODE(pix8_0)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (5) 8-bit rgb 1 */
    base->xmapall.mode =  PIX_MODE(pix8_1)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (6) 4-bit rgb 0 */
    base->xmapall.mode =  PIX_MODE(pix4_0)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (7) 4-bit rgb 1 */
    base->xmapall.mode =  PIX_MODE(pix4_1)| PD_MODE(pd_rgb) | AUX_PG(auxVal) | POU_PG(pouVal);
    /* (8) 12-bit ci 0 */
    base->xmapall.mode = 	PIX_MODE(pix12_0) | PIX_PG(pix_pg) | PD_MODE(pd_ci) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (9) 12-bit ci 1 */
    base->xmapall.mode = 	PIX_MODE(pix12_1) | PIX_PG(pix_pg) | PD_MODE(pd_ci) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (a) 8-bit ci 0 */
    base->xmapall.mode = 	PIX_MODE(pix8_0) | PIX_PG(pix_pg) | PD_MODE(pd_ci) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (b) 8-bit ci 1 */
    base->xmapall.mode = 	PIX_MODE(pix8_1) | PIX_PG(pix_pg) | PD_MODE(pd_ci) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (c) 4-bit ci 0 */
    base->xmapall.mode = 	PIX_MODE(pix4_0) | PIX_PG(pix_pg) | PD_MODE(pd_ci) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (d) 4-bit ci 1 */
    base->xmapall.mode = 	PIX_MODE(pix4_1) | PIX_PG(pix_pg) | PD_MODE(pd_ci) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (e) 4-bit ci 0, mm */
    base->xmapall.mode = 	PIX_MODE(pix4_0) | PIX_PG(pix_pg) | PD_MODE(pd_cimm) |  AUX_PG(auxVal) | POU_PG(pouVal);
    /* (f) 4-bit ci 1, mm */
    base->xmapall.mode = 	PIX_MODE(pix4_1) | PIX_PG(pix_pg) | PD_MODE(pd_cimm) |  AUX_PG(auxVal) | POU_PG(pouVal);

    return 0;
}

