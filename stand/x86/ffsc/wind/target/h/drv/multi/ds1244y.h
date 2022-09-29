/* ds1244y.h - Dallas SRAM/Clock chip structures and addresses */

/*
modification history
--------------------
01b,23jul93,caf  added support for c++.
01a,07aug92,sas  written.
*/

/*
 * Structures for control of the Dallas Semiconductor DS1244Y Static-Ram-and-
 * Real-Time-Clock chip.
 *
 */

#ifndef	__INCds1244yh
#define	__INCds1244yh

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#define THEDAYS { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" }

#define THEMONTHS \
	 { "Jan", "Feb", "Mar", "Apr", "May", "Jun", \
       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }

/*
 * Here is the magic key to access the clock (rather than the RAM) on the
 * chip. These bits must be shifted into the clock until all 64 bits have
 * been read in. This unlocks the clock for one additional 64-bit access,
 * which may be a read or a write.
 */

#define CLOCK_KEY \
	 {0xc5, 0x3a, 0xa3, 0x5c, 0xc5, 0x3a, 0xa3, 0x5c}

/* tm structure for dealing with time of day ala UNIX */
/* all fields are in binary */
typedef struct {
    unsigned char tm_hsec;  /* hundredths of seconds (0-99) */
    unsigned char tm_sec;   /* seconds (0-59) */
    unsigned char tm_min;   /* minutes (0-59) */
    unsigned char tm_hour;  /* hours (0-23) */
    unsigned char tm_mday;  /* day of month (1-31) */
    unsigned char tm_mon;   /* month of year (0-11) */
    unsigned char tm_year;  /* year - 1900 */
    unsigned char tm_wday;  /* day of week (Sunday=0) */
    unsigned char tm_jmsb;  /* MSB of julian date (100's day of year)*/
    unsigned char tm_jlsb;  /* LSB of julian date */
/*  unsigned char tm_isdst; nonzero if Daylight Savings Time in effect */
} TIME_DATA ;

/*
 * These defines are for various conversions to and from the binary
 * structure above and the values that actually get loaded into the
 * clock.
 */

#define MIN_YEAR    1990
#define MAX_YEAR    2089
#define BIAS_YEAR  -1990
 
#define MIN_MONTH    1
#define MAX_MONTH   12
#define BIAS_MONTH  -1
 
#define MIN_DAY      1
#define MAX_DAY      7
#define BIAS_DAY    -1
 
#define MIN_MDAY     1
#define MAX_MDAY    31
#define BIAS_MDAY    0
 
#define MIN_HOUR     0
#define MAX_HOUR    23
#define BIAS_HOUR    0
 
#define MIN_MINUTE   0
#define MAX_MINUTE  59
#define BIAS_MINUTE  0
 
#define MIN_SECOND   0
#define MAX_SECOND  59
#define BIAS_SECOND  0
 
#define MIN_HSEC     0
#define MAX_HSEC    99
#define BIAS_HSEC    0

typedef struct {
    unsigned char data[8];
} CLOCK_DATA;

/*
 * the following macros convert from BCD to binary and back.
 * Be careful that the arguments are chars, and watch for overflows.
 */

#define BCD_TO_BIN(bcd) ( ((((bcd)&0xf0)>>4)*10) + ((bcd)&0xf) )
#define BIN_TO_BCD(bin) ( (((bin)/10)<<4) + ((bin)%10) )

#define FRAC_SECS	0
#define SECS		1
#define MINS		2
#define HOURS		3
#define DAYS		4
#define DATE		5
#define MONTH		6
#define YEAR		7

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* __INCds1244yh */
