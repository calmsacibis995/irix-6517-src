#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "saioctl.h"
#include "uif.h"
#include "fault.h"
#include "sys/IP20.h"

#include "slot_hpc_dma.h"

/* 
 * scsi_lpbk - SCSI loopback test 
 *
 * This program first writes 64 bytes data to the scsi fifo in hpc and 
 * read back to check if they match.
 */

volatile unsigned int  *scsi_control ; 


#define	Dprintf	if(slot_debug)printf

int slot_debug = 0 ;

static char * hpc_names [] = {
		"Primary hpc",
		"Unused hpc",
		"Slot 0 hpc",
		"Slot 1 hpc",
	};

static unsigned char *read_cbp = 0;
static unsigned char *write_cbp = 0;
static struct md *read_mem_desc = 0;
static struct md *write_mem_desc = 0;

static unsigned int hpc_base ;

gio_master_logic_test(argc,argv)
int argc ;
char *argv[];
{
	extern unsigned int _gio64_arb;
	int error_count0 = 0;
	int error_count1 = 0;
	unsigned int i;
	int slot_no ;
	int value1,value2;

	if ( argc < 2 ){
		printf("Usage:- %s <slot number>\n",argv[0]);
		printf("slot number : 0 - slot 0, 1 - slot 1, 2 - both\n");
		return(-1);
	}

	if(atoi(argv[1]) == 1 ) slot_no = 1 ;
	else if(atoi(argv[1]) == 2 ) slot_no = 2 ;
	else slot_no = 0;
        /* Set up CPU Control Register 1 and GIO64 CPU Arbitration */

	value1 = _gio64_arb;			/* cant always read arb reg */
	*(volatile unsigned int *)(PHYS_TO_K1(CPUCTRL1)) = value1 |
	CPUCTRL1_HPC_FX | CPUCTRL1_EXP0_FX | CPUCTRL1_EXP1_FX;

	value2 = *((volatile unsigned int *)(PHYS_TO_K1(GIO64_ARB)));
	*(volatile unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = (value2 |
	GIO64_ARB_EXP1_RT | GIO64_ARB_EXP0_MST | GIO64_ARB_EXP1_MST)
	& ~GIO64_ARB_EXP0_SIZE_64 & ~GIO64_ARB_EXP1_SIZE_64 &
	~GIO64_ARB_HPC_SIZE_64;



	msg_printf(DBG,"\n------GIO MASTER LOGIC TEST------------\n");

	switch(slot_no){
	 case 0: error_count0 = scsi_dma_test(0); break ;
	 case 1: error_count1 = scsi_dma_test(1); break ;
	 case 2: error_count0 =scsi_dma_test(0);
	  	 error_count1 =scsi_dma_test(1);break ;
	 default: break ;
	}

	if(error_count0)
		sum_error("Gio Master Logic - Slot 0");
	if(error_count1)
		sum_error("Gio Master Logic - Slot 1");

        /* restore old values */
	*((volatile unsigned int *)(PHYS_TO_K1(CPUCTRL1))) = value1;
	*((volatile unsigned int *)(PHYS_TO_K1(GIO64_ARB))) = value2;


	msg_printf(DBG,"\n------END OF GIO MASTER LOGIC TEST------------\n");
	return (error_count0 + error_count1);
}

int 
scsi_dma_test(slot_no)
int slot_no;
{
	int mismatch_count = 0;
    int bcount;
	int bytes ;
	unsigned char tmp[64];	
	unsigned char ch ;
	int hpc ;

	switch(hpc = slot_no+2){
	  case 0 : hpc_base = 0x1fb80000 ; break ;
	  case 1 : hpc_base = 0x1fb00000 ; break ;
	  case 2 : hpc_base = 0x1f980000 ; break ; /* slot 0 */
	  case 3 : hpc_base = 0x1f900000 ; break ; /* slot 1 */
	}

	scsi_control=(volatile unsigned int*) 
			PHYS_TO_K1(hpc_base+SCSI_CTRL_OFFSET);


    msg_printf (DBG,"HPC-SCSI loopback test for %s\n",hpc_names[hpc]);
    msg_printf (DBG,"hpc_base = %#x\n",hpc_base);

    /* reset scsi  logic on HPC on slot
     */

    /* get buffers and memory descriptor that lie within a page
     */
    read_cbp = (unsigned char *) align_malloc (DATASIZE+1, 0x1000);
    write_cbp = (unsigned char *) align_malloc (DATASIZE+1, 0x1000);


    if (read_cbp == NULL || write_cbp == NULL ) {
		msg_printf (VRB, "Failed to allocate output data buffer\n");
		return (-1);
    }

    read_mem_desc = (struct md *) align_malloc (sizeof(struct md), 0x1000);
    write_mem_desc = (struct md *) align_malloc (sizeof(struct md), 0x1000);

    if (read_mem_desc == NULL || write_mem_desc == NULL ) {
		align_free (read_cbp);
		align_free (write_cbp);
		msg_printf (VRB, "Failed to allocate memory descriptor\n");
		return (-1);
    }


	Dprintf("Addr of read_cbp = %#x\nAddr of write_cbp = %#x\n,Addr of read_mem_desc = %#x\n Addr of write_mem_desc = %#x\n",
	read_cbp,write_cbp,read_mem_desc,write_mem_desc);

    for ( bcount = 0; bcount < DATASIZE ;  bcount++) {
		 write_cbp[bcount] = (unsigned char ) bcount  + '0' ; 
		 read_cbp[bcount] = '*' ; 
	}	
	read_cbp[DATASIZE] = 0 ; /* make convenient for printing */
	write_cbp[DATASIZE] = 0 ;

    /* do write first, then read */

	dma_mem_to_scsi ();
	scsi_dma_wait ();

	
	chk_single_read_scsi_fifo();
	

	print_info(0);

    msg_printf(DBG, "Writing data: Finished data writing\n");

	msg_printf(DBG, "Reading Data: About to start dma\n");

	dma_scsi_to_mem();
	scsi_dma_wait (); 

	print_info(1);


	/* Compare data */


	msg_printf(DBG,"Comparing read buf with write buf \n");

	for (bcount=0;	bcount < DATASIZE ;  bcount++) {
		if (read_cbp[bcount] != write_cbp[bcount]) {
			Dprintf("mismatch at offset %d read -> %#x expected  -> %#x\n",
			 bcount,read_cbp[bcount], write_cbp[bcount] );
		   mismatch_count ++ ;
		} /* if of mismatched data */

	 } /* for */ 


    align_free (read_cbp);
    align_free (write_cbp);
    align_free (read_mem_desc);
    align_free (write_mem_desc);

    if (mismatch_count) 
		msg_printf(DBG,"FAIL:%d mismatch%s\n",mismatch_count,
					mismatch_count > 1 ? "es" :"");
    else  
		msg_printf(DBG,"PASS:all data matched\n");
	
	return mismatch_count ;
} 



dma_mem_to_scsi()
{

    /* initialise the memory descriptor 
     */
    write_mem_desc->bc =  DATASIZE ;

    write_mem_desc->cbp = 
		 (unsigned int)(0x80000000 | KDM_TO_PHYS((unsigned)write_cbp)); /* set the EOX bit */

    write_mem_desc->nbdp = 0;

	Dprintf("--- starting mem to scsi dma ----------\n");

    /* start the scsi dma 
     */

    *(volatile unsigned int *)PHYS_TO_K1(hpc_base+SCSI_BC_OFFSET)
					= 0xbad00bad;
    *(volatile unsigned int *)PHYS_TO_K1(hpc_base+SCSI_CBP_OFFSET)
					= 0xbad00bad;

    *(volatile unsigned int*)PHYS_TO_K1(hpc_base+SCSI_NBDP_OFFSET)
					= KDM_TO_PHYS(write_mem_desc);

	/* setting 0 also clears direction bit which  
	    means mem to scsi */

    *scsi_control = 0;  
    Dprintf("scsi_control = %#x\n",*scsi_control);

    *scsi_control |= SCSI_CTRL_STRTDMA ;
    Dprintf("scsi_control = %#x\n",*scsi_control);

	Dprintf("--- dma mem to scsi over ----------\n");
	
}


dma_scsi_to_mem()
{


	Dprintf("--- dma scsi to mem ----------\n");

    read_mem_desc->bc =  DATASIZE ; /* may not be necessary */

	/* set the EOX bit */
    read_mem_desc->cbp = (unsigned int)
		(0x80000000 | KDM_TO_PHYS((unsigned)read_cbp)); 

    read_mem_desc->nbdp = 0;

    *(volatile unsigned int*)PHYS_TO_K1(hpc_base+SCSI_NBDP_OFFSET)
				=KDM_TO_PHYS(read_mem_desc);

    *scsi_control = SCSI_CTRL_RESET | SCSI_CTRL_DMA_DIR ;

    Dprintf("scsi_control = %#x\n",*scsi_control);

	/* release reset */
    *scsi_control &= ~SCSI_CTRL_RESET ;

    Dprintf("scsi_control = %#x\n",*scsi_control);

    *scsi_control |= SCSI_CTRL_STRTDMA ;

    Dprintf("scsi_control = %#x\n",*scsi_control);

	/* load fifo count (0x40 )*/

    *(volatile unsigned int*)PHYS_TO_K1(hpc_base+SCSI_PNTR_OFFSET)
				= DATASIZE << SCSI_COUNT_SHIFT ; /*0x00400000 */

    *scsi_control |= SCSI_CTRL_FLUSH ;

    Dprintf("scsi_control = %#x\n",*scsi_control);

	msg_printf(DBG,"--- dma scsi to mem over ----------\n");
}

scsi_dma_wait()
{
    int timeout;

	/* just wait for some time */

    for (timeout=0; timeout < SCSI_DELAY; ++timeout) {
	us_delay (MS1);
    }

}

print_info(dma_read_over)
int dma_read_over ;
{
   
   Dprintf("write_mem_desc->bc = %d \nwrite_mem_desc->cbp = %s \n",write_mem_desc->bc,write_mem_desc->cbp);

   if (dma_read_over) 
     Dprintf("read_mem_desc->bc = %d \nread_mem_desc->cbp = %s \n",read_mem_desc->bc,read_mem_desc->cbp);

   Dprintf("read_cbp = %s \n",read_cbp);
   Dprintf("write_cbp = %s \n",write_cbp);
	
}

chk_single_read_scsi_fifo()
{
	int fifoaddr ;
	unsigned int upper ,lower ,value;
	extern  scsififo_read ();

	for (fifoaddr = 0; fifoaddr < 16 ; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    scsififo_read(PHYS_TO_K1(hpc_base),fifoaddr,&upper,&lower);
		Dprintf("upper = %#x lower = %#x\n",upper,lower);
		if(lower != 
			(value = *(unsigned int *)(((unsigned int*)write_cbp)+fifoaddr)))
			msg_printf(DBG,"*********** VRBOR FIFO READ ** mismatch at word %d read -> %#x expected -> %#x\n",fifoaddr,lower,value); 
	}
}
