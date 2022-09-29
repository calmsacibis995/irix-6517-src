/*
 * This file contains the definition of all of the diagnostic strings
 * in the system.  We moved it into and include file to avoid problems
 * with keeping multiple copies of the file up-to-date.
 *
 * This file may only be included in only one file in the entire 
 * system. 
 */

#include <sys/EVEREST/evdiag.h>

diagval_t diagval_map[] = {
    /* sucess */
    {EVDIAG_PASSED,		0, 0,	"Device passed diagnostics."},

    /* cache tests */
    {EVDIAG_DCACHE_DATA,	0, 0,	"Failed dcache data test."},
    {EVDIAG_DCACHE_ADDR,	0, 0,	"Failed dcache addr test."},
    {EVDIAG_DCACHE_TAG,		0, 0,	"Failed dcache tag test."},
    {EVDIAG_SCACHE_TAG,		0, 0,	"Failed scache tag test."},

    {EVDIAG_SCACHE_DATA,	0, 0,	"Failed scache data test."},
    {EVDIAG_SCACHE_ADDR,	0, 0,	"Failed scache addr test."},

    {EVDIAG_ICACHE_DATA,	0, 0,	"Failed icache data test."},
    {EVDIAG_ICACHE_ADDR,	0, 0,	"Failed icache addr test."},

    {EVDIAG_DCACHE_HANG,	0, 0,	"Dcache test hung."},
    {EVDIAG_SCACHE_HANG,	0, 0,	"Scache test hung."},
    {EVDIAG_ICACHE_HANG,	0, 0,	"Icache test hung."},
    {EVDIAG_CACHE_INIT_HANG,	0, 0,	"I & D cache init hung."},
    {EVDIAG_BUSTAG_DATA,	0, 0, 	"Failed duplicate bustag data test."},

    /* memory tests */
    {EVDIAG_BIST_FAILED,	0, 0,	"Memory built-in self-test failed."},
    {EVDIAG_NO_MEM,		0, 0,	"No working memory was found."},
    {EVDIAG_BAD_ADDRLINE,	0, 0,	"Memory address line test failed."},
    {EVDIAG_BAD_DATALINE,	0, 0,	"Memory data line test failed."},
    {EVDIAG_BANK_FAILED,	0, 0,	"Bank failed configured memory test."},
    {EVDIAG_WRCFG_HANG,		0, 0,	"Slave hung writing to memory."},
    {EVDIAG_DOWNREVMA,		0, 0,	"Bank disabled due to downrev MA chip."},
    {EVDIAG_MC3CONFBUSERR,	0, 0,   "A bus error occurred during MC3 config."},
    {EVDIAG_MC3TESTBUSERR,	0, 0,   "A bus error occurred during MC3 testing."},
    {EVDIAG_MC3DOUBLEDIS,	0, 0,	"PROM attempted to disable the same bank twice."},
    {EVDIAG_MC3NOTENOUGH,	0, 0,	"Not enough memory to load the IO4 PROM."},
    {EVDIAG_NOMC3,		0, 0,	"No memory boards were recognized."},
    {EVDIAG_MEMREENABLED,	0, 0,	"Bank forcibly re-enabled by the PROM."},

    /* EBus tests */
    {EVDIAG_R4K_INTS,		0, 0,	"CPU doesn't get interrupts from CC."},
    {EVDIAG_GROUP_FAIL, 	0, 0,	"Group interrupt test failed."},
    {EVDIAG_LOST_INT,		0, 0,	"Lost a loopback interrupt."},
    {EVDIAG_STUCK_HPIL,		0, 0,	"Bit in HPIL register stuck."},

    /* IO4 tests */
    {EVDIAG_NOIO4,		0, 0,	"No working IO4 is present."},
    {EVDIAG_BADCKSUM,		0, 0,	"Bad checksum on IO4 PROM."},
    {EVDIAG_BADENTRY,		0, 0,	"Bad entry point in IO4 PROM."},
    {EVDIAG_TOOLONG,		0, 0,	"IO4 PROM claims to be too long."},
    {EVDIAG_BADSTART,		0, 0,	"Bad entry point in IO4 PROM."},
    {EVDIAG_BADMAGIC,		0, 0,	"Bad magic number in IO4 PROM."},
    {EVDIAG_DLBUSERR,		0, 0,	"Bus error while downloading IO4 PROM."},
    {EVDIAG_NOEPC,		0, 0,	"No EPC chip found on master IO4."},
    {EVDIAG_CFGBUSERR,		0, 0,	"Bus error while configuring IO4."},
    {EVDIAG_IAREG_BUSERR,	0, 0,	"Bus error during IA register test."},
    {EVDIAG_IAPIO_BUSERR,	0, 0,	"Bus error during IA PIO test."},
    {EVDIAG_IAREG_FAILED,	0, 0,	"IA chip register test failed."},
    {EVDIAG_IAPIO_BADERR,	0, 0,	"Wrong error reported for bad PIO."},
    {EVDIAG_IAPIO_NOINT,	0, 0,	"IA error didn't generate interrupt."},
    {EVDIAG_IAPIO_WRONGLVL,	0, 0,	"IA error generated wrong interrupt."},
    {EVDIAG_EPCREG_FAILED,	0, 0,	"EPC register test failed."},
    {EVDIAG_MAPRDWR_BUSERR,	0, 0,	"Bus error on map RAM rd/wr test."},
    {EVDIAG_MAPADDR_BUSERR,	0, 0,	"Bus error on map RAM address test."},
    {EVDIAG_MAPWALK_BUSERR,	0, 0,	"Bus error on map RAM walking 1 test."},
    {EVDIAG_MAP_BUSERR,		0, 0,	"Bus error during map RAM testing."},
    {EVDIAG_MAPRDWR_FAILED,	0, 0,	"Map RAM read/write test failed."},
    {EVDIAG_MAPADDR_FAILED,	0, 0,	"Map RAM address test failed."},
    {EVDIAG_MAPWALK_FAILED,	0, 0,	"Map RAM walking 1 test failed."},
    {EVDIAG_LOOPBACK_FAILED,	0, 0,	"EPC UART loopback test failed."},
    {EVDIAG_EPCREG_BUSERR,	0, 0,	"Bus err while testing EPC."},

    /* CPU board error codes */
    {EVDIAG_CANTSEEMEM,		0, 0,	"CPU can't access memory"},
    {EVDIAG_BUSTAGDATA,         0, 0,   "CC bus tag data test failed."},
    {EVDIAG_BUSTAGADDR,         0, 0,   "CC bus tag addr test failed."},
    {EVDIAG_CPUREENABLED,	0, 0,	"CPU forcibly re-enabled by the PROM."},
    {EVDIAG_UNFIXABLE_EAROM,	0, 0,	"CPU's EAROM can't be corrected."},
    {EVDIAG_EAROM_CKSUM,	0, 0,	"CPU's EAROM checksum is bad."},
    {EVDIAG_EAROM_REPAIRED,	0, 0,	"CPU's EAROM has been repaired - power-cycle the system to re-enable."},
    {EVDIAG_ERTOIP,		0, 0, 	"CPU's ERTOIP errors."},

    /* CC registers */
    {EVDIAG_CCJOIN, 		0, 0,   "CC join counter reg test failed."},
    {EVDIAG_WG,			0, 0,   "CC write gatherer reg test failed. "}, 
    {EVDIAG_FPU,		0, 0,   "FPU functionality test failed. "}, 
    /* Miscellaneous */
    {EVDIAG_WRCPUINFO,		0, 0,	"CPU writing configuration info."},

    {EVDIAG_TESTING_CCTAGS,	0, 0,   "CPU testing CC tags."},
    {EVDIAG_TESTING_DCACHE,	0, 0,	"CPU testing dcache."},
    {EVDIAG_TESTING_ICACHE,	0, 0,	"CPU testing icache."},
    {EVDIAG_TESTING_SCACHE,	0, 0,	"CPU testing scache."},
    {EVDIAG_INITING_CACHES,	0, 0,	"CPU initializing caches."},
    {EVDIAG_INITING_SCACHE,	0, 0,   "CPU initializing SCACHE."},
    {EVDIAG_TESTING_CCJOIN,	0, 0,   "CPU testing CC Join counters."},
    {EVDIAG_TESTING_CCWG,	0, 0, 	"CPU testing write gatherer."},
    
    {EVDIAG_RETURNING,		0, 0,	"CPU returning from master's code."},
    {EVDIAG_PANIC,		0, 0,	"Unexpected exception."},
    {EVDIAG_NMI,		0, 0,	"A nonmaskable interrupt occurred."},
    {EVDIAG_DEBUG,		0, 0,	"POD mode switch set or POD key pressed."},
    {EVDIAG_TBD,		0, 0,	"Unspecified diagnostic failure."},
    {EVDIAG_NOT_SET,		0, 0,	"Diagnostic value unset."},
    {EVDIAG_NOT_PRESENT,	0, 0,	"Device not present."}
};
