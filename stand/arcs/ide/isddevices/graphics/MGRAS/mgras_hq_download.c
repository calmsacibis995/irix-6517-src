#include <math.h>
#include "uif.h"
#include "sys/mgrashw.h"

#include "mgras_diag.h"
#include "parser.h"
#include "mgras_hq3.h"
#include "ide_msg.h"


__uint32_t 
mgrasDownloadHQ3Ucode(mgras_hw *mgbase, __uint32_t *UcodeTable, __uint32_t UcodeTableSize, __uint32_t print_msg)
{
	__uint32_t j;
	__uint32_t rdata;
	__uint32_t error;

#ifndef MGRAS_BRINGUP

	HQ3_PIO_RD(HQ3_CONFIG_ADDR, ~0x0, rdata);
	rdata |= HQ3_UCODE_STALL;
#ifdef DEBUG
	fprintf(stderr,"UcodeStall Status  :- %x\n" ,rdata);
#endif
	HQ3_PIO_WR(HQ3_CONFIG_ADDR, rdata, ~0x0);

#ifdef DEBUG
	fprintf(stderr,"UcodeStall Programmed  :- %x\n" ,rdata);
#endif

	/* Clear out RAM with initial value
	 * XXX For now, use 0xffffffff to find errors in microcode
	 * Use 0 later.
	 */
#if 0
	for (j = 0; j < HQ3_UCODERAM_LEN;j++)
#ifndef MGRAS_DIAG_SIM 
		mgbase->hquc[j] = MGRAS_UCODERAM_INITVAL; 
#else
		HQ3_PIO_WR((j << 2), MGRAS_UCODERAM_INITVAL, ~0x0, BE_MOD, 0x0);
#endif

#endif

	/* Load HQ3 ucode 
	 */

	msg_printf(DBG, "Load HQ ucode\n");

	for (j = 0; j < UcodeTableSize;j+=2) {
#ifndef MGRAS_DIAG_SIM 
		mgbase->hquc[UcodeTable[j]] = (UcodeTable[j+1] >> 8); 
#else
		HQ3_PIO_WR((UcodeTable[j] << 2), (UcodeTable[j+1] >> 8), ~0x0, BE_MOD, 0);
#endif
	}

	/* Read back and compare*/
	msg_printf(DBG, "Read back and compare\n");

	for (error = 0,j = 0; j < UcodeTableSize;j+=2)
	{
#ifndef MGRAS_DIAG_SIM 
		rdata = mgbase->hquc[UcodeTable[j]];
#else
		HQ3_PIO_RD((UcodeTable[j] << 2), ~0x0, rdata);
#endif
		rdata &= 0xffffff;
		if (rdata  != ((UcodeTable[j+1] >> 8) & 0xffffff)) 
		{
#ifdef DEBUG
		fprintf(stderr, "read = %x; expected = %x\n", rdata, (UcodeTable[j+1] >> 8));
			if (print_msg) 
			{
				msg_printf(ERR, "ERROR: HQ offset 0x%x: ",j);
				msg_printf(ERR, "expected 0x%x, returned 0x%x\n",
					(UcodeTable[j+1] >> 8),rdata);
			}
#endif
			error++;
		}
	}

	if (error)  {
		msg_printf(ERR, "%d errors were detected in HQ ucode\n", error);
	}
	else  {
		msg_printf(VRB, "\nHQ ucode download was successful -- no errors detected\n");
	}

#endif /* MGRAS_BRINGUP */
	return error;	
}
