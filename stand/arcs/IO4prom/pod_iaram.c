/***********************************************************************\
*	File:		pod_iaram.c					*
*									*
*       Check the IARAM area for its correctness.                       *
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_iaram.c $Revision: 1.14 $"

#include "libsc.h"
#include "libsk.h"
#include "pod_fvdefs.h"

/* Macro definitions for the IARAM Test. */

#ifdef	NEWTSIM
#define	IARAM_SIZE		0x20
#else
#define	IARAM_SIZE		0x1fff
#endif

static  jmp_buf	mapbuf;

/* 
 * Function : check_iaram
 * Description:
 *	Run tests to check the IARAM. 
 *	Since the IARAM is a static ram, chances of an error are far less.
 *	so we just limit ourselves to a few tests.
 */
int
check_iaram(unsigned window, int io4slot)
{
    int		result=0;
    caddr_t 	ram_addr;


#ifdef	DEBUG_POD
    print_hex(0x52414d<<8 | window);
#endif

    setup_globals(window, io4slot, 0);
    message(IARAM_TEST);

    Nlevel++;

    if ((ram_addr = tlbentry(window, 0, 0)) == (caddr_t)0)
	return(FAILURE);

    
    if (setjmp(mapbuf) == 0){
	set_nofault(mapbuf);
        iaram_rdwr(ram_addr+4);   result++;
	iaram_addr(ram_addr+4);   result++;
        iaram_walk1s(ram_addr+4); 
	result = SUCCESS;
    }
    else{
	/* Returned here due to an exception	*/
	message("Mapram Test returned due to an Exception\n");
	show_fault();
	switch(result){
	    case 0 : message(IRAM_RWTESTF); break;
	    case 1 : message(IRAM_ADTESTF); break;
	    case 2 : message(IRAM_W1TESTF); break;
	    default: message(IARAM_UNKN_ERR); break;
	}
	result = FAILURE;
    }

    clear_nofault();
    Nlevel--;

#ifndef	NEWTSIM
    (result == SUCCESS) ? message(IARAM_TESTP): message(IARAM_TESTF);
#endif

    tlbrmentry(ram_addr);

#ifdef	DEBUG_POD
    print_hex(0x52414e<<8 | window);
#endif

    Return (result);

}

typedef	unsigned 	mapram_t;

int
iaram_rdwr(caddr_t ram_addr)
{

    int	i,j,error=0;
    mapram_t value;
    volatile mapram_t *addr = (mapram_t *)(ram_addr);

    /*
     * IARAM memory is available at each double word address.
     * Hence the addr+=2; 
     */
    message(IRAM_RWTEST);

    for(i=0; i < IARAM_SIZE ; i++, addr+=2 ){
	for (j=0; j < PATRN_SIZE; j++){

	    *(addr) = (mapram_t)patterns[j];
	    if((value = *(addr)) != (mapram_t)patterns[j]){
#ifdef	DEBUG_POD
		print_hex(value); print_hex(patterns[j]);
#endif
		mk_msg(IRAM_RDWR_ERR, (__scunsigned_t)addr&0xffffff,
			patterns[j], (unsigned)value);
		analyze_error(1);
	    }
	}
    }

    message(IRAM_RWTESTP);

    Return(error);
}

int
iaram_addr(caddr_t ram_addr)
{
    int	i,error=0;
    unsigned value;
    volatile unsigned *addr = (unsigned *)ram_addr;

    message(IRAM_ADTEST);

    for (i=0; i < IARAM_SIZE ; i++, addr += 2)
        *addr = i;

    addr = (unsigned *)ram_addr;

    for (i=0; i < IARAM_SIZE; i++, addr+=2){
        if((value = *(addr)) != i){
#ifdef	DEBUG_POD
	    print_hex(value); print_hex(i);
#endif
            mk_msg(IRAM_ADDR_ERR,(__scunsigned_t)addr&0xffffff, addr, value);
	    analyze_error(1); 
        }
    }

    message(IRAM_ADTESTP);

    Return(error);
}

/*
 * Function : iaram_walk1s
 * Description :  Walk 1s in IARAM Memory and check if it's ok
 */
int 
iaram_walk1s(caddr_t vaddr)
{
    volatile unsigned *addr = (unsigned *)vaddr;

    int i,j;
    unsigned mask,value;
    int	error=0;

    message(IRAM_W1TEST);

    for(i=0; i <IARAM_SIZE; i++,addr+=2 ){
	for(j=0; j <sizeof(unsigned); j++){
	    mask = (1 << j);
	    *(addr) = mask;
	    if ((value = *(addr)) != mask){
                mk_msg(IRAM_WALK1_ERR, (__scunsigned_t)addr&0xffffff,
			mask, value);
		analyze_error(1);
		error++;
	    }
	}
    }

    message(IRAM_W1TESTP);

    Return(error);
}

/*ARGSUSED*/
int
check_mapram(unsigned io4_window, int io4slot)
{
    unsigned 	i;
    unsigned	mask, val;

    if (setjmp(mapbuf)){
	message("Unexpected exception while testing mapram\n");
	show_fault();
	clear_nofault();
	return FAILURE;
    }
    set_nofault(mapbuf);
    for (i=0; i < 0x10000; i+=8)
	for (mask=0; mask < 32; mask++){
	    put_word(io4_window, 0, i, (1 << mask));
            val = get_word(io4_window, 0, i);
	    if (val != (1 << mask)){
		message("mapram bad At 0x%x wrote 0x%x Read 0x%x\n",
			i, (1 << mask), val);
	    }
	}

    message("Testing Mapram Completed\n");
    return SUCCESS;
}
    
    
