#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <setjmp.h>
#include "uif.h"
#include <libsc.h>
#include <libsk.h>
#include <tiles.h>
#include <IP32_status.h>
#include "sio.h"
#include "TL16550.h"


struct int_event{
	unsigned long long mace_int_status;
	unsigned long long int_serviced;
	unsigned long long control;
	unsigned long long read_ptr;
	unsigned long long write_ptr;
	unsigned long long depth;
	unsigned long long ust;
	int	rx_count;
	int	tx_count;
	unsigned char MCR_reg;
	unsigned char IIR_reg;
	unsigned char MSR_reg;
};

unsigned int error_one;
unsigned int error_two;
unsigned char error_bits[5]; 

code_t sio_ecode;
unsigned long mace_rb_base_addr;
void *pa;
volatile struct pc_base_regs *mace_rb_base = (struct pc_base_regs *)PC_BASE_REGS;
volatile struct counters *cntrs = (struct counters *)(COUNTERS);


int sio_timeout; 
unsigned char tx1_data_buf[6000];
unsigned char tx1_cntrl_buf[6000];
unsigned char rx1_data_buf[6000];
unsigned char rx1_status_buf[6000];

unsigned char tx2_data_buf[6000];
unsigned char tx2_cntrl_buf[6000];
unsigned char rx2_data_buf[6000];
unsigned char rx2_status_buf[6000];
struct int_event interrupts[1024];
unsigned int rx1_byte_count, rx2_byte_count, tx1_byte_count, tx2_byte_count, \
		interrupt_count, tx1_len, tx2_len;



volatile SERIAL_DMA_RING_REG_DESCRIPTOR *pSer_dma_regs_1 = (SERIAL_DMA_RING_REG_DESCRIPTOR *) SERIAL_A_DMA_REG_ADDRS;

volatile SERIAL_DMA_RING_REG_DESCRIPTOR *pSer_dma_regs_2 = (SERIAL_DMA_RING_REG_DESCRIPTOR *) SERIAL_B_DMA_REG_ADDRS;

volatile isa_ace_regs *pSerial_1 = (isa_ace_regs *)(SERIAL_A_ADDRS);
volatile isa_ace_regs *pSerial_2 = (isa_ace_regs *)(SERIAL_B_ADDRS);
int baud_rate;
int internal_loop;



/**********************************************
 * sioDMATests
 * This module contains a series of functions
 * which tests the DMA ring buffers
 *
 */
int
SioLoopbackTest(int argc, char* argv[])
{

    int i, j, k, start, skip, channels;

    unsigned long *pRingBaseReg  = (unsigned long *)ISA_RING_BASE_REG;

    unsigned char *pSerIntfReg, *pReg, *pString, cntrl, data;
    unsigned int  baseRingAddr, ringReg, e_status;
    char *cons; 
    size_t string_len;
    unsigned char string_1[5] = {0xaa,0x55,0x33,0xcc,0xff};
    unsigned char cntrl_bytes[6] = {TX_CNTRL_INVALID, \
					TX_CNTRL_MCR, \
					TX_CNTRL_DELAY, \
					TX_CNTRL_INVALID | POST_TX_INT, \
					TX_CNTRL_MCR | POST_TX_INT, \
					TX_CNTRL_DELAY | POST_TX_INT};

    unsigned char data_bytes[6] = {0x55,0x12,0x00,0x55,0x12,0x00};

    sio_ecode.sw = 0x08;
    sio_ecode.func = 0x0e;
    sio_ecode.test = 0x01;
    sio_ecode.code = 0;
    sio_ecode.w1 = 0;
    sio_ecode.w2 = 0;
    if (argc >= 2){
	if (*argv[1]=='i')
		internal_loop = 1;
     }
     else{
	internal_loop = 0;
    }
    if (argc >= 3)
	baud_rate = atoi(argv[2]);
    else
	baud_rate = 115;

    msg_printf(DBG,"baud rate = %x\n",baud_rate);

    e_status = 0; /* zero = pass, incremented for failures */
    error_one = 0;
    error_two = 0;  /* not used here, must be zero */

    /*
     * take the bus out of reset state
     */

    *pRingBaseReg = 0;

    msg_printf(DBG,"\nRing Base Reg: %x", pRingBaseReg);

    setup_mace_ringbuf();

    msg_printf(DBG,"\npRingBaseAddr: %x content: %x mace_rb_base_addr: %x", 
	   pRingBaseReg, (unsigned int)*pRingBaseReg,
	   mace_rb_base_addr);


    /*
     *do dma loopback test
     */

    cons = getenv("console");
    if (*cons == 'g' || *cons == 'G'){
    msg_printf(VRB,"\nDMA loopback test, Serial A\n");

    Clear_tx_buffs();

    tx1_len = 135;
    channels = SERIAL_1;

    Setup_tx_data(channels);

    cntrl = TX_CNTRL_MCR;
    data = UART_MCR_LOOP | UART_MCR_RTS;
    data = UART_MCR_LOOP;
    start = 5;
    skip = 7;

    Modify_tx_buffs(cntrl,data,start,skip,channels);

    cntrl = POST_TX_INT;
    data = 0;
    start = 0x30;
    skip = 0x40;

    Modify_tx_buffs(cntrl,data,start,skip,channels);

    cntrl = TX_CNTRL_INVALID;
    data = 0x01;
    start = 0x31;
    skip = 0x0e;

    Modify_tx_buffs(cntrl,data,start,skip,channels);

    Init_DMA_Channel(pSer_dma_regs_1,(unsigned char *)SERIAL_A_ADDRS);

    Clear_rx_buffs();
    sio_timeout = 0x18000;
    e_status += DoLoopback (channels);

    e_status += compare_data();

    e_status += verify_interrupts();

    }
	/************* end of console port test ****************/

    msg_printf(VRB,"\nDMA loopback test, Serial B\n");

    Clear_tx_buffs();
    Clear_rx_buffs();

    tx2_len = 4300;
    channels = SERIAL_2;

    Setup_tx_data(channels);

    cntrl = TX_CNTRL_INVALID;
    for(start=0x3f;start<0x110;start+=0xf){
    	data = 0x40;
    	skip = 0x20;
    	Modify_tx_buffs(cntrl,data,start,skip,channels);
    }
    cntrl = POST_TX_INT;
    data = 0;
    start = 0x100;
    skip = 0x600;

    Modify_tx_buffs(cntrl,data,start,skip,channels);


    Init_DMA_Channel(pSer_dma_regs_2,(unsigned char *)SERIAL_B_ADDRS);
    if (e_status == 0){
    sio_timeout = 0x18000;
    e_status += DoLoopback (channels);

    e_status += compare_data();

    e_status += verify_interrupts();
    }

    if (*cons == 'g' || *cons == 'G'){
    msg_printf(VRB,"\nDMA loopback test, Serial A and B\n");

    Clear_tx_buffs();
    Clear_rx_buffs();

    tx1_len = 2200;
    tx2_len = 2300;
    channels = SERIAL_1 | SERIAL_2;

    Setup_tx_data(channels);
/*
 * modify buffers so all two byte sequences of control bytes are used
 * both with and without tx pair interrupts.
 */

    i = 0;
    for(start=0x3f;start<0x110;start+=0xf){
    	cntrl = cntrl_bytes[i%6];
    	data = data_bytes[i%6];
    	skip = 0x20;
    	Modify_tx_buffs(cntrl,data,start,skip,channels);
    	k=0;
	for(j=(start-1);j<0x110;j+=0x20){
		cntrl = cntrl_bytes[k%6];
		data = data_bytes[k%6];
		skip = 0;
		Modify_tx_buffs(cntrl,data,j,skip,channels);
		k++;
	}
	i++;
    }


    Init_DMA_Channel(pSer_dma_regs_2,(unsigned char *)SERIAL_B_ADDRS);
    Init_DMA_Channel(pSer_dma_regs_1,(unsigned char *)SERIAL_A_ADDRS);

    if(e_status == 0){
    sio_timeout = 0x18000;
    e_status += DoLoopback (channels);

    e_status += compare_data();

    e_status += verify_interrupts();
    }
    }
    freeTile(pa);
    return(e_status);

}                   /*SioLoopbackTest*/


int
DoLoopback(channels)
int channels;
{
	int i, j, k, e_status, tx1_load_cnt, tx2_load_cnt, ringId;
	unsigned long long mace_int_status, crime_int, ust;
	unsigned char *pRingAddr;

	e_status = 0;

	if (channels & SERIAL_1){
	   if((tx1_len*2) < 0xfe0){
		tx1_load_cnt = tx1_len*2;
		tx1_load_cnt += 31;
		tx1_load_cnt &= 0xfe0;
	   }
	   else
	   	tx1_load_cnt &= 0xfe0;

	   tx1_byte_count = tx1_load_cnt;
	   ringId = SERIAL_A_TX_RING;
           pRingAddr = (unsigned char *)(mace_rb_base_addr | (ringId << 12));
	   k = 0;
	   for(i=0;i<tx1_load_cnt;i+=8){
	   	for(j=0;j<4;j++){
		   pRingAddr[i+j+4] = tx1_data_buf[k+j];
		   pRingAddr[i+j] = tx1_cntrl_buf[k+j];
		}
	   k += 4;
	   }
	}
	
	if (channels & SERIAL_2){
	   if((tx2_len*2) < 0xfe0){
		tx2_load_cnt = tx2_len*2;
		tx2_load_cnt += 31;
		tx2_load_cnt &= 0xfe0;
	   }
	   else
	   	tx2_load_cnt = 0xfe0;

	   tx2_byte_count = tx2_load_cnt;
	   ringId = SERIAL_B_TX_RING;
           pRingAddr = (unsigned char *)(mace_rb_base_addr | (ringId << 12));
	   msg_printf(DBG,"p ring addr = %x\n",pRingAddr);
	   k = 0;
	   for(i=0;i<tx2_load_cnt;i+=8){
	   	for(j=0;j<4;j++){
		   pRingAddr[i+j+4] = tx2_data_buf[k+j];
		   pRingAddr[i+j] = tx2_cntrl_buf[k+j];
		}
	   k += 4;
	   }
	}

/*
 * enable dma for tx and rx on selected channels
 */


	if (channels & SERIAL_1){
	   pSer_dma_regs_1->rxCont |= (DMA_ENABLE | DMA_INTR_RING_NEMPTY);
	   pSer_dma_regs_1->txCont |= (DMA_ENABLE | DMA_INTR_RING_EMPTY);
	}
	if (channels & SERIAL_2){
	   pSer_dma_regs_2->rxCont |= (DMA_ENABLE | DMA_INTR_RING_NEMPTY);
	   pSer_dma_regs_2->txCont |= (DMA_ENABLE | DMA_INTR_RING_EMPTY);
	}

/*
 *	update tx wr reg to start tx dma
 */
	if(channels & SERIAL_1){
		pSer_dma_regs_1->txWrite = tx1_load_cnt; 
	}
	if(channels & SERIAL_2){
		pSer_dma_regs_2->txWrite = tx2_load_cnt; 
	}

	mace_rb_base->pc_int_mask = 0; /* disable all ints */
	us_delay(10);
	if(channels & SERIAL_1){
		mace_rb_base->pc_int_mask |= ALL_CH1_INTS; /* enable all ints */
	}
	if(channels & SERIAL_2){
		mace_rb_base->pc_int_mask |= ALL_CH2_INTS; /* enable all ints */
	}
/*
 *	wait for interrupt
 */
	while (1){
	   i = 0;
	   crime_int = 0;
           while ((i < sio_timeout) && (!crime_int)){
		crime_int = GetInterruptStatus();
		i++;
		if(i%1000 == 0)
		   msg_printf(DBG,"i = %x\n",i);
           }
	   if(i==sio_timeout)
		break;

	   mace_int_status = mace_rb_base->pc_int_status;
	   if(0 == mace_int_status){
		sio_ecode.code = 0x01;
		sio_ecode.w1 = mace_int_status;
		sio_ecode.w2 = crime_int;
		report_error(&sio_ecode);
		msg_printf(ERR,"ERROR:");
		msg_printf(ERR,"mace int status = %llx\n",mace_int_status);
		msg_printf(ERR,"crime int = %llx\n",crime_int);
		e_status++;
	   }
	   ust = cntrs->ust;
	   if ((TX1_MEM_ERROR | TX2_MEM_ERROR | RX1_ERROR | RX2_ERROR) & mace_int_status){
		error_int(mace_int_status,ust);
		e_status++;
		break;
	   }
	   if ((RX1_THRESHOLD | RX2_THRESHOLD) & mace_int_status)
		rx_int(mace_int_status,ust);
	   if ((TX1_PAIR | TX2_PAIR) & mace_int_status)
		tx_pair_int(mace_int_status,ust);
	   if ((CH1_DEVICE | CH2_DEVICE) & mace_int_status)
		uart_int(mace_int_status,ust);
	   if ((TX1_THRESHOLD | TX2_THRESHOLD) & mace_int_status)
		tx_int(mace_int_status,ust);
	   if(interrupt_count == 1000)
		interrupt_count--;	/* end of array, no more space */

	} /* end while */

/*	and rx complete, (wait for int timeout?) */

	msg_printf(DBG,"tx1 regs %llx, %llx, %llx, %llx\n", \
		pSer_dma_regs_1->txCont, \
		pSer_dma_regs_1->txRead, \
		pSer_dma_regs_1->txWrite, \
		pSer_dma_regs_1->txDepth);
	
	msg_printf(DBG,"rx1 regs %llx, %llx, %llx, %llx\n", \
		pSer_dma_regs_1->rxCont, \
		pSer_dma_regs_1->rxRead, \
		pSer_dma_regs_1->rxWrite, \
		pSer_dma_regs_1->rxDepth);
	
	msg_printf(DBG,"tx2 regs %llx, %llx, %llx, %llx\n", \
		pSer_dma_regs_2->txCont, \
		pSer_dma_regs_2->txRead, \
		pSer_dma_regs_2->txWrite, \
		pSer_dma_regs_2->txDepth);
	
	msg_printf(DBG,"rx2 regs %llx, %llx, %llx, %llx\n", \
		pSer_dma_regs_2->rxCont, \
		pSer_dma_regs_2->rxRead, \
		pSer_dma_regs_2->rxWrite, \
		pSer_dma_regs_2->rxDepth);
	
/*	stop dma on both channels */

	msg_printf(DBG,"Disable all channels\n");
	pSer_dma_regs_1->rxCont = DMA_RESET;
	pSer_dma_regs_1->txCont = DMA_RESET;
	pSer_dma_regs_2->rxCont = DMA_RESET;
	pSer_dma_regs_2->txCont = DMA_RESET;
	
	mace_rb_base->pc_int_mask = 0; /* disable all ints */

	msg_printf(DBG,"Uart1 Regs %x, %x, %x, %x, %x, %x, %x\n", \
			pSerial_1->ier, pSerial_1->iir_fcr, pSerial_1->lcr, pSerial_1->mcr, \
			pSerial_1->lsr, pSerial_1->msr, pSerial_1->scr);

	msg_printf(DBG,"Uart2 Regs %x, %x, %x, %x, %x, %x, %x\n", \
			pSerial_2->ier, pSerial_2->iir_fcr, pSerial_2->lcr, pSerial_2->mcr, \
			pSerial_2->lsr, pSerial_2->msr, pSerial_2->scr);

	return(e_status);
}


/*
 * compare rx to tx buffs
 */

int
compare_data()
{
	int i, j, k, rx_valid_count, tx_valid_count, rx1_len, rx2_len, bytes_compared, \
		e_status, limit;

	e_status = 0; /* zero = no errors */

	msg_printf(DBG,"\n\nrx1 data\n");
	msg_printf(DBG,"rx1 status\n");
	for(j=0;j<tx1_len;j+=32){
	for(i=0;i<32;i++){
		msg_printf(DBG,"%2x",rx1_data_buf[i+j]);
	}
	msg_printf(DBG,"\n");
	for(i=0;i<32;i++){
		msg_printf(DBG,"%2x",rx1_status_buf[i+j]);
	}
	msg_printf(DBG,"\n");
	}

	msg_printf(DBG,"\n\nrx2 data\n");
	msg_printf(DBG,"rx2 status\n");
	for(j=0;j<tx2_len;j+=32){
	for(i=0;i<32;i++){
		msg_printf(DBG,"%2x",rx2_data_buf[i+j]);
	}
	msg_printf(DBG,"\n");
	for(i=0;i<32;i++){
		msg_printf(DBG,"%2x",rx2_status_buf[i+j]);
	}
	msg_printf(DBG,"\n");
	}

	if(rx1_byte_count == 0 && rx2_byte_count == 0){
		e_status++;
		sio_ecode.code = 0x02;
		sio_ecode.w1 = 0;
		sio_ecode.w2 = 0;
		report_error(&sio_ecode);
		msg_printf(ERR,"ERROR: no rx data\n");
		return(e_status); 
	}

	rx_valid_count = 0;
	for(i=0;i<sizeof rx1_status_buf;i++){
		if(rx1_status_buf[i] == RX_STATUS_VALID)
			rx_valid_count++;
	}
	tx_valid_count = 0;
	for(i=0;i<sizeof tx1_cntrl_buf;i++){
		if((tx1_cntrl_buf[i] & ~POST_TX_INT) == TX_CNTRL_VALID)
			tx_valid_count++;
	}
	if(rx_valid_count != tx_valid_count){
		e_status++;
		sio_ecode.code = 0x03;
		sio_ecode.w1 = rx_valid_count;
		sio_ecode.w2 = tx_valid_count;
		report_error(&sio_ecode);
		msg_printf(ERR,"ERROR: rx1 count %x != tx1 count %x\n",rx_valid_count, \
				tx_valid_count);
	}


	rx_valid_count = 0;
	for(i=0;i<sizeof rx2_status_buf;i++){
		if(rx2_status_buf[i] == RX_STATUS_VALID)
			rx_valid_count++;
	}
	tx_valid_count = 0;
	for(i=0;i<sizeof tx2_cntrl_buf;i++){
		if((tx2_cntrl_buf[i] & ~POST_TX_INT) == TX_CNTRL_VALID)
			tx_valid_count++;
	}
	if(rx_valid_count != tx_valid_count){
		e_status++;
		sio_ecode.code = 0x04;
		sio_ecode.w1 = rx_valid_count;
		sio_ecode.w2 = tx_valid_count;
		report_error(&sio_ecode);
		msg_printf(ERR,"ERROR: rx2 count %x != tx2 count %x\n",rx_valid_count, \
				tx_valid_count);
	}



	if(rx1_byte_count){
	    j = 0;
	    i = 0;
	    bytes_compared = 0;
	    rx1_len = (rx1_byte_count/2);
	    limit = (tx1_byte_count/2);
	    msg_printf(VRB,"rx1 length = %x\n",rx1_len);
	    while((j<rx1_len) && (i<limit)){
		while(((tx1_cntrl_buf[i] & ~POST_TX_INT) != TX_CNTRL_VALID) \
					 && (i<limit))
			i++;
		while((!(rx1_status_buf[j] & RX_STATUS_VALID)) && (j<rx1_len))
			j++;
		if(tx1_data_buf[i] != rx1_data_buf[j]){
			sio_ecode.code = 0x05;
			sio_ecode.w1 = j;
			sio_ecode.w2 = i;
			report_error(&sio_ecode);
		     msg_printf(ERR,"ERROR: miscompare @ %x,%x  rx = %2x, tx = %2x\n", \
			j,i,rx1_data_buf[j],tx1_data_buf[i]);
		     e_status++;
		 }
		j++;
		i++;
		bytes_compared++;
	    }
	    msg_printf(VRB,"channel 1 bytes compared = %x\n",bytes_compared);
	}


	if(rx2_byte_count){
	    j = 0;
	    i = 0;
	    bytes_compared = 0;
	    rx2_len = rx2_byte_count/2;
	    limit = (tx2_byte_count/2);
	    msg_printf(VRB,"rx2 length = %x\n",rx2_len);
	    while(j<rx2_len && (i<limit)){
		while(((tx2_cntrl_buf[i] & ~POST_TX_INT) != TX_CNTRL_VALID) \
					 && (i<limit))
			i++;
		while(!(rx2_status_buf[j] & RX_STATUS_VALID) && (j<rx2_len))
			j++;
		if(tx2_data_buf[i] != rx2_data_buf[j]){
			sio_ecode.code = 0x06;
			sio_ecode.w1 = j;
			sio_ecode.w2 = i;
			report_error(&sio_ecode);
		     msg_printf(ERR,"ERROR: miscompare @ %x,%x  rx = %2x, tx = %2x\n", \
			j,i,rx2_data_buf[j],tx2_data_buf[i]);
		     e_status++;
		 }
		j++;
		i++;
		bytes_compared++;
	    }
	    msg_printf(VRB,"channel 2 bytes compared = %x\n",bytes_compared);
	}

	return(e_status);
}


int
verify_interrupts()
{
	int i, j, k, e_status;

	e_status = 0; /* zero = no errors */
	msg_printf(DBG,"verify interrupts\n");
	
	if(interrupt_count == 0){
		sio_ecode.code = 0x07;
		sio_ecode.w1 = 0;
		sio_ecode.w2 = 0;
		report_error(&sio_ecode);
		e_status++;
		msg_printf(ERR,"ERROR: no interrupts\n");
		return(e_status);
	}

	for(i=0;i<interrupt_count;i++){
		msg_printf(DBG,"int %x\n",i);
		msg_printf(DBG,"status = %llx ",interrupts[i].mace_int_status);
		msg_printf(DBG,"serviced = %llx\n",interrupts[i].int_serviced);
		msg_printf(DBG,"control = %llx ",interrupts[i].control);
		msg_printf(DBG,"read ptr = %llx ",interrupts[i].read_ptr);
		msg_printf(DBG,"write ptr = %llx ",interrupts[i].write_ptr);
		msg_printf(DBG,"depth = %llx\n",interrupts[i].depth);
		msg_printf(DBG,"ust = %llx\n",interrupts[i].ust);
		msg_printf(DBG,"rx count = %x ",interrupts[i].rx_count);
		msg_printf(DBG,"tx count = %x\n",interrupts[i].tx_count);
		msg_printf(DBG,"MCR = %x\n",interrupts[i].MCR_reg);
		msg_printf(DBG,"IIR = %x\n",interrupts[i].IIR_reg);
		msg_printf(DBG,"MSR = %x\n",interrupts[i].MSR_reg);
	}
	return(e_status);
}

int
uart_int(status,ust)
unsigned long long status;
unsigned long long ust;
{
	int i, j, e_status;

	e_status = 0;
	msg_printf(DBG,"uart int\n");

	i = interrupt_count;

	interrupts[i].ust = ust;
       	interrupts[i].mace_int_status = status;
       	interrupts[i].int_serviced = ((CH1_DEVICE | CH2_DEVICE) & status);
	
	/* get depth, rd pointer, buf pointer */

	if(CH1_DEVICE & status){
       	   interrupts[i].IIR_reg = pSerial_1->iir_fcr;
	   interrupts[i].MCR_reg = pSerial_1->mcr;
	   interrupts[i].MSR_reg = pSerial_1->lsr; /* need to read lsr to clear int */

       	   interrupts[i].control = pSer_dma_regs_1->rxCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_1->rxRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_1->rxWrite;
       	   interrupts[i].depth = pSer_dma_regs_1->rxDepth;
       	   interrupts[i].rx_count = rx1_byte_count;
       	   interrupts[i].tx_count = tx1_byte_count;
	   interrupt_count += 1;
	   i = interrupt_count;
	  /* mace_rb_base->pc_int_mask &= ~CH1_DEVICE;  disable int */

	}
	
	if(CH2_DEVICE & status){
       	   interrupts[i].IIR_reg = pSerial_2->iir_fcr;
	   interrupts[i].MCR_reg = pSerial_2->mcr;
	   interrupts[i].MSR_reg = pSerial_2->lsr; /* need to read lsr to clear int */

       	   interrupts[i].control = pSer_dma_regs_2->rxCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_2->rxRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_2->rxWrite;
       	   interrupts[i].depth = pSer_dma_regs_2->rxDepth;
       	   interrupts[i].rx_count = rx2_byte_count;
       	   interrupts[i].tx_count = tx2_byte_count;
	   interrupt_count += 1;
	  /* mace_rb_base->pc_int_mask &= ~CH2_DEVICE;  disable int */
	}
	return(e_status);
}





int
tx_pair_int(status,ust)
unsigned long long status;
unsigned long long ust;
{
	int i, j, e_status;

	e_status = 0;
	msg_printf(DBG,"tx pair int\n");

	i = interrupt_count;

	interrupts[i].ust = ust;
       	interrupts[i].mace_int_status = status;
       	interrupts[i].int_serviced = ((TX1_PAIR | TX2_PAIR) & status);
	mace_rb_base->pc_int_status &= ~(interrupts[i].int_serviced);
	/* get depth, rd pointer, buf pointer */

	if(TX1_PAIR & status){
       	   interrupts[i].MCR_reg = pSerial_1->mcr;
	   interrupts[i].MSR_reg = pSerial_1->msr;
       	   interrupts[i].control = pSer_dma_regs_1->rxCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_1->rxRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_1->rxWrite;
       	   interrupts[i].depth = pSer_dma_regs_1->rxDepth;
       	   interrupts[i].rx_count = rx1_byte_count;
       	   interrupts[i].tx_count = tx1_byte_count;
	   interrupt_count += 1;
	   i = interrupt_count;

/*
 *      force an error by writing directly to the lsr register
 */
           if(error_one != 0){
                printf("error_one = %x\n",error_one);
                pSerial_1->lsr =  0x61 | error_bits[error_one];
                error_one--;
           }

	}
	



	if(TX2_PAIR & status){
       	   interrupts[i].MCR_reg = pSerial_2->mcr;
	   interrupts[i].MSR_reg = pSerial_1->msr;
       	   interrupts[i].control = pSer_dma_regs_2->rxCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_2->rxRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_2->rxWrite;
       	   interrupts[i].depth = pSer_dma_regs_2->rxDepth;
       	   interrupts[i].rx_count = rx2_byte_count;
       	   interrupts[i].tx_count = tx2_byte_count;
	   interrupt_count += 1;

/*
 *      force errors by asserting break during transmission 
 */
	   if(error_two != 0){
		printf("error_two = %x\n",error_two);
		pSerial_2->lcr |= 0x40;
		us_delay(100);
		pSerial_2->lcr &= 0xbf;
		error_two--;
	   }


	}
	return(e_status);
}
	

void
error_int(status,ust)
unsigned long long status;
unsigned long long ust;
{
/* for error int save ust and regs for all channels */

	int i;

	i = interrupt_count;
	msg_printf(ERR,"error int\n");

	interrupts[i].ust = ust;
       	interrupts[i].mace_int_status = status;
       	interrupts[i].int_serviced = ((TX1_MEM_ERROR|TX2_MEM_ERROR|RX1_ERROR|RX2_ERROR) & status);

       	   interrupts[i].MCR_reg = pSerial_1->mcr;
	   interrupts[i].MSR_reg = pSerial_1->msr;
       	   interrupts[i].control = pSer_dma_regs_1->txCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_1->txRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_1->txWrite;
       	   interrupts[i].depth = pSer_dma_regs_1->txDepth;
       	   interrupts[i].tx_count = tx1_byte_count;
	   
	interrupt_count += 1;
	   i = interrupt_count;
	   interrupt_count += 1;
       	   interrupts[i].MCR_reg = pSerial_2->mcr;
	   interrupts[i].MSR_reg = pSerial_1->msr;
       	   interrupts[i].control = pSer_dma_regs_2->txCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_2->txRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_2->txWrite;
       	   interrupts[i].depth = pSer_dma_regs_2->txDepth;
       	   interrupts[i].tx_count = tx2_byte_count;
	   
	/* stop all channels */

	pSer_dma_regs_1->rxCont &= ~DMA_ENABLE;
	pSer_dma_regs_1->txCont &= ~DMA_ENABLE;
	pSer_dma_regs_2->rxCont &= ~DMA_ENABLE;
	pSer_dma_regs_2->txCont &= ~DMA_ENABLE;
	
	   
}

void
tx_int(status,ust)
unsigned long long status;
unsigned long long ust;
{

	int i, j, k, temp, depth, offset, ringId, ring_base;
	unsigned char *write_ptr;

	i = interrupt_count;
	msg_printf(DBG,"tx int\n");

	interrupts[i].ust = ust;
       	interrupts[i].mace_int_status = status;
       	interrupts[i].int_serviced = ((TX1_THRESHOLD | TX2_THRESHOLD) & status);
	
	/* get depth, rd pointer, buf pointer */

	if(TX1_THRESHOLD & status){
       	   interrupts[i].control = pSer_dma_regs_1->txCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_1->txRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_1->txWrite;
       	   interrupts[i].depth = pSer_dma_regs_1->txDepth;
       	   interrupts[i].rx_count = rx1_byte_count;
       	   interrupts[i].tx_count = tx1_byte_count;

	   depth = (int)(0xfe0 - (interrupts[i].depth & 0xfe0));
	   msg_printf(DBG,"depth reg = %llx\n",interrupts[i].depth);
	   msg_printf(DBG,"tx1_byte_count = %x\n",tx1_byte_count);
	   msg_printf(DBG,"tx1_len = %x\n",tx1_len);
	   msg_printf(DBG,"tx int depth = %x\n",depth);
	   if(tx1_byte_count >= (tx1_len*2)){
		depth = 0;
		mace_rb_base->pc_int_mask &= ~TX1_THRESHOLD; /* disable tx1 empty int */
		}
	   else if(depth > ((tx1_len*2) - tx1_byte_count)){
		depth = ((tx1_len*2) - tx1_byte_count);
	   msg_printf(DBG,"tx int depth = %x\n",depth);
		depth += 31;
	   msg_printf(DBG,"tx int depth = %x\n",depth);
		depth &= 0xfe0;
	   }
	   msg_printf(DBG,"tx int depth = %x\n",depth);
	   offset = (interrupts[i].write_ptr & 0xfe0);
	   msg_printf(DBG,"tx int offset = %x\n",offset);
	   ringId = SERIAL_A_TX_RING;
	   ring_base = (mace_rb_base_addr | (ringId << 12));
	   for(i=0;i<depth;i+=8){
	       k = tx1_byte_count/2;
               write_ptr = (unsigned char *)ring_base;
	       write_ptr += offset;
	   	for(j=0;j<4;j++){
		   write_ptr[j+4] = tx1_data_buf[k+j];
		   write_ptr[j] = tx1_cntrl_buf[k+j];
		}
	
		offset += 8;
		offset &= 0xff8;
		tx1_byte_count +=8;
	   }
	   offset += 31;
	   offset &= 0xfe0;	/* force offset to 0x20 alignment */
	   pSer_dma_regs_1->txWrite = (unsigned long long)offset;

	   i = interrupt_count;
       	   interrupts[i].MCR_reg = pSerial_1->mcr; /* put this here for syncing */

	   interrupt_count += 1;
	   i = interrupt_count;
	}

	if(TX2_THRESHOLD & status){
       	   interrupts[i].control = pSer_dma_regs_2->txCont;
       	   interrupts[i].read_ptr = pSer_dma_regs_2->txRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_2->txWrite;
       	   interrupts[i].depth = pSer_dma_regs_2->txDepth;
       	   interrupts[i].rx_count = rx2_byte_count;
       	   interrupts[i].tx_count = tx2_byte_count;

	   depth = (int)(0xfe0 - (interrupts[i].depth & 0xfe0));
	   msg_printf(DBG,"depth reg = %llx\n",interrupts[i].depth);
	   msg_printf(DBG,"tx2_byte_count = %x\n",tx2_byte_count);
	   msg_printf(DBG,"tx2_len = %x\n",tx2_len);
	   msg_printf(DBG,"tx int depth = %x\n",depth);
	   if(tx2_byte_count > (tx2_len*2)){
		depth = 0;
		mace_rb_base->pc_int_mask &= ~TX2_THRESHOLD; /* disable tx2 empty int */
		}
	   else if(depth > ((tx2_len*2) - tx2_byte_count)){
		depth = (tx2_len*2) - tx2_byte_count;
	   msg_printf(DBG,"tx2 int depth = %x\n",depth);
		depth += 31;
	   msg_printf(DBG,"tx2 int depth = %x\n",depth);
		depth &= 0xfe0;
	   msg_printf(DBG,"tx2 int depth = %x\n",depth);
	   }
	   offset = (interrupts[i].write_ptr & 0xfe0);
	   msg_printf(DBG,"tx int offset = %x\n",offset);
	   ringId = SERIAL_B_TX_RING;
	   ring_base = (mace_rb_base_addr | (ringId << 12));
	   for(i=0;i<depth;i+=8){
	       k = tx2_byte_count/2;
               write_ptr = (unsigned char *)ring_base;
	       write_ptr += offset;
	   	for(j=0;j<4;j++){
		   write_ptr[j+4] = tx2_data_buf[k+j];
		   write_ptr[j] = tx2_cntrl_buf[k+j];
		}
	
		offset += 8;
		offset &= 0xff8;
		tx2_byte_count +=8;
	   }
	   offset += 31;
	   offset &= 0xfe0;	/* force offset to 0x20 alignment */
	   pSer_dma_regs_2->txWrite = (unsigned long long)offset; 

	   i = interrupt_count;
       	   interrupts[i].MCR_reg = pSerial_2->mcr; /* put this here for syncing */

	   interrupt_count += 1;
	}
}
void
rx_int(status,ust)
unsigned long long status;
unsigned long long ust;
{

	int i, j, k, depth, offset, ringId;
	unsigned char *read_ptr;

	i = interrupt_count;
	msg_printf(DBG,"rx int\n");

	interrupts[i].ust = ust;
       	interrupts[i].mace_int_status = status;
       	interrupts[i].int_serviced = ((RX1_THRESHOLD | RX2_THRESHOLD) & status);
	
	/* get depth, rd pointer, buf pointer */

	if(RX1_THRESHOLD & status){
       	   interrupts[i].MCR_reg = pSerial_1->mcr;
	   interrupts[i].MSR_reg = pSerial_1->msr;
       	   interrupts[i].control = pSer_dma_regs_1->rxCont;
       	   interrupts[i].depth = pSer_dma_regs_1->rxDepth;
       	   interrupts[i].read_ptr =pSer_dma_regs_1->rxRead; 
       	   interrupts[i].write_ptr = pSer_dma_regs_1->rxWrite;
       	   interrupts[i].rx_count = rx1_byte_count;
       	   interrupts[i].tx_count = tx1_byte_count;

	   depth = (interrupts[i].depth & 0xfe0);
	   offset = (interrupts[i].read_ptr & 0xfe0);

	   ringId = SERIAL_A_RX_RING;
	   k = rx1_byte_count/2;
	   for(i=0;i<depth;i+=8){
               read_ptr = (unsigned char *)(mace_rb_base_addr | (ringId << 12));
	       read_ptr += offset;
	   	for(j=0;j<4;j++){
		   rx1_data_buf[k+j] = read_ptr[j+4]; 
		   rx1_status_buf[k+j] = read_ptr[j]; 
		}
		k += 4;
		offset += 8;
		offset &= 0xff8;
	   }
	   pSer_dma_regs_1->rxRead = (unsigned long long)offset;	   
	   rx1_byte_count += depth;
	   interrupt_count += 1;
	   i = interrupt_count;
	   j = (int)pSer_dma_regs_1->rxRead; /* to make sure int is cleard before exit */
	}

	if(RX2_THRESHOLD & status){
       	   interrupts[i].MCR_reg = pSerial_2->mcr;
	   interrupts[i].MSR_reg = pSerial_1->msr;
       	   interrupts[i].control = pSer_dma_regs_2->rxCont;
       	   interrupts[i].depth = pSer_dma_regs_2->rxDepth;
       	   interrupts[i].read_ptr = pSer_dma_regs_2->rxRead;
       	   interrupts[i].write_ptr = pSer_dma_regs_2->rxWrite;
       	   interrupts[i].rx_count = rx2_byte_count;
       	   interrupts[i].tx_count = tx2_byte_count;

	   depth = (interrupts[i].depth & 0xfe0);
	   offset = (interrupts[i].read_ptr & 0xfe0);

	   ringId = SERIAL_B_RX_RING;
	   k = rx2_byte_count/2;
	   for(i=0;i<depth;i+=8){
               read_ptr = (unsigned char *)(mace_rb_base_addr | (ringId << 12));
	       read_ptr += offset;
	   	for(j=0;j<4;j++){
		   rx2_data_buf[k+j] = read_ptr[j+4]; 
		   rx2_status_buf[k+j] = read_ptr[j]; 
		}
		k += 4;
		offset += 8;
		offset &= 0xff8;
	   }
		   
	   pSer_dma_regs_2->rxRead = (unsigned long long)offset;	   
	   rx2_byte_count += depth;
	   interrupt_count += 1;
	   j = (int)pSer_dma_regs_2->rxRead; /* to make sure int is cleard before exit */
	}


}




		/* load tx data and control buffers */
void
Setup_tx_data(channels)
int channels;
{

	int i, j, k;

	j = 0;

	if (channels & SERIAL_1){
	 while(j < tx1_len){
	   	for(i=0;i<16;i++){
		   tx1_data_buf[j+i] = (unsigned char)(j+i);
		   tx1_cntrl_buf[j+i] = TX_CNTRL_VALID; 
		}
		j += 16;
	 }
	}

	j = 0;

	if (channels & SERIAL_2){
	 while(j < tx2_len){
	   	for(i=0;i<16;i++){
		   tx2_data_buf[j+i] = (unsigned char)~(j+i);
		   tx2_cntrl_buf[j+i] = TX_CNTRL_VALID; 
		}
	   	j += 16;
	 }
	}
}


		/* modify the tx data and control buffers */


void
Modify_tx_buffs(cntrl,data,start,skip,channels)
unsigned char cntrl;
unsigned char data;
int start;
int skip;
int channels;
{

	int i, j ,k;

	if(SERIAL_1 & channels){

	   if(skip==0){
		i = start;
	   	tx1_data_buf[i] = data;
	   	tx1_cntrl_buf[i] = cntrl;
	   }
	   else{
	   	j = sizeof tx1_data_buf;
	   	j -= skip;
	   	for(i=start;i<j;i+=skip){
	   	   tx1_data_buf[i] = data;
	   	   tx1_cntrl_buf[i] = cntrl;
		}
	   }
	}
	if(SERIAL_2 & channels){

	   if(skip==0){
		i = start;
	   	tx2_data_buf[i] = data;
	   	tx2_cntrl_buf[i] = cntrl;
	   }
	   else{
	   	j = sizeof tx2_data_buf;
	   	j -= skip;
	   	for(i=start;i<j;i+=skip){
	   	   tx2_data_buf[i] = data;
	   	   tx2_cntrl_buf[i] = cntrl;
		}
	   }
	}
}





void
Init_DMA_Channel(pSerDmaReg, pAceRegs)
volatile SERIAL_DMA_RING_REG_DESCRIPTOR *pSerDmaReg;
unsigned char * pAceRegs;
{

	int ringId;

	pSerDmaReg->rxCont =  DMA_RESET;
	pSerDmaReg->txCont =  DMA_RESET;

	InitAceRegs(pAceRegs);

	pSerDmaReg->rxCont =  DMA_CHAN_ACTIVE;
	pSerDmaReg->rxWrite = 0x0;
	pSerDmaReg->rxRead = 0x0;

	pSerDmaReg->txCont =  DMA_CHAN_ACTIVE;
	pSerDmaReg->txWrite = 0x0;
	pSerDmaReg->txRead = 0x0;

	msg_printf(DBG,"\nrx ring write: %x",
	    (unsigned int)pSerDmaReg->rxWrite );

	msg_printf(DBG,"\nrx ring read: %x",(unsigned int)pSerDmaReg->rxRead );

	rx1_byte_count = 0;
	tx1_byte_count = 0;
	rx2_byte_count = 0;
	tx2_byte_count = 0;
	interrupt_count = 0;

	ringId = 4;
	msg_printf(DBG,"pAceRegs = %x, SAR = %x\n",pAceRegs,SERIAL_A_ADDRS);
	if((int)SERIAL_A_ADDRS == (int)pAceRegs)
	   ringId = SERIAL_A_TX_RING;
	if((int)SERIAL_B_ADDRS == (int)pAceRegs)
	   ringId = SERIAL_B_TX_RING;

	ClearRingBuffs(ringId);

	ringId++;
	ClearRingBuffs(ringId);


}


/**** could add args for each register value also *******/
void
InitAceRegs(pSerIntfReg)
unsigned char *pSerIntfReg;
{

    unsigned char *pRegRBR_THR,*pRegIER,*pRegIIR_FCR,*pRegLCR,*pRegMCR,
		  *pRegLSR,*pRegMSR,*pRegSCR,*pRegDLL,*pRegDLM;


    /*
     * Enable the 16550 fifos and CTS
     */

    pRegRBR_THR = pSerIntfReg + (REG_DATA_IN_OUT * REG_INT_VIEW_OFFSET) + 7;
    pRegIER =  pSerIntfReg + ( (REG_IER * REG_INT_VIEW_OFFSET) + 7);
    pRegIIR_FCR = pSerIntfReg + ( (REG_FCR * REG_INT_VIEW_OFFSET) + 7);
    pRegLCR = pSerIntfReg +  (REG_LCR * REG_INT_VIEW_OFFSET) + 7;
    pRegMCR = pSerIntfReg +  (REG_MCR * REG_INT_VIEW_OFFSET) + 7;
    pRegLSR = pSerIntfReg +  (REG_LSR * REG_INT_VIEW_OFFSET) + 7;
    pRegMSR = pSerIntfReg +  (REG_MSR * REG_INT_VIEW_OFFSET) + 7;
    pRegSCR = pSerIntfReg +  (REG_SCR * REG_INT_VIEW_OFFSET) + 7;
    pRegDLL = pSerIntfReg + (REG_DATA_IN_OUT * REG_INT_VIEW_OFFSET) + 7;
    pRegDLM = pSerIntfReg + (REG_IER * REG_INT_VIEW_OFFSET) + 7;


    *pRegLCR = UART_LCR_DLAB;

    msg_printf(DBG,"\nLCR reg: %x, Val: %x\n", pRegLCR, *pRegLCR);

    *pRegSCR = 0x6;	/* prescale of 3 for 7.33Mhz clock */
    switch(baud_rate){

	case 230:
		*pRegDLL = 0x2;    /* speed 230K */
		*pRegDLM = 0;
		break;
	case 460:
		*pRegDLL = 0x1;    /* speed 460K */
		*pRegDLM = 0;
		break;
	case 115:
	default:
		*pRegDLL = 0x4;    /* speed 115K */
		*pRegDLM = 0;
		break;

    }

    msg_printf(DBG,"DLL = %x, DLM = %x\n",*pRegDLL,*pRegDLM);
    *pRegLCR = 0;      /*dlab=0*/

	msg_printf(DBG,"LCR addr = %x\n",pRegLCR);
    *pRegLCR = UART_LCR_WLS0 | UART_LCR_WLS1 | UART_LCR_PEN; /* 8bit, odd parity */

	msg_printf(DBG,"FCR addr = %x\n",pRegIIR_FCR);
    *pRegIIR_FCR= UART_FCR_FIFO_ENABLE |UART_FCR_DMA_MODE \
		 |UART_FCR_RCV_TRIG_LSB |UART_FCR_RCV_TRIG_MSB; 

	msg_printf(DBG,"IER addr = %x\n",pRegIER);
    *pRegIER = UART_IER_ERLSI | UART_IER_EMSI;

	msg_printf(DBG,"MCR addr = %x\n",pRegMCR);
    if (internal_loop == 0)
	*pRegMCR = UART_MCR_RTS | UART_MCR_AFCE;
    else
	*pRegMCR = UART_MCR_RTS | UART_MCR_LOOP;


    msg_printf(DBG,"\nIER reg: %x, Val: %x", pRegIER, *pRegIER);
    msg_printf(DBG,"\nIIR reg: %x, Val: %x", pRegIIR_FCR, *pRegIIR_FCR);
    msg_printf(DBG,"\nLCR reg: %x, Val: %x", pRegLCR, *pRegLCR);
    msg_printf(DBG,"\nMCR reg: %x, Val: %x", pRegMCR, *pRegMCR);
    msg_printf(DBG,"\nLSR reg: %x, Val: %x", pRegLSR, *pRegLSR);
    msg_printf(DBG,"\nMSR reg: %x, Val: %x", pRegMSR, *pRegMSR);
    msg_printf(DBG,"\nSCR reg: %x, Val: %x", pRegSCR, *pRegSCR);


    return;

}


/*
 *This function zeros the ring buffers
 */

void
ClearRingBuffs(ringId)
int ringId;
{
    unsigned int ringAddr, ringBase, i;
    long *pRingAddr;


    /*
     *zero out the ring buffs, start with channel 1 tx and walk thru all 4
     */

/************************************************
    ringBase = mace_rb_base_addr;

    ringAddr = ringBase | (ringId << 12);
    pRingAddr = (long *) ringAddr;
**************************************************/
    pRingAddr = (long *)(mace_rb_base_addr | (ringId << 12));
    msg_printf(DBG,"pRingAddr = %x\n",pRingAddr);

    for (i = 0; i < DMA_RING_SIZE/4 ; i++) 
	    *pRingAddr++ = (long)0;

}

void
Clear_tx_buffs()
{
	int i;
	size_t j;

	j = sizeof tx1_data_buf;

	for (i=0;i<j;i++)
	   tx1_data_buf[i] = 0;

	j = sizeof tx1_cntrl_buf;

	for (i=0;i<j;i++)
	   tx1_cntrl_buf[i] = 0;

	j = sizeof tx2_data_buf;

	for (i=0;i<j;i++)
	   tx2_data_buf[i] = 0;

	j = sizeof tx2_cntrl_buf;

	for (i=0;i<j;i++)
	   tx2_cntrl_buf[i] = 0;

}
void
Clear_rx_buffs()
{
	int i;
	size_t j;

	j = sizeof rx1_data_buf;

	for (i=0;i<j;i++)
	   rx1_data_buf[i] = 0;

	j = sizeof rx1_status_buf;

	for (i=0;i<j;i++)
	   rx1_status_buf[i] = 0;

	j = sizeof rx2_data_buf;

	for (i=0;i<j;i++)
	   rx2_data_buf[i] = 0;

	j = sizeof rx2_status_buf;

	for (i=0;i<j;i++)
	   rx2_status_buf[i] = 0;

	j = 1024;

	for (i=0;i<j;i++){
       	   interrupts[i].mace_int_status = 0;
       	   interrupts[i].int_serviced = 0;
       	   interrupts[i].control = 0;
       	   interrupts[i].read_ptr = 0;
       	   interrupts[i].write_ptr = 0;
       	   interrupts[i].depth = 0;
       	   interrupts[i].ust = 0;
       	   interrupts[i].rx_count = 0;
       	   interrupts[i].tx_count = 0;
       	   interrupts[i].MCR_reg = 0;
       	   interrupts[i].IIR_reg = 0;
       	   interrupts[i].MSR_reg = 0;
	}


}


/*
 *This function calculates the location of the DMA ring ptr based on the 
 *parameters
 */

unsigned int
getRingAddress(pRingBase, ringId, offset)
unsigned int  *pRingBase;
unsigned int ringId;
unsigned int offset;
{

    unsigned int ringAddr, ringBase;
    ringBase = (unsigned int)pRingBase;
    ringAddr = ringBase | (ringId << 12) | (offset << 5);
    msg_printf(DBG,"\nIn func ringAddr: %x", ringAddr); 
    return(ringAddr);

}


void
setup_mace_ringbuf(void)
{
  register int fd;
  static int pgsize;


  /* Create 8 page aligned pages of new address space */
  /* setup mace dma address */
  pa = getTile();
  if (0 == pa)
	msg_printf(ERR,"ERROR: can't allocate tile for dma area");

  mace_rb_base->ring_base = (unsigned long)(K1_TO_PHYS(pa));

  mace_rb_base_addr = (unsigned int)pa;
}

