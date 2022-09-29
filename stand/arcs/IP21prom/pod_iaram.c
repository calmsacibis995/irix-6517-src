/***********************************************************************\
*	File:		pod_iaram.c					*
*									*
*       Check the IARAM area for its correctness.                       *
*									*
\***********************************************************************/

#include "ip21prom.h"
#include "pod_iadefs.h"
#include "prom_externs.h"

void	store_lwin_word(uint, uint, uint);
uint	load_lwin_word(uint, uint);
uint	store_load_lwin_word(uint, uint, uint);

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
    jmp_buf	mapbuf;
    uint	*prev_fault;
    uint	result;

    ccloprintf("Testing map RAM in master IO4's IA chip (slot %b)...\n", io4slot);

    if (setfault(mapbuf, &prev_fault) == 0) {
        result = iaram_rdwr(window);
	cur_test++;
	result = iaram_addr(window);
	cur_test++;
        result = iaram_walk1s(window);
    } else {
	/* Returned here due to an exception	*/
	ccloprintf("*** Map RAM test took an exception.\n");
	result = (uint)FAILURE;
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


void clear_iaram(uint window)
{
    uint	i;

    for(i = 4; i < IO4_MAPRAM_SIZE; i += 8)
	    store_lwin_word(window, i, 0);
}


uint iaram_rdwr(uint window)
{
    uint	i, j, value;

    /*
     * IARAM memory is available at each double word address.
     * Hence the i += 8; 
     */

    for(i = 4; i < IO4_MAPRAM_SIZE; i += 8) {
	for (j = 0; j < PATRN_SIZE; j++) {
	    value = store_load_lwin_word(window, i, patterns[j]);
	    if (value != patterns[j]) {
		ccloprintf("*** ERROR in map RAM at 0x%x, Wrote %x, Read %x\n",
			i, patterns[j], 
			(unsigned)value);
		return EVDIAG_MAPRDWR_FAILED;
	    }
	}
    }

    return 0;
}

uint iaram_addr(uint window)
{
    int	i;
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

    return 0;
}

/*
 * Function : iaram_walk1s
 * Description :  Walk 1s in IARAM Memory and check if it's ok
 */
uint iaram_walk1s(uint window)
{
    int i, j;
    unsigned mask, value;

    for(i = 4; i < IO4_MAPRAM_SIZE; i += 8) {
	for(j = 0; j < sizeof(unsigned); j++) {
	    mask = (1 << j);
	    value = store_load_lwin_word(window, i, mask);
	    if (value != mask) {
                ccloprintf("*** ERROR in walking 1s at 0x%x, Wrote %x, Read %x\n",
				i, mask, value);
		return EVDIAG_MAPWALK_FAILED;
	    }
	}
    }

    return 0;
}
