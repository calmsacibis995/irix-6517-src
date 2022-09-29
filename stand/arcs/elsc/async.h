#pragma option -l;  	// Disable header inclusion in listing
//#ifndef	_ASYNC_H_
//#define	_ASYNC_H_

/*
 		Silicon Graphics Computer Systems
        Header file for SN0 Entry Level System Controller

                        async.h

        (c) Copyright 1995   Silicon Graphics Computer Systems

		Brad Morrow
*/

/* Control Characters */
#define CNTL_T				0x14		/* Control T ascii code */
#define CNTL_U				0x15		/* Control U ascii code */

/*
 *	Async port related defines and structures
 */


/* circular buffer header */
#define ASYNCBUFSZ	22		/* size of SCI buffers */
#define HIWATER		19		/* hi water value */
#define LOWWATER		5		/* low water value */

#ifndef INDIRECT_Q
#define	SCI_IN		1
#define	SCI_OUT		2
#define	I2C_IN		3
#define	I2C_OUT		4
#endif

struct cbufh {
	char	*c_inp;		/* input pointer	*/
	char 	*c_outp;		/* output pointer	*/
	char	c_len;		/* # chars in buffer	*/
	char 	*c_start;	/* physical buf start	*/
	char 	*c_end;		/* physical buf end	*/
	char	c_maxlen;	/* max # of chars	*/
	};


//#endif	/*_ASYNC_H_*/
#pragma option +l;   
