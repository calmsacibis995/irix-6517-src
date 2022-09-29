/***********************************************************************\
*	File:		pod_iaram.c					*
*									*
*       Check the IARAM area for its correctness.                       *
*									*
\***********************************************************************/

#include "pod_iadefs.h"

/* Macro definitions for the IARAM Test. */

#define	IO4_LWIN_BASE		0x04000000	/* 0x0400000000 >> 8	*/
#define	IO4_LWIN_SIZE		0x00400000	/* 1GB >> 8		*/
#define	ADAP_LWIN_SIZE		0x08000000	/* 128 Mb		*/

static unsigned patterns[PATRN_SIZE] = {ALL_1S, ALL_5S, ALL_AS, ALL_0S};

/* 
 * Function : check_iaram
 * Description:
 *	Run tests to check the IARAM. 
 *	Since the IARAM is a static ram, chances of an error are far less.
 *	so we just limit ourselves to a few tests.
 */
uint check_iaram(unsigned window, int io4slot)
{
    unchar	cur_test = 0;
    caddr_t 	ram_addr;
    jmp_buf	mapbuf;
    uint	*prev_fault;
    uint	result;

    result = 0;

    ccloprintf("Testing map RAM in master IO4's IA chip (slot %b)...\n", io4slot);

    if (setfault(mapbuf, &prev_fault) == 0) {
	restorefault(prev_fault);
        if (result = iaram_rdwr(window))
		cur_test++;
	else if (result = iaram_addr(window))
		cur_test++;
        else if (result = iaram_walk1s(window))
		cur_test++;
	else
		result = 0;
    } else {
	/* Returned here due to an exception	*/
	ccloprintf("*** Map RAM test took an exception.\n");
	result = EVDIAG_PANIC;
	switch(cur_test) {
	    case 0 :
		ccloprintf("*** Map RAM read/write test FAILED\n");
		result = EVDIAG_MAPRDWR_BUSERR;
		break;
	    case 1 :
		ccloprintf("*** Map RAM addres test FAILED\n");
		result = EVDIAG_MAPADDR_BUSERR;
		break;
	    case 2 :
		ccloprintf("*** Map RAM walking 1 test FAILED\n");
		result = EVDIAG_MAPWALK_BUSERR;
		break;
	    default:
		ccloprintf("*** Map RAM test unknown error\n");
		result = EVDIAG_MAP_BUSERR;
		break;
	}
	restorefault(prev_fault);
	return result;
    }

    clear_iaram(window);

    restorefault(prev_fault);
    return result;
}


void clear_iaram(int window)
{
    uint	i, j;

    for(i = 4; i < IO4_MAPRAM_SIZE; i += 8)
	    store_lwin_word(window, i, 0);
}


uint iaram_rdwr(int window)
{
    uint	i, j;
    int error = 0;
    uint value;

    /*
     * IARAM memory is available at each double word address.
     * Hence the i += 8; 
     */

    for(i = 4; i < IO4_MAPRAM_SIZE; i += 8) {
	for (j = 0; j < PATRN_SIZE; j++) {
	    store_lwin_word(window, i, patterns[j]);
	    value = load_lwin_word(window, i);
	    if (value != patterns[j]) {
		ccloprintf("*** ERROR in map RAM at 0x%x, Wrote %x, Read %x\n",
			i, patterns[j], 
			(unsigned)value);
		return EVDIAG_MAPRDWR_FAILED;
	    }
	}
    }

    if (error)
	return EVDIAG_MAPRDWR_FAILED;
    else
	return 0;
}

uint iaram_addr(int window)
{
    int	i;
    int error = 0;
    unsigned value;

    for (i = 4; i < IO4_MAPRAM_SIZE; i += 8)
	store_lwin_word(window, i, i);

    for (i = 4; i < IO4_MAPRAM_SIZE; i += 8) {
	value = load_lwin_word(window, i);
        if(value != i) {
            ccloprintf("*** ERROR in addressing at 0x%x, Wrote %x, Read %x\n",
						i, i, value);
	    return EVDIAG_MAPADDR_FAILED;
        }
    }

    if (error)
	return EVDIAG_MAPADDR_FAILED;
    else
	return 0;
}

/*
 * Function : iaram_walk1s
 * Description :  Walk 1s in IARAM Memory and check if it's ok
 */
uint iaram_walk1s(int window)
{
    int i, j;
    unsigned mask, value;
    int	error = 0;

    for(i = 4; i < IO4_MAPRAM_SIZE; i += 8) {
	for(j = 0; j < sizeof(unsigned); j++) {
	    mask = (1 << j);
	    store_lwin_word(window, i, mask);
	    value = load_lwin_word(window, i);
	    if (value != mask) {
                ccloprintf("*** ERROR in walking 1s at 0x%x, Wrote %x, Read %x\n",
				i, mask, value);
		return EVDIAG_MAPWALK_FAILED;
	    }
	}
    }

    if (error)
	return EVDIAG_MAPWALK_BUSERR;
    else
	return 0;
}

