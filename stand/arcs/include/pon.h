#ident	"include/pon.h:  $Revision: 1.20 $"

/*
 * pon.h - power-on diagnostics
 */

/* Bitmasks indicating which diagnostics have failed
 */
#define FAIL_NOGRAPHICS		0x00000001
#define FAIL_BADGRAPHICS	0x00000002
#define FAIL_INT2		0x00000004
#define FAIL_MEMORY		0x00000008
#define FAIL_CACHES		0x00000010
#define FAIL_SCSIFUSE		0x00000020
#define FAIL_SCSICTRLR		0x00000100
#define FAIL_SCSIDEV		0x00000200
#define FAIL_DUART		0x00000400
#define FAIL_PCCTRL		0x00000800
#define FAIL_PCKBD		0x00001000
#define FAIL_PCMS		0x00002000
#define FAIL_ISDN		0x00004000
#define FAIL_CPU		0x00008000
#define FAIL_NIC_CPUBOARD	0x00010000
#define FAIL_NIC_BASEBOARD	0x00020000
#define FAIL_NIC_FRONTPLANE	0x00040000
#define FAIL_NIC_PWR_SUPPLY	0x00080000
#define	FAIL_NIC_EADDR		0x00100000
#define FAIL_WIDGET		0x00200000
#define	FAIL_ENET		0x00400000
#define	FAIL_AUDIO		0x00800000
#define FAIL_ILLEGALGFXCFG	0x01000000


/* Masks to indicate which bytes/words are bad within a bank, as determined
 * by memory configuration and diags
 */
#if IP20 || IP22 || IP26 || IP28
#define	BAD_SIMM0	0x00008000
#define	BAD_SIMM1	0x00004000
#define	BAD_SIMM2	0x00002000
#define	BAD_SIMM3	0x00001000
#define	BAD_ALLSIMMS	0x0000f000
#endif
#if IP30
#define	BAD_DIMM0	0x00000002
#define	BAD_DIMM1	0x00000001
#define	BAD_ALLDIMMS	(BAD_DIMM0|BAD_DIMM1)
#endif


#if defined(_LANGUAGE_C)
extern unsigned int simmerrs;
extern void run_uncached(void);
extern void run_cached(void);
extern void setstackseg(__psunsigned_t);
extern int pon_pcctrl(void);
extern int pon_pckbd(void);
extern int pon_pcms(void);
extern int pon_graphics(void);
extern int pon_ioc3_pckm(void);
extern int pon_nic(void);
extern int pon_scsi(unsigned);
extern int pon_enet(void);
#endif /* _LANGUAGE_C */
