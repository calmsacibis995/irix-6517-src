/*
 * Code to generate Test Vectors (tv format) for the basic hq bring-up
 */
#include <math.h>
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "tv_common.h"
#include "mgras_diag.h"
#include "hq3test.h"
#include "mgras_sim.h"

type_tv_GENERIC	tv;

mgras_hw  *mgbase;
mgras_hw mgbaseactual;

void *srcAddr = 0;
uint   hregsBase, hiWord, loWord;
uint   curAddr, mgbaseAddr, addrOffset;


#undef MGRAS_GFX_CHECK()
#define	MGRAS_GFX_CHECK()

extern char *getenv(char *);


int genUcodeGE(int opt) {
	fprintf(stderr, "This program does not generate GE ucode.\n");
	return __LINE__;
}

int genUcodeHQ(int opt) {
	fprintf(stderr, "This program does not generate HQ ucode.\n");
	return __LINE__;
}

int testcode(int opt) {

	int rc = 0;
	int TestToken;

	TestToken = atoi(getenv("TestNumber"));

	/*
	 * The tv_skel.c skeleton declares and sets opt to
	 * the number <n> of the current directory opt<n>.
	 */

/* Customize by writing your own test code from here >>>>>>>>>>>>>>>>>>>>>>>>>*/

	if (opt == 1 || opt == 2) {

    		hregsBase = &h->hquc[0];
    		mgbase = (struct mgras_hw *) &mgbaseactual;

    		HostHqComment("==== Start of test");


TestToken = VC3_ADDR_BUS_TEST;
		mgras_DCBinit(mgbase);

		HostHqComment("CURSOR MODE Test");
		mgras_VC3CursorMode(GLYPH_32);
		mgras_StartTiming();
#if 0
		MgrasDownloadGE11Ucode();

		switch (TestToken) {
			case	VC3_ADDR_BUS_TEST:
				HostHqComment("VC3_DATA_BUS_TEST");
				mgras_VC3DataBusTest();

				HostHqComment("VC3 Address 64 Test");
				mgras_VC3Addrs64();

				HostHqComment("XMAP 0 DCB Test");
				_mgras_XmapDcbRegTest(0);

				HostHqComment("XMAP 1 DCB Test");
				_mgras_XmapDcbRegTest(1);

				HostHqComment("VC3_REG_TEST");
				mgras_VC3InternalRegTest();

				HostHqComment("VC3_ADDR_BUS_TEST");
				mgras_VC3AddrsBusTest();

				HostHqComment("VC3_ADDR_UNIQ_TEST");
				mgras_VC3AddrsUniqTest();

				HostHqComment("VC3_PATRN_TEST");
				mgras_VC3PatrnTest();

				HostHqComment("CMAP REV");
				mgras_CmapRevision();

				HostHqComment("CMAP UniqTest");
				mgras_CmapUniqTest();

				HostHqComment("CMAP0_ADDR_BUS_TEST");
				_mgras_CmapAddrsBusTest(0);

				HostHqComment("CMAP0_DATA_BUS_TEST");
				_mgras_CmapDataBusTest(0);

				HostHqComment("CMAP0_ADDR_UNIQ_TEST");
				_mgras_CmapAddrsUniqTest(0);

				HostHqComment("CMAP0_PATRN_TEST");
				_mgras_CmapPatrnTest(0);

				HostHqComment("CMAP1_ADDR_BUS_TEST");
				_mgras_CmapAddrsBusTest(1);

				HostHqComment("CMAP1_DATA_BUS_TEST");
				_mgras_CmapDataBusTest(1);

				HostHqComment("CMAP1_ADDR_UNIQ_TEST");
				_mgras_CmapAddrsUniqTest(1);

				HostHqComment("CMAP1_PATRN_TEST");
				_mgras_CmapPatrnTest(1);

				break;

			default :
				HostHqComment("Unknown Test Number");
			break;
		}
#endif
		HostHqComment("===== End of Test =====");

    HostHqIdleThenDie(1000);
    INT_host_csimKill (0, tv, 0, 0, 100);
    HostHqPassTv (&tv);

	} else {
		fprintf(stderr, "Only tests opt[12] are coded so far\n");
		rc = __LINE__;
	}
	return rc;
}

char *shortdesc = "BackEnd bringup test";

char *fulldesc = "BackEnd bringup test: \
brief tests covering most of BackEnd";

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< to here*/
