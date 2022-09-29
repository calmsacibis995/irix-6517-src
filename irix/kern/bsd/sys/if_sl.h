/* public definitions for SLIP
 */
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.10 $"


/* 'types' of SLIP, or protocol varients */
#define SC_STD		0		/* vanilla */
#define SC_COMP		1		/* SGI header compression */
#define SC_CKSUM	2		/* do not generate compressed TCP */
#define SC_CSLIP	3		/* Van Jacobson compression */

/* bits passed in c_oflag */
#define SC_NOICMP	1		/* drop ICMP packets */


typedef __uint32_t SL_FM;		/* type of filter bit mask */
#define SFM (sizeof(SL_FM)*8)		/* size of filter bit mask in bits */

/* port activity filter
 * 1=>do not worry about this packet
 */
struct afilter {
	SL_FM	port[0x10000/SFM];
	SL_FM	icmp[256/SFM];
};
#define IGNORING_PORT(f,p)  ((f) != 0 && (((f)->port[(p)/SFM]>>((p)%SFM)) & 1))
#define IGNORING_ICMP(f,t)  ((f) != 0 && (((f)->icmp[(t)/SFM]>>((t)%SFM)) & 1))
#define SET_PORT(f,p)	    ((f)->port[(p)/SFM] |= ((SL_FM)1<<((p)%SFM)))
#define SET_ICMP(f,t)	    ((f)->icmp[(t)/SFM] |= ((SL_FM)1<<((t)%SFM)))

#ifdef _KERNEL
#define SIOC_SL_AFILTER	    _IOW('P', 1, __uint32_t)
#else
#if _MIPS_SIM == _ABI64
		/* this interface is only for 32-bit mode programs */
#else
#define SIOC_SL_AFILTER	    _IOW('P', 1, struct afilter*)
#endif
#endif

#define BEEP		(HZ*15)		/* poke daemon this often */

#define BEEP_ACTIVE	1		/* looks like long activity */
#define BEEP_DEAD	2		/* the modem is dead */
#define BEEP_WEAK	3		/* the streams module says "Hi" */
#ifdef __cplusplus
}
#endif
