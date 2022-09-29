#include <sys/cpu.h>
#include <sys/mace.h>
#include <sio.h>
#include <TL16550.h>

int Test_ace_regs(void);
int mace_serial_loopback(int channel);
void delay(int count);


int
mace_serial_test(void)
{

	uint64_t *pBaseReg=(uint64_t *)(ISA_RING_BASE_REG); 
	uint64_t *pMiscReg=(uint64_t *)(ISA_RING_BASE_REG+8);
	int channel, status;


	*pBaseReg = 0;	/* take the isa bus out of reset state*/

	status = Test_ace_regs();

	if(status != 0){	/* error in loopback test */
		*pMiscReg = 0x20;	/* red led on, green off */
		delay(1000000);	/* wait one second */
		*pMiscReg = 0x30;	/* red off, green off */
	}
	channel = 1;		/* test first serial port */
	status = mace_serial_loopback(channel);
	if(status != 0){	/* error in loopback test */
		*pMiscReg = 0x20;	/* red led on, green off */
		delay(1000000);	/* wait one second */
		*pMiscReg = 0x30;	/* red off, green off */
	}
	channel = 2;
	status = mace_serial_loopback(channel);
	if(status != 0){	/* error in loopback test */
		*pMiscReg = 0x20;	/* red led on, green off */
		delay(1000000);	/* wait one second */
		*pMiscReg = 0x30;	/* red off, green off */
		*pMiscReg = 0x20;	/* red led on, green off */
		delay(1000000);	/* wait one second */
		*pMiscReg = 0x30;	/* red off, green off */
	}
	return(status);

}


int
Test_ace_regs(void)
{

	volatile isa_ace_regs_t *pSerial_1 =(isa_ace_regs_t *)(SERIAL_A_ADDRS);
	volatile isa_ace_regs_t *pSerial_2 =(isa_ace_regs_t *)(SERIAL_B_ADDRS);
	unsigned char tmp;
	int error = 0;

    	 

	pSerial_1->scr = 0x55;
	tmp = pSerial_2->scr;
	tmp = pSerial_1->scr;
	if (tmp != 0x55)
		error |= 0x01;

	pSerial_1->scr = 0xaa;
	tmp = pSerial_2->scr;
	tmp = pSerial_1->scr;
	if (tmp != 0xaa)
		error |= 0x02;

	pSerial_1->scr = 0xf0;
	tmp = pSerial_2->scr;
	tmp = pSerial_1->scr;

	pSerial_1->scr = 0x0f;
	tmp = pSerial_2->scr;
	tmp = pSerial_1->scr;

	tmp = pSerial_1->rbr_thr;
	tmp = pSerial_1->ier;
	tmp = pSerial_1->iir_fcr;
	tmp = pSerial_1->lcr;
	tmp = pSerial_1->mcr;
	tmp = pSerial_1->lsr;
	tmp = pSerial_1->msr;


        pSerial_2->scr = 0x55;
        tmp = pSerial_1->scr;
        tmp = pSerial_2->scr;
	if (tmp != 0x55)
		error |= 0x04;

        pSerial_2->scr = 0xaa;
        tmp = pSerial_1->scr;
        tmp = pSerial_2->scr;
	if (tmp != 0xaa)
		error |= 0x08;

        pSerial_2->scr = 0xf0;
        tmp = pSerial_1->scr;
        tmp = pSerial_2->scr;

        pSerial_2->scr = 0xf0;
        tmp = pSerial_1->scr;
        tmp = pSerial_2->scr;

        tmp = pSerial_2->rbr_thr;
        tmp = pSerial_2->ier;
        tmp = pSerial_2->iir_fcr;
        tmp = pSerial_2->lcr;
        tmp = pSerial_2->mcr;
        tmp = pSerial_2->lsr;
        tmp = pSerial_2->msr;

    return(error);

}

int
mace_serial_loopback(int channel)
{

	volatile isa_ace_regs_t *pSerial;
	unsigned char rxbuff[256], tx_char, tmp;
	int i, timeout, error = 0;
	

	if(channel == 1)
	   pSerial =(isa_ace_regs_t *)(SERIAL_A_ADDRS);
	else{
	  if(channel ==2)
	     pSerial =(isa_ace_regs_t *)(SERIAL_B_ADDRS);
	  else
		return(0x200);
	}

	/* init registers */

	pSerial->lcr = UART_LCR_DLAB;	/* select divisor regs */
 
	pSerial->scr = 0x18;		/* set prescacler divisor, select uart */
	pSerial->ier = 0x00;
	pSerial->rbr_thr = 0x0c;		/* set to 9600 baud */
			/* select rx/tx regs, set 8 bit no parity */	
	pSerial->lcr = UART_LCR_WLS0 | UART_LCR_WLS1;
	pSerial->iir_fcr = UART_FCR_FIFO_ENABLE; 
	pSerial->ier = 0x0;	/* UART_IER_ERLSI | UART_IER_EMSI; - no ints yet */
	pSerial->mcr = UART_MCR_RTS | UART_MCR_DTR | UART_MCR_LOOP;

	i = 0;	/* clean out input fifo */
	while(pSerial->lsr & UART_LSR_DR && i <= 0x1f)
		rxbuff[i++] = pSerial->rbr_thr;
	i = 0;
	tx_char = 0;
	while(tx_char < 0xff){
		timeout = 10000;
		while(!(pSerial->lsr & UART_LSR_THR) && timeout > 0){
			delay(100);
			timeout--;
		}
		pSerial->rbr_thr = tx_char++;

		while(pSerial->lsr & UART_LSR_DR && i <= 0xff)
			rxbuff[i++] = pSerial->rbr_thr;
	}

	delay(10000);
	while(pSerial->lsr & UART_LSR_DR && i <= 0xff){
		rxbuff[i++] = pSerial->rbr_thr;
		delay(10000);
	}

	for (i=0;i<0xff;i++){
		if (rxbuff[i] != (unsigned char)i){
			error = i | 0x100;
			return(error);
		}
	}
	return(error);
}

void
delay(int count)
{

	volatile counters_t * pCounters = (counters_t *)COUNTERS;
	unsigned long long	tmp;

	tmp = (pCounters->ust & 0xffffffff);
	tmp += (long long)count;
	while(tmp > (pCounters->ust & 0xffffffff));
}

