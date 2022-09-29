#ident	"IP32/mace/rt_clock.c:  $Revision: 1.3 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/i8254clock.h"
#include "sys/ds1286.h"
#include "uif.h"
#include "sio.h"
#include "TL16550.h"
#include "IP32_status.h"
#include <sys/ds17287.h>
#include <sys/mace.h>
#include <arcs/types.h>
#include <arcs/time.h>

#define	CLOCK_COUNTER		10
#define	POWER_FAIL_DELAY_ENABLE	(0x02)
#define POWER_FAIL_INT_ENABLE	(0x80)
#define	TIMESAVE_COUNTER	5
#define COUNTER2_VAL		1000	/* counter2 output = 1 kHz */
#define COUNTER1_VAL		2000	/* counter1 output = 2 sec */
#define CAUSE_COUNTER1		CAUSE_IP6

code_t rtc_ecode;

/*
    This test was added to detect some bad parts we encountered 
    And is not intended to be a complete register test  
*/
int
rtc_reg_chk()
{
    volatile register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    char rega, regb, regc, regd, xrega, xregb;
    char tmp;
    int errcnt = 0;


    msg_printf (VRB, "Dallas RTC register check\n");
    rtc_ecode.sw = 0x08;
    rtc_ecode.func = 0x11;
    rtc_ecode.test = 0x00;
    rtc_ecode.code = 0;
    rtc_ecode.w1 = 0;
    rtc_ecode.w2 = 0;

    rega = clock->registera;
    regb = clock->registerb;
    regc = clock->registerc;
    regd = clock->registerd;
    xrega = clock->ram[DS_BANK1_XCTRLA];
    xregb = clock->ram[DS_BANK1_XCTRLB];
    msg_printf(VRB, "Reg A = 0x%x, Reg B = 0x%x, Reg C = 0x%x, Reg D = 0x%x\n"
		    "XReg 4A = 0x%x, XReg 4B = 0x%x\n",
	    	     rega, regb, regc, regd, xrega, xregb);
/* Check to see that the DV) and DV! bits are set in reg A */
    tmp = rega & (DS_REGA_DV0 | DS_REGA_DV1);
    if ( tmp != (DS_REGA_DV0 | DS_REGA_DV1) ) {
        rtc_ecode.code = 1;
        rtc_ecode.w1 = DS_REGA_DV0 | DS_REGA_DV1;
        rtc_ecode.w2 = tmp;
        report_error(&rtc_ecode);
	msg_printf(ERR, "DV0 and DV1 not set in Reg A\n");
	errcnt++;
    }
/* Check Register D for the VRT bit.  This reg should always be 0x80*/
    tmp = regd ;
    if ( tmp != DS_REGD_VRT ) {
        rtc_ecode.code = 2;
        rtc_ecode.w1 = DS_REGD_VRT;
        rtc_ecode.w2 = tmp;
        report_error(&rtc_ecode);
	msg_printf(ERR, "The VRT bit not set in Reg D, Battery is probably dead\n");
	errcnt++;
    }
/* Check to see that the VRT2 bit is set in extended reg 4A */
    tmp = xrega & DS_XCTRLA_VRT2;
    if ( tmp != DS_XCTRLA_VRT2 ) {
        rtc_ecode.code = 3;
        rtc_ecode.w1 = DS_XCTRLA_VRT2;
        rtc_ecode.w2 = tmp;
        report_error(&rtc_ecode);
	msg_printf(ERR, "Aux Battery bit not set in XReg 4A\n");
	errcnt++;
    }
/* Check Extended Reg 4B for ABE and KSE set */
    tmp = xregb & (DS_XCTRLB_ABE | DS_XCTRLB_KSE);
    if ( tmp != (DS_XCTRLB_ABE | DS_XCTRLB_KSE) ) {
        rtc_ecode.code = 4;
        rtc_ecode.w1 = DS_XCTRLB_ABE | DS_XCTRLB_KSE;
        rtc_ecode.w2 = tmp;
        report_error(&rtc_ecode);
	msg_printf(ERR, "Aux Battery Enable and Kick Start Enable bits not set in XReg 4B\n");
	errcnt++;
    }
/* Write to Reg A to make sure the chip can be written to */
    clock->registera = 0x35;
    flushbus();
    tmp = clock->registera;
    if ( tmp != 0x35 ) {
        rtc_ecode.code = 5;
        rtc_ecode.w1 = 0x35;
        rtc_ecode.w2 = tmp;
        report_error(&rtc_ecode);
	msg_printf(ERR, "Can't write to RTC register A\n");
	errcnt++;
    }
/* Restore registera's contents */
    clock->registera = rega;
    if ( errcnt != 0 ) 
	sum_error("RTC Register Check");
    else {
	okydoky();
    }
return (errcnt);
}

int
rt_clock ()
{
    int	sanity, counter;
    static char	*counter_name[CLOCK_COUNTER] = {
	"Ten milliseconds",
	"Second",
	"Minute",
	"Hour",
	"Day of month",
	"Month",
	"Year",
	0, 		/* don't care */
	0, 		/* don't care */
	"Day of week"
    };
    volatile ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
    int errcount = 0;
    int i, offset, sec, min, hr, day;
    unsigned char a, b, c;
    unsigned long CRIMEIntStatus, MACEIntStatus, RTCIntStatus;
    unsigned long *pIntrStatusReg = (unsigned long *)INTR_STATUS_REG;
    unsigned long *pIntrMaskReg = (unsigned long *)INTR_MASK_REG;
    unsigned long long ust1, ust2;
    volatile struct counters *cntrs = (struct counters *)(COUNTERS);
    TIMEINFO *current_time;
    TIMEINFO rt_clock; 
    TIMEINFO pt;
    msg_printf (VRB, "Time of day clock test\n");
    rtc_ecode.sw = 0x08;
    rtc_ecode.func = 0x11;
    rtc_ecode.test = 0x01;
    rtc_ecode.code = 0;
    rtc_ecode.w1 = 0;
    rtc_ecode.w2 = 0;

    msg_printf (VRB,"Testing nvram\n");

    offset = 0x7e;		/* use last two bytes of nvram for testing */
    for (a=0;a<0xff;a++){
	write_nvram(offset,1,&a);
	b = ~a;
	write_nvram(offset+1,1,&b);
	c = read_nvram(offset);
	if (c != a){
		rtc_ecode.code = 1;
		rtc_ecode.w1 = a;
		rtc_ecode.w2 = c;
		report_error(&rtc_ecode);
		++errcount;
		msg_printf(ERR,"nvram error: expected %x, actual %x\n",a,c);
	}
	c = read_nvram(offset+1);
	if (c != b){
		rtc_ecode.code = 2;
		rtc_ecode.w1 = b;
		rtc_ecode.w2 = c;
		report_error(&rtc_ecode);
		++errcount;
		msg_printf(ERR,"nvram error: expected %x, actual %x\n",b,c);
	}
    }

    /* save current time
     */
	current_time = GetTime();

    /* Set alarm to activate when minutes, hour and day match.
     */
    rtc_ecode.test = 0x02;
    msg_printf(VRB,"current time: day = %d, hr =  %d, min =  %d, sec = %d\n", \
			current_time->Day, \
			current_time->Hour, \
			current_time->Minutes, \
			current_time->Seconds);
    msg_printf(VRB,"\t\t year = %d\n", \
			current_time->Year);
    sec = current_time->Seconds + 3;
    if(sec >= 60)
	sec = 2;

    min = 0xff;
    hr = 0xff; /* hr and min are don't care */


     while (clock->registera & DS_REGA_UIP)
    	if (--sanity==0){
		rtc_ecode.code = 9;	/* number out of order */
		rtc_ecode.w1 = 0;
		rtc_ecode.w2 = 0;
		report_error(&rtc_ecode);
		++errcount;
		msg_printf(ERR,"register A error: UIP bit stuck on\n");
	}

    clock->registerb |= DS_REGB_SET;	/* freeze updates */
    clock->hour_alarm = (char) hr;
    clock->min_alarm  = (char) min;
    clock->sec_alarm = (char) (((sec  / 10) << 4) | (sec  % 10));
    RTCIntStatus = clock->registerc;	/* clear out old interrupts */
    clock->registerb &= ~(DS_REGB_PIE | DS_REGB_UIE);	/* disable periodic, update */
    clock->registerb |= DS_REGB_AIE;	/* enable alarm interrupt */
    clock->registerb &= ~DS_REGB_SET;	/* unfreeze updates */
    *pIntrMaskReg = ISA_INT_RTC_IRQ;	/* enable RTC int from mace to crime */
    flushbus();


    /* wait for alarm interrupt
     */
    MACEIntStatus = 0;
    /* Increase from 0x40000, it wasn't long enough.  Also change them below */
    for (i = 0; i < 0x4000000; i++)
    {
        /* Wait for interrupt */
	/* Mask out the ethernet interrupt bit */
        if (CRIMEIntStatus = GetInterruptStatus() & ~0x08)
                {
                    MACEIntStatus = *pIntrStatusReg;
                    break;
                }
    }

    RTCIntStatus = clock->registerc;
    *pIntrMaskReg = 0;	/* disable all interrupts */
    cpu_get_tod(&rt_clock);		/* get time at alarm interrupt */
    if ( i == 0x4000000 )
        msg_printf(VRB,"Never got the interrupt in Crime loopcnt: 0x%x\n", i);
    else
        msg_printf(VRB,"Got the interrupt loopcnt: 0x%x\n", i);


    if (CRIMEIntStatus != 0x20){
	rtc_ecode.code = 1;
	rtc_ecode.w1 = 0x20;
	rtc_ecode.w2 = CRIMEIntStatus;
	report_error(&rtc_ecode);
	++errcount;
    	msg_printf(ERR,"Crime Int Status = %x\n",CRIMEIntStatus);
    }
    if (MACEIntStatus != 0x100){
	rtc_ecode.code = 2;
	rtc_ecode.w1 = 0x100;
	rtc_ecode.w2 = MACEIntStatus;
	report_error(&rtc_ecode);
	++errcount;
    	msg_printf(ERR,"Mace Int Status = %x\n",MACEIntStatus);
    }
    if (!(RTCIntStatus & 0x20)){
	rtc_ecode.code = 3;
	rtc_ecode.w1 = 0x20;
	rtc_ecode.w2 = RTCIntStatus;
	report_error(&rtc_ecode);
	++errcount;
	msg_printf(ERR,"RTC Int Status = %x\n",RTCIntStatus);
    }



    /* verify clock counters, all counters must match exactly
     */
    for (counter = 0; counter < CLOCK_COUNTER; counter++)
	if (counter_name[counter])
	        switch (counter) {
        	case 0:
                	break;
        	case 1:
	    		if ((rt_clock.Seconds & 0xff) != 
	        		sec) {
				rtc_ecode.code = 4;
				rtc_ecode.w1 = sec;
				rtc_ecode.w2 = rt_clock.Seconds;
				report_error(&rtc_ecode);
				++errcount;
				msg_printf (ERR,\
		    		"%s clock counter, Expected: 0x%02x, \
				Actual: 0x%02x\n",counter_name[counter], \
				sec,\
				rt_clock.Seconds & 0xff);
	    		}   /* if */
                	break;
        	case 2:
                	break;
        	case 3:
                	break;
        	case 4:
                	break;
        	case 5:
                	break;
        	case 6:
			if ((rt_clock.Year & 0xfff) !=
	        		current_time->Year) {
				rtc_ecode.code = 5;
				rtc_ecode.w1 = current_time->Year;
				rtc_ecode.w2 = rt_clock.Year;
				report_error(&rtc_ecode);
				++errcount;
				msg_printf (ERR,\
		    		"%s clock counter, Expected: 0x%02x, \
				Actual: 0x%02x\n",counter_name[counter], \
				current_time->Year,\
				rt_clock.Year & 0xff);
	    		}   /* if */
                	break;
        	case 9:
                	break;
        	}

    msg_printf (VRB, "Periodic Interrupt test\n");

    rtc_ecode.test = 3;

    clock->registerb |= DS_REGB_SET;	/* freeze updates */
    clock->registerb &= ~DS_REGB_AIE;	/* disable alarm interrupt */
    clock->ram[DS_BANK1_XCTRLB] &= ~DS_XCTRLB_E32K;
    RTCIntStatus = clock->registerc;	/* clear out old interrupts */
    clock->registera |= ( DS_REGA_RS3 | DS_REGA_RS2 | DS_REGA_RS1 | DS_REGA_RS0);
    /* set period to 500ms */
    clock->registerb |= DS_REGB_PIE;	/* enable periodic interrupt */
    clock->registerb &= ~DS_REGB_SET;	/* unfreeze updates */
    *pIntrMaskReg = ISA_INT_RTC_IRQ;	/* enable RTC int from mace to crime */
    flushbus();

    /* wait for periodic interrupt 
     */
    MACEIntStatus = 0;
    for (i = 0; i < 0x400000; i++)
    {
        /* Wait for interrupt */
	/* Mask out the ethernet interrupt bit */
        if (CRIMEIntStatus = GetInterruptStatus() & ~0x08)
                {
                    MACEIntStatus = *pIntrStatusReg;
                    break;
                }
    }
    ust1 = (READ_REG64(&cntrs->ust, long long) & 0xffffffffLL);
    RTCIntStatus = clock->registerc;


    /* wait for periodic interrupt
     */
    MACEIntStatus = 0;
    for (i = 0; i < 0x400000; i++)
    {
        /* Wait for interrupt */
	/* Mask out the ethernet interrupt bit */
        if (CRIMEIntStatus = GetInterruptStatus() & ~0x08)
                {
                    MACEIntStatus = *pIntrStatusReg;
                    break;
                }
    }
    ust2 = (READ_REG64(&cntrs->ust, long long) & 0xffffffffLL);
    RTCIntStatus = clock->registerc;


    if (CRIMEIntStatus != 0x20){
	rtc_ecode.code = 1;
	rtc_ecode.w1 = 0x20;
	rtc_ecode.w2 = CRIMEIntStatus;
	report_error(&rtc_ecode);
	++errcount;
    	msg_printf(ERR,"Crime Int Status = %x\n",CRIMEIntStatus);
    }
    if (MACEIntStatus != 0x100){
	rtc_ecode.code = 2;
	rtc_ecode.w1 = 0x100;
	rtc_ecode.w2 = MACEIntStatus;
	report_error(&rtc_ecode);
	++errcount;
    	msg_printf(ERR,"Mace Int Status = %x\n",MACEIntStatus);
    }
    if (!(RTCIntStatus & 0x40)){
	rtc_ecode.code = 3;
	rtc_ecode.w1 = 0x40;
	rtc_ecode.w2 = RTCIntStatus;
	report_error(&rtc_ecode);
	++errcount;
	msg_printf(ERR,"RTC Int Status = %x\n",RTCIntStatus);
    }

    msg_printf(VRB,"ust1 = %llx\n",ust1);
    msg_printf(VRB,"ust2 = %llx\n",ust2);

    if(ust2 > ust1){
	ust2 -= ust1;
    	if(ust2>521000 || ust2<520700){
		rtc_ecode.code = 4;
		rtc_ecode.w1 = (int)(ust2);
		rtc_ecode.w2 = 0;
		report_error(&rtc_ecode);
		++errcount;
		msg_printf(ERR,"RTC/UST frequncey check failed - diff = %llx\n",ust2);
	}
    }

    *pIntrMaskReg = 0;	/* disable all interrupts */
    clock->registera &= ~(DS_REGA_RS3 | DS_REGA_RS2 |  DS_REGA_RS1 | DS_REGA_RS0);
    clock->registerb &= ~DS_REGB_PIE;	/* disable periodic interrupt */
    
    if (errcount != 0)
	sum_error ("time of day clock");
    else
	okydoky ();

    return (errcount);
}   /* rt_clock */

