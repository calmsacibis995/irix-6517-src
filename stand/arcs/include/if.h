/*  Configuration data for if.c
 */

#ifndef __H_IF__
#define __H_IF__

#ident "$Revision: 1.3 $"

struct ioblock;

/* network config switch */
struct if_func {
	void (*iff_init)(void);			/* interface init routine */
	int (*iff_probe)(int);			/* interface probe routine */
	int (*iff_open)(struct ioblock*);	/* interface open routine */
	int (*iff_close)(struct ioblock*);	/* interface close routine */
	int iff_type;				/* interface "type" */
	char *iff_desc;				/* interface description */
};

#define	IF_SGI	0x80		/* type for SGI board */
#define	IF_CMC	0x81		/* type for CMC board */

extern struct if_func _if_func[];
extern int _nifcs;

#endif /* __H_IF__ */
