/*************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  Filename:  vid_util.c
 *  Purpose: Diagnostic test routines and utilities used by video tests
 *
 *
 *      $Revision: 1.12 $
 *
 */

#include "mgv_diag.h"

/********************************************************************/


/********************************************************************
*
*		Function: dump_registers
*
*		Description:
*
*		Return: None
*
*********************************************************************/
void dump_registers()
{
	int i;
	unsigned char reg_val;
	unsigned char gal_reg_num[] = {	0, 1,  2, 3, 4, 5, 
									6, 7, 12, 13, 14, 
									15, 16, 17, 19, 22,
									23, 24, 25, 26, 0xFF};

	unsigned char cc1_reg_num[] = {	32, 33, 34, 35, 36,
									37, 38, 39, 40, 41,
									42, 43, 44, 45, 46, 
									47, 48, 49, 50, 51,
									52, 53, 54, 55, 56, 
									57, 58, 0xFF };

	msg_printf(DBG,"\n\tIDE REGISTER DUMP");

	i = 0; 
	while( gal_reg_num[i] != 0xFF )
	{
		GAL_IND_R1(mgvbase,  gal_reg_num[i] , reg_val);
		msg_printf(DBG,"%s%2d: %2x", (i%5) ? "\t " : "\ngalind",
										gal_reg_num[i], 
										(reg_val & register_mask[ gal_reg_num[i++] ]));
	}
msg_printf(DBG,"\n\n");

	CC_R1(mgvbase, CC_SYSCON, reg_val);
	msg_printf(DBG,"CCdir0: %2x ", reg_val);

	CC_R1(mgvbase, CC_FRAME_BUF, reg_val);
	msg_printf(DBG,"CCdir3: %2x ", reg_val);

	CC_R1(mgvbase, CC_FLD_FRM_SEL, reg_val);
	msg_printf(DBG,"CCdir4: %2x ", reg_val);

	AB_R1(mgvbase, AB_SYSCON, reg_val);
	msg_printf(DBG,"ABdir0: %2x\n", reg_val);

	i = 0; 
	while( cc1_reg_num[i] != 0xFF )
	{
		CC_W1( mgvbase, CC_INDIR1, cc1_reg_num[i]);
		CC_R1( mgvbase, CC_INDIR2, reg_val );
		msg_printf(DBG,"%s%2d: %2x", (i%5) ? "\t " : "\ncc1ind",
						cc1_reg_num[i++], reg_val);
	}

}


/***********************************************************************
*
*	Function: load_tests ( int*, int )
*	
*	Purpose: to load register and memory bit-field test requests
*		 and the register/memory addresses on which to perform 
*		 the tests into a test data structure.  The tests are not
*		 performed in this routine.
*
*	Return: -1 - invalid or non tests requested
*		 	 0 - all data loaded successfully
*	
********************************************************************************/

int load_tests( int *seq_p, int tests)
{
    int regmem_num = 0, bit_num = 0, test_num = 0;
    /* if no, or invalid, tests requested, exit */
    if( (tests == 0) || (tests & INVALID_TESTS) )
        return(-1);


    /* Load the test data for each test requested */
    while( (*(seq_p + regmem_num) != -1) && (test_num < MAX_TESTS_PER_REGMEM))
    {
        /* store the register indirect addr */
        batch_test[regmem_num].addr = *(seq_p + regmem_num);

        /* check bits for batch test requests */
        for(bit_num = 0, test_num = 0; bit_num < NUM_BIT_PATTERNS; bit_num++)
        {
/*			msg_printf(DBG,"LOAD:rm# %d, t# %d, b# %d\n",regmem_num,test_num,bit_num);
*/
			if( test_num > MAX_TESTS_PER_REGMEM)
			{
				msg_printf(ERR, "Too many tests requested for register %d",
										batch_test[regmem_num].addr);
				return(-1);
			}

            /* if test requested, store test data */
            else if( tests & (1 << bit_num) )
            {
				/* allocate test data structure space to store info */
				batch_test[regmem_num].tests_p[test_num] = 
												malloc( sizeof(test_data) );

				/* store test name */
				batch_test[regmem_num].tests_p[test_num]->test_name_p = 
														asc_bit_pat_p[bit_num];

				/* store test value */
                batch_test[regmem_num].tests_p[test_num]->test_value = 
														test_values[bit_num];

				/* store bit mask to only consider result of valid reg. bits */
                batch_test[regmem_num].tests_p[test_num]->mask   = 
										register_mask[ *(seq_p+regmem_num) ];
				  
				/* set to FAIL - no default PASS */ 
                batch_test[regmem_num].tests_p[test_num]->result  = 0;

				/* all data loaded for this test */
#if 0
			msg_printf(DBG,"LOAD:adr# %d, tv# %d, msk# %x\n",
						batch_test[regmem_num].addr,
						batch_test[regmem_num].tests_p[test_num]->test_value,
						batch_test[regmem_num].tests_p[test_num]->mask);
#endif
				test_num++;
            }
        }
		/* all test requests loaded for this regmem */
		regmem_num++;
    }
    return(0);
}

/*****************************************************************************
*
*		Function: test_gal15_rw1
*
*		Description:  	Reads the complete 'batch_test' test data structure
*						and, for each register loaded, performs the tests
*						requested.
*						For each test:
*							PASS: batch_test[i].tests_p[j]->result = 0
*							FAIL: failed bit(s) is(are) set in result
*								  batch_test[i].pass = FAIL
*
*		Returns: 1 - Too many tests loaded for register
*				 0 - test program completed successfully (maybe failures or not)
*
******************************************************************************/

int test_gal15_rw1(void)
{
    unsigned char read_value = 0, write_value = 0, loop = 0, lsb_shift = 0;
	unsigned char regmem_num = 0, test_num = 0, m;

    /* Load the test data for all tests requested */
    while(batch_test[regmem_num].addr != -1)  
    {

		if(test_num >= MAX_TESTS_PER_REGMEM )
		{
			msg_printf(ERR,"Loaded tests exceed maximum for register %d",
									batch_test[regmem_num].addr);
			return(-1);
		}

        /* Perform each test request */
        while( batch_test[regmem_num].tests_p[test_num] != NULL )
        {
			/* setup for the test */
			switch( batch_test[regmem_num].tests_p[test_num]->test_value )
			{
				case WALK1:
					write_value = WALK1;
					loop = 8;
					lsb_shift = 0;
					break;
				case WALK0:
					write_value = WALK0;
					loop = 8;
					lsb_shift = 1;
					break;
				case ALLONES:
				case ALLZEROES:
				case ALT10:
				case ALT01:
					write_value = batch_test[regmem_num].tests_p[test_num]->test_value;
					loop = 1;
					lsb_shift = 0;
					break;
			}	

			/* enter walk loop - do once if not walk test */
			for(m = 0; m < loop; m++)
			{	
				/* write to reg */
				GAL_IND_W1(mgvbase, batch_test[regmem_num].addr, write_value & batch_test[regmem_num].tests_p[test_num]->mask );
				GAL_IND_R1(mgvbase, batch_test[regmem_num].addr, read_value);
				read_value &= batch_test[regmem_num].tests_p[test_num]->mask;

				/* read reg, xor with write value, overlay result */
				batch_test[regmem_num].tests_p[test_num]->result |= ( read_value ^ (write_value & batch_test[regmem_num].tests_p[test_num]->mask) );

				msg_printf(DBG,"TEST:addr# %2d, test# %d, wr# %2x, mask# %2x, rd# %2x, xor_result# %2x\n",
						batch_test[regmem_num].addr,
						test_num,
						write_value & batch_test[regmem_num].tests_p[test_num]->mask,
						batch_test[regmem_num].tests_p[test_num]->mask,
						read_value,
						batch_test[regmem_num].tests_p[test_num]->result);

				/* shift write value to walk left */
				write_value <<= 1;

				/* add lsb=1 if WALK0 test */
				write_value |= lsb_shift;
			}

			/* check result, if =0 -> PASS, else FAIL */
			batch_test[regmem_num].pass = 
				(batch_test[regmem_num].tests_p[test_num]->result) ? FAIL : PASS;

			test_num++;

        }     /* while test */
		regmem_num++;
		test_num = 0;

    }		  /* while regmem */
	return( 0 );
}


/*****************************************************************************
*
*		Function: report_results( int )
*
*		Description:  	Scans the regmem test data structure and displays
*						any test data stored in it.
*
*		Returns: None
*
******************************************************************************/


void report_results( int cmd )
{
	unsigned char regmem_num = 0, test_num = 0;

	/* have any tests been loaded? */
	while( (regmem_num < MAX_REGMEMS) && (test_num < MAX_TESTS_PER_REGMEM) )
	{
    	switch( cmd )
    	{
        	case RESET_TSTBUF:
				/* reset register information */
				batch_test[regmem_num].reg_name_p 	= "";
				batch_test[regmem_num].addr 		= -1;
				batch_test[regmem_num].pass 	 	= PASS;

				/* clear all test information for that register */
				for(test_num = 0; batch_test[regmem_num].tests_p[test_num] != NULL; test_num++ )
				{
					batch_test[regmem_num].tests_p[test_num]->test_name_p = "";	
					batch_test[regmem_num].tests_p[test_num]->test_value = 0;
					batch_test[regmem_num].tests_p[test_num]->mask		 = 0;
					batch_test[regmem_num].tests_p[test_num]->result	 = 0;
			
					/* deallocate memory space for that test data */
					free( batch_test[regmem_num].tests_p[test_num] );
				}
            	break;

        	case DSPY_TSTBUF:
				if( batch_test[regmem_num].addr == -1 )
					break;

				/* display errors in registers with failed tests */
            	if(batch_test[regmem_num].pass == FAIL) 
            	{
					/* if one of the tests failed, report which one */
                	for(test_num = 0; batch_test[regmem_num].tests_p[test_num] != NULL; test_num++)
					{
						/* display only failed tests for that register */
						if( batch_test[regmem_num].tests_p[test_num]->result )
                    		msg_printf(ERR, "Failed Indirect Address: %2d, Test: %s, Bits: %2x\n", 
                				batch_test[regmem_num].addr,
								batch_test[regmem_num].tests_p[test_num]->test_name_p,
								batch_test[regmem_num].tests_p[test_num]->result );
					}
				}
				else
					msg_printf(SUM, "\nAddress: %d passed all register bit tests",
								batch_test[regmem_num].addr);
            	break;

        	default:
            	msg_printf(ERR, "Error in command to 'report_result' program\n");
            	break;
    	}			/* switch */

		regmem_num++;		/* move to next regmem */

	}				/* while  */
}


/*********************************************************************
*
*		Function: _mgv_make_vid_array( uint*,   uchar,
									uint, uint,
									uint, uint,
									int,   int,
									int,   int )
*
*		Purpose: to load an array with an 8 or 10-bit YUV 4:2:2 or
*				AUYV4:4:4:4 test pattern.
*				The array can have different values for Y, U, V, & A
*				however strange this may look.  This does not apply
*				for walking patterns; Whichever walking pattern is in
*				'yval' is the one used for YUV(A).  The pattern can be
*				horizontally and vertically offset, so you can overlay
*				patterns on top of one another.
*
*		Return:  None.
*
**********************************************************************/
void _mgv_make_vid_array( unsigned int *buffer_p, 
					unsigned char bits,
					unsigned int  yval,
					unsigned int  uval,
					unsigned int  vval,
					unsigned int  aval,
					int h_length, int v_length,
					int h_offset, int v_offset )
{
	int i, j;
	unsigned char *addr8_p, load_byte;
	unsigned int  *addr10_p, load_word;
	unsigned char pixel_width = (aval) ? 3:2;
	unsigned char walk = ( (yval == WALK1) || (yval == WALK0) );

	/* 
		if TENBIT set, 8 bit range should be <<2 to make 10bit value
	   	if TENBIT set, <<6 so tenbit value is upper 10, not lower
		this is combined into <<8

	 	walking values are adjusted when created 
	*/
	if( !walk )
	{
#if 0
		yval = (yval > 235) ? 235 : ((yval < 16) ? 16: yval);
		uval = (uval > 235) ? 235 : ((uval < 16) ? 16: uval);
		vval = (vval > 235) ? 235 : ((vval < 16) ? 16: vval);
		if( aval != 0 )
			aval = (aval > 235) ? 235 : ((aval < 16) ? 16: aval);
#endif

		if(bits == TENBIT)
		{
			yval = (yval & 0x00c0) | (yval << 8);
			uval = (uval & 0x00c0) | (uval << 8);
			vval = (vval & 0x00c0) | (vval << 8);
			aval = (aval & 0x00c0) | (aval << 8);
		}
	}
	/* Traverse vertical length */
	for(i = 0; i < v_length; i++)
	{
		/*Set array ptr. at starting pixel on new line-adjust for voff & hoff*/ 
		addr10_p = buffer_p + ((v_offset + i) * (h_offset + h_length) * 
													pixel_width) + h_offset;
		addr8_p  = (unsigned char *)buffer_p + ((v_offset + i) * 
							(h_offset + h_length) * pixel_width) + h_offset;

		/* Horizontal YUV(A) pixel progression along line */
		for(j = 0; j < h_length; j++)
		{
			if(bits == TENBIT)
			{
				if( walk )
				{
					load_word = (yval == WALK1) ? (1 << (j%10)): ~(1 << (j%10));
#if 0
					load_word = (load_word > 970) ? 970 : ((load_word < 64) ? 64: load_word);
#endif
					/* left justify real 10-bit values with lsb 0s */
					load_word <<= 6;
				}

#if 0
msg_printf(DBG,"%4x%4x%4x%4x%4x%c ",yval,uval,vval,aval,load_word,walk ? 'W':' ');
#endif
				if( aval )  /* 10-bit AYUV4:4:4:4 */
				{
					*(addr10_p++)	= walk ? load_word : vval; /* V0 */
					*(addr10_p++)	= walk ? load_word : yval; /* Y0 */
					*(addr10_p++)	= walk ? load_word : uval; /* U0 */
					*(addr10_p++)	= walk ? load_word : aval; /* A0 */
				}
				else		/* 10-bit YUV4:2:2 */
				{
					*(addr10_p++)	= walk ? load_word : uval; /* U0 */
					*(addr10_p++)	= walk ? load_word : yval; /* Y0 */
					*(addr10_p++)	= walk ? load_word : vval; /* V0 */
					j++;
					if( walk )
					{
						load_word =(yval == WALK1)?(1 << (j%10)):~(1 << (j%10));
#if 0
						load_word = (load_word > 970) ? 970 : ((load_word < 64) ? 64: load_word);
#endif
						/* left justify real 10-bit values with lsb 0s */
						load_word <<= 6;
					}
					*(addr10_p++)	= walk ? load_word : yval; /* Y1 */
				}
			}
			else  /* 8-bit */
			{
				if( walk )
				{
					load_byte = (yval == WALK1) ? (1 << (j%8)) : ~(1 << (j%8));
#if 0
					load_byte = (load_byte > 235) ? 235 : ((load_byte < 16) ? 16: load_byte);
#endif
				}
				if( aval) 	/* 8-bit AUYV4:4:4:4 */
				{
					*(addr8_p++)	= walk ? load_byte : (unsigned char)vval; /* V0 */
					*(addr8_p++)	= walk ? load_byte : (unsigned char)yval; /* Y0 */
					*(addr8_p++)	= walk ? load_byte : (unsigned char)uval; /* U0 */
					*(addr8_p++) 	= walk ? load_byte : (unsigned char)aval; /* A0 */
				}
				else		/* 8-bit YUV4:2:2 */
				{
					*(addr8_p++)	= walk ? load_byte : (unsigned char)uval; /* U0 */
					*(addr8_p++)	= walk ? load_byte : (unsigned char)yval; /* Y0 */
					*(addr8_p++)	= walk ? load_byte : (unsigned char)vval; /* V0 */
					j++;
					if( walk )
					{
						load_byte = (yval == WALK1) ? (1 << (j%8)) : ~(1 << (j%8));
#if 0
						load_byte = (load_byte > 235) ? 235 : ((load_byte < 16) ? 16: load_byte);
#endif
					}

					*(addr8_p++)	= walk ? load_byte : (unsigned char)yval; /* Y1 */
				}
			} 	/* else */
		}		/* for */
	}			/* for */
}				


/***********************************************************************************
** NAME: write_CC1_field
**
** FUNCTION: This routine writes the specified area of a field
**           from data stored in the provided buffer.
**           The length of the active area must be a multiple of
**           48 bytes.
**
** INPUTS: hor_off -> starting place for each line
**         hor_len -> number of bytes to write per line
**         vert_off -> starting line number
**         vert_len -> number of lines to write
**         field -> 0 = write the odd field
**                  1 = write the even field
**         buf_ptr -> pointer to where info is stored
**
** OUTPUTS: none
*/
int write_CC1_field(uint hor_off,  uint hor_len,
			uint vert_off, uint vert_len,
                	uint field, unsigned char *buf_ptr)
{
	int xx, yy, zz, tmp;
	unsigned int num_48pages, partial_page;
	unsigned long ctr=0;

#if 0
	unsigned char quickbytes[48];
	unsigned int *word_buf_ptr = (unsigned int *)quickbytes;
	int num_words = sizeof(quickbytes)/sizeof(int);
#endif


	num_48pages = hor_len / 48;
	partial_page = hor_len % 48;

#if 0
msg_printf(SUM,"\nDUMP BUFFER");
for(xx= 0; xx < 100;xx++)
	msg_printf(SUM," %d:%2x ",xx, *(buf_ptr+xx));
#endif

	/* set field bit */
    CC_W1(mgvbase, CC_FLD_FRM_SEL,  ((field & 0x1) << 4));

	/* first line # (-lsb) in field */
	CC_W1(mgvbase, CC_INDIR1, CC_FRM_BUF_PTR); /* #49 */
	CC_W1(mgvbase, CC_INDIR2, (vert_off >> 1) & 0xFF);

	/* Set Frame Buffer Setup #48 to write to FB(0),FB input from CPU(1) */
	CC_W1(mgvbase, CC_INDIR1, CC_FRM_BUF_SETUP); /* #48 */
	CC_R1(mgvbase, CC_INDIR2, tmp);
	tmp &= 0xFE;
	tmp |= 0x02;
	CC_W1(mgvbase, CC_INDIR1, CC_FRM_BUF_SETUP); /* #48 */
	CC_W1(mgvbase, CC_INDIR2, tmp );

	/* load 1st fbuf, line # lsb, horiz. posn defaults to 0 */
    CC_W1(mgvbase, CC_FRAME_BUF, (0xc0 | ((vert_off & 0x1) << 5)));

	/* reset FIFO ptrs. */
	CC_W1(mgvbase, CC_FRAME_BUF, 0) ;

msg_printf(DBG,"\nReady to write to %s fbuf ", field ? "even":"odd");


	/* Write to the CC1 framebuffer, line by line */
	for (yy = 0; yy < vert_len; yy++)
	{
	  	/* Write the line into the framebuffer */
	 	for (xx = 0;xx < num_48pages; xx++)
	  	{
			if( _mgv_CC1PollFBRdy(10) )
				return( -1 );

#if 0
			for(zz = 0; zz < 48; zz++, buf_ptr++)
				quickbytes[zz] = *buf_ptr;

		  	/* Transfer a full 48 byte page */
		 	for (zz = 0; zz < num_words; zz++)
			{
				if( ctr == 95 )
				{
					msg_printf(DBG," W%d/%d/%d:%2x ",
				yy,xx,zz,*((unsigned char*)word_buf_ptr) );
				}
				ctr++;		
		   		CC_W1(mgvbase,CC_FRM_BUF_DATA,*word_buf_ptr);
			}
#endif

#if 1
		  	/* Transfer a full 48 byte page */
		 	for (zz = 0; zz < 48; zz++)
			{
				if( ctr == 95 )
					msg_printf(DBG," W%d/%d/%d:%2x ",yy,xx,zz,*buf_ptr );
				ctr++;		
				*buf_ptr = (*buf_ptr < 16) ? 16 : ((*buf_ptr > 235) ? 235:*buf_ptr);
		   		CC_W1(mgvbase,CC_FRM_BUF_DATA,*buf_ptr);
				buf_ptr++;
			}
#endif

		}
		if( partial_page )
		{
			if( _mgv_CC1PollFBRdy(10) )
				return( -1 );

		  	/* Transfer a partial 48 byte page */
		 	for (zz = partial_page; zz > 0; zz--)
			{
		   		CC_W1(mgvbase, CC_FRM_BUF_DATA, 0x80);
				msg_printf(SUM," PP:80 ");
			}
		}
	}

	return (0);
}

/******************************************************************/
/*
** NAME:rd_CC1_field
**
** FUNCTION: This routine reads the specified area of a field
**           to data stored in the provided buffer.
**
** INPUTS: hor_off -> starting place for each line
**         hor_len -> number of bytes to write per line
**         vert_off -> starting line number
**         vert_len -> number of lines to read
**         field -> 0 = read the odd field
**                  1 = read the even field
**         buf_ptr -> pointer to where read data is stored
**
** OUTPUTS: none
*/
int rd_CC1_field(uint hor_off,  uint hor_len,
			uint vert_off, uint vert_len,
                	uint field, unsigned char *buf_ptr)
{
	int xx, yy, zz, tmp;
	unsigned int num_48pages, partial_page;

	num_48pages = hor_len / 48;
	partial_page = hor_len % 48;
	 
	/* set field bit */
    CC_W1(mgvbase, CC_FLD_FRM_SEL,  ((field & 0x1) << 4));

	/* first line # (-lsb) in field */
	CC_W1(mgvbase, CC_INDIR1, CC_FRM_BUF_PTR); /* #49 */
	CC_W1(mgvbase, CC_INDIR2, (vert_off >> 1) & 0xFF);

	/* Set Frame Buffer Setup #48 to read from FB(0),output to CPU(2) */
	CC_W1(mgvbase, CC_INDIR1, CC_FRM_BUF_SETUP); /* #48 */
	CC_R1(mgvbase, CC_INDIR2, tmp);
	tmp |= 0x05;
	CC_W1(mgvbase, CC_INDIR1, CC_FRM_BUF_SETUP); /* #48 */
	CC_W1(mgvbase, CC_INDIR2, tmp );
	CC_R1(mgvbase, CC_INDIR2, tmp);
	msg_printf(SUM,"\nCCIND48:%2x    ",tmp);

	/* load 1st fbuf, line # lsb, horiz. posn defaults to 0 */
    CC_W1(mgvbase, CC_FRAME_BUF, (0xc0 | ((vert_off & 0x1) << 5)));

	/* reset FIFO ptrs. */
	CC_W1(mgvbase, CC_FRAME_BUF, 0) ;

msg_printf(DBG,"\nReady to write to %s fbuf ", field ? "even":"odd");


	/* Read from the CC1 framebuffer, line by line */
	for (yy = 0; yy < vert_len; yy++)
	{
	  	/* Read the pixel line into the framebuffer */
	 	for (xx = 0;xx < num_48pages; xx++)
	  	{
			if( _mgv_CC1PollFBRdy( 10 ) )
				return( -1 );

		  	/* Transfer a full 48 byte page */
		 	for (zz = 0; zz < 48; zz++ )
			{
		   		CC_R1(mgvbase, CC_FRM_BUF_DATA, tmp);
				*buf_ptr = tmp;
				msg_printf(DBG,"R%d/%d/%d:%2x ",yy,xx,zz,*buf_ptr);
				buf_ptr++;
			}
		}
		if( partial_page )
		{
			if( _mgv_CC1PollFBRdy(10) )
				return( -1 );

		  	/* Transfer a partial 48 byte page */
		 	for (zz = partial_page; zz > 0; zz--)
			{
		   		CC_R1(mgvbase, CC_FRM_BUF_DATA, *buf_ptr);
				buf_ptr++;
			}
		}
	}
	return(0);
}


/******************************************************
*
*	Function: read_cc1f
*
*	Description:
*
*	Return: None
*
******************************************************/
void read_cc1(unsigned char *odd_array, unsigned char *even_array)
{
	int i;

	msg_printf(DBG,"WRITE ARRAY\n");
	for(i = 0; i < 100; i++)
		msg_printf(DBG,"W%d:%x.%x ",i, *(odd_array+i), *(even_array+i) );


	for(i = 0; i < 100; i++)
	{
		*(odd_array+i) = 0;
		*(even_array+i) = 0;
	}

	rd_CC1_field(
		0,
		100,
		0,
		0,
		ODD_FIELD,
		odd_array);
	rd_CC1_field(
		0,
		100,
		0,
		0,
		EVEN_FIELD,
		even_array);

	msg_printf(DBG,"CC1 READ BACK\n");
	for(i = 0; i < 100; i++)
		msg_printf(DBG,"R%d:%x.%x ",i, *(odd_array+i), *(even_array+i) );
}


/*************************************************************************
*
*		Function: _mgv_compare_byte_buffers( long compare_length, 
*					long max_length,
*					unsigned char *buffer1,
*					unsigned char *buffer2,
*					unsigned char start1,
*					unsigned char start2)
*
*		Description: Compares two buffers byte by byte.
*		The routine searches for 'start1' value for the
*		starting location of buffer1, and similarly with the
*		'start2' value for 'buffer2' (if 'start1' or 'start2'
*		is 0, the starting buffer index is 0).
*		It then compares the buffer values over the distance
*		'compare_length'.
*
*		Any differences are displayed with each array index,
*		both buffer values, and their exclusive or result.

*		Return:  0 - the buffers were identical over the given length
*			 1 - the buffers differed over the given length
*			-1 - a start value was not found within a buffer
*
*		NOTE: 	Since there is no checking for buffer length the
*			'length' must be <= the size of both array buffers
*			individually.
*
***************************************************************************/
int _mgv_compare_byte_buffers( long compare_length, 
				long max_length,
				unsigned char *buffer1,
				unsigned char *buffer2,
				unsigned char start1,
				unsigned char start2)
{

unsigned char buf1ready = FALSE, buf2ready = FALSE;
unsigned char error = FALSE, error_cnt=0;
long i1 = 0, i2 = 0, zz;


    /* setup buffer1 to start value location */

	if( start1 != 0 )
	{
		while(i1 < max_length)
		{
			if( start1 == *buffer1++ )
			{
	            buf1ready = TRUE;
				buffer1--;
				break;
			}
			i1++;
		}
	}
    /* setup buffer2 to start value location */
	if( start2 != 0 )
	{
		while(i2 < max_length)
		{
			if( start2 == *buffer2++ )
			{
				buf2ready = TRUE;
				buffer2--;
				break;
			}
			i2++;
		}
	}

	if( (start1 == 0) && (start2 == 0) )
	{
		buf1ready = buf2ready = TRUE;
	}
msg_printf(DBG,"\nbuf1ready %d buf2ready %d ", buf1ready, buf2ready);

	if( buf1ready && buf2ready )
	{
	/* adjust compare_length if start + complngth now exceeds max_length */
		compare_length =    (compare_length > (max_length - i1)) ?
					(max_length - i1) : compare_length;
		compare_length =    (compare_length > (max_length - i2)) ?
					(max_length - i2) : compare_length;


        /* compare the two buffers */
		msg_printf(SUM,"\nComparing output and input buffers  ");

		for(zz = 0; zz < compare_length; zz++, buffer1++, buffer2++)
		{
			if( *(buffer1) != *(buffer2) )
			{
				msg_printf(ERR,"\nPixel %d: Sent: %2x  Received:%x Difference:%x ",
				zz, *buffer1, *buffer2, (*buffer1)^(*buffer2) );

				if( error_cnt++ > MAX_ERRORS_DISPLAYED)
				{
				msg_printf(ERR,"\nErrors exceeded %d. Test aborted.", MAX_ERRORS_DISPLAYED);
					return(-1);
				}
			}
		}
		if( error_cnt == 0)
			msg_printf(SUM,"    PASSED");
	}
	else
		error = -1;

	return(error);
}


/*************************************************************************
*
*		Function: _mgv_compare_word_buffers( long compare_length, 
*					long max_length,
*					unsigned int *buffer1,
*					unsigned int *buffer2,
*					unsigned int start1,
*					unsigned int start2,
*					unsigned char decimation )
*
*		Description: Compares two buffers word by word.
*		The routine searches for 'start1' value for the
*		starting location of buffer1, and similarly with the
*		'start2' value for 'buffer2' (if 'start1' or 'start2'
*		is 0, the starting buffer index is 0).
*		It then compares the buffer values over the distance
*		'compare_length'.
*
*		Any differences are displayed with each array index,
*		both buffer values, and their exclusive or result.
*
*		Return:  0 - the buffers were identical over the given length
*			 1 - the buffers differed over the given length
*			-1 - a start value was not found within a buffer
*
*		NOTE: 	Since there is no checking for buffer length the
*			'length' must be <= the size of both array buffers
*			individually.
*
***************************************************************************/
int _mgv_compare_word_buffers( long compare_length, 
				long max_length,
				unsigned int *buffer1,
				unsigned int *buffer2,
				unsigned int start1,
				unsigned int start2,
				unsigned char decimation)
{

	unsigned char buf1ready = FALSE, buf2ready = FALSE;
	unsigned char error = FALSE, error_cnt=0;
	long i1 = 0, i2 = 0, zz;
    /* setup buffer1 to start value location */
	if( start1 != 0 )
	{
		while(i1 < max_length)
		{
			if( start1 == *buffer1++ )
			{
	            buf1ready = TRUE;
				buffer1--;
				break;
			}
			i1++;
		}
	}
    /* setup buffer2 to start value location */
	if( start2 != 0 )
	{
		while(i2 < max_length)
		{
			if( start2 == *buffer2++ )
			{
				buf2ready = TRUE;
				buffer2--;
				break;
			}
			i2++;
		}
	}
	if( start1 == start2 == 0 )
		buf1ready = buf2ready = TRUE;
msg_printf(DBG,"\nbuf1ready %d buf2ready %d ", buf1ready, buf2ready);
	if( buf1ready && buf2ready )
	{
		/* adjust compare_length if start + complngth now exceeds max_length */
		compare_length =    (compare_length > (max_length - i1)) ?
										(max_length - i1) : compare_length;
		compare_length =    (compare_length > (max_length - i2)) ?
										(max_length - i2) : compare_length;


        /* compare the two buffers */
		msg_printf(SUM,"\nComparing output and input buffers");

		for(zz = 0; zz < compare_length; zz++, buffer1++, buffer2++)
		{
			if( *(buffer1) != *(buffer2) )
			{
				msg_printf(ERR,"\nPixel %d: Sent: %2x  Received:%x Difference:%x",
								zz, *buffer1, *buffer2, (*buffer1)^(*buffer2) );

				if( error_cnt++ > MAX_ERRORS_DISPLAYED)
				{
					msg_printf(ERR,"\nErrors exceeded %d. Test aborted.", MAX_ERRORS_DISPLAYED);
					return(-1);
				}
			}
		}
		if( error_cnt == 0)
			msg_printf(SUM,"    PASSED");
	}
	else
		error = -1;

	return(error);
}




/*************************************************************************/


float
sin (float x) 
{
	return(x);
}

float
cos(float x)
{
	return(x);
}



