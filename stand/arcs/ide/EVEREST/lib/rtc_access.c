/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	rtc_access.c - read/write routines for the RTC/NVRAM		*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evconfig.h>
#include <rtc_access.h>

#ident "arcs/ide/EVEREST/pbus/rtc_access.c $Revision: 1.4 $"

/*
 * Get NVRAM values.  These routines are endian-independent. 
 */
#define NVRAM_ADDR(_region, _adap, _x) \
	(SWIN_BASE(_region, _adap) + EPC_NVRTC + (_x) + BYTE_SELECT)

#define NVRAM_GET(_region, _adap, _reg) \
	( *((unsigned char *) NVRAM_ADDR(_region,_adap, _reg)))
#define NVRAM_SET(_region, _adap, _reg, _value) \
	NVRAM_GET(_region, _adap, _reg) = (_value);

#define	EPC_PRST_RTC	0x8		/* PRST bit for Pbus device 3 (RTC) */
#define DEL_100_MS	100000		/* 100 Ms delay for us_delay */

/*
 * rtc_reset()
 *	Resets the RTC chip
 */
void rtc_reset(int region, int adap)
{
    /*
     * resets the RTC chip, holds it reset for 100 ms, then relases it
     * and waits 500 ms for chip to recover before returning
     */
    EPC_SET(region, adap, EPC_PRSTSET, EPC_PRST_RTC);
    us_delay(DEL_100_MS);
    EPC_SET(region, adap, EPC_PRSTCLR, EPC_PRST_RTC);
    us_delay(5*DEL_100_MS);
}
    

/*
 * ide_get_nvreg()
 *	Read a byte from the NVRAM.
 */
uint
ide_get_nvreg(int region, int adap, uint offset)
{
    uint value;

    /* First set up the XRAM Page register */
    NVRAM_SET(region, adap, NVR_XRAMPAGE, XRAM_PAGE(offset));

    value = (uint) NVRAM_GET(region, adap, XRAM_REG(offset));	
    return value;
}


/*
 * ide_set_nvreg()
 *	Writes a byte into the NVRAM
 */
int
ide_set_nvreg(int region, int adap, uint offset, char byte)
{
    uint value; 

    /* First set up the XRAM Page register */
    NVRAM_SET(region, adap, NVR_XRAMPAGE, XRAM_PAGE(offset));
        
    NVRAM_SET(region, adap, XRAM_REG(offset), byte);  

    value = NVRAM_GET(region, adap, XRAM_REG(offset));

    if (value == byte)
	return 0;
    else
	return -1;	
}

/*
 * get_rtcreg()
 *	Read a byte from a RTC register
 */
uint
get_rtcreg(int region, int adap, uint offset)
{
    uint value;

    /* First set up the RTC address register */
    NVRAM_SET(region, adap, (NVR_RTC + NVR_RTC_ADDR), offset);

    /*
     * just an experiment to see if we have timing problems
     */
    us_delay(50);

    /* Now read the correct value from the RTC data register */
    value = (uint) NVRAM_GET(region, adap, (NVR_RTC + NVR_RTC_DATA));	
    return value;
}


/*
 * set_rtcreg()
 *	Writes a byte into a RTC register
 */
int
set_rtcreg(int region, int adap, uint offset, char byte)
{
    uint value; 

    /* First set up the RTC address register */
    NVRAM_SET(region, adap, (NVR_RTC + NVR_RTC_ADDR), offset);

    /*
     * just an experiment to see if we have timing problems
     */
    us_delay(50);

    /* write the value to the RTC data register */
    NVRAM_SET(region, adap, (NVR_RTC + NVR_RTC_DATA), byte);

    /*
     * just an experiment to see if we have timing problems
     */
    us_delay(50);

    /* check to make sure we can read the value we set back */
    value = (uint) NVRAM_GET(region, adap, (NVR_RTC + NVR_RTC_DATA));

    if (value == byte)
	return 0;
    else
	return -1;	
}

/*
 * get_rtcvals()
 *	Saves current RTC values into an array of chars of size RTCSIZE
 *	since it saves the registers in order, the calling routine can
 *	modify the saved contents at will.
 */
void
get_rtcvals(int region, int adap, char *savearr)
{
    int regnum;
    char savloc;

    /*
     * disable updates while reading
     */
    savloc = get_rtcreg(region, adap, NVR_RTCB);
    set_rtcreg(region, adap, NVR_RTCB,  savloc|NVR_SET);

    for(regnum = 0; regnum < RTCSIZE; regnum++)
	*savearr++ = get_rtcreg(region, adap, regnum);

    /*
     * restore previous operating values
     */
    set_rtcreg(region, adap, NVR_RTCB, savloc);
}

/*
 * set_rtcvals()
 *	Sets RTC to parameters passed via an array of chars of size RTCSIZE
 *	This routine presumes that the contents of the array are in the same
 *	order as the registers in the RTC chip, and does *no* bounds or error
 *	checking - ie, garbage in, garbage out
 */
void
set_rtcvals(int region, int adap, char *savearr)
{
    int regnum;
    char *savep;

    /*
     * hold off updates, but set up rest of format to match the value to be
     * written
     */
    set_rtcreg(region, adap, NVR_RTCB, NVR_SET|savearr[NVR_RTCB]);
    set_rtcreg(region, adap, NVR_RTCA, savearr[NVR_RTCA]);

    /*
     * write the time info into the timer registers
     */
    for(regnum = 0, savep = savearr; regnum <= NVR_YEAR; regnum++, savep++)
	set_rtcreg(region, adap, regnum, *savep);
    
    /*
     * restore reg B (especially the SET bit) to the currect value. If the
     * set bit is clear, this should restart the clock
     */
    set_rtcreg(region, adap, NVR_RTCB, savearr[NVR_RTCB]);
}

/*
 * used by add_time
 */
static int month_days[12] = {
        31,     /* January */
        28,     /* February */
        31,     /* March */
        30,     /* April */
        31,     /* May */
        30,     /* June */
        31,     /* July */
        31,     /* August */
        30,     /* September */
        31,     /* October */
        30,     /* November */
        31      /* December */
};

/*
 * add_time(timearr, secs) - adds "secs" seconds to the values in "timearr"
 *
 * This routine assumes that the time is stored in an array of chars in
 * the same order as the registers in the RTC. It also assumes that the
 * times given are in binary, 24 hour, format. 
 *
 * Vastly simplified - it ignores leap years, daylight savings, etc. It
 * also skips bounds checking for ridiculously large values of "secs".
 */
void
add_time(char *timearr, int secs)
{
    int sec, min, hour, day, dayw, month, year;

    sec = timearr[NVR_SEC];
    min = timearr[NVR_MIN];
    hour = timearr[NVR_HOUR];
    day = timearr[NVR_DAY];
    dayw = timearr[NVR_WEEKDAY];
    month = timearr[NVR_MONTH];
    year = timearr[NVR_YEAR];

    sec += secs;
    while (sec > 59) {
	min++;
	sec -= 60;
    }

    while (min > 59) {
	hour++;
	min -= 60;
    }

    while (hour > 23) {
	day++;
	dayw++;
	hour -= 24;
    }

    while (dayw > 7) {
	dayw -= 7;
    }

    while (day > month_days[year]) {
	day -= month_days[year];
	month++;
	if (month > 12) {
	    year++;
	    month -= 12;
	}
    }
}

static char *month[] = {
    "",
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

void
date_stamp(void)
{
    int sec, min, hour, day, mon, year;

    sec = get_rtcreg(EPC_REGION, EPC_ADAPTER, NVR_SEC);
    min = get_rtcreg(EPC_REGION, EPC_ADAPTER, NVR_MIN);
    hour = get_rtcreg(EPC_REGION, EPC_ADAPTER, NVR_HOUR);
    day = get_rtcreg(EPC_REGION, EPC_ADAPTER, NVR_DAY);
    mon= get_rtcreg(EPC_REGION, EPC_ADAPTER, NVR_MONTH);
    year = get_rtcreg(EPC_REGION, EPC_ADAPTER, NVR_YEAR);

    /* date from the epoch */
    year += 1970;

    printf("%s %u %u, %02u:%02u:%02u GMT\n",
	month[mon], day, year, hour, min, sec);
}
