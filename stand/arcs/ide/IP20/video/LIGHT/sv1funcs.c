#include <sys/types.h>
#include <sys/sbd.h>
#include "sv.h"
#include "ide_msg.h"
#include <libsc.h>

/*
 * The following definition is some kind of far-out approximation of the
 * delay factor needed by the delayWritedata function when used on
 * a 33MHz MIPS system.
 */
#define LOOPS_PER_MICROSEC	3 

/* declare global pointer to SV1 Board */
volatile long *theSVboard = (volatile long *)(SVB_ADDR);

static void
wait4key(void)
{
    char inbuf[80];

    printf("\tPress <ENTER> to continue ");
    gets(inbuf);
}

/*
 * Poll I2C for status ready or Time Out
 */
int
pollI2C(void)
{
    int theDelay;
    int theTimeout = SV_I2C_TIMEOUT;
    static int attempt = 0;

#ifdef I2C_DEBUG
    status_poll[poll_num++] = theSVboard[SV_I2C_CS_CTRL];
#endif
    /* loop for good I2C stat or time */    
    while(theSVboard[SV_I2C_CS_CTRL] & SV_I2C_SERIAL_BUSY) {
	for(theDelay = SV_I2C_TIMEOUT_DELAY ;
				theDelay != 0 ; theDelay--);
	if (--theTimeout == 0)	/* whack the timeout */ 
	    break;
    }
#ifdef I2C_DEBUG
    status_poll[poll_num++] = theSVboard[SV_I2C_CS_CTRL];
#endif

    if (theTimeout == 0) {	/* no responce on I2C bus */
#ifdef I2C_DEBUG
	printf("\nNo response on I2C bus: %d\n", attempt);
#endif
	return ERR_I2C_TIMEOUT;
    }
    attempt++;

    return NO_ERROR;
}

/*
 *  dummy cycle to delay GIO writes
 */
int
delayWritedata(long delay)
{
    long loopcount = 0xFFFF;
    volatile long foo = 1;

    delay *= LOOPS_PER_MICROSEC;
    for(loopcount = 0 ; loopcount != delay ; loopcount++) {
	foo += loopcount;
	foo += loopcount;
	foo += loopcount;
    }

    return NO_ERROR;
}


/*
 *  Write Long Data Item to I2C with status poll
 */
int
writeI2Cdata(long theData)
{
    int tmp;

    tmp = pollI2C();		/* wait 'till last data gone */
    if (tmp)
	return tmp;

    theSVboard[SV_I2C_CS_DATA] = theData;
    delayWritedata(GIO_BUS_DELAY);  /* dummy for GIO bus */
    if (*Reportlevel == 5) {
	printf("I2C Data (w/poll) = 0x%04x    ", theData);
	wait4key();
    }

    return NO_ERROR;
}


/*
 *  Write Long Data Item to I2C with NO status poll
 */
int
writeI2CdataNP(long theData)
{
    theSVboard[SV_I2C_CS_DATA] = theData;
    delayWritedata(GIO_BUS_DELAY);  /* dummy for GIO bus */
    if (*Reportlevel == 5) {
	printf("I2C Data =          0x%04x    ", theData);
	wait4key();
    }

    return NO_ERROR;
}


/*
 *  Write Control Register Data Item to I2C
 */
int
writeI2Cctrl(long theData)
{
    theSVboard[SV_I2C_CS_CTRL] = theData;
    delayWritedata(GIO_BUS_DELAY);  /* dummy for GIO bus */
    if (*Reportlevel == 5) {
	printf("I2C Ctrl write =    0x%04x    ", theData);
	wait4key();
    }

    return NO_ERROR;
}


/*
 *  Write Long Data Item to indexed location with delay
 */
int
writeParallelData(long theIndex, long theData)
{
    theSVboard[theIndex] = theData;
    delayWritedata(GIO_BUS_DELAY);  /* dummy for GIO bus */
    if (*Reportlevel == 5) {
	switch (theIndex) {
	    case SV_FUNC_CTL:
		printf("FUNC_CTL =  ");
		break;
	    case SV_CLOCK_MODE:
		printf("CLOCK_MODE =");
		break;
	    case SV_BUS_CTL:
		printf("BUS_CTL =   ");
		break;
	    case SV_DMACTL:
		printf("DMACTL =    ");
		break;
	    default:
		printf("(0x%04x) =  ", theIndex);
		break;
	}
	printf(" 0x%02x    ", theData);
	wait4key();
    }

    return NO_ERROR;
}
