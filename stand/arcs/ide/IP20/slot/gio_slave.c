#include "stdio.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "uif.h"

#define MS1 	1000  /* 1 micro second */

#define NBPC		4096
#define NWPC		1024
#define MAXWORDS	128
#define WORDS_1PG	8

static unsigned int pattern[] = {

				0xffffffff, ~0xffffffff,
				0xabcdef10, ~0xabcdef10,
				0x12345678, ~0x12345678,
				0xf0f0f0f0, ~0xf0f0f0f0,
				0x01100110, ~0x01100110,
				0x11111111, ~0x11111111,
				0xaaaaaaaa, ~0xaaaaaaaa,
				0x55555555, ~0x55555555,
				0x77777777, ~0x77777777,
				0x10101010, ~0x10101010,
				0x87654321, ~0x87654321,
				0xfedcba98, ~0xfedcba98,
				0x98abcdef, ~0x98abcdef,
			};

#define		TOTAL_PATTERN		(sizeof(pattern)/sizeof(unsigned int))

/*
** 	Initiates a 3 way transfer and reads it back(single read) and
**  compares the data.Calls trigger_3way() for 3way transfer.
*/

int
gio_slave_logic_test( argc,argv)
int argc ;
char * argv [] ;
{
    int error_count = 0;
	int slot_no ;

	if (argc == 1) {
	    printf("Usage: %s <slot number> [words] [times] [delay]\n", 
			argv[0]);
		printf("slot number : 0 - slot 0, 1 - slot 1, 2 - both\n");
	    return(-1);
	}

   	msg_printf (VRB, "\n-----GIO SLAVE LOGIC TEST-----\n");

	if ( atoi(argv[1]) == 1 ) slot_no = 1;
	else if ( atoi(argv[1]) == 2 ) slot_no = 2;
	else slot_no = 0 ;


	switch(slot_no){
	 case 0: error_count = slot_3way_xfer(0,argc,argv);break ;
	 case 1: error_count = slot_3way_xfer(1,argc,argv); break ;
	 case 2: error_count = slot_3way_xfer(0,argc,argv)+
							slot_3way_xfer(1,argc,argv); break;
	 default: break ;
	}

	if(error_count)
		sum_error("Gio Slave Logic ");
	else okydoky();

   	msg_printf (VRB, "\n-----END OF GIO SLAVE LOGIC TEST-----\n");
	return error_count;
}


int
slot_3way_xfer(slot_no,argc,argv)
int slot_no,argc;
char *argv[];
{
	int nwords;
	unsigned long *wbuf, *wpat, *start_ptr;
	int rflag, ntimes;
	int i;
	unsigned char j;
	unsigned long slot_addr;
	unsigned long start_addr, end_addr;
	unsigned long dest_addr;
	int error_count = 0;
	unsigned int *rpat_addr;

	int l,n_count = 0;
	int l_err_count ;
	int err_occurances[1000] ;
	int delay ,l_delay ;


   	msg_printf (DBG, "Three way transfer test\n");

/*

	if(!(*(uint *)(PHYS_TO_K1(CPU_CONFIG)) & CONFIG_GR2)) {
		msg_printf (DBG, "No three way transfer test for GR1 mode\n");
                okydoky ();
		return (error_count);
	}
 
*/


 	slot_addr = 0x1f700000 ;  /* slot 0 , by default */
	 
	switch(slot_no){

	   default :
	   case 0 : slot_addr = 0x1f700000 ; break ;
	   case 1 : slot_addr = 0x1f800000 ; break ;

	}

	/* Always convert to physical address */
	slot_addr &= 0x1fffffff;

	nwords = 127  ; /* default values */
	rflag = 1;
	ntimes = 1;
	delay = 1;

	switch(argc) {
		case 3:
	    		atob(argv[2], &nwords);
			break;
		case 4:
	    		atob(argv[2], &nwords);
	    		atob(argv[3], &ntimes);
			break;
		case 5:
	    		atob(argv[2], &nwords);
	    		atob(argv[3], &ntimes);
	    		atob(argv[4], &delay);
			break;
		default:
			break;
	}

	if (nwords > MAXWORDS || nwords <= 0 )	
			nwords = MAXWORDS; 
			/* it constantly fails for nwords == MAXWORDS(128) */

	/* allocate buffer which crosses pages */
	wbuf = (unsigned long *) align_malloc(NBPC+nwords*sizeof(long), NBPC);

	if ( wbuf == NULL ) {
		printf("Error in allocating memory for wbuf\n");
		return(-1);
	}

	/* physical address */
	dest_addr = TW_ARM | slot_addr; 

	/* Set up the 3waySUBS and 3wayMASK */

     *(volatile unsigned int *)PHYS_TO_K1(TW_MASK_ADDR) = 0;
     *(volatile unsigned int *)PHYS_TO_K1(TW_SUBS_ADDR) = 0;

   	 wpat = wbuf;

	/* skipping the buffer.  We have 8 words in one page, the others
	   in the next page */
    	for (i=0; i<(NWPC - WORDS_1PG); i++)
		*wpat++ = 0xffffffff;

    	start_ptr = wpat;

	/*
	for (i=0; i<nwords; i++) {
	    j = (i & 0xff);
	    *wpat++ = (j << 24) | (j << 16) | (j << 8) | j;
	} 
	*/

	for (i=0; i<nwords; i++ ) 
		*wpat++ = pattern[i%TOTAL_PATTERN] ;

    start_addr = (unsigned long) start_ptr;
	end_addr = (unsigned long) (wpat-1); 

	msg_printf(DBG,"start addr = %#x end addr = %#x words = %d\n",
		start_addr, end_addr, (end_addr-start_addr+sizeof(unsigned long))/sizeof(unsigned long));

	for(l=0 ; l < ntimes ; l++ ){

		l_err_count = 0 ;
	    /* Start the 3way transfer */
	    trigger_3way(dest_addr, start_addr, end_addr);

	    if (rflag) {
	    /* Check the results */
			/*
			msg_printf(DBG,"Checking the results after transfer\n");
			*/
	        rpat_addr = (unsigned int *)PHYS_TO_K1(slot_addr);

		wpat = start_ptr;
	        for (i=0; i<nwords; i++) {
	            if (*rpat_addr != *wpat) {
		        l_err_count++;
		        error_count++;
		        msg_printf(DBG,
		   "Expected: 0x%08x, Actual: 0x%08x offset = %d loop no= %d\n",
		           *wpat, *rpat_addr,i,l);
	            }
	    	    rpat_addr++;
		    wpat++;
	        }

			if ( l_err_count && n_count < 1000 ) 
				err_occurances[n_count++] = l;
	    }

	   l_delay = delay ;
       while (l_delay --)
	      us_delay (MS1);
	}
	
	align_free(wbuf);

	if( n_count ) 
		msg_printf(DBG,"Failed in the following occurances:\n");
	for(l=1 ; l <= n_count ; l ++)
		msg_printf(DBG,"%8d %s",err_occurances[l-1], (l%8) ? " ":"\n");
	msg_printf(DBG,"\n");

    return (error_count);
}
