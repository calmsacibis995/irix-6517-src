
#include	"stdio.h"
#include 	"sys/cpu.h"
#include 	"sys/param.h"
#include	"sys/sbd.h"
#include	"uif.h"

#define		SLOT0_ADDR	0x1f400000 /* actually 0x1f4xxxxx */
#define		SLOT1_ADDR	0x1f500000 /* actually 0x1f5xxxxx */

#define		GIO_INT0	LIO_FIFO /* 0x01 in local interrupt 0 */
#define		GIO_INT1	LIO_GE   /* 0x40 in local interrupt 0 */
#define		GIO_INT2	LIO_VR   /* 0x80 in local interrupt 1 */

#define		dbgprintf	if(0)printf

#define		WAIT_COUNT	1000

static volatile unsigned char *slot_test_addr[] = {
		(unsigned char*)PHYS_TO_K1(SLOT0_ADDR),
		(unsigned char*)PHYS_TO_K1(SLOT1_ADDR)
	};

/*
	
	Writing onto addresses 1f4xxxxx makes GIO interrupts on slot0 active.
	Reading addresses 1f4xxxxx makes GIO interrupts on slot0 inactive.

	Writing onto addresses 1f5xxxxx makes GIO interrupts on slot1 active.
	Reading addresses 1f5xxxxx makes GIO interrupts on slot1 inactive.
*/

gio_interrupt_test(argc,argv)
int argc;
char *argv[] ;
{

	
	int slot_no ;
	int err_count0 = 0;
	int err_count1 = 0;

	if ( argc < 2 ){
		printf("Usage:- %s <slot number>\n",argv[0]);
		printf("slot number : 0 - slot 0, 1 - slot 1, 2 - both\n");
		return(-1);
	}

	if(atoi(argv[1]) == 1 ) slot_no = 1 ;
	else if(atoi(argv[1]) == 2 ) slot_no = 2 ;
	else slot_no = 0;


	msg_printf(DBG,"------GIO BUS INTERRUPT TEST------------\n");
	msg_printf(DBG,"Before starting\n");

	switch(slot_no){
	 case 0: err_count0 = gio_int_test(0); break ;
	 case 1: err_count1 = gio_int_test(1); break ;
	 case 2: err_count0 = gio_int_test(0);
	  	 	 err_count1 = gio_int_test(1);break ;
	 default: break ;
	}
	
	if(err_count0)
		sum_error("GIO Interrupt Logic - Slot 0");
	if(err_count1)
		sum_error("GIO Interrupt Logic - Slot 1");

	msg_printf(DBG,"------END OF GIO BUS INTERRUPT TEST------------\n");
	return (err_count0+err_count1) ;
}

gio_int_test(slot_no)
int slot_no ;
{
	volatile unsigned char *listat0_addr=(unsigned char*)PHYS_TO_K1(LIO_0_ISR_ADDR);
	volatile unsigned char *listat1_addr=(unsigned char*)PHYS_TO_K1(LIO_1_ISR_ADDR);

	unsigned char  tmp ;
	int err = 0;
	int i,j   ;
	int int0_occurred_ok ,int1_occurred_ok,int0_not_occurred_ok,int1_not_occurred_ok ;
	char tmp1;

	/* clear the interrupts to start with - needs to be done*/


	/* The following write should activate the interrupts */
	*slot_test_addr[slot_no] = 0 ; 

	DELAY(1000);

	if ( (*listat0_addr & (GIO_INT0 | GIO_INT1 )) !=
		(GIO_INT0|GIO_INT1)){
		int0_occurred_ok = 0;
		err++;
	}
	else
		int0_occurred_ok = 1;

	/* The GIO int 2 check cannot be done for EXPRESS since reading from the
	   board does not disable this interrupt as gio int2 is ORed with Vertical Retrace
	   (and Vertical Retrace causes the interrupt and hence it cannot be 
	   checked whether gio int2  is disabled)
	*/
	/* The following read should disable the interrupts */
	tmp =  *slot_test_addr[slot_no] ;	/* read */
	DELAY(20000);

	if ( (*listat0_addr & GIO_INT0 ) != 0 ){
		int0_not_occurred_ok = 0;
		err++;
	}
	else
		int0_not_occurred_ok = 1;

	if(!int0_occurred_ok)
			msg_printf(DBG,"FAIL: int0 and/or int1 interrupt not active\n");
	else
			msg_printf(DBG,"PASS: int0 & int1 interrupts are active\n");


	if(!int0_not_occurred_ok)
			msg_printf(DBG,"FAIL: int0 and/or int1 interrupt still active\n");
	else
			msg_printf(DBG,"PASS: int0 & int1 interrupts are inactive\n");

	return err;

}

