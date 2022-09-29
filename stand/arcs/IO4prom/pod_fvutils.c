/***********************************************************************\
*	File:		pod_fvutils.c					*
*									*
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_fvutils.c $Revision: 1.15 $"


#include "pod_fvdefs.h"


/*
 * Instead of going through the TLB entries to access the 
 * Large windows, I am planning to use the R4000 in 64 bit 
 * mode for window access, so that I can use the asm library
 * provided by Stever
 */
#define	IO4_LWIN_BASE	0x04000000	/* 0x0400000000	>> 8	*/
#define	IO4_LWIN_SIZE	0x00400000	/* 1GB >> 8		*/

#define	ADAP_LWIN_SIZE	0x08000000	/* 128 Mb		*/


/* Some random values used for writing to registers		*/
/* Has about 60 values in the array 				*/

unsigned random_nos[] = {
0x44AECF12, 0x2E62D3A0, 0x4E463D6F, 0x61532EA4, 0x66CE26AE, 0x11D60246, 
0x31FA6F80, 0x37CFDB96, 0x33FB0122, 0xAE021908, 0x4D6E7899, 0x4B8CFEDB, 
0x3B7FF6E5, 0x1D5947AE, 0xB4E8BF24, 0x2FECACF0, 0x4B1737BF, 0xC092D3A0, 
0x37674567, 0x6996C16C, 0x59BF258A, 0x67E50C82, 0x1CB55555, 0x2C2D7E4A, 
0x5B6BDB96, 0x376471C6, 0x8DCD5555, 0x57E80919, 0x27594C3B, 0x2428641F, 
0x307369CF, 0x1CE0ACF1, 0x5E652C5E, 0x6478C83E, 0x4F6B4566, 0x5EC091A1,
0x36760FED, 0x6E48BF24, 0x36505F92, 0x57E3530D, 0x1E398765, 0x31FDBE02, 
0x77396788, 0x2B83E4B1, 0x23865431, 0x47ABE4B0, 0x281A740D, 0x186C962F,
0x4E6CDF00, 0x4F41D035, 0x2F8F6542, 0x2A028641, 0x5FE59998, 0x36F17E4A, 
0x77B4D5E5, 0x1493F6E5, 0x42C6BCDE, 0x24DF2EA6, 0x69174566, 0x49CF530E, 
};

unsigned patterns[PATRN_SIZE] = {ALL_1S, ALL_5S, ALL_AS, ALL_0S};

/* Some Global Variables used across all */
int		IO4_slot = (-1);
int		Window = (-1);
int		Ioa_num = (-1);
unsigned	Chip_base = 0;
caddr_t		Mapram=(0);

/*
 * Function : get_word
 * Description : Get data in the large window defined by 'window' 
 *	and 'adap' at offset 'offset
 */
int
get_word(int window, int adap, int offset)
{
    return u64lw((IO4_LWIN_BASE + (IO4_LWIN_SIZE * window)),
    		(ADAP_LWIN_SIZE * adap) + offset);
}

/*
 * Function : put_word
 * Description : Write 'data' to the large window defined by 'window' 
 *	and 'adap' at offset 'offset
 */
void
put_word(int window, int adap, int offset, int data)
{

    u64sw(IO4_LWIN_BASE + (IO4_LWIN_SIZE * window),
    		(ADAP_LWIN_SIZE * adap) + offset, data);
}


/*
 * Function : check_regbits 
 * Description :
 * 	Check if in the given register all bits are working properly.
 * 	This routine will scan through the register bits and test each
 * 	bit inidvidually
 *
 *	regmask indicates the  bits which can be Rd/Wr in the register
 *	'regno'. Test each of the bit indicated by regmask 
 *
 * Result :
 *	0 - if no error.
 *	rslt - having those bits set which failed the test.
 */

int
check_regbits(__psunsigned_t regno, unsigned testbits, int	upr_word)
{
    int		i;
    unsigned long long	old, val, mask;
    unsigned	regmask;

    if (testbits == 0)
    	return 0;
    
    for (i=0, regmask=testbits; regmask != 0; i++, regmask >>= 1){

        if (!(regmask & 0x1))
            continue;

	mask = 1 << i;
	if (upr_word)
		mask <<= 32;

	old  = RD_REG(regno);
	WR_REG(regno, mask);
	val = RD_REG(regno);
	WR_REG(regno, old);

	if ((unsigned)mask != (unsigned)(val & testbits)){
    	    mk_msg(REGR_RDWR_ERR, regno, (unsigned)mask, (unsigned)val);
	    return (unsigned)val;
	}
     }

     return 0;
}

