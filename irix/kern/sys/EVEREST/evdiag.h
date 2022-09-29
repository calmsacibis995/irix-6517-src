
#ifndef __EVDIAG_H__
#define __EVDIAG_H__


#ifdef _LANGUAGE_C
/*
 * diagval_t contains the structure definitions for the
 * diagval string table.
 */

typedef struct {
    unsigned char dv_code;
    unsigned char dv_parmmask;
    unsigned char dv_fru;
    char *dv_msg;
} diagval_t;

extern diagval_t diagval_map[];

#endif


/*
 * Standard diagnostic return values - all others indicate errors.
 */
#define EVDIAG_PASSED		0x0
#define EVDIAG_TBD		0xfd
#define EVDIAG_NOT_SET		0xfe
#define EVDIAG_NOTFOUND		0xff
#define EVDIAG_NOT_PRESENT	0xff
/* 
 * These are failure codes for the various pod_* tests.  All pod tests
 * return zero on success and one of these codes on failure.
 */

/* Cache error codes */

#define EVDIAG_DCACHE_DATA	1	/* Failed dcache1 data test */
#define EVDIAG_DCACHE_ADDR	2	/* Failed dcache1 address test */
#define EVDIAG_SCACHE_DATA	3	/* Failed scache1 data test */
#define EVDIAG_SCACHE_ADDR	4	/* Failed scache1 address test */
#define EVDIAG_ICACHE_DATA	5	/* Failed icache data test */
#define EVDIAG_ICACHE_ADDR	6	/* Failed icache address test */
#define EVDIAG_DCACHE_HANG	7	/* Dcache test hung */
#define EVDIAG_SCACHE_HANG	8	/* Scache test hung */
#define EVDIAG_ICACHE_HANG	9	/* Icache test hung */
#define EVDIAG_CACHE_INIT_HANG	10	/* Cache initialization */
#define	EVDIAG_DCACHE_TAG	11	/* dcache tag ram test */
#define EVDIAG_SCACHE_TAG	12	/* scache tag ram test */
#define EVDIAG_SCACHE_FTAG	13	/* FPU scache tag ram test */
#define	EVDIAG_BUSTAG_DATA	14	/* Duplicate bus tag test failed */

/* Memory error codes */

#define	EVDIAG_BIST_FAILED	40	/* BIST failed on at least 1 mem bank */
#define EVDIAG_NO_MEM		41	/* Couldn't configure any memory */
#define EVDIAG_BAD_ADDRLINE	42	/* The address line test failed. */
#define EVDIAG_BAD_DATALINE	43	/* The data line test failed. */
#define EVDIAG_BANK_FAILED	44	/* A bank failed the configured memory
					 	test. */
#define EVDIAG_WRCFG_HANG	45	/* Slave hung writing config. */
#define EVDIAG_DOWNREVMA	46	/* Contains MA chip rev 1 or 2 */
#define EVDIAG_MC3CONFBUSERR	47	/* Bus error during MC3 config. */
#define EVDIAG_MC3TESTBUSERR	48	/* Bus error during MC3 testing. */
#define EVDIAG_MC3DOUBLEDIS	49	/* Tried to re-disable mem in test */
#define EVDIAG_MC3NOTENOUGH	50	/* Not enough memory to load IO4 prom */
#define EVDIAG_NOMC3		51	/* Couldn't see any memory boards. */
#define EVDIAG_MEMREENABLED	52	/* Bank forcibly re-enabled by PROM */

/* EBUS error codes */

#define EVDIAG_R4K_INTS		60	/* r4k doesn't get interrupt signals */
#define EVDIAG_GROUP_FAIL	61	/* Group interrupt test failed. */
#define EVDIAG_LOST_INT		62	/* Lost a looped interrupt */
#define EVDIAG_STUCK_HPIL	63	/* HIPL sticks */

/* IO4 error codes */
#define EVDIAG_NOIO4		70	/* No IO4 recognized */
#define EVDIAG_BADCKSUM		71	/* Bad checksum on IO4 prom. */
#define EVDIAG_BADENTRY		72	/* Bad entry point on IO4 prom. */
#define EVDIAG_TOOLONG		73	/* IO4 prom is too long */
#define EVDIAG_BADSTART		74	/* IO4 prom start addr bad */
#define EVDIAG_BADMAGIC		75	/* Bad magic number in IO4 prom */
#define EVDIAG_BADIA		76	/* Bad IA chip */
#define EVDIAG_BADMAPRAM	77	/* IA map RAM test fails */
#define EVDIAG_DLBUSERR		78	/* Bus err while downloading IO4 */
#define EVDIAG_NOEPC		79	/* No EPC chip on master IO4 */
#define EVDIAG_CFGBUSERR	80	/* Bus error configuring IO4 */
#define EVDIAG_IAREG_BUSERR	81	/* IA reg test bus errored. */
#define EVDIAG_IAPIO_BUSERR	82	/* IA PIO test bus errored. */
#define EVDIAG_IAREG_FAILED	83	/* IA register test failed. */
#define EVDIAG_IAPIO_BADERR	84	/* Wrong error code on bad PIO */
#define EVDIAG_IAPIO_NOINT	85	/* No error interrupt received */
#define EVDIAG_IAPIO_WRONGLVL	86	/* Error interrupt at wrong level */
#define EVDIAG_EPCREG_FAILED	87	/* EPC register test failed. */
#define EVDIAG_MAPRDWR_BUSERR	88	/* Map RAM read/write test bus erred. */
#define EVDIAG_MAPADDR_BUSERR	89	/* Map RAM address test bus erred. */
#define EVDIAG_MAPWALK_BUSERR	90	/* Map RAM walking 1 test bus erred. */
#define EVDIAG_MAP_BUSERR	91	/* Map RAM unknown source bus error */
#define EVDIAG_MAPRDWR_FAILED	92	/* Map RAM read/write test failed. */
#define EVDIAG_MAPADDR_FAILED	93	/* Map RAM address test failed. */
#define EVDIAG_MAPWALK_FAILED	94	/* Map RAM walking 1 test failed. */
#define EVDIAG_LOOPBACK_FAILED	95	/* EPC UART loopback test failed. */
#define EVDIAG_EPCREG_BUSERR	96	/* Bus err while testing EPC. */

/* CPU error codes: */
#define EVDIAG_CANTSEEMEM	120	/* Processor can't get to memory */
#define EVDIAG_BUSTAGSFAILED	121	/* (Obsolete) Bus tag test failed. */
#define EVDIAG_NIBFAILED	122	/* Niblet MP tests failed. */
#define EVDIAG_BUSTAGDATA	123	/* Bus tags data test failed. */
#define EVDIAG_BUSTAGADDR	124	/* Bus tags addr test failed. */
#define EVDIAG_CPUREENABLED	125	/* CPU forcibly re-enabled by PROM. */
#define EVDIAG_UNFIXABLE_EAROM	126	/* EAROM value can't be corrected. */
#define EVDIAG_EAROM_CKSUM	127	/* EAROM checksum is bad. */
#define EVDIAG_EAROM_REPAIRED	128	/* EAROM has been repaired... reset */
#define	EVDIAG_ERTOIP		129	/* ERTOIP has stuck error */
#define	EVDIAG_ERTOIP_COR	130	/* ERTOIP has correctable stuck ERR */

/* CC registers codes */
#define EVDIAG_CCJOIN		140	/* CC join counter reg test failed */
#define EVDIAG_WG		141	/* CC write gatherer reg test failed */

/* FPU test error codes */
#define EVDIAG_FPU		142	/* FPU test failed */

/* Miscellaneous */
#define EVDIAG_WRCPUINFO	240	/* Writing CPU info.
					 * if you find this value in diagval
					 * too long after starting the
					 * operation, it's probably hung.
					 */
#define	EVDIAG_TESTING_CCTAGS	241	/* Testing duplicate tags */
#define EVDIAG_POD_CMDERR	242	/* Error in POD command. */
#define EVDIAG_TESTING_DCACHE	243	/* Starting dcache test. See above. */
#define EVDIAG_TESTING_ICACHE	244	/* Starting icache test.  See above */
#define EVDIAG_TESTING_SCACHE	245	/* Same for scache. */
#define EVDIAG_INITING_CACHES	246	/* Invalidate I & D caches */
#define EVDIAG_INITING_SCACHE	247	/* Invalidate I & D caches */
#define EVDIAG_TESTING_CCJOIN	248
#define EVDIAG_TESTING_CCWG	249
                   
#define EVDIAG_RETURNING	250	/* Returning to POD mode. */
#define EVDIAG_PANIC		251	/* PROM panic. */
#define EVDIAG_NMI		252	/* Got an NMI */
#define EVDIAG_DEBUG		253	/* Going to POD mode at user request. */

#endif /* __EVDIAG_H__ */
