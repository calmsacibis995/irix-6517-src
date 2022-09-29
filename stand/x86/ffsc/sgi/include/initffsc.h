/*
 * init.h
 *	Initialization function prototypes and declarations for the FFSC
 */

#ifndef _INITFFSC_H_
#define _INITFFSC_H_


/* Initialization constants */
#define TTY_RX_BUF_SIZE	4096
#define TTY_TX_BUF_SIZE 4096


/* Initialization functions */
STATUS	ffscDebugInit(void);
STATUS	ffscEarlyInit(void);
STATUS	ffscInit(void);


/* Task functions */
int	waitReq(void);
int	swdelay(void);
void    guimain( int argc, char *argv[] );      


#endif  /* _INITFFSC_H_ */
