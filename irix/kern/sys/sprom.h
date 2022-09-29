#ident "$Revision: 3.5 $"

/* Header file for PROM driver special ioctls */

/* Special IOCTL for prom stream module */
#define PRIOC	('P'<<8)
#define PRSWTCH	(PRIOC|1)	/* Special ioctl for prom driver. Exact
				   meaning is determined by argument */ 

/* PRSWTCH IOCTL arguments */
#define PROMIO		0	/* do IO to proms. */
#define PASSDATA	1	/* do IO to to stream head. */
#define INPUTOFF	2	/* turn on PROM input timer. */
#define INPUTON		3	/* turn off PROM input timer. */
#define LIMBO		4	/* Limbo state. We can't do any output here. */
#define INPROMIO	5	/*  are we in promio mode? */
