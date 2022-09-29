/*
 * Code to generate Test Vectors (tv format) for the basic hq bring-up
 */
#include "hq3test.h"
#include "ge11symtab.h"
#include <math.h>
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "mgras_sim.h"
#include "initRdRam.c"
#include "initTexMode.c"
#include "mgras_dma.h"

type_tv_GENERIC	tv;

mgras_hw  *mgbase;
mgras_hw mgbaseactual;

void *srcAddr = 0;
uint hregsBase;
uint mgbaseAddr,  curAddr, baseAddr, addrOffset;

#define TE_BASE		30
#define TR_BASE		40
#define RD_BASE		50
#define PP_BASE		60

#define RE4_READ_RE_REG_TEST	72
#define RE4_WRITE_RE_REG_TEST	73
#define RE4_WRITE_RAC_REG_TEST	74
#define RE4_READ_RAC_REG_TEST	75
#define TE1_READ_TE_REG_TEST	76
#define TE1_WRITE_TE_REG_TEST	77
#define RD_WRITE_REG_TEST	78
#define RD_READ_PIO		79
#define RD_WRITE_PIO		80

extern char *getenv(char *);
extern __uint32_t PIXEL_TILE_SIZE;

__uint32_t Reportlevel=4;

int genUcodeGE(int opt) {
	fprintf(stderr, "This program does not generate GE ucode.\n");
	return __LINE__;
}

int genUcodeHQ(int opt) {
	fprintf(stderr, "This program does not generate HQ ucode.\n");
	return __LINE__;
}

int
testcode(int opt)
{

	int TestToken;
	ui32 HQStatus = 0;
        ui32 HQBusy = 0;
	ui32 errors = 0;

	TestToken = atoi(getenv("TestNumber"));

	/*
	 * The tv_skel.c skeleton declares and sets opt to
	 * the number <n> of the current directory opt<n>.
	 */

/* Customize by writing your own test code from here >>>>>>>>>>>>>>>>>>>>>>>>>*/


    		hregsBase = &h->hquc[0];
    		mgbase = (struct mgras_hw *) &mgbaseactual;

#if 0
		HostGeTestPreface();
    		HostHqComment("Start of test");
		
HostHqComment("-------- About to do dcb transactions --------");
                ConvertScript(0);

HostHqComment("-------- About to Poll for DCB Idle ----------");

                HostHqPollWord( 0, HQ_GIOSTAT_DCB_BUSY_BIT, &h->gio_status);

HostHqComment("-------- DCB is now IDLE ----------------------");

		HostHqComment("-------- About to call GE global initialize --------");
		GE_init();

		/* mgras_HQ3Init(mgbase); */

                SaveCommand(BUILD_CONTROL_COMMAND(0, 0, 0, 0,
                        CP_PASS_THROUGH_GE_FIFO_3, 2 * sizeof(int)),
                      1, mge_REBusSync);
		HostHqComment("-------- About to restore REBUS sync request --------");
                RestoreCommands();
		HostHqComment("-------- About to poll for REBUS sync request --------");
                HostHqPollWord( 1, 1, (void *)&h->reif_ctx.rebus_sync );
		HostHqComment("-------- Saw REBUS sync request --------");

#endif
		mgras_HQ3Init(mgbase);

		switch (opt) {
			case 	RE4_STATUS_REG_TEST: /* 0 */
				HostHqComment("RE4_STATUS_REG_TEST");
				mgras_REStatusReg(1, "\0");
			break;
			case	RE4_RDWR_REGS_TEST: /* 1 */
				HostHqComment("RE4_RDWR_REGS_TEST");
				mgras_RERdWrRegs(1, "\0");
			break;
			case	RE4_RAC_REG_TEST: /* 2 */
				HostHqComment("RE4_RAC_REG_TEST");
				mgras_RERACReg(1, "\0");
			break;
			case	RE4_INTERNALRAM_TEST: /* 4 */
				HostHqComment("RE4_INTERNALRAM_TEST");
				mgras_REInternalRAM(1, "\0");
			break;
			case	RE4_READ_RE_REG_TEST: /* 72 */
				HostHqComment("RE4_READ_RSS_REG_TEST");
				mgras_ReadRSSReg(1, "\0");
			break;
			case	RE4_WRITE_RE_REG_TEST: /* 73 */
				HostHqComment("RE4_WRITE_RSS_REG_TEST");
				mgras_WriteRSSReg(1, "\0");
			break;
			case	RE4_READ_RAC_REG_TEST: /* 75 */
				HostHqComment("RE4_READ_RAC_REG_TEST");
				mgras_ReadRACReg(1, "\0");
			break;
			case	RE4_WRITE_RAC_REG_TEST: /* 74 */
				HostHqComment("RE4_WRITE_RAC_REG_TEST");
				mgras_WriteRACReg(1, "\0");
			break;
			case	TE_BASE + TE1_REV_TEST: /* 30 */
				HostHqComment("TE1_REV_TEST");
				mgras_TERev(1, "\0");
			break;
			case	TE_BASE + TE1_RDWRREGS_TEST: /* 31 */
				HostHqComment("TE1_RDWRREGS_TEST");
				mgras_TERdWrRegs(1, "\0");
			break;
			case	TE1_READ_TE_REG_TEST: /* 76 */
				HostHqComment("TE1_READ_TE_REG_TEST");
				mgras_ReadTEReg(1, "\0");
			break;
			case	TE1_WRITE_TE_REG_TEST: /* 77 */
				HostHqComment("TE1_WRITE_TE_REG_TEST");
				mgras_WriteTEReg(1, "\0");
			break;
			case	RD_BASE + RDRAM_READ_REG_TEST: /* 50 */
				HostHqComment("RDRAM_READ_REG_TEST");
				mgras_ReadRDRAMReg(1, "\0");
			break;
			case	RD_BASE + RDRAM_ADDRBUS_TEST: /* 51 */
				HostHqComment("RDRAM_ADDRBUS_TEST");
				mgras_RDRAM_Addrbus(1, "\0");
			break;
			case	RD_BASE + RDRAM_DATABUS_TEST: /* 52 */
				HostHqComment("RDRAM_DATABUS_TEST");
				mgras_RDRAM_Databus(1, "\0");
			break;
			case	RD_BASE + RDRAM_UNIQUE_TEST: /* 53 */
				HostHqComment("RDRAM_UNIQUE_TEST");
				mgras_RDRAM_unique(1, "\0");
			break;
			case	RD_BASE + RDRAM_PIO_MEM_TEST: /* 54 */
				HostHqComment("RDRAM_PIO_MEM_TEST");
				mgras_RDRAM_PIO_memtest(1, "\0");
			break;
			case	RD_WRITE_REG_TEST: /* 78 */
				HostHqComment("RD_WRITE_REG_TEST");
				mgras_WriteRDRAMReg(1, "\0");
			break;
			case	RD_READ_PIO: /* 79 */
				HostHqComment("RD_READ_PIO");
				mgras_ReadRDRAM_PIO(1, "\0");
			break;
			case	RD_WRITE_PIO: /* 80 */
				HostHqComment("RD_WRITE_PIO");
				mgras_WriteRDRAM_PIO(1, "\0");
			break;
			case	90:
				HostHqComment("RSS_INIT");
				mgras_rss_init(1, "\0");
			break;
			case 	100:
				errors = 0;
				HostHqComment("******** ALL RSS TESTS *******");

				HostHqComment("RE4_STATUS_REG_TEST");
				errors += mgras_REStatusReg(1, "\0");

				HostHqComment("RE4_RDWR_REGS_TEST");
				errors += mgras_RERdWrRegs(1, "\0");

				HostHqComment("RE4_RAC_REG_TEST");
				errors += mgras_RERACReg(1, "\0");

				HostHqComment("TE1_REV_TEST");
				errors += mgras_TERev(1, "\0");

				HostHqComment("RE4_READ_RSS_REG_TEST");
                                mgras_ReadRSSReg(1, "\0");

				HostHqComment("RE4_WRITE_RSS_REG_TEST");
                                mgras_WriteRSSReg(1, "\0");

				HostHqComment("RE4_READ_RAC_REG_TEST");
                                mgras_ReadRACReg(1, "\0");

				HostHqComment("RE4_WRITE_RAC_REG_TEST");
                                mgras_WriteRACReg(1, "\0");

				HostHqComment("TE1_RDWRREGS_TEST");
				errors += mgras_TERdWrRegs(1, "\0");

				/* has errors due to csim reading back 0s */	
				HostHqComment("RE4_INTERNALRAM_TEST");
				mgras_REInternalRAM(1, "\0");

#if 0
				HostHqComment("TE1_READ_TE_REG_TEST");
                                mgras_ReadTEReg(1, "\0");

				HostHqComment("TE1_WRITE_TE_REG_TEST");
                                mgras_WriteTEReg(1, "\0");
#endif

				HostHqComment("RDRAM_READ_REG_TEST");
                                mgras_ReadRDRAMReg(1, "\0");

				HostHqComment("RDRAM_ADDRBUS_TEST");
				errors += mgras_RDRAM_Addrbus(1, "\0");

				HostHqComment("RDRAM_DATABUS_TEST");
				errors += mgras_RDRAM_Databus(1, "\0");

				HostHqComment("RDRAM_UNIQUE_TEST");
				errors += mgras_RDRAM_unique(1, "\0");

				HostHqComment("RDRAM_PIO_MEM_TEST");
				errors += mgras_RDRAM_PIO_memtest(1, "\0");

				HostHqComment("RD_WRITE_REG_TEST");
                                mgras_WriteRDRAMReg(1, "\0");

				HostHqComment("RD_READ_PIO");
                                mgras_ReadRDRAM_PIO(1, "\0");

                                HostHqComment("RD_WRITE_PIO");
                                mgras_WriteRDRAM_PIO(1, "\0");

				HostHqComment("RSS_INIT");
                                mgras_rss_init(1, "\0");

				HostHqComment("ALL RSS DONE");
				HostHqComment("");
				if (errors == 0) {
					HostHqComment("****** PASSED!! ******");
				}
				else {
					HostHqComment("****** FAILED!! ******");
				}
				break;
			case 210: 
				HostHqComment("PIO_REDMA TEST - PP1");
				mgras_hqpio_redma(1, "\0");
				break;
			case 220: 
				HostHqComment("HQ-REDMA TEST - PP1");
				mgras_hq_redma_PP1(1, "\0");
				break;
			case 221:
				HostHqComment("HQ3(DMA Read, PIO Write) RE4(DMA))");
				PIXEL_TILE_SIZE = 192;
				_mgras_hq_redma(DMA_PP1, PIO_OP, DMA_BURST, DMA_OP, DMA_BURST, PAT_NORM, 1, 192, 1024, 1280, 1);
				break;
			case 230: 
				HostHqComment("HQDMA-REDMA TEST - TRAM");
				mgras_hq_redma_TRAM(1, "\0");
				break;
			case 231: 
				HostHqComment("HQDMA-REDMA TEST - TRAM,HQDMA");
				_mgras_hq_redma(DMA_TRAM, DMA_OP, DMA_BURST, DMA_OP, DMA_BURST);
				break;
			case 300:
				HostHqComment("COLOR TEST");
				mgras_color_tri(1, "\0");
				break;
			case 301:
				HostHqComment("Z TEST - scene 1");
				_mgras_z_tri(1);
				break;
			case 302:
				HostHqComment("Z TEST - scene 2");
				_mgras_z_tri(2);
				break;
			case 303:
				HostHqComment("Point TEST");
				mgras_points(1, "\0");
				break;
			case 304:
				HostHqComment("Line TEST");
				mgras_lines(1, "\0");
				break;
			case 305:
				HostHqComment("Stipple Polygon TEST");
				mgras_stip_tri(1, "\0");
				break;
			case 306:
				HostHqComment("Block TEST");
				mgras_block(1, "\0");
				break;
			case 307:
				HostHqComment("Character TEST");
				mgras_chars(1, "\0");
				break;
			case 308:
				HostHqComment("Tex Poly TEST");
				mgras_tex_poly(1, "\0");
				break;
			case 309:
				HostHqComment("No-Tex Poly TEST");
				mgras_notex_poly(1, "\0");
				break;
			case 310:
				HostHqComment("LOGIC OP TEST");
				mgras_logicop(1, "\0");
				break;
			case 311:
				HostHqComment("DITHER TEST");
				mgras_dither(1, "\0");
				break;
			case 312:
				HostHqComment("TE LOD TEST");
				mgras_telod(1, "\0");
				break;
			case 313:
				HostHqComment("TE DETAIL TEX TEST");
				mgras_detail_tex(1, "\0");
				break;
			case 314:
				HostHqComment("Load/Resample Test");
				mgras_ldrsmpl(1, "\0");
				break;
			case 315:
				HostHqComment("PP-DAC Test");
				mgras_pp_dac_sig();
				break;
			case 400:
				HostHqComment("mg0_clear_color");
				mg0_clear_color(1, "\0");
				break;
			case 401:
				HostHqComment("mg0_tri");
				mg0_tri(1, "\0");
				break;
			case 402:
				HostHqComment("mg0_point");
				mg0_point(1, "\0");
				break;
			case 403:
				HostHqComment("mg0_line");
				mg0_line(1, "\0");
				break;
			case 404:
				HostHqComment("B/W bars");
				mg0_bars(1, "\0");
				break;
			case 405:
				HostHqComment("DO_INDIRECT");
				mgras_do_indirect(1, "\0");
				break;
			case 406:
				HostHqComment("mg0_rect");
				mg0_rect(1, "\0");
				break;
			case 407:
				HostHqComment("mg_read_fb");
				mgras_block(1, "\0");
				mg_read_fb(1, "\0");
				break;
			case 408:
				HostHqComment("mg_xy_to_rdram");
				mg_xy_to_rdram(1, "\0");
				break;
			case 600:
				HostHqComment("mgras_RSS_64Databus");
				/* mgras_RSS_64DataBus(); */
				break;
			default :
				HostHqComment("Unknown Test Number");
			break;
		}
		HostHqComment("===== End of Test =====");

    ClearRebusSync();
    HostHqIdleThenDie(1000);
    INT_host_csimKill (0, tv, 0, 0, 100);
    HostHqPassTv (&tv);

    return errors;
}

GE_init()
{
        int ge;

        for (ge = 0; ge < NumGe; ge++) {
                SaveCommand(BUILD_CONTROL_COMMAND(0, 0, 0, 0,
                        CP_PASS_THROUGH_GE_FIFO_3, 2 * sizeof(int)),
                        1,
                        mge_GlobalInitialize);
		SaveCommand(BUILD_CONTROL_COMMAND(0, 0, 0, 0,
                                CP_PASS_THROUGH_GE_FIFO, 2 * sizeof(int)),
                        1,
                        0x1);
              }
        RestoreCommands();
}


char *shortdesc = "BackEnd bringup test";

char *fulldesc = "BackEnd bringup test: \
brief tests covering most of BackEnd";

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< to here*/
