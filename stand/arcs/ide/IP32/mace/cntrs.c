#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <sys/if_me.h>
#include <fault.h>
#include <setjmp.h>
#include <libsc.h>
#include <libsk.h>
#include <IP32_status.h>
#include <uif.h>
#include "sio.h"
#include "TL16550.h"

#define TIMEOUT 10000
#define CRM_INT_STATUS 0xffffffff14000000
#define MACE_UST_MASK	0xffffffff


static unsigned long long data[4];
code_t cntrs_ecode;

long long
GetInterruptStatus()
{
	long long status;

	status = (READ_REG64(PHYS_TO_K1(CRM_INTSTAT),long long) &
			0x0000ffffLL);
	msg_printf(VDBG, "GetInterruptStatus: status==0x%llx\n", status);
	return(status);
}

void
clear_mace_intmask(void)
{

	long long read_data = 0;
	volatile unsigned long long *ptr2 = 
		(unsigned long long *)PC_INT_MASK;
	WRITE_REG64(read_data, ptr2, long long);
}

int
cntrs_test(void)
{
	int i, i_timeout, e_status;
	long long return_value;
	unsigned char nvram;
	unsigned long long read_data;
	unsigned long long read_data1;
	unsigned long long cnt_value;
	unsigned long long cnt_increment = 0;
	volatile struct counters *cntrs = (struct counters *)(COUNTERS);
	volatile unsigned long long *ptr2 = 
		(unsigned long long *)PC_INT_STATUS;
	volatile unsigned long long *prom =
				(unsigned long long *)PROM;
	volatile unsigned char *ptr1 = (unsigned char *)RTC;

	unsigned long long prom_data;

	cntrs_ecode.sw = 0x08;
	cntrs_ecode.func = 0x15;
	cntrs_ecode.test = 0x01;
	cntrs_ecode.code = 0;
	cntrs_ecode.w1 = 0;
	cntrs_ecode.w2 = 0;

	msg_printf(VRB, "MACE Counters Test\n");
	data[0] = 0xffffffff55555555LL;
	data[1] = 0xffffffffaaaaaaaaLL;
	data[2] = 0xffffffffffffffffLL;
	data[3] = 0xffffffff00000000LL;
	e_status = 0;
	/* if Report level is DBG or above set the timer increment large */
	if ( *Reportlevel >= DBG ) {
		msg_printf(DBG, "Counter Increment Set to 0x100000\n");
		cnt_increment = 0x100000;
		i_timeout = TIMEOUT*0x1000;
	} else {
		cnt_increment = 0x4000;
		i_timeout = TIMEOUT;
	}
	/* save first read of counter */
	cnt_value = (READ_REG64(&cntrs->ust, long long) & 0xffffffffLL);
	/* read the counter a few more times */
	for(i=0;i<4;i++){
		read_data = READ_REG64(&cntrs->ust, long long)& 0xffffffffLL;
		prom_data = READ_REG64(prom, long long);
		msg_printf(DBG, "read = 0x%llx from 0x%x\n",
					read_data, &cntrs->ust);
	};
	msg_printf(DBG, "prom read data = 0x%llx from 0x%x\n",
					prom_data, prom);
	/* the saved read should be different from the last read */
	if ( cnt_value == read_data ) {
		msg_printf(ERR, "ERROR: ust unchanged from first read\n");
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 2;
	/* read the interrupt status */
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG,"read_data = 0x%llx from 0x%x\n", read_data, ptr2);

	for(i=0; i<4; i++) {
		WRITE_REG64(data[i], &cntrs->cmp1, long long);
		WRITE_REG64(data[((i+1)%4)], &cntrs->cmp2, long long);
		WRITE_REG64(data[((i+2)%4)], &cntrs->cmp3, long long);
		read_data = READ_REG64(&cntrs->cmp1, long long);
		msg_printf(DBG, "cmp1 read == 0x%llx\n", read_data);
		if (data[i] != read_data){
			msg_printf(ERR, "cmp1 r/w, read 0x%llx from 0x%x\n",
				read_data, &cntrs->cmp1);
			msg_printf(ERR,"\tshould be 0x%llx\n", data[i]);
			report_error(&cntrs_ecode);
			e_status++;
		};
		cntrs_ecode.code++; 
		read_data = READ_REG64(&cntrs->cmp2, long long);
		msg_printf(DBG, "cmp2 read == 0x%llx\n", read_data);
		if (data[((i+1)%4)] != read_data){
			msg_printf(ERR, "cmp2 r/w, read 0x%llx from 0x%x\n",
				read_data, &cntrs->cmp2);
			msg_printf(ERR,"\tshould be 0x%llx\n",data[((i+1)%4)]);
			report_error(&cntrs_ecode);
			e_status++;
		};
		cntrs_ecode.code++; 
		read_data = READ_REG64(&cntrs->cmp3, long long);
		msg_printf(DBG, "cmp3 read == 0x%llx\n", read_data);
		if (data[((i+2)%4)] != read_data){
			msg_printf(ERR, "cmp3 r/w, read 0x%llx from 0x%x\n",
				read_data, &cntrs->cmp3);
			msg_printf(ERR,"\tshould be 0x%llx\n",data[((i+2)%4)]);
			report_error(&cntrs_ecode);
			e_status++;
		};
		cntrs_ecode.code++; 
	};
	cntrs_ecode.test = 2;
	cntrs_ecode.code = 1;
	ptr2 = (unsigned long long *)PC_INT_MASK;
	msg_printf(DBG, "initial mask==0x%llx\n",
			(read_data = READ_REG64(ptr2, long long)));
	read_data |= (CMP1_INT_BIT | CMP2_INT_BIT | CMP3_INT_BIT);
	msg_printf(DBG, "writing 0x%llx to 0x%x\n", read_data, ptr2);
	WRITE_REG64(read_data, ptr2, long long);
	msg_printf(DBG, "mask==0x%llx from 0x%x\n",
			READ_REG64(ptr2, long long), ptr2);
	read_data = READ_REG64(&cntrs->ust, long long)& 0xffffffffLL; 
	msg_printf(DBG, "UST read as 0x%llx\n", read_data);
	cnt_value = (cnt_increment + read_data) & MACE_UST_MASK;
	msg_printf(DBG, "writing 0x%llx to (cmp3) 0x%x\n",
			cnt_value, &cntrs->cmp3);
	WRITE_REG64(cnt_value, &cntrs->cmp3, long long);
	msg_printf(DBG, "writing 0x%llx to (cmp2) 0x%x\n",
			((0x2000 + cnt_value) & MACE_UST_MASK), &cntrs->cmp2);
	WRITE_REG64(((0x2000 + cnt_value) & MACE_UST_MASK),
					&cntrs->cmp2, long long);
	msg_printf(DBG, "writing 0x%llx to (cmp1) 0x%x\n",
			((0x4000 + cnt_value) & MACE_UST_MASK), &cntrs->cmp1);
	WRITE_REG64(((0x4000 + cnt_value) & MACE_UST_MASK),
					&cntrs->cmp1, long long);
	read_data = READ_REG64(&cntrs->cmp3, long long)& 0xffffffffLL;
	read_data1 = READ_REG64(&cntrs->ust, long long)& 0xffffffffLL;
	msg_printf(DBG, "cmp3==0x%llx when read, UST==0x%llx\n",
				read_data, read_data1);
	if ( read_data != cnt_value ) {
		msg_printf(ERR, "cmp3 write failed, read 0x%llx from 0x%x\n",
			read_data, &cntrs->cmp3);
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 2;
	ptr2 = (unsigned long long *)CMP3_ALIAS;
	read_data = READ_REG64(ptr2, long long); 
	if ( read_data != cnt_value ) {
		msg_printf(ERR,"cmp3 alias incorrect, read 0x%llx from 0x%x\n",
			read_data, ptr2);
		report_error(&cntrs_ecode);
	}

        /* Wait for interrupt */
	i = i_timeout;
        while (!(return_value = GetInterruptStatus()) && i) {
                us_delay(5);
		i--;
        }
	if (i == 0) {		/* above loop counted down i */
		msg_printf(ERR,"interrupt timeout (CRM_INTSTAT==0x%llx)\n",
					return_value);
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 3;
	msg_printf(DBG, "Interrupt Status = 0x%llx\n", return_value);

	ptr2 = (unsigned long long *)PC_INT_STATUS;
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "status==0x%llx from 0x%x\n", read_data, ptr2);
	while (!(CMP3_INT_BIT & read_data) &&
	    ((0x100 + cnt_value) >= (read_data1=READ_REG64(&cntrs->ust, long long)& 0xffffffffLL))) {
		read_data = READ_REG64(ptr2, long long);
	}
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "CMP3_INT_BIT: read_data==0x%llx,read_data1==0x%llx\n",
				read_data, read_data1);
	if (!(CMP3_INT_BIT & read_data)) {
		msg_printf(ERR,"cmp3 interrupt failed\n");
		if ( *Reportlevel < DBG ) {
			msg_printf(ERR,"status==0x%llx, UST==0x%llx\n",
				 read_data, read_data1);
		}
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 4;
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "status==0x%llx from 0x%x\n", read_data, ptr2);
	while (!(CMP2_INT_BIT & read_data) &&
		((0x2100+cnt_value) >= 
		    (read_data1=READ_REG64(&cntrs->ust, long long)& 0xffffffffLL))) {
		read_data = READ_REG64(ptr2, long long);
	}
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "CMP2_INT_BIT: read_data==0x%llx,read_data1==0x%llx\n",
				read_data, read_data1);
	if(!(CMP2_INT_BIT & read_data)){
		msg_printf(ERR,"cmp2 interrupt failed\n");
		if ( *Reportlevel < DBG ) {
			msg_printf(ERR,"status==0x%llx, UST==0x%llx\n",
				 read_data, read_data1);
		}
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 5;
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "status==0x%llx from 0x%x\n", read_data, ptr2);
	while (!(CMP1_INT_BIT & read_data) &&
		((0x4100 + cnt_value) >= (READ_REG64(&cntrs->ust, long long)& 0xffffffffLL))) {
		read_data = READ_REG64(ptr2, long long);
	}
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "CMP1_INT_BIT: read_data==0x%llx,read_data1==0x%llx\n",
				read_data, read_data1);
	if (!(CMP1_INT_BIT & read_data)) {
		msg_printf(ERR, "cmp1 interrupt failed\n");
		if ( *Reportlevel < DBG ) {
			msg_printf(ERR,"status==0x%llx, UST==0x%llx\n",
				 read_data, read_data1);
		}
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 6;
	
	read_data = READ_REG64(&cntrs->ust, long long);
	msg_printf(DBG, "UST read==0x%llx\n", read_data);

	ptr2 = (unsigned long long *)CMP1_ALIAS;
	read_data = READ_REG64(ptr2, long long)& 0xffffffffLL;
	if (read_data != (read_data1=(READ_REG64(&cntrs->cmp1, long long)& 0xffffffffLL))) {
		msg_printf(ERR, "CMP1_ALIAS error (0x%llx!=0x%llx)\n",
				read_data, read_data1);
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 6;
	msg_printf(DBG, "CMP1_ALIAS==0x%llx from 0x%x\n", read_data, ptr2);
	WRITE_REG64((long long)0, ptr2, long long);
	read_data1 = READ_REG64(ptr2, long long)& 0xffffffffLL;
	msg_printf(DBG, "CMP1_ALIAS==0x%llx after clear\n", read_data1);

	ptr2 = (unsigned long long *)CMP2_ALIAS;
	read_data = READ_REG64(ptr2, long long)& 0xffffffffLL;
	if (read_data != (read_data1=READ_REG64(&cntrs->cmp2, long long)& 0xffffffffLL)) {
		msg_printf(ERR, "CMP2_ALIAS error (0x%llx!=0x%llx)\n",
				read_data, read_data1);
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 7;
	msg_printf(DBG, "CMP2_ALIAS==0x%llx from 0x%x\n", read_data, ptr2);
	WRITE_REG64((long long)0, ptr2, long long);
	read_data1 = READ_REG64(ptr2, long long)& 0xffffffffLL;
	msg_printf(DBG, "CMP2_ALIAS==0x%llx after clear\n", read_data1);

	ptr2 = (unsigned long long *)CMP3_ALIAS;
	read_data = READ_REG64(ptr2, long long)& 0xffffffffLL;
	if (read_data != (read_data1=READ_REG64(&cntrs->cmp3, long long)& 0xffffffffLL)) {
		msg_printf(ERR, "CMP3_ALIAS error (0x%llx!=0x%llx)\n",
				read_data, read_data1);
		report_error(&cntrs_ecode);
		e_status++;
	}
	cntrs_ecode.code = 8;
	msg_printf(DBG, "CMP3_ALIAS==0x%llx from 0x%x\n", read_data, ptr2);
	WRITE_REG64((long long)0, ptr2, long long);
	read_data1 = READ_REG64(ptr2, long long)& 0xffffffffLL;
	msg_printf(DBG, "CMP3_ALIAS==0x%llx after clear\n", read_data1);

	ptr2 = (unsigned long long *)PC_INT_STATUS;
	read_data = READ_REG64(ptr2, long long);
	msg_printf(DBG, "Final Interrupt Status==0x%llx\n", read_data);

#ifdef notdef
	nvram = 1;
	while (nvram != 0) {
		us_delay(10);
		nvram = *ptr1;
	}
#endif /* notdef */

	clear_mace_intmask();

	if (e_status){
	   msg_printf(ERR,"e_status = 0x%x\n",e_status);
	   return(1);
	}
	else
	   return(0);
}
