/*
 * tb_test.c
 *
 * This program expands the hex address into binary address.
 *
 *
 */

/* #define DEBUG */
/* #define DEBUG1 */
/* #define DEBUG2 */
/* #define DEBUG3 */

#ifdef MFG_USED

#include <stdio.h>
#include <strings.h>
#include "sys/mc.h"
#include "ide_msg.h"
#include <sys/sbd.h>
#include "sys/types.h"

#define COL_ADR_BITS 10
#define ROW_ADR_BITS 10
#define MIN_UNIT_SIZE 32
#define ONE_K 1024

#define VLD0 0x20000000
#define ADDR_MASK_02 0xff0000
#define MEMCFG_BNK_02 0x40000000
#define SIMM_SIZE_13 0x1f00
#define SIMM_SIZE_02 0x1f000000

static u_int phys_addr;

static int richra;
static int richca;
static col_to_CA();
static row_to_RA();

	/* Define the repeating pattern we want in the cell array.
	 * Define the pattern as the minimum unit to be replicated by
	 * specifying the number of rows and columns (columns must be a
	 * multiple of 2) needed to define the minimum unit. Then 
	 * specify the data for each cell within the unit.
	 */

/*
 *      addr2: 0x6db6db6d, addr1: 0xdb6db6db, addr0: 0xb6db6db6
 *
 *                      011011
 *                      110110
 *                      101101
 *                      011011
 *                      110110
 *                      101101
*/
static int pat_array[18][6] = {
/* pattern 1 */
	{1,1,0,1,1,0},
	{0,1,1,0,1,1},
	{1,0,1,1,0,1},
/*
 *      addr2:0x49249249 , addr1:0x92492492 , addr0: 0x24924924
 *
 *                      010010
 *                      100100
 *                      001001
 *                      010010
 *                      100100
 *                      001001
 *	pattern 2 
*/
	{0,1,0,0,1,0},
	{0,0,1,0,0,1},
	{1,0,0,1,0,0},
/*
 *      addr2:0xdb6db6db , addr1:0xb6db6db6 , addr0: 0x6db6db6d
 *                      
 *                      110110
 *                      101101
 *                      011010
 *                      110110
 *                      101101
 *                      011010
 *	pattern 3
*/
	{0,1,1,0,1,1},
	{1,0,1,1,0,1},
	{1,1,0,1,1,0},
/*
 *      addr2:0x92492492 , addr1:0x24924924 , addr0: 0x49249249
 *
 *                      100100
 *                      001001
 *                      010010
 *                      100100
 *                      001001
 *                      010010
 *	pattern 4
*/
	{0,0,1,0,0,1},
	{1,0,0,1,0,0},
	{0,1,0,0,1,0},
/*
 *      addr2:0xb6db6db6 , addr1:0x6db6db6d , addr0: 0xdb6db6db
 *
 *                      101101
 *                      011011
 *                      110110
 *                      101101
 *                      011011
 *                      110110
 *	pattern 5
*/
	{1,0,1,1,0,1},
	{1,1,0,1,1,0},
	{0,1,1,0,1,1},
/*
 *      addr2:0x24924924 , addr1:0x49249249 , addr0: 0x92492492
 *
 *                      001001
 *                      010010
 *                      100100
 *                      001001
 *                      010010
 *                      100100
 *	pattern 6
*/
	{1,0,0,1,0,0},
	{0,1,0,0,1,0},
	{0,0,1,0,0,1}
};

int
tb_test(u_int first, u_int last, struct tstvar *tstparms)
{
	u_int localMEMCFG0, localMEMCFG1;
	u_int addr_mask, bank_base_addr, num_banks;
	int doublehigh, simmsize;
	u_int offset;
	int i, j, k, x, repeat_reads;
	int fail1, fail2;
	int bank0, bank1, bank2, bank3, start_bank;
	int num_pats = 6;

	simmsize = 0;
	fail1 = fail2 = 0;
	repeat_reads = 6;
	bank0 = bank1 = bank2 = bank3 = 0;

	/* only test upper banks */
	localMEMCFG0 = 	*(unsigned int *)PHYS_TO_K1(MEMCFG0);
	localMEMCFG1 = 	*(unsigned int *)PHYS_TO_K1(MEMCFG1);

	msg_printf(VRB, "MEMCFG0: 0x%x\n", localMEMCFG0);
	msg_printf(VRB, "MEMCFG1: 0x%x\n", localMEMCFG1);

	msg_printf(DBG, "vld0:0x%x & memcfg0:0x%x = %x\n",
		VLD0, localMEMCFG0, (VLD0 & localMEMCFG0));
	if ((VLD0 & localMEMCFG0) == 0) { /* bank 0 is invalid */
		msg_printf(INFO,"Bank 0 is invalid or empty.\n");
	}
	else bank0 = 1;

	msg_printf(DBG, "vld:0x%x & memcfg0:0x%x = %x\n",
		MEMCFG_VLD, localMEMCFG0, (MEMCFG_VLD & localMEMCFG0));
	if ((MEMCFG_VLD & localMEMCFG0) == 0) { /*  0x2000, bank 1 is invalid */
		msg_printf(INFO,"Bank 1 is invalid or empty.\n");
	}
	else bank1 = 1;


	if (VLD0 & localMEMCFG1)  /* bank 2 is valid! */
		bank2 = 1;
	if (MEMCFG_VLD & localMEMCFG1)
		bank3 = 1;

	num_banks = bank0 + bank1 + bank2 + bank3;
	msg_printf(DBG,"num_banks: %d\n", num_banks);

	/* determine which banks to test */
	if (bank0) {
		if (bank1) {
			start_bank = 1;
		}
		else {
			msg_printf(SUM,"Only bank 0 is valid. Skipping test\n");
			return(0);		
		}
	}
	else {
		if (bank1 & bank2) {
			start_bank = 2;
			msg_printf(SUM,"Banks 1 and 2 are valid. Testing bank 2\n");
			/* alter num_banks for the for loop */
			num_banks = 3;
		}
		else {
			msg_printf(SUM,"Only bank is valid. Skipping test\n");
			return(0);
		}
	}

	/* write memory */
   for (x = 1; x <= num_pats; x++) { 
	msg_printf(INFO, "Writing memory...\n");
	for (i = start_bank; i < num_banks; i++) { 
	   /* set which addr mask to use */
	   if (i == 1 || i == 3) 
		addr_mask = MEMCFG_ADDR_MASK; /* 0x00ff */
	   else if (i == 2)
		addr_mask = ADDR_MASK_02;

	   if (i == 1) 
		bank_base_addr = addr_mask & localMEMCFG0;
	   else if (i == 2) {
		bank_base_addr = addr_mask & localMEMCFG1;
		bank_base_addr = bank_base_addr >> 16; /* put into low byte */
	   }
	   else if (i == 3) {
		bank_base_addr = addr_mask & localMEMCFG1;
	   }
	   
	   msg_printf(DBG, "bank_base_addr: 0x%x\n", bank_base_addr);
	   /* now shift 22 bits left to get the actual base address */
	   bank_base_addr = (bank_base_addr << 22);

 	   for (j = 0; j < 4; j++) { /* write single-sided,1st side of double */
		   msg_printf(INFO,"Writing bank %d, simm %d, pat: %d\n",i,j,x);
		   tb_1simm_wr(bank_base_addr, j, x); 
	   }

	   /* check for double high simms */
	   doublehigh = 0;
	   if (i == 1) {
		if (MEMCFG_BNK & localMEMCFG0) {
				doublehigh = 1;
				simmsize = (SIMM_SIZE_13 & localMEMCFG0);
				simmsize = simmsize >> 8;
		}
	   }
	   else if (i == 2) {
		if (MEMCFG_BNK_02 & localMEMCFG1) {
				doublehigh = 1;
				simmsize = (SIMM_SIZE_02 & localMEMCFG1);
				simmsize = simmsize >> 24;
		}
	   }
	   else if (i == 3) {
		if (MEMCFG_BNK & localMEMCFG1) {
				doublehigh = 1;
				simmsize = (SIMM_SIZE_13 & localMEMCFG1);
				simmsize = simmsize >> 8;
		}
	   }
	   msg_printf(DBG,"simmsize: 0x%x, dhigh: %d\n", 
			simmsize, doublehigh);
	   if (doublehigh) {
		switch (simmsize) {
			case 1: offset = 0x400000; break;
			case 7: offset = 0x1000000; break;
			case 0x1f: offset = 0x8000000; break;
			default: offset = 0; break;
		}
		msg_printf(DBG,"DOUBLE HIGH WRITE, offset: 0x%x\n", offset);
 	   	for (j = 0; j < 4; j++) { /* foreach simm double-sided */
			msg_printf(INFO,"Writing bank %d, simm %d hi, pat:%d\n",
				i, j, x);
			tb_1simm_wr(bank_base_addr + offset, j, x);
	    	} 
	   } /* double */
	} /* foreach bank */	

	/* read memory */
	msg_printf(INFO,"Reading memory...\n");
	for (i = start_bank; i < num_banks; i++) {
	   /* set which addr mask to use */
	   if (i == 1 || i == 3) 
		addr_mask = MEMCFG_ADDR_MASK; /* 0x00ff */
	   else if (i == 2)
		addr_mask = ADDR_MASK_02;

	   if (i == 1) 
		bank_base_addr = addr_mask & localMEMCFG0;
	   else if (i == 2) {
		bank_base_addr = addr_mask & localMEMCFG1;
                bank_base_addr = bank_base_addr >> 16; /* put into low byte */
	   }
	   else if (i == 3) 
		bank_base_addr = addr_mask & localMEMCFG1;

	   
	   msg_printf(DBG, "bank_base_addr: 0x%x\n", bank_base_addr);
	   /* now shift 22 bits left to get the actual base address */
	   bank_base_addr = (bank_base_addr << 22);

 	   for (j = 0; j < 4; j++) { /* rd single-sided, 1st side of double */
		   msg_printf(INFO,"Reading bank %d, simm %d, pat: %d\n",i,j,x);
		   fail1 |= tb_1simm_rd(bank_base_addr, j, x); 
	   }

	   /* check for double high simms */
	   doublehigh = 0;
	   if (i == 1) {
		if (MEMCFG_BNK & localMEMCFG0) {
				doublehigh = 1;
				simmsize = (SIMM_SIZE_13 & localMEMCFG0);
				simmsize = simmsize >> 8;
		}
	   }
	   else if (i == 2) {
		if (MEMCFG_BNK_02 & localMEMCFG1) {
				doublehigh = 1;
				simmsize = (SIMM_SIZE_02 & localMEMCFG1);
				simmsize = simmsize >> 24;
		}
	   }
	   else if (i == 3) {
		if (MEMCFG_BNK & localMEMCFG1) {
				doublehigh = 1;
				simmsize = (SIMM_SIZE_13 & localMEMCFG1);
				simmsize = simmsize >> 8;
		}
	   }
	   msg_printf(DBG,"simmsize: 0x%x, dhigh: %d\n", 
			simmsize, doublehigh);
	   if (doublehigh) {
		switch (simmsize) {
			case 1: offset = 0x400000; break;
			case 7: offset = 0x1000000; break;
			case 0x1f: offset = 0x8000000; break;
			default: offset = 0; break;
		}
		msg_printf(DBG,"DOUBLE HIGH READ, offset 0x%x\n", offset);	

		for (j = 0; j < 4; j++) { /* foreach simm double-sided */
			msg_printf(INFO,"Reading bank %d, simm %d hi, pat:%d\n",
				i, j, x);
			fail2 |= tb_1simm_rd(bank_base_addr + offset, j,x);
	        } 
	   }
	} /* foreach bank */	
   } /* foreach pattern */
	if (fail1 || fail2)
		return (1);
	else
		return (0);
}
				
int 
tb_1simm_wr(u_int first, int simm_num, int patnum)
{
	int i, j, k, tmp;
	register int num_rows, num_cols;
	register int row_a, col_a;
	int d_in;
	int data[2];
	int unit_cell_array[MIN_UNIT_SIZE][MIN_UNIT_SIZE]; /* [row][col] */	
	int num_invert, num_same;
	u_int *ptr;

	num_invert = num_same = 0;

	/* initialize cell_array */
	for (i = 0; i < MIN_UNIT_SIZE; i++)
		for (j = 0; j < MIN_UNIT_SIZE; j++)
			unit_cell_array[i][j] = 0;


	first = 0xa0000000 | first;

	num_rows = 3;
	num_cols = 6;

	/* Now we populate the minimun unit with the correct data */
	/* This is for three-bit */

	/* row 0 */
	/* unit_cell_array[0][5] = 0; /* row 0, col 2-1 - note bit swap */
	/* unit_cell_array[0][4] = 1; /* row 0, col 2-2 - note bit swap */
	/* unit_cell_array[0][3] = 1; /* row 0, col 1-2 */
	/* unit_cell_array[0][2] = 0; /* row 0, col 1-1 */
	/* unit_cell_array[0][1] = 1; /* row 0, col 0-2 */
	/* unit_cell_array[0][0] = 1; /* row 0, col 0-1 */

	/* populate the unit_cell_array */
	for (i = 0; i < num_rows; i++)
	   for (j = 0; j < num_cols; j++)
		unit_cell_array[i][j] = pat_array[3*patnum - 3 + i][j];

	/* Descramble the row_adr, col_adr into a physical hex address */
	for (row_a = 0; row_a < ONE_K; row_a++) {
	   for (col_a = 0; col_a < ONE_K; col_a++) {
		col_to_CA(col_a);
		row_to_RA(row_a);
		/* Put the row and column together to form the physical addr */
                phys_addr= richra << 13;
                phys_addr = phys_addr | ((richca & 0x1ff) << 4);
                phys_addr = phys_addr | ((richca & 0x200) << 14);


		/* Generate the 'internal data' and then see if we need 
		 * to invert it. The inversion is based on :
		 * 	~((RA0 ^ RA1) ^ CA2 ) is 1, then invert the data
		 *
		 * Also, every 4n+2, 4n+3 columns will swap bits 1 and 2 (0 
		 * and 1 for programming uses).
		 */

		data[0] = 
		   unit_cell_array[row_a % num_rows][(col_a*2) % num_cols];
		data[1] = 
		   unit_cell_array[row_a % num_rows][(col_a*2+1) % num_cols];
		
		/* now figure out if we need to swap bits */
		if (col_a % 4 == 2 || col_a % 4 == 3) { /* do swap if true */
			tmp = data[0];
			data[0] = data[1];
			data[1] = tmp;
		}

		/* now figure out if we invert the data based on 
		 * if ((RA[0]^RA[1])^CA[1] == 0 ) 
		 */
		if( (richra &1) ^ ((richra >> 1) &1) ^ ((richca >>1) &1) == 0){

#ifdef DEBUG3
			num_invert++;
#endif
			if (data[0] == 0)
				data[0] = 1;
			else
				data[0] = 0;
			if (data[1] == 0)
				data[1] = 1;
			else
				data[1] = 0;
		}
#ifdef DEBUG3
		else num_same++;
#endif

#ifdef DEBUG
		printf("%d%d\t", data[1], data[0]);
#endif

		/* now output the 32-bit data that we should write */
		if (data[0] == 0 && data[1] == 0)
			d_in = 0x0;
		else if (data[0] == 1 && data[1] == 0)
			d_in = 0x5555;
		else if (data[0] == 0 && data[1] == 1)
			d_in = 0xaaaa;
		else if (data[0] == 1 && data[1] == 1)
			d_in = 0xffff;		
	
#ifdef DEBUG
		if(simm_num ==0) {
			printf("din 0x%x\t0x%x  ", d_in, phys_addr);
			printf("row_a %x, col_a %x\n", row_a, col_a);
		}
#endif
		/* now we actually store the data */
		/* make phys_addr in k1 space */


		phys_addr = first | phys_addr;

		/* now or the phys_addr according to which simm we're testing */
		phys_addr = phys_addr | (4 * simm_num);
		if (phys_addr >= 0xa8000000) {
			ptr = (u_int *)phys_addr;
			*ptr = d_in;
		 /* msg_printf(VRB, 
			  "Addr:0x%x, data: 0x%x, row_a:0x%x, col_a:0x%x\n",
				 ptr, *ptr, row_a, col_a);
			msg_printf(DBG, "first: 0x%x, simm_num: %d\n", 						first, simm_num);
		*/	
		}
		else {
			printf("Addr is < 8MB: 0x%x, data: 0x%x\n", 
							phys_addr, d_in);
			msg_printf(DBG, "first: 0x%x, simm_num: %d\n", 						first, simm_num);
		}

	   } /* for col_a */
	} /* for row_a */
	return (0);
#ifdef DEBUG3
	printf("num_invert: %d, num_same: %d\n", num_invert, num_same);
#endif
} /* tb_1simm_wr */


int 
tb_1simm_rd(u_int first, int simm_num, u_int patnum)
{
	int i, j, k, tmp;
	register int num_rows, num_cols;
	register int row_a, col_a;
	int d_in, test_data;
	int data[2];
	int unit_cell_array[MIN_UNIT_SIZE][MIN_UNIT_SIZE]; /* [row][col] */	
	int num_invert, num_same;
	u_int *ptr;
	int fail = 0;
	char str[8];

	num_invert = num_same = 0;

	/* initialize cell_array */
	for (i = 0; i < MIN_UNIT_SIZE; i++)
		for (j = 0; j < MIN_UNIT_SIZE; j++)
			unit_cell_array[i][j] = 0;


	first = 0xa0000000 | first;

	num_rows = 3;
	num_cols = 6;

	/* Now we populate the minimun unit with the correct data */
	/* This is for three-bit */

	/* row 0 */
	/* unit_cell_array[0][5] = 0; /* row 0, col 2-1 - note bit swap */
	/* unit_cell_array[0][4] = 1; /* row 0, col 2-2 - note bit swap */
	/* unit_cell_array[0][3] = 1; /* row 0, col 1-2 */
	/* unit_cell_array[0][2] = 0; /* row 0, col 1-1 */
	/* unit_cell_array[0][1] = 1; /* row 0, col 0-2 */
	/* unit_cell_array[0][0] = 1; /* row 0, col 0-1 */

	/* populate the unit_cell_array */
	for (i = 0; i < num_rows; i++)
	   for (j = 0; j < num_cols; j++)
		unit_cell_array[i][j] = pat_array[3*patnum - 3 + i][j];

	/* Descramble the row_adr, col_adr into a physical hex address */
	for (row_a = 0; row_a < ONE_K; row_a++) {
	   for (col_a = 0; col_a < ONE_K; col_a++) {
		col_to_CA(col_a);
		row_to_RA(row_a);
		/* Now put the row and column address together to form
		 * the physical address.
		 */
                phys_addr= richra << 13;
                phys_addr = phys_addr | ((richca & 0x1ff) << 4);
                phys_addr = phys_addr | ((richca & 0x200) << 14);


		/* Generate the 'internal data' and then see if we need 
		 * to invert it. The inversion is based on :
		 * 	~((RA0 ^ RA1) ^ CA2 ) is 1, then invert the data
		 *
		 * Also, every 4n+2, 4n+3 columns will swap bits 1 and 2 (0 
		 * and 1 for programming uses).
		 */
	
		data[0] = 
		   unit_cell_array[row_a % num_rows][(col_a*2) % num_cols];
		data[1] = 
		   unit_cell_array[row_a % num_rows][(col_a*2+1) % num_cols];
		
		/* now figure out if we need to swap bits */
		if (col_a % 4 == 2 || col_a % 4 == 3) { /* do swap if true */
			tmp = data[0];
			data[0] = data[1];
			data[1] = tmp;
		}

#ifdef DEBUG
		if(simm_num ==0) {
			printf("din 0x%x\t0x%x  ", d_in, phys_addr);
			printf("row_a %x, col_a %x\n", row_a, col_a);
		}
#endif

		/* now figure out if we invert the data based on 
		 * if ((RA[0]^RA[1])^CA[1] == 0 )
		 */
		if( (richra &1) ^ ((richra >> 1) &1) ^ ((richca >>1) &1) ==0 ) {
#ifdef DEBUG3
			num_invert++;
#endif
			if (data[0] == 0)
				data[0] = 1;
			else
				data[0] = 0;
			if (data[1] == 0)
				data[1] = 1;
			else
				data[1] = 0;
		}
#ifdef DEBUG3
		else num_same++;
#endif

#ifdef DEBUG
		printf("%d%d\t", data[1], data[0]);
#endif

		/* now output the 32-bit data that we should write */
		if (data[0] == 0 && data[1] == 0)
			d_in = 0x0;
		else if (data[0] == 1 && data[1] == 0)
			d_in = 0x5555;
		else if (data[0] == 0 && data[1] == 1)
			d_in = 0xaaaa;
		else if (data[0] == 1 && data[1] == 1)
			d_in = 0xffff;		
	
#ifdef DEBUG
		printf("rdin 0x%x\t0x%x\n", d_in, phys_addr);
	
#endif
		/* now we actually read the data */
		/* make phys_addr in k1 space */


		phys_addr = first | phys_addr;

		/* now or the phys_addr according to which simm we're testing */
		phys_addr = phys_addr | (4 * simm_num);
		ptr = (u_int *)phys_addr;
		test_data = *ptr;
		if (test_data != d_in)  {
			   msg_printf(ERR, 
			     "ERR: Addr: 0x%x, expected: 0x%x, read: 0x%x\n", 
				ptr, d_in, test_data);
			   msg_printf(ERR, "row_a: 0x%x, col_a: 0x%x\n", 
				row_a, col_a);
			   msg_printf(ERR, "first: 0x%x, simm_num: %d\n", 
				first, simm_num);
			   fail = 1;
			   msg_printf(ERR,"Hit CR to continue: ");
			   gets(str);
		}	
	/* 
		else {
		   msg_printf(DBG, "Read OK: addr:0x%x, data:0x%x\n",
				ptr, test_data);
			}
	*/
	   } /* for col_a */
	} /* for row_a */
	return (fail);
#ifdef DEBUG3
	printf("num_invert: %d, num_same: %d\n", num_invert, num_same);
#endif
}

/* translates a col_a to the corresponding CA-address */
static int
col_to_CA(int col_a)
{
	int i, tmp;
	register int ca;
	register int column_adr;

	column_adr = col_a;

	/* Bit 9 */
	ca = (~(column_adr & 1) & 1) << 9;

	/* Bit 8 */
	ca |= (column_adr & 0x200) >>1;

	/* Bit 7 */
	ca |= (column_adr & 0x100) >>1;

	/* Bit 6 */
	ca |= (column_adr & 0x80) >>1;

	/* Bit 5 */
	ca |= (column_adr & 0x40) >>1;

	/* Bit 4 */
	ca |= (column_adr & 0x20) >>1;

	/* Bit 3 */
	ca |= (column_adr & 0x10) >>1;

	/* Bit 2 */
	ca |= (column_adr & 0x8) >>1;

	/* Bit 1 */
	ca |= (column_adr & 0x4) >>1;

	/* Bit 0 */
	ca |= (column_adr & 0x2) >>1 ^  ((ca & 0x2) >>1);
	msg_printf(DBG, "  bit 0 ca %x, col_a %x\n", ca, column_adr);
	richca = ca;
	return;
}


/* translates row_addr to RA-address */
static int
row_to_RA(int row_a)
{
        int i, tmp;
	register int ra;
	register int tmp2;
	register int rowa;

	rowa = row_a;

	/* Bit 9 */
	ra = ( ~(rowa & 0x200) & 0x200);

	/* Bit 8 */
	ra |= (~(rowa & 0x100) & 0x100);

	tmp2 = ra & 0x100;
	/* Bit 7 */
	ra |= (rowa & 0x80);

	/* Bit 6 */
	ra |= (rowa & 0x40);

	/* Bit 5 */
	ra |= (rowa & 0x20);

	/* Bit 4 */
	ra |= (rowa & 0x10);

	/* Bit 3 */
	ra |= (rowa & 0x8);

	/* Bit 2 */
	ra |= (rowa & 0x4);
	
	/* Bit 1 */
	ra |= (rowa & 0x2)  ^ (ra & 0x4) >> 1;

	/* Bit 0 */
	ra |= (~((rowa & 0x1)  ^ (ra & 0x2) >> 1)) & 1;

	richra = ra;
	return;
}

#endif	/* MFG_USED */
