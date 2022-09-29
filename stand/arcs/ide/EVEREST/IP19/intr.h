
/*
 * .h stuff local to the interrupt path fine family of tests
 *
 * $Revision: 1.2 $
 */

#define	NCPUS	4	/* XXX don't be fooled it's not that modular :-)  */
#define	NPROC	8
#define	APAGE	4096
#define	AMEG	0x100000

#ifdef	not
/*
 * cuz bvg does not pull Addr Here on the ebus thus causing a level 3 err
 * so instead we store to a carefully selected k1seg address so it will
 * appear in proc0_waves
 */
#define	print(s,p,a,val)	bvg_print_hex((s), (p), (val))
#else	/* way */
#define	print(s,p,a,val)	dosd((val),(a))
#endif	/* way */

/*
 * Some Everest register code implemented as macros
 */


#define senddint(s, p, l)	EV_SET_LOCAL(EV_SENDINT, ((l) << 8) | ((s) << 2) | (p));

/* send group interrupt */
#define	sendgrint(l, g)		EV_SET_LOCAL(EV_SENDINT, ((l) << 8) | 0x40 | (g))

/* send broadcast interrrupt */
#define	sendbint(l)		EV_SET_LOCAL(EV_SENDINT, ((l) << 8) | 0x7F)

/* slot, proc, and reg all better be no wider than they should */
#define	get_cc_conf(r) \
		dold(dold(CC_SPNUM) << 9 | (r) << 3 | CC_CONFIG_REGS)
#define	set_cc_conf(r, v) \
		dosd((v), dold(CC_SPNUM) << 9 | (r) << 3 | CC_CONFIG_REGS)
#define get_cc_config(s, p, r)	 \
		dold((s)<<11 | (p)<<9 | (r)<<3 | CC_CONFIG_REGS)
#define set_cc_config(s, p, r, v) \
		dosd((v), (s)<<11 | (p)<<9 | (r)<<3 | CC_CONFIG_REGS)

/* big endian is 1, little is 0 */
#define	get_endian()		((get_config() & CONFIG_BE) >> CONFIG_BE_SHFT)

/* level to (double-word) address of CC_IP? */
#define	ltoip(l)	(CC_IP0 + 8 * ((l) >> 6))
/* get word containing bit of this level */
#define	get_cc_ip(l) ((l) & 0x20 ? doldhi(ltoip(l)) : dold(ltoip(l)))

#define bvg_print_hex(s, p, n) \
	(*(volatile uint*)(PRINT_HEX_BASE + ((s) << 5 | (p) << 3)) = (n))

#define	bvg_prhex_sp(sp, n) \
	(*(volatile uint*)(PRINT_HEX_BASE + ((sp) << 3)) = (n))

#define	bvg_prhex(n) \
	(*(volatile uint*)(PRINT_HEX_BASE + (dold(CC_SPNUM) << 3)) = (n))

#define	splockbus(l)	splock(l)
#define	spunlockbus(l)	(get_cc_conf(CC_CONF_DIT0), spunlock(l))

