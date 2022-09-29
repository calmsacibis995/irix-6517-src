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

/*
 * Runs a TV file (output of the simulator) on the hardware
 */

#include <math.h>
#include "sys/mgrashw.h"
#include "sys/sbd.h"    /* Needed to define PHYS_TO_K1 */
#include "tv.h"
#ifndef _STANDALONE
#include <stdio.h>
#endif

int verbose = 1;

/* XXX: ??? */
#define	FULL_DISPLAY		1

#define	ZST_BUF			0
#define	OVL_BUF			1
#define	AB_BUF			2

#define MGRAS_BASE0_PHYS        0x1F0F0000 	/* HQ3 base addr */
#define	MGRAS_PX_RE4_PHYS	(0x1f200000 - 0x7c000)

#define	MGRAS_TV_TIME_OUT	10

#define	XMIN			0
#define	YMIN			0
#define	tvXMAX			1344
#define	tvYMAX			1024
#define	WSIZEX			(tvXMAX - XMIN)
#define	WSIZEY			(tvYMAX - YMIN)

#define	PAGEX 192
#define PAGEY 16
#define PAGE PAGEX*PAGEY

#define FLAG_CLR_PRIV_ADDR      0x5000c
#define	REIF_CTX_BASE		0x50200
#define	CFIFO1_ADDR		0x700f0
#define	CFIFO2_ADDR		0x700f8
#define	CFIFO_PRIV1_ADDR	0x500f0
#define	CFIFO_PRIV2_ADDR	0x500f8
#define	RSS_BASE		0x7c000

#define	REIF_SCAN_WIDTH		0x2
#define	REIF_DMA_TYPE		0x6
#define	REBUS_SYNC		0x7
#define	REBUS_OUT1		0x9
#define	REBUS_OUT2		0xa

#define	RSS_DEV_ADDR		0x15c
#define	RSS_DEV_DATA		0x15d
#define RSS_PIXCMD_PP1          0x160
#define RSS_FILLCMD_PP1		0x161
#define RSS_COLORMASK_MSB_PP1	0x162
#define RSS_COLORMASK_LSBA_PP1	0x163
#define RSS_COLORMASK_LSBB_PP1	0x164
#define RSS_DRBPTRS_PP1         0x16d
#define RSS_DRBSIZE_PP1		0x16e

#define HQ_RSS_SPACE(reg)	(reg << 2)

/*
 * Display Control Bus (DCB) macros
 */

#define DCB_DATAWIDTH_MASK  (0x3)
#define DCB_ENDATAPACK      BIT(2)
#define DCB_ENCRSINC        BIT(3)
#define DCB_CRS_SHIFT    4
#define DCB_CRS_MASK     (0x7 << DCB_CRS_SHIFT)
#define DCB_ADDR_SHIFT   7
#define DCB_ADDR_MASK    (0xf << DCB_ADDR_SHIFT)
#define DCB_ENSYNCACK       BIT(11)
#define DCB_ENASYNCACK      BIT(12)
#define DCB_CSWIDTH_SHIFT   13
#define DCB_CSWIDTH_MASK    (0x1f << CSWIDTH_SHIFT)
#define DCB_CSHOLD_SHIFT    18
#define DCB_CSHOLD_MASK     (0x1f << CSHOLD_SHIFT)
#define DCB_CSSETUP_SHIFT   23
#define DCB_CSSETUP_MASK    (0x1f << CSSETUP_SHIFT)
#define DCB_SWAPENDIAN      BIT(28)


/*
 * Some values for DCBMODE fields
 */
#define DCB_DATAWIDTH_4		0x0
#define DCB_DATAWIDTH_1		0x1
#define DCB_DATAWIDTH_2		0x2
#define DCB_DATAWIDTH_3		0x3

#define	BRINGUP		1

#ifdef TESTING		
	/* XXX: Just prints some statements */
#define HQ3_PIO_WR_RE(reg, val, mask) {                                                         \
        printf("HQ3_PIO_WR_RE..... reg = 0x%x; val = 0x%x; mask = 0x%x\n", reg, val, mask);     \
}
#define HQ3_PIO_RD_RE(reg, mask, actual) {                                                      \
        printf("HQ3_PIO_RD_RE..... reg = 0x%x; mask = 0x%x;\n", reg, mask);                     \
}
#define HQ3_PIO_WR(offset, val, mask) {                                                         \
        printf("HQ3_PIO_WR..... offset = 0x%x; val = 0x%x; mask = 0x%x\n", offset, val, mask);  \
}
#define HQ3_PIO_RD(offset, mask, actual) {                                                      \
        printf("HQ3_PIO_RD..... offset = 0x%x; mask = 0x%x\n", offset, mask);                   \
}

#else
#ifdef BRINGUP		
	/* XXX: Uses the REX3 & REBus Kludge Logic */

#include "sys/ng1hw.h"

typedef struct {
	rex3Chip rex3;
	unsigned char pad0[0x800000-sizeof(rex3Chip)];
	raster rss;       /* RE4 addr space*/
} mgras_hw_bu;
mgras_hw_bu *mgbase  = (mgras_hw_bu *)PHYS_TO_K1(MGRAS_BASE0_PHYS);
mgras_hw_bu *re4base = (mgras_hw_bu *)PHYS_TO_K1(MGRAS_PX_RE4_PHYS);
extern	mgras_hw_bu	*mgbase;

#define HQ3_PIO_WR_RE(reg, val, mask) {                                                         \
	*((__uint32_t *)&(re4base->rss.s) + reg) = (val & mask);				\
}
#define HQ3_PIO_RD_RE(reg, mask, actual) {                                                      \
	actual = (__uint32_t) *((__uint32_t *)&(re4base->rss.s) + reg) & mask;		\
}
#define HQ3_PIO_WR(offset, val, mask) {                                                         \
        *(__uint32_t *)((__uint32_t) (mgbase + offset)) = val & mask;				\
}
#define HQ3_PIO_RD(offset, mask, actual) {                                                      \
        actual = (*(__uint32_t *)((__uint32_t) (mgbase + offset))) & mask;			\
}

#else			/* XXX: Does the actual stuff on P0 Board */

mgras_hw *mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS)
#define HQ3_PIO_WR_RE(reg, val, mask)                                                           \
        *(__uint32_t *)((__uint32_t) (mgbase + RSS_BASE + (HQ_RSS_SPACE(reg)))) = val & mask
#define HQ3_PIO_RD_RE(reg, mask, actual)                                                        \
        actual = (*(__uint32_t *)((__uint32_t) (mgbase + RSS_BASE + (HQ_RSS_SPACE(reg))))) & mask
#define HQ3_PIO_WR(offset, val, mask)                                                           \
        *(__uint32_t *)((__uint32_t) (mgbase + offset)) = val & mask
#define HQ3_PIO_RD(offset, mask, actual)                                                        \
        actual = (*(__uint32_t *)((__uint32_t) (mgbase + offset))) & mask
#endif
#endif

static __uint32_t mgras_hq3_write_addr = 0xdeadbeef;
static __uint32_t mgras_hq3_read_addr = 0xdeadbeef;
static unsigned short pixs[WSIZEX*WSIZEY][4];  /* pixel storage */
static char filename[40];       /* Holds name of dump file */
static int counter = 0;         /* Number of times that pixs[][] has */
				/* been updated...needed so that the */
				/* dump files don't overwrite */
				/* eachother. */
unsigned short rdm[6][WSIZEX*WSIZEY];   /* rdram storage */
static FILE *(fp[2][3]);        /* Array that holds the file pointers */
				/* for the dump files. */
int  	rdmp[6], startTile, endTile, ZFlag = 0;
int	sb = 0x0; 		/* start address of color buffer */

int 
main(int argc, char **argv)
{
    FILE 	*fp;
    __uint32_t	bufType;

    bufType = ZST_BUF;
    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Failed to open input TV file\n");
	exit(1);
    }
    get_tv_input(fp); /* read in the TV file and send GIO transaction in HW */

    read_frame_buffer(tvXMAX, tvYMAX, bufType);
    dumpFB();	/* dump frame buffer */
}

int 
get_tv_input(FILE *fp)
{
    char *st;
    int tv_max_size;
    type_tv_GENERIC tv_in;

    tv_max_size = TV_MAX_SIZE;
    /*
     * op_tv_hq_HQ_RE_WrReg:
     *            If format=0 then datalo[31:0] only contains the register value.
     *            If format=1 then this is a two register transfer where
     *              datahi[25:0] and datalo[25:0] contain the two reg values.
     *            If format=2 then this is an extended transfer where
     *            datahi[19:0] appended to datalo[31:0] contains the 52-bit
     *            extended value.
     */

    /* read in vector file, decode op_code, format RE bus transactions for HQ's CFIFO */
    while(!feof(fp)){
      	if (fread(&(tv_in), TV_MAX_SIZE, 1, fp)) { /* read in stdio */
           /* Depending on what the particular opcode is, call the */
           /* appropriate handler function. */
           switch(tv_in.osd.opc) {
		case op_tv_hq_HQ_DCB_Wr:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_DCB_Wr+++++++++++++++\n");
			}
			tv_hq_HQ_DCB_Wr_Handle(tv_in);
			break;
        	case op_tv_hq_HQ_RE_WrReg:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_WrReg+++++++++++++++\n");
			}
          		tv_hq_HQ_RE_WrReg_Handle(tv_in);
          		break;
        	case op_tv_hq_HQ_RE_WrDMA:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_WrDMA+++++++++++++++\n");
			}
          		tv_hq_HQ_RE_WrDMA_Handle(tv_in);
          		break;
        	case op_tv_hq_HQ_RE_MaskWrDMA:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_MaskWrDMA+++++++++++++++\n");
			}
          		tv_hq_HQ_RE_MaskWrDMA_Handle(tv_in);
          		break;
      		case op_tv_hq_HQ_RE_RdReg:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_RdReg+++++++++++++++\n");
			}
        		tv_hq_HQ_RE_RdReg_Handle(tv_in);
        		break;
      		case op_tv_hq_HQ_RE_MaskRdReg:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_MaskRdReg+++++++++++++++\n");
			}
        		tv_hq_HQ_RE_MaskRdReg_Handle(tv_in);
        		break;
      		case op_tv_hq_HQ_RE_RdDMA:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_RdDMA+++++++++++++++\n");
			}
        		/* op_tv_hq_HQ_RE_RdDMA_Handle(tv_in); */
        		break;
      		case op_tv_hq_HQ_RE_PollReg:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_RE_PollReg+++++++++++++++\n");
			}
        		tv_hq_HQ_RE_PollReg_Handle(tv_in);
        		break;
      		case op_tv_hq_REBUS_P:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_REBUS_P+++++++++++++++\n");
			}
        		tv_hq_REBUS_P_Handle(tv_in);
        		break;
      		case op_tv_hq_REBUS_V:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_REBUS_V+++++++++++++++\n");
			}
        		tv_hq_REBUS_V_Handle(tv_in);
        		break;
      		case op_tv_hq_CFIFO_Wr:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_CFIFO_Wr+++++++++++++++\n");
			}
        		tv_hq_CFIFO_Wr_Handle(tv_in);
        		break;
      		case op_tv_host_PROC_HQ_WrAddr:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_host_PROC_HQ_WrAddr+++++++++++++++\n");
			}
        		tv_host_PROC_HQ_WrAddr_Handle(tv_in);
        		break;
      		case op_tv_host_PROC_HQ_WrData:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_host_PROC_HQ_WrData+++++++++++++++\n");
			}
        		tv_host_PROC_HQ_WrData_Handle(tv_in);
        		break;
      		case op_tv_host_PROC_HQ_RdAddr:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_host_PROC_HQ_RdAddr+++++++++++++++\n");
			}
        		tv_host_PROC_HQ_RdAddr_Handle(tv_in);
        		break;
      		case op_tv_host_PROC_HQ_RdData:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_host_PROC_HQ_RdData+++++++++++++++\n");
			}
        		tv_host_PROC_HQ_RdData_Handle(tv_in);
        		break;
      		case op_tv_host_PROC_HQ_Poll:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_host_PROC_HQ_Poll+++++++++++++++\n");
			}
        		tv_host_PROC_HQ_Poll_Handle(tv_in);
        		break;
      		case op_tv_hq_HQ_Wait:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_hq_HQ_Wait+++++++++++++++\n");
			}
        		tv_hq_HQ_Wait_Handle(tv_in);
        		break;
      		case op_tv_Comment:
			if (verbose > 0) {
			   printf("++++++++++++++op_tv_Comment+++++++++++++++\n");
			}
        		tv_Comment_Handle(tv_in);
        		break;
      		default:
        		printf("Warning: Input file contains unknown bus transaction\n");
        		break;
      	   } /* end of switch */
#if 0
           printf("tv_format, addr, data = %x, %x, %x\n",tmp_re_in->format,sim_rebus_addr,sim_rebus_data);
#endif
    	}
    }
    return(1);
}

int tv_hq_HQ_DCB_Wr_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_hq_HQ_DCB_Wr tmp_dcb_wr;
    unsigned int dcbmode = 0x0;

    tmp_dcb_wr = (ptr_tv_hq_HQ_DCB_Wr) &(tv_in);
    dcbmode = (	(tmp_dcb_wr->addr 	<< DCB_ADDR_SHIFT)	|
		(tmp_dcb_wr->crs	<< DCB_CRS_SHIFT)	|
		(tmp_dcb_wr->cssetup	<< DCB_CSSETUP_SHIFT)	|
		(tmp_dcb_wr->cswidth	<< DCB_CSWIDTH_SHIFT)	|
		(tmp_dcb_wr->cshold	<< DCB_CSHOLD_SHIFT)
	      );
    (mgbase->rex3).set.dcbmode = dcbmode;
    (mgbase->rex3).set.dcbdata0.bybyte.b3 = tmp_dcb_wr->data;

    return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a WrReg transaction, */
/* then deals with the data in the struct. */
int tv_hq_HQ_RE_WrReg_Handle (type_tv_GENERIC tv_in) {
    unsigned long          sim_rebus_data;
    unsigned long          sim_rebus_addr;
    unsigned long          mask = ~0x0;
    ptr_tv_re_HQ_RE_WrReg tmp_re_in;

    tmp_re_in = (ptr_tv_re_HQ_RE_WrReg) &(tv_in);
    if (tmp_re_in->format == 0) { /* single register write */
      	if (verbose > 1) {
        	printf("Info: Using single register load transaction\n");
      	}
    	sim_rebus_data = (unsigned long)tmp_re_in->datalo; 
    	sim_rebus_addr = (unsigned long)tmp_re_in->addr |
      		(tmp_re_in->execute ? 0x400 : 0); /* Execute bit */
    
    	HQ3_PIO_WR_RE(sim_rebus_addr, sim_rebus_data, mask);

    	if (sim_rebus_addr == 0x413) {
#if 0
		wait();
#endif
    	}
    } else if (tmp_re_in->format == 1) { /* two register write */
    	if (verbose > 1) {
      		printf("Info: Using double register load transaction\n"); 
    	}
    	sim_rebus_data = ((unsigned long)tmp_re_in->datahi &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = ((unsigned long)tmp_re_in->addr)&0x1ff;

    	HQ3_PIO_WR_RE(sim_rebus_addr, sim_rebus_data, mask); 
    
    	sim_rebus_data = ((unsigned long)tmp_re_in->datalo &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_in->addr)&0x1ff)+1 |
      		(tmp_re_in->execute ? 0x400 : 0); /* Execute bit */

    	HQ3_PIO_WR_RE(sim_rebus_addr, sim_rebus_data, mask); 
    } else if (tmp_re_in->format == 2) { /* extended register write */
    	if (verbose > 1) {
      		printf("Info: Using extended register load transaction\n"); 
    	}
    	/* sim_rebus_data = ((unsigned long)tmp_re_in->datahi &
       		(unsigned long)0xfffff);  */
    	sim_rebus_data = (((unsigned long)tmp_re_in->datahi & 
			 (unsigned long)0xfffff)<<6) |
			 (((unsigned long)tmp_re_in->datalo & 
			 (unsigned long)0xfc000000)>>26); 
    	sim_rebus_addr = ((unsigned long)tmp_re_in->addr)&0x1ff; 

    	HQ3_PIO_WR_RE(sim_rebus_addr, sim_rebus_data, mask); 
    
    	sim_rebus_data = ((unsigned long)tmp_re_in->datalo & 
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_in->addr)&0x1ff)+1 |
      		(tmp_re_in->execute ? 0x400 : 0); /* Execute bit */

    	HQ3_PIO_WR_RE(sim_rebus_addr, sim_rebus_data, mask); 
    } else if (tmp_re_in->format == 3) { /* noop */
	;
    }
    return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a Comment transaction, */
/* then deals with the data in the struct.  Prints out the text */
/* contained in the struct, properly handling long comments split up */
/* among multiple transactions. */
int tv_Comment_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_Comment tmp_comment;

    static int commentFlag = 0;	/* Indicates whether we are currently */
				/* processing a multi-part comment. */

    tmp_comment = (ptr_tv_Comment) &(tv_in);
    /* Only print the initial "Comment: " if this is the first part of a */
    /* comment transaction. Also set commentFlag. */
    if (!commentFlag) {
      	printf("Comment: ");
    	commentFlag = 1;
    }
    printf("%s", tmp_comment->comment);

    /* If this is the last part of a comment transaction, print a */
    /* newline and unset commentFlag */
    if (tmp_comment->numrecs == 0) {
    	printf("\n");
    	commentFlag = 0;
    }
    return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a HQ_Wait transaction, */
/* then deals with the data in the struct. */
/* HACK: Doesn't do anything yet.  Should somehow delay a certain */
/* number of clocks. */
int tv_hq_HQ_Wait_Handle (type_tv_GENERIC *tv_in) {
/*  wait();*/
  return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a MaskRdReg transaction, */
/* then deals with the data in the struct. */
/* HACK: Prints out extra info. */
int tv_hq_HQ_RE_MaskRdReg_Handle (type_tv_GENERIC tv_in) {
    unsigned long          sim_rebus_data;
    unsigned long          sim_real_data;
    unsigned long          sim_rebus_addr;
    unsigned long          mask = ~0x0;
    ptr_tv_re_HQ_RE_MaskRdReg tmp_re_rd_in;

    /* Same code as WrReg, except the data is ignored, and the */
    /* address given is read from. */
    /* HACK: MaskRdReg should be different, but it doesn't really */
    /* matter yet, because I am not checking the return value from rdreg */
    tmp_re_rd_in = (ptr_tv_re_HQ_RE_MaskRdReg) &(tv_in);
    if (tmp_re_rd_in->format == 0) { /* single register read */
    	if (verbose > 1) {
      		/* printf("Info: Using single register load transaction\n"); */
    	}
    	sim_rebus_data = (unsigned long)tmp_re_rd_in->datalo;
    	sim_rebus_addr = (unsigned long)tmp_re_rd_in->addr |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("MaskRdReg of Address: 0x%x Expected: 0x%x Received: 0x%x", 
					sim_rebus_addr,sim_rebus_data,sim_real_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("  (mismatch)\n");
    	} else {
      		printf("\n");
    	}
    } else if (tmp_re_rd_in->format == 1) { /* two register write */
    	if (verbose > 1) {
      		/* printf("Info: Using double register load transaction\n"); */
    	}
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datahi &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = ((unsigned long)tmp_re_rd_in->addr)&0x1ff;
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datalo &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_rd_in->addr)&0x1ff)+1 |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    } else if (tmp_re_rd_in->format == 2) { /* extended register write */
    	if (verbose > 1) {
      		printf("Info: Using extended register load transaction\n"); 
    	}
    	/* sim_rebus_data = ((unsigned long)tmp_re_rd_in->datahi &
       		(unsigned long)0xfffff);  */
    	sim_rebus_data = (((unsigned long)tmp_re_rd_in->datahi & 
			 (unsigned long)0xfffff)<<6) |
			 (((unsigned long)tmp_re_rd_in->datalo & 
			 (unsigned long)0xfc000000)>>26);
    	sim_rebus_addr = ((unsigned long)tmp_re_rd_in->addr)&0x1ff; 
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datalo & 
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_rd_in->addr)&0x1ff)+1 |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    } else if (tmp_re_rd_in->format == 3) { /* noop */
	;
    }
  return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a RdReg transaction, */
/* then deals with the data in the struct. */
/* HACK: Prints out extra info. */
int tv_hq_HQ_RE_RdReg_Handle (type_tv_GENERIC tv_in) {
    unsigned long          sim_rebus_data;
    unsigned long          sim_real_data;
    unsigned long          sim_rebus_addr;
    unsigned long          mask = ~0x0;
    ptr_tv_re_HQ_RE_RdReg tmp_re_rd_in;

    /* Same code as WrReg, except the data is ignored, and the */
    /* address given is read from. */
    tmp_re_rd_in = (ptr_tv_re_HQ_RE_RdReg) &(tv_in);
    if (tmp_re_rd_in->format == 0) { /* single register read */
    	if (verbose > 1) {
      		/* printf("Info: Using single register load transaction\n"); */
    	}
    	sim_rebus_data = (unsigned long)tmp_re_rd_in->datalo;
    	sim_rebus_addr = (unsigned long)tmp_re_rd_in->addr |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    } else if (tmp_re_rd_in->format == 1) { /* two register write */
    	if (verbose > 1) {
      		/* printf("Info: Using double register load transaction\n"); */
    	}
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datahi &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = ((unsigned long)tmp_re_rd_in->addr)&0x1ff;
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datalo &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_rd_in->addr)&0x1ff)+1 |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    } else if (tmp_re_rd_in->format == 2) { /* extended register write */
    	if (verbose > 1) {
      		printf("Info: Using extended register load transaction\n"); 
    	}
    	sim_rebus_data = (((unsigned long)tmp_re_rd_in->datahi & 
			       (unsigned long)0xfffff)<<6) |
				 (((unsigned long)tmp_re_rd_in->datalo & 
				   (unsigned long)0xfc000000)>>26);
    	sim_rebus_addr = ((unsigned long)tmp_re_rd_in->addr)&0x1ff; 
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datalo & 
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_rd_in->addr)&0x1ff)+1 |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    	HQ3_PIO_RD_RE(sim_rebus_addr, mask, sim_real_data);
    	printf("Address:  0x%x\n", sim_rebus_addr);
    	printf("Real:     0x%x\n", sim_real_data);
    	printf("Expected: 0x%x\n", sim_rebus_data);
    	if (sim_real_data != sim_rebus_data) {
      		printf("Warning: register read mismatch.\n");
    	}
    } else if (tmp_re_rd_in->format == 3) { /* noop */
	;
    }
    return 1;
}

int tv_hq_REBUS_P_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_hq_REBUS_P tmp_rebus;

    tmp_rebus = (ptr_tv_hq_REBUS_P) &(tv_in);

    HQ3_PIO_WR((REIF_CTX_BASE + (REBUS_SYNC << 2)), 0x1, ~0x0);

    return 1;
}

int tv_hq_REBUS_V_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_hq_REBUS_V tmp_rebus;

    tmp_rebus = (ptr_tv_hq_REBUS_V) &(tv_in);

    HQ3_PIO_WR((REIF_CTX_BASE + (REBUS_SYNC << 2)), 0x0, ~0x0);

    return 1;
}


int tv_host_PROC_HQ_WrAddr_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_host_PROC_HQ_WrAddr tmp_host;

    tmp_host = (ptr_tv_host_PROC_HQ_WrAddr) &(tv_in);

    mgras_hq3_write_addr = tmp_host->addrlo;

    return 1;
}

int tv_host_PROC_HQ_WrData_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_host_PROC_HQ_WrData tmp_host;

    tmp_host = (ptr_tv_host_PROC_HQ_WrData) &(tv_in);

    HQ3_PIO_WR(mgras_hq3_write_addr, tmp_host->datahi, ~0x0);

    return 1;
}

int tv_host_PROC_HQ_RdAddr_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_host_PROC_HQ_RdAddr tmp_host;

    tmp_host = (ptr_tv_host_PROC_HQ_RdAddr) &(tv_in);

    mgras_hq3_read_addr = tmp_host->addrlo;

    return 1;
}

int tv_host_PROC_HQ_RdData_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_host_PROC_HQ_RdData tmp_host;
    __uint32_t data;

    tmp_host = (ptr_tv_host_PROC_HQ_RdData) &(tv_in);

    HQ3_PIO_RD(mgras_hq3_read_addr, ~0x0, data);

    return data;
}

int tv_hq_CFIFO_Wr_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_hq_CFIFO_Wr tmp_cfifo_wr;

    tmp_cfifo_wr = (ptr_tv_hq_CFIFO_Wr) &(tv_in);

    if (tmp_cfifo_wr->tagpriv) { /* previlege write */
    	HQ3_PIO_WR(CFIFO1_ADDR, tmp_cfifo_wr->datahi, ~0x0);
    	HQ3_PIO_WR(CFIFO2_ADDR, tmp_cfifo_wr->datalo, ~0x0);
    } else {
    	HQ3_PIO_WR(CFIFO_PRIV1_ADDR, tmp_cfifo_wr->datahi, ~0x0);
    	HQ3_PIO_WR(CFIFO_PRIV2_ADDR, tmp_cfifo_wr->datalo, ~0x0);
    }

    return 1;
}

int tv_hq_HQ_RE_WrDMA_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_re_HQ_RE_WrDMA tmp_re_in_dma;
    __uint32_t data_high, data_low;

    tmp_re_in_dma = (ptr_tv_re_HQ_RE_WrDMA) &(tv_in);

    data_high = (__uint32_t) tmp_re_in_dma->datahi;
    data_low  = (__uint32_t) tmp_re_in_dma->datalo;

    HQ3_PIO_WR((REIF_CTX_BASE + (REBUS_OUT2 << 2)), data_high, ~0x0);
    HQ3_PIO_WR((REIF_CTX_BASE + (REBUS_OUT1 << 2)), data_low , ~0x0);

    return 1;
}

int tv_hq_HQ_RE_RdDMA_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_re_HQ_RE_RdDMA tmp_re_in_dma;
    __uint32_t data_high, data_low;

    tmp_re_in_dma = (ptr_tv_re_HQ_RE_RdDMA) &(tv_in);

    HQ3_PIO_RD((REIF_CTX_BASE + (REBUS_OUT2 << 2)), ~0x0, data_high);
    HQ3_PIO_RD((REIF_CTX_BASE + (REBUS_OUT1 << 2)), ~0x0, data_low);

    tmp_re_in_dma->datahi = data_high;
    tmp_re_in_dma->datalo = data_low;

    return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a WrDMA transaction, */
/* then deals with the data in the struct. */
int tv_hq_HQ_RE_MaskWrDMA_Handle (type_tv_GENERIC tv_in) {
    ptr_tv_re_HQ_RE_MaskWrDMA tmp_re_in_dma;
    __uint32_t data_high, data_low, mask_high, mask_low;

    tmp_re_in_dma = (ptr_tv_re_HQ_RE_WrDMA) &(tv_in);

    data_high = (__uint32_t) tmp_re_in_dma->datahi;
    data_low  = (__uint32_t) tmp_re_in_dma->datalo;

    if (tmp_re_in_dma->usemask) {
    	mask_high = (__uint32_t) tmp_re_in_dma->maskhi;
    	mask_low  = (__uint32_t) tmp_re_in_dma->masklo;
    } else {
	mask_high = mask_low = ~0x0;
    }

    HQ3_PIO_WR((REIF_CTX_BASE + (REBUS_OUT2 << 2)), data_high, mask_high);
    HQ3_PIO_WR((REIF_CTX_BASE + (REBUS_OUT1 << 2)), data_low , mask_low);

    return 1;
}

int tv_host_PROC_HQ_Poll_Handle(type_tv_GENERIC tv_in) {
    ptr_tv_host_PROC_HQ_Poll tmp_hq_pool;
    __uint32_t exp, rcv, addr, mask, time;

    tmp_hq_pool = (ptr_tv_host_PROC_HQ_Poll) &(tv_in);

    exp  = tmp_hq_pool->data;
    addr = tmp_hq_pool->addrlo;
    mask = tmp_hq_pool->mask;
    time = MGRAS_TV_TIME_OUT;

    do {
  	HQ3_PIO_RD(addr, mask, rcv);
	time--;
    } while ((exp != rcv) && time);

    return 1;
}

/* Takes a tv_GENERIC struct and recasts it as a RdReg transaction, */
/* then deals with the data in the struct. */
/* HACK: Prints out extra info. */
int tv_hq_HQ_RE_PollReg_Handle (type_tv_GENERIC tv_in) {
    unsigned long          sim_rebus_data;
    unsigned long          sim_real_data;
    unsigned long          sim_rebus_addr;
    unsigned long          mask = ~0x0;
    __uint32_t	   time = MGRAS_TV_TIME_OUT;
    ptr_tv_re_HQ_RE_PollReg tmp_re_rd_in;

    /* Same code as WrReg, except the data is ignored, and the */
    /* address given is read from. */
    tmp_re_rd_in = (ptr_tv_re_HQ_RE_PollReg) &(tv_in);
    if (tmp_re_rd_in->format == 0) { /* single register read */
    	if (verbose > 1) {
      		/* printf("Info: Using single register load transaction\n"); */
    	}
    	sim_rebus_data = (unsigned long)tmp_re_rd_in->datalo;
    	sim_rebus_addr = (unsigned long)tmp_re_rd_in->addr |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
    
	do {
    	    HQ3_PIO_RD_RE(sim_rebus_addr, tmp_re_rd_in->masklo, sim_real_data);
	    time--;
    	    printf("Address:  0x%x\n", sim_rebus_addr);
    	    printf("Real:     0x%x\n", sim_real_data);
    	    printf("Expected: 0x%x\n", sim_rebus_data);
	} while ((sim_real_data != sim_rebus_data) && time);
    } else if (tmp_re_rd_in->format == 1) { /* two register write */
    	if (verbose > 1) {
      		/* printf("Info: Using double register load transaction\n"); */
    	}
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datahi &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = ((unsigned long)tmp_re_rd_in->addr)&0x1ff;

	do {
    	    HQ3_PIO_RD_RE(sim_rebus_addr, tmp_re_rd_in->maskhi, sim_real_data);
	    time--;
    	    printf("Address:  0x%x\n", sim_rebus_addr);
    	    printf("Real:     0x%x\n", sim_real_data);
    	    printf("Expected: 0x%x\n", sim_rebus_data);
    	} while ((sim_real_data != sim_rebus_data) && time);
    
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datalo &
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_rd_in->addr)&0x1ff)+1 |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */
	
	time = MGRAS_TV_TIME_OUT;
	do {
    	    HQ3_PIO_RD_RE(sim_rebus_addr, tmp_re_rd_in->masklo, sim_real_data);
	    time--;
    	    printf("Address:  0x%x\n", sim_rebus_addr);
    	    printf("Real:     0x%x\n", sim_real_data);
    	    printf("Expected: 0x%x\n", sim_rebus_data);
    	} while ((sim_real_data != sim_rebus_data) && time);
    } else if (tmp_re_rd_in->format == 2) { /* extended register write */
    	if (verbose > 1) {
      		printf("Info: Using extended register load transaction\n"); 
    	}
    	sim_rebus_data = (((unsigned long)tmp_re_rd_in->datahi & 
			       (unsigned long)0xfffff)<<6) |
				 (((unsigned long)tmp_re_rd_in->datalo & 
				   (unsigned long)0xfc000000)>>26);
    	sim_rebus_addr = ((unsigned long)tmp_re_rd_in->addr)&0x1ff; 

	do {
    	   HQ3_PIO_RD_RE(sim_rebus_addr, tmp_re_rd_in->maskhi, sim_real_data);
	   time--;
    	   printf("Address:  0x%x\n", sim_rebus_addr);
    	   printf("Real:     0x%x\n", sim_real_data);
    	   printf("Expected: 0x%x\n", sim_rebus_data);
    	} while ((sim_real_data != sim_rebus_data) && time);
    
    	sim_rebus_data = ((unsigned long)tmp_re_rd_in->datalo & 
			      (unsigned long)0x3ffffff);
    	sim_rebus_addr = (((unsigned long)tmp_re_rd_in->addr)&0x1ff)+1 |
      		(tmp_re_rd_in->execute ? 0x400 : 0); /* Execute bit */

	time = MGRAS_TV_TIME_OUT;
	do {
    	    HQ3_PIO_RD_RE(sim_rebus_addr, tmp_re_rd_in->masklo, sim_real_data);
	    time--;
    	    printf("Address:  0x%x\n", sim_rebus_addr);
    	    printf("Real:     0x%x\n", sim_real_data);
    	    printf("Expected: 0x%x\n", sim_rebus_data);
    	} while ((sim_real_data != sim_rebus_data) && time);
    } else if (tmp_re_rd_in->format == 3) { /* noop */
	;
    }
    return 1;
}

int read_frame_buffer(xmax, ymax, bufType)
__uint32_t xmax, ymax, bufType;
{
    __uint32_t numRows = xmax + 1;
    __uint32_t numCols = ymax + 1;
    __uint32_t pixComponents = 4;
    __uint32_t pixComponentSize = 1;
    __uint32_t beginSkipBytes = 0;
    __uint32_t strideSkipBytes = 0;
    __uint32_t rss_clrmsk_msb_pp1 = ~0x0;
    __uint32_t rss_clrmsk_lsba_pp1 = ~0x0;
    __uint32_t rss_clrmsk_lsbb_pp1 = ~0x0;
    __uint32_t xfrsize, xyStart, xyEnd, bytesPerPixel, scanWidth, pixCmdPP1;
    __uint32_t reif_dmaType, nWords, i, j, bufCode, drb_ptrs;
    __uint32_t tiles_in_y, tiles_in_x, span_per_tile, pix_per_tilespan;
    __uint32_t st_x, st_y, tile_x, tile_y, rdm_index, rd_ram, rd_ram_temp;
    __uint32_t pix_index, pix_pair, pix_rg, pix_ba, pix_gb;

    int 	 pp, rd, imp, x, y, pprd, rp;

    fillmodeu    fillMode = {0};
    xfrmodeu     xfrMode = {0};
    ppfillmodeu  ppfillMode = {0};
    iru          ir = {0};
    xfrcontrolu  xfrCtrl = {0};

    drb_ptrs = 0x0;
    ZFlag = 0x0;
    switch (bufType) {
      case ZST_BUF:
		bufCode = 0xe;
    		ZFlag = 0x1;
		break;
      case OVL_BUF:
		bufCode = 0x4;
		break;
      case AB_BUF:
		drb_ptrs = (0x240 | (0x240 << 10));
		bufCode = 0x0;
		break;
      default:
		printf(stderr, "Error: Shouldn't be here\n");
		exit(-1);
		break;
    } /* switch */


    /* Clear the CD Flag bit, so we'll know when we're done */
    HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, 0x0, ~0x0);

    nWords = numCols * numRows;

    bytesPerPixel = pixComponents * pixComponentSize;
    scanWidth = bytesPerPixel * numCols;

    fillMode.bits.BlockType  = 0x2 ; /* ((dmakind == DMA_BURST) ? 0x4 : 0x2); */
    fillMode.bits.BlockType |= 0x0; /* ((readOrWrite == DMA_WRITE) ? 0x1: 0x0); */

    xfrMode.bits.StrideSkipBytes = strideSkipBytes;
    xfrMode.bits.BeginSkipBytes = beginSkipBytes;
    xfrMode.bits.PixelFormat = 0x8;  /* XXX: Hard-coded for RGBA */

    ppfillMode.bits.PixType = 0x3; /* XXX: RGB 36bit */
    ppfillMode.bits.BufCount = 0;
    ppfillMode.bits.BufSize  = 1;
    ppfillMode.bits.LogicOP = 3; /* src */
    ppfillMode.bits.DrawBuffer  = 1;
    ppfillMode.bits.ReadBuffer  = bufCode;

    ir.bits.Setup = 1;
    ir.bits.Opcode = 0x8; /* BLOCK MODE */
 
    xfrCtrl.bits.XFR_DMAStart = 0x0; /* ((dmakind == DMA_BURST) ? 0x1 : 0x0); */
    xfrCtrl.bits.XFR_PIOStart = 0x1; /* ((dmakind == DMA_BURST) ? 0x0 : 0x1); */
    xfrCtrl.bits.XFR_Device   = 0x0; /* ((xfrDevice == DMA_PP1) ? 0x0 : 0x1); */
    xfrCtrl.bits.XFR_Direction = 0x1; /* ((readOrWrite == DMA_WRITE) ? 0x0 : 0x1); */
 
    pixCmdPP1 = 0x2; /* ((readOrWrite == DMA_WRITE) ? 0x3 : 0x2); */
    reif_dmaType = 0x2; /* ((readOrWrite == DMA_WRITE) ? 0x0 : 0x2); */

    xfrsize = (numRows << 16) + numCols;
    xyStart = 0x0;
    xyEnd = (((numCols - 1) << 16) + (numRows - 1));

    /* Program REIF registers */
    HQ3_PIO_WR((REIF_CTX_BASE + (REIF_SCAN_WIDTH << 2)), scanWidth, ~0x0);
    HQ3_PIO_WR((REIF_CTX_BASE + (REIF_DMA_TYPE << 2)), reif_dmaType, ~0x0);

    /* Program RE registers for data transder */
    HQ3_PIO_WR_RE(FILLMODE, fillMode.val, ~0x0);
    HQ3_PIO_WR_RE(XFRMODE, xfrMode.val, ~0x0);
    HQ3_PIO_WR_RE(XFRSIZE, xfrsize, ~0x0);
    HQ3_PIO_WR_RE(XFRCOUNTERS, xfrsize, ~0x0);
    HQ3_PIO_WR_RE(BLOCK_XYSTARTI, xyStart, ~0x0);
    HQ3_PIO_WR_RE(BLOCK_XYENDI, xyEnd, ~0x0);
    HQ3_PIO_WR_RE(RSS_PIXCMD_PP1, pixCmdPP1, ~0x0);
    HQ3_PIO_WR_RE(RSS_FILLCMD_PP1, ppfillMode.val, ~0x0);
    HQ3_PIO_WR_RE(RSS_DRBPTRS_PP1, drb_ptrs, ~0x0);
    HQ3_PIO_WR_RE(RSS_DRBSIZE_PP1, 0x31e, ~0x0);
    HQ3_PIO_WR_RE(RSS_COLORMASK_MSB_PP1, rss_clrmsk_msb_pp1, ~0x0);
    HQ3_PIO_WR_RE(RSS_COLORMASK_LSBA_PP1, rss_clrmsk_lsba_pp1, ~0x0);
    HQ3_PIO_WR_RE(RSS_COLORMASK_LSBB_PP1, rss_clrmsk_lsbb_pp1, ~0x0);
    HQ3_PIO_WR_RE(IR, ir.val, ~0x0);
    HQ3_PIO_WR_RE(XFRCONTROL, xfrCtrl.val, ~0x0);

#if 1
    for (i = 0; i < nWords; i+=2) {
	HQ3_PIO_RD((RSS_BASE + (CHAR_H << 2) + 0x1000), ~0x0, pix_rg);

	/* grab 12 bits of each component */
	pixs[j][0] = ((pix_rg & 0xffff0000)>>16); /* red   */
	pixs[j][1] = ((pix_rg & 0x0000ffff));     /* green */

	HQ3_PIO_RD((RSS_BASE + (CHAR_L << 2)), ~0x0, pix_ba);

	/* grab 12 bits of each component */
	pixs[j][2] = ((pix_ba & 0xffff0000)>>16); /* blue  */
	pixs[j][3] = ((pix_ba & 0x0000ffff));     /* alpha */
    }
#endif

    /* Complete the DMA process */
    HQ3_PIO_WR_RE(RSS_PIXCMD_PP1, 0x0, ~0x0);

    for (x = 0; x < 6; x++) {
    /* HACK */
      for (y = 0; y < (WSIZEX*WSIZEY); y++) {
    	rdm[x][y] = 0;
      }
    }

    tiles_in_y = WSIZEY/16;
    tiles_in_x = WSIZEX/192;
    span_per_tile = 16;
    pix_per_tilespan = 192;

    /* scan through each pixel of each supper tile stored in pixs[] and 
       copy into rdm array. rdm is an array containing the pixel data in 
       the format stored in the RDRAM and is used by the dump routine. 
       The format of rdm is: rdm[6][[WSIZEX*WSIZEY/3], 
       pixel=(rdm[index+3]<<9+rdm[index+2],rdm[index+1]<<9+rdm[index] 
       pixs is an array containing the DMA read pixel data. 
       pixs[window_xsize*window_ysize][r,g,b,a]
       Note: format of color component in Char_h and Char_l regs is: 
       color lsb @ bit 4, color msb @ bit 15, color msbs replicated in 
       char_l/h bits 3-0 
    */

#define swiz_b_0(blu) ((blu &0x2000)>>5)|((blu &0x4000)>>9)|((blu &0x8000)>>13)

#define swiz_r_0(red) ((red &0x2000)>>6)|((red &0x4000)>>10)|((red &0x8000)>>14)

#define swiz_g_0(grn) ((grn &0x2000)>>7)|((grn &0x4000)>>11)|((grn &0x8000)>>15)


#define swiz_b_1(blu) ((blu &0x400)>>2)|((blu &0x800)>>6)|((blu &0x1000)>>10)

#define swiz_r_1(red) ((red &0x400)>>3)|((red &0x800)>>7)|((red &0x1000)>>11)

#define swiz_g_1(grn) ((grn &0x400)>>4)|((grn &0x800)>>8)|((grn &0x1000)>>12)


#define swiz_b_2(blu) ((blu &0x80)<<1)|((blu &0x100)>>3)|((blu &0x200)>>7)

#define swiz_r_2(red) ((red &0x80)>>0)|((red &0x100)>>4)|((red &0x200)>>8)

#define swiz_g_2(grn) ((grn &0x80)>>1)|((grn &0x100)>>5)|((grn &0x200)>>9)


#define swiz_b_3(blu) ((blu &0x10)<<4)|((blu &0x20)>>0)|((blu &0x40)>>4)

#define swiz_r_3(red) ((red &0x10)<<3)|((red &0x20)>>1)|((red &0x40)>>5)

#define swiz_g_3(grn) ((grn &0x10)<<2)|((grn &0x20)>>2)|((grn &0x40)>>6)


    rdm_index = 0;	/* used to index through rdm[][] array */ 
    rd_ram =0; /* keeps track of which rd_ram is at (0,0) of the supertile */
    rd_ram_temp =0; /* used to adjust rd_ram at start of each supertile in y */
  
    for (tile_y = 0; tile_y <= tiles_in_y - 1; tile_y++) {
      for (tile_x = 0; tile_x <= tiles_in_x - 1; tile_x++) { 
        rd_ram = rd_ram_temp; 
        for (st_y = 0; st_y <= span_per_tile - 1; st_y++) {
          for (st_x = 0; st_x <= (pix_per_tilespan/(3*2*2)) - 1; st_x++) {  
		/* 3*2*2 = pix per rd_pp_pix_pair */
            for (rd = 0; rd <= 2; rd++) {
	     for (pix_pair=0; pix_pair <= 1; pix_pair++) {
              for (pp = 0; pp <= 1; pp++) {
	        pix_index =(tile_y * (tiles_in_x * pix_per_tilespan * 
						span_per_tile) 
	  	        + tile_x * (pix_per_tilespan) 
 		        + st_y * (tiles_in_x * pix_per_tilespan) 
                        + st_x*3*2*2 + rd*4 + pix_pair*2 + pp);	 
	      
	        /* transform rgba from pixs array into rdram format */
                rdm[rd_ram + pp*3][rdm_index+pix_pair*4   ] = 
				swiz_r_0 (pixs[pix_index][0]) |
                                swiz_g_0 (pixs[pix_index][1]) |
                                swiz_b_0 (pixs[pix_index][2]) ;

                rdm[rd_ram + pp*3][rdm_index+pix_pair*4 +1] = 
				swiz_r_1 (pixs[pix_index][0]) |
                                swiz_g_1 (pixs[pix_index][1]) |
                                swiz_b_1 (pixs[pix_index][2]) ;

                rdm[rd_ram + pp*3][rdm_index+pix_pair*4 +2] = 
				swiz_r_2 (pixs[pix_index][0]) |
                                swiz_g_2 (pixs[pix_index][1]) |
                                swiz_b_2 (pixs[pix_index][2]) ;

                rdm[rd_ram + pp*3][rdm_index+pix_pair*4 +3] = 
				swiz_r_3 (pixs[pix_index][0]) |
                                swiz_g_3 (pixs[pix_index][1]) |
                                swiz_b_3 (pixs[pix_index][2]) ;

              } /* pp */
             } /* pix_pair */
	    if(rd_ram == 2) { rd_ram = 0; } else { rd_ram++; }
            } /* rd */
            rdm_index = rdm_index +8;
          } /* st_x */
          if(rd_ram == 2) { rd_ram = 0; } else { rd_ram++; } 		
	  /* at end of every span, inc rd_ram */
        } /* st_y */
      } /* tile_x */
      if(rd_ram_temp == 2) { rd_ram_temp = 0; } else { rd_ram_temp++; } 	/* at end of every tile in y, inc rd_ram_temp */
    } /* tile_y */

}

unsigned int getPIO(int pp, int rd, unsigned int adr) {
    unsigned int k,delay, value;
 
    delay = 200;
    if (!ZFlag) {
	HQ3_PIO_WR_RE(RSS_DEV_ADDR, ((pp<<28)|0x04120000+(adr*2)+(rd*0x01000000)), ~0x0);
    } else {
	HQ3_PIO_WR_RE(RSS_DEV_ADDR, ((pp<<28)|0x04000000+(adr*2)+(rd*0x01000000)), ~0x0);
    }
    for (k=0; k!=delay; k++) {}
    HQ3_PIO_RD_RE(RSS_DEV_DATA, ~0x0, value);
    return(value);
}

void getData(register ushort *rdmm, int pp, int rd, int vc, int dump) {
  register int i,j;
  unsigned int adr, index;
  register int retData;
  unsigned short d[2];
  int tmp;
  
  register int lastRetData;
  long int lastData = -1;
  int repeatFlag = 1;
  
  adr = startTile*(1<<10);
  index = sb+(2*adr);
  
  for (j = startTile; j <= endTile; j++ ) { 
    if (verbose == 2) {
      printf(".");
      fflush(stdout);
    }
    
   for (i = 0; i < PAGE/3; i++ ) {
/* HACK */
#ifdef FULL_DISPLAY
      if (!dump) { 
#else
      if ((!dump)&&(((j%7)==0)||(((j-1)%7)==0))) {

#endif
	retData = getPIO(pp, rd, adr);
	d[0] = retData & 0x1FF;
	d[1] = (retData >> 9) & 0x1FF;
	rdmm[index] = d[0];
	rdmm[index + 1] = d[1];
      }
/* HACK */
#ifdef FULL_DISPLAY
      if (dump) { 
#else
      if ((dump)&&(((j%7)==0)||(((j-1)%7)==0))) {
#endif

	retData = (rdmm[index+1]<<9)+rdmm[index];
	if (i&1) {
	  /* The 0x10 below makes the address the */
	  /* correct address for the starting page */
	  /* (0x0?008000) instead of (0x0?000000). */
	  if (((retData<<18)+lastRetData != lastData)||(i==1)) { 
    	    if (!ZFlag) {
	        fprintf(fp[pp][rd],"%0.3X.%0.3X ",0x240+((((adr-1)>>10)&0xff)),((adr-1)<<1)&((1<<11)-1));
    	    } else {
                fprintf(fp[pp][rd],"%0.3X.%0.3X ",0x000+((((adr-1)>>10)&0xff)),((adr-1)<<1)&((1<<11)-1));
    	    }
	    fprintf(fp[pp][rd],"%0.6o%0.6o\n",retData,lastRetData);
	    repeatFlag = 1;
	  } else if (repeatFlag) {
	    fprintf(fp[pp][rd],"...\n");
	    repeatFlag = 0;
	  }
	  lastData = (retData<<18)+lastRetData;
	}
	lastRetData = retData;
      }
      index = index+2;
      adr++;
    }
  }
  if (verbose == 2) {
    printf("\n");
  }
}


/* Update fb and dump to a file. */
int dumpFB (void) {
  register int i, j, k;
  
  int pprd;
  
  /* Open the files to dump to; one for each RDRAM. */
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 3; j++) {
	sprintf(filename, "PP%d_RD%d.%d.emou", i, j, counter);
	fp[i][j] = fopen(filename,"w");
	if (fp[i][j]) {
	  chmod(filename, 0666);
	  pprd = (i * 3) + j;
	  rdmp[pprd] = sb;
	  if (verbose == 2) {
	    printf("Now dumping data from PP%d_RD%d", i, j);
	  }
	  getData(rdm[pprd], i, j, 1, 1);
	  fclose(fp[i][j]);
	} else {
	  if (verbose == 2) {
	    printf("Could not open file %s for writing.\n", filename);
	  }
	}
    }
  }
  
  counter++;
}


