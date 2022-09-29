/*
 * IDE universal status messages and notifier functions.
 */

#ident "$Revision: 1.2 $"

#include <saioctl.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include <IP32_status.h>
#include <sys/cpu.h>
#include <sys/ds17287.h>

#define USAGE		"Usage: led n (0=off, 1=green, 2=red, 3=amber)\n"
#define MAXVALUE	3

#define SETLED(X)	ip32_set_cpuled(X)

unsigned char read_nvram(int);
void write_nvram(int, int, unsigned char *);


/*
 * Command to turn LED various colors (on/off on IP20).
 */
int
setled(int argc,char *argv[])
{
	extern int failflag;
	int value = -1;
	char *usage = USAGE;

	if ( argc != 2 ) {
		printf(usage);
		return (1);
	}

	atob(argv[1],&value);
	if (value < 0 || value > MAXVALUE) {
		printf(usage);
		return (1);
	}

	SETLED(value);

	return (0);
}

/*
 * Print out error summary of diagnostics test
 */
void
sum_error (char *test_name)
{
#ifdef PROM
	msg_printf (SUM, "\r\t\t\t\t\t\t\t\t- ERROR -\n");
#endif /* PROM */
	msg_printf (SUM, "\rHardware failure detected in %s test\n", test_name);
}   /* sum_error */

/*
 *  Print out commonly used successful completion message
 */
void
okydoky(void)
{
    msg_printf(VRB, "Test completed with no errors.\n");
}

void
gfx_exit(void)
{
    if (console_is_gfx())
    {
	/*
	 *  Now we need to reintialize the graphics console, but first
	 *  we need do some voodoo to convince the ioctl that graphics
	 *  really need initializing.
	 */
	ioctl(1, TIOCREOPEN, 0);
    }
    buffoff();
}


#ifndef PROM
/*
 *  emfail - "electronics module failure"
 *
 *  Report diagnostic-detected failure in terms of the appropriate field
 *  replaceable unit. Exit from IDE restoring the state of the console
 *  if necessary.
 *      ------------- Change this to dump out NVRAM error codes ----------
 */
int
emfail(int argc, char *argv[])
{
			/* XXX: review for IP30 and IP32 */
    int _scsi_diag_fail=0 ; /* XXX  was  extern int _scsi_diag_fail; 
					from scsi_diag.c, I believe */
    int unit_num, scshift, type = 0;
    char *unit_name;
    char line[80];

    if (argc != 2)
    {
	msg_printf(SUM, "usage: emfail unit_number\n");
	return (1);
    }
    atob(argv[1], &unit_num);

    switch(unit_num)
    {
	case 1:				/* CPU */
	case 4:				/* tlb */
	case 5:				/* FPU */
	    type = 2;
	    break;
	case 2:
	    type = 1;
	    gfx_exit();
	    break;
	case 6:	/* SCSI */
	    if(_scsi_diag_fail == 0x10000) /*  No devices found when not diskless;
		could be a cabling, termination, or power problem... */
		unit_name = "termination or cable";
	    else { /* note that low bit won't ever be set, because
		* we use ID 0 for host adapter.  This might change someday. */
		line[0] = '\0';
		unit_name = line;
		if(_scsi_diag_fail & 0xff) {	/* controller 0 */
			strcat(line, "CTLR#0: device ");
			unit_num = _scsi_diag_fail ;
			for(scshift=0;unit_num; scshift++, unit_num >>= 1) {
				char devn[3];
				if(unit_num&1) {
					sprintf(devn, "%d ", scshift);
					strcat(line, devn);
				}
			}
		}
		if(_scsi_diag_fail & 0xff00) {	/* controller 1 */
			if(_scsi_diag_fail & 0xff)
				strcat(line, "and ");
			strcat(line, "CTLR#1: device ");
			unit_num = _scsi_diag_fail >> 8;
			for(scshift=0;unit_num; scshift++, unit_num >>= 1) {
				char devn[3];
				if(unit_num&1) {
					sprintf(devn, "%d ", scshift);
					strcat(line, devn);
				}
			}
		}
	    }
	    printf("ERROR:  Failure detected in SCSI (%s).\n",
		    unit_name);
	    return (1);
#if IP22 || IP26
	case 14:			/* audio on daughter card */
	    type = 3;
	    break;
#else
	case 14:			/* audio on mother board */
#endif
	case 3:				/* int2 */
	case 7:				/* timer */
	case 8:				/* duart */
	case 9:				/* eeprom */
	case 10:			/* hpc */
	case 11:			/* parity */
	case 12:			/* secondary cache */
	case 13:			/* memory */
	case 15:			/* scsi ctrl */
	default:
	    break;
    }
    printf("ERROR:  Failure detected on the ");
			/* XXX: review for IP30 and IP32 */
    switch (type) {
    case 1:
	printf("graphics board.\n");
	break;
    case 2:
	printf("CPU module.\n");
	break;
#if IP22 || IP26
    case 3:
	printf("Audio board.\n");
	break;
#endif
    default:
	printf("CPU base board.\n");
	break;
    }
    return (1);
}
#endif /* ! PROM */

#if 1
void
set_test_code(code_t *test)
{

	write_nvram(NVRAM_TEST_CODE, sizeof(code_t),(unsigned char *) test);
}

void
report_error(code_t* error)
{

	code_t tmp;
	unsigned char nv_byte, ts[4];
	int offset, i;
	ds17287_clk_t *clock = RTDS_CLOCK_ADDR;

	/* store error code in rtc nvram */
	offset = NVRAM_ERROR_STACK;
        clock->registera &= ~DS_REGA_DV0; /* make sure bank 0 selected */

	nv_byte = read_nvram(offset);
	i = 0;
	while (nv_byte != 0 && i<3){	/* find empty entry field */
		offset += NVRAM_ERROR_SIZE;
		i++;
		nv_byte = read_nvram(offset);
	}
	write_nvram(offset, sizeof(code_t),(unsigned char *)error);
	clock->registerb |= DS_REGB_SET;        /* freeze updates */

	ts[0] = (char)clock->min;
        ts[1] = (char)((clock->hour) & 0x3f);
        ts[2] = (char)((clock->date) & 0x3f);
        ts[3] = (char)((clock->month) & 0x1f);
	clock->registerb &= ~DS_REGB_SET;
	offset += sizeof(code_t);
	write_nvram(offset, 4, &ts);

	msg_printf(SUM,"Error code = IP32.%02X.%02X.%02X.%02X, w1 = %08x, w2 = %08x\n", \
			error->sw,error->func,error->test,error->code,error->w1,error->w2);

}

unsigned char
read_nvram(int offset)
{

	unsigned char c;
	int timeout = 100000;
	unsigned char *rtc_nvram = PHYS_TO_K1(ISA_RTC_BASE);
	ds17287_clk_t *clock = RTDS_CLOCK_ADDR;

	while (clock->registera & DS_REGA_UIP)
		if (--timeout==0)
			return;

        clock->registera &= ~DS_REGA_DV0; /* make sure bank 0 selected */
	if(offset < NVRAM_TEST_CODE || offset > NVRAM_END){
		msg_printf(ERR,"read_nvram: bad offset %x\n",offset);
		return(0);
	}
	msg_printf(DBG,"read_nvram: offset = %x\n",offset);
	c = rtc_nvram[(offset*256)+7];
	return(c);
}

void
write_nvram(int offset, int length, unsigned char * ptr)
{

	unsigned char c;
	int timeout = 100000;
	unsigned char *rtc_nvram = PHYS_TO_K1(ISA_RTC_BASE);
	ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
	while (clock->registera & DS_REGA_UIP)
		if (--timeout==0)
			return;


        clock->registera &= ~DS_REGA_DV0; /* make sure bank 0 selected */

	msg_printf(DBG,"write_nvram: offset = %x, length = %x, data = %x\n", \
			offset, length, *ptr);

	if(offset < NVRAM_TEST_CODE || offset+length-1 > NVRAM_END){
		msg_printf(ERR,"write_nvram: bad offset %x\n",offset);
		return;
	}

	for(;length>0;length--){
		rtc_nvram[((offset+length-1)*256)+7] = ptr[length-1];
	}
}


void
dump_nvram(void)
{

	code_t stored_code;
	unsigned char time_stamp[4];
	int i, offset;

	offset = NVRAM_TEST_CODE;

	for(i=0;i<5;i++){

		stored_code.sw = read_nvram(offset++);
		stored_code.func = read_nvram(offset++); 
		stored_code.test = read_nvram(offset++); 
		stored_code.code = read_nvram(offset++); 
		stored_code.w1 |= (int) read_nvram(offset++)<< 24;
		stored_code.w1 |= (int) read_nvram(offset++)<< 16;
		stored_code.w1 |= (int)read_nvram(offset++) << 8;
		stored_code.w1 = (int) read_nvram(offset++);
		stored_code.w2 |= (int) read_nvram(offset++)<< 24;
		stored_code.w2 |= (int) read_nvram(offset++)<< 16;
		stored_code.w2 |= (int) read_nvram(offset++)<< 8;
		stored_code.w2 = (int) read_nvram(offset++);
		time_stamp[0] =  read_nvram(offset++);
		time_stamp[1] =  read_nvram(offset++);
		time_stamp[2] =  read_nvram(offset++);
		time_stamp[3] =  read_nvram(offset++);


		printf(" %x: = IP32.%02X.%02X.%02X.%02X, w1 = %08x, w2 = %08x\n", \
			i,stored_code.sw,stored_code.func,stored_code.test,stored_code.code,stored_code.w1,stored_code.w2);
		printf("\t\ttime stamp = %02x/%02x - %02x:%02x GMT\n",time_stamp[3],time_stamp[2],time_stamp[1],time_stamp[0]);
	if(offset == 32)
		offset = 50;	/* skip over RTC_SAVE_UST, RTC_SAVE_REG and RTC_RESET_CTR from IP32.h */
	}
}

void
clear_nvram(void)
{
	unsigned char ptr[144] = 0;

	write_nvram(NVRAM_TEST_CODE,NVRAM_END-NVRAM_TEST_CODE,&ptr);
}
#endif
