/*
 * Constants for kernel crash dumping stuff
 *
 * $Revision: 3.5 $
 */

#ifdef	_KERNEL
#define	DUMP_OPEN	1		/* initialize device */
#define	DUMP_WRITE	2		/* write some data */
#define	DUMP_CLOSE	3		/* close the device */
#endif

/*
 * Values for led's during a crash
 */
#define	LED_PATTERN_BADVA	2	/* kernel bad virtual address */
#define	LED_PATTERN_FAULT	3	/* random kernel fault */
#define	LED_PATTERN_BOTCH	4	/* nofault botch */
#define	LED_PATTERN_PKG		5	/* bad package linkage */
