
#include	"stdio.h"
#include	"sys/sbd.h"
#include	"uif.h"

#define	PBUS_LOGIC_SLOT0_ADDR	0x1fbc0800 
#define	PBUS_LOGIC_SLOT1_ADDR	0x1fbc0840 

static volatile unsigned int  *slot_addrs[] = { 
		(unsigned int*)PHYS_TO_K1(PBUS_LOGIC_SLOT0_ADDR),
		(unsigned int*)PHYS_TO_K1(PBUS_LOGIC_SLOT1_ADDR)

		};

pbus_logic_test(argc,argv)
int argc ;
char *argv[];
{

	int error_count = 0;
	int slot_no;

	if ( argc < 2 ){
		printf("Usage:- %s <slot number>\n",argv[0]);
		printf("slot number : 0 - slot 0, 1 - slot 1, 2 - both\n");
		return(-1);
	}

	if(atoi(argv[1]) == 1 ) slot_no = 1 ;
	else if(atoi(argv[1]) == 2 ) slot_no = 2 ;
	else slot_no = 0;

	msg_printf(VRB,"\n------PBUS LOGIC TEST------------\n");

	switch(slot_no){
	 case 0: error_count = pbus_test(0); break ;
	 case 1: error_count = pbus_test(1); break ;
	 case 2: error_count = pbus_test(0)+ pbus_test(1);break ;
	 default: break ;
	}

	if(error_count)
		sum_error("Pbus Logic");
	else okydoky();

	msg_printf(VRB,"\n------END OF PBUS LOGIC TEST------------\n");
	return error_count;
}

pbus_test(slotno)
int slotno ;
{

	volatile unsigned int  * tmp ; /* should be an int, not a char */
	int err=0;
	unsigned int i;

	msg_printf(DBG,"Cheking read/write for slot %d\n",slotno);

	tmp = slot_addrs[slotno];

	/* write to those addresses */
	/* increment addresses in the range a6-a2, so tmp ++( 4 bytes increment)*/
	for(i = 0 ; i < 16 ; i++ , tmp ++   )  {
			*tmp = (unsigned int)((i * 0x1234 ) & 0xff ); /*assign a byte only*/
	}

	/* read from  those addresses and compare */
	tmp = slot_addrs[slotno];

	for(i = 0 ; i < 16 ; i++ , tmp ++  ) 
			if ( ( *tmp & 0xff) != ((i*0x1234) & 0xff) ) {
				err++ ;
				msg_printf(DBG,"unmatched : value read at %#x->%#x ,should be %#x\n",tmp,(*tmp & 0xff),(i*0x1234)&0xff );
			}

	if ( err) 
		msg_printf(DBG,"FAIL: %d values did not match\n",err);
	else 
		msg_printf(DBG,"PASS: all  values  matched\n");
	msg_printf(DBG,"\n------END OF PBUS LOGIC TEST------------\n");

	return err;
}	
