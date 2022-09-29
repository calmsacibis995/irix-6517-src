/*
**               Copyright (C) 1989, Silicon Graphics, Inc.              
**                                                                      
**  These coded instructions, statements, and computer programs  contain 
**  unpublished  proprietary  information of Silicon Graphics, Inc., and 
**  are protected by Federal copyright law.  They  may  not be disclosed  
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc. 
*/

/*
 *      FileName:       mgv_edh_mem.c
 *      $Revision: 1.2 $
 */

/*
 * These files get compiled only for speed racer
 */
#ifdef IP30
#include "mgv_diag.h"

int _mgvEdhWordTest(int);
int mgvEdhWordTest(void);
int set_all_edh(int);
int set_all_edh_rw(uchar_t);
int set_edh_rw(int, int, uchar_t);
int read_edh_rw(int, int);
void _mgv_init_shadow_write(void);
void _mgv_init_shadow_read(void);
int _mgv_test_edh_read(void);
int mgv_test_edh_read(void);
int _mgv_active_crc(void);
int _mgv_full_crc(void);
int _mgv_standard(void);

int
mgvEdhWordTest(void)
{
  int edh_num;     /* 0 or 1 */
  int channel ;    /* 0 or 1 */
  int i, status = 0;

  /*** Reset I2C control ***/
  mgv_reset_i2c;
  delay(300);
  /*** Initialize I2C bus ***/
  mgv_initI2C();

  for (channel = 0; channel < 2; channel++) {
     for (edh_num=0; edh_num < 2; edh_num++) {

        msg_printf(SUM,"Testing EDH %d on channel %d\n",edh_num, channel);

/* Get specific EDH ready for read and write */
	mgv_select_edh_part(channel,edh_num,0,0);

  status = _mgvEdhWordTest(edh_num);
      if (status) {
          for (i=0;i<15; i++) {
	     msg_printf(SUM,"Value of word %d = 0x%x\n", i, shadow_edh_iic_read[i]);
          }
          return(status);
       }  /* end if */
     }
  }
  return(status);
}

int
_mgvEdhWordTest(int edh_num)
{

  char send_val, rcv_val;
  int rw, rw_bit;
  int status = 0;

/* Get specific EDH ready for read and write */
  _mgv_init_shadow_write();

  _mgv_init_shadow_read();

/* Set up through DCB bus */

/* Walking 1s and Walking 0s test */

/* Walking 1s  test */
   msg_printf(SUM,"Walking 1s test......\n");

/* For Reserved words 1 to 7 */
   for (rw = 1; rw <= 7; rw++) {
     for (rw_bit = 2; rw_bit <= 7; rw_bit++) {

       if (status = set_all_edh_rw(0)){ 
	 msg_printf(SUM, "ERROR Clearing all EDH for: RW %d RW_BIT %d\n",rw, rw_bit);
	 return(status);
       };
 
       if (status = set_edh_rw(rw, rw_bit,1)){ 
	 msg_printf(SUM, "ERROR Setting EDH: RW %d RW_BIT %d\n",rw, rw_bit);
	 return(status);
       };

	/* Inititialize shadow read regs to all 0s */
  	_mgv_init_shadow_read();

       if (read_edh_rw(rw,rw_bit) != 1){ 
	 msg_printf(SUM, "ERROR in reading  RW %d RW_BIT %d\n",rw, rw_bit);
	 return(-1);
       };
 
     } /* end inner for */

   } /* end outer for */


/* Walking 0s  test */
   msg_printf(SUM,"Walking 0s test......\n");

/* For Reserved words 1 to 7 */
   for (rw = 1; rw <= 7; rw++) {
     for (rw_bit = 2; rw_bit <= 7; rw_bit++) {

       if (status = set_all_edh_rw(1)){ 
	 msg_printf(SUM, "ERROR Setting all EDH for: RW %d RW_BIT %d\n",rw, rw_bit);
	 return(status);
       };
 
       if (status = set_edh_rw(rw, rw_bit,0)){ 
	 msg_printf(SUM, "ERROR Clearing EDH: RW %d RW_BIT %d\n",rw, rw_bit);
	 return(status);
       };

       if (read_edh_rw(rw,rw_bit) != 0){ 
	 msg_printf(SUM, "ERROR in reading  RW %d RW_BIT %d\n",rw, rw_bit);
	 return(-1);
       }

     } /* end inner for */

   } /* end outer for */

  return(status);

} /* end of _mgvEdhWordTest */

void 
_mgv_init_shadow_write(void)
{

   edh_w1.val = shadow_edh_iic_write[0] = 0x0;
   edh_w2.val = shadow_edh_iic_write[1] = 0x0;
   edh_w3.val = shadow_edh_iic_write[2] = 0x0;
   edh_w4.val = shadow_edh_iic_write[3] = 0x80;
   edh_w5.val = shadow_edh_iic_write[4] = 0xff;
   edh_w6.val = shadow_edh_iic_write[5] = 0x9f;
   edh_w7.val = shadow_edh_iic_write[6] = 0x0;
   edh_w8.val = shadow_edh_iic_write[7] = 0x0;
   edh_w9.val = shadow_edh_iic_write[8] = 0x0;
   edh_w10.val = shadow_edh_iic_write[9] = 0x0;
   edh_w11.val = shadow_edh_iic_write[10] = 0x0;
   edh_w12.val = shadow_edh_iic_write[11] = 0x0;
}

void
_mgv_init_shadow_read(void)
{
   int i;

   for (i=0; i<16; i++) {
	shadow_edh_iic_read[i] = 0x0;
   }
   edh_r1.val = edh_r2.val = edh_r3.val = 0x0;
   edh_r4.val = edh_r5.val = edh_r6.val = 0x0;
   edh_r7.val = edh_r8.val = edh_r9.val = 0x0;
   edh_r10.val = edh_r11.val = edh_r12.val = 0x0;
   edh_r13.val = edh_r14.val = edh_r15.val = 0x0;

}

int
_mgv_standard(void) 
{
    int tv_standard;

    tv_standard = shadow_edh_iic_read[2];
    tv_standard &= 0x04;
    tv_standard >>= 2;

    return tv_standard;
}

/*
 * returns crc of active picture in last frame
 */
int
_mgv_active_crc(void)
{
    int active_crc_hi, active_crc_lo, active_crc;

    active_crc_hi = shadow_edh_iic_read[5];
    active_crc_lo = shadow_edh_iic_read[6];

    active_crc = (active_crc_hi & 0x00FF) << 8
               | (active_crc_lo & 0x00FF);

    return active_crc;
}


/*
 * returns crc of last full frame
 */
int
_mgv_full_crc(void)
{
    int full_crc_hi, full_crc_lo, full_crc;

    full_crc_hi = shadow_edh_iic_read[7];
    full_crc_lo = shadow_edh_iic_read[8];

    full_crc = (full_crc_hi & 0x00FF) << 8 | (full_crc_lo & 0x00FF);

    return full_crc;
}


int
mgv_test_edh_read(void)
{
  int edh_num;     /* 0 or 1 */
  int channel ;    /* 0 or 1 */
  int status = 0;
  unsigned char  xp;
  int active_crc, full_field_crc, tv_std;

  /*** Reset I2C control ***/
  mgv_reset_i2c;
  delay(300);
  /*** Initialize I2C bus ***/
  mgv_initI2C();

  /*** Set up VBAR to send vin signal to vout ***/
  xp = GAL_DIGITAL_CH1_IN_SHIFT | GAL_OUT_CH1;
  GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
  xp = GAL_DIGITAL_CH2_IN_SHIFT | GAL_OUT_CH2;
  GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
 
  for (channel = 0; channel < 2; channel++) {
     for (edh_num=0; edh_num < 2; edh_num++) {

   msg_printf(SUM,"Testing channel %d, EDH %d\n",channel, edh_num);

/* Get specific EDH ready for read and write */
	mgv_select_edh_part(channel,edh_num,0,0);

        status = _mgv_test_edh_read();
        if (!status) {
		if (tv_std = _mgv_standard()) {
			msg_printf(SUM,"TV Standard = PAL(%x)......\n",tv_std);
		}
		else {
			msg_printf(SUM,"TV Standard = NTSC(%x).....\n",tv_std);
		} 
		active_crc = _mgv_active_crc();
		msg_printf(SUM,"Active picture CRC  = 0x%x\n",active_crc);
		full_field_crc = _mgv_full_crc();
		msg_printf(SUM,"Full Field CRC  = 0x%x\n",full_field_crc);
         }
         else return(status);
     }
  }
  return(status);
}


/*** Test read routine ***/

int 
_mgv_test_edh_read(void)
{
   int i, status = 0;

   status = set_all_edh_rw(1);
   if (status) {
	msg_printf(SUM, "Error setting all 15 words \n");
        return(status);
   }

   delay(60);
   _mgv_init_shadow_read();

   status = _mgv_edh_r1(15,&(edh_r15.val));
   if (status) {
	msg_printf(SUM, "Error reading all 15 words \n");
   }

   for (i=0;i<15; i++) {
	msg_printf(DBG,"Value of word %d = 0x%x\n", i, shadow_edh_iic_read[i]);
   }
 
   return(status);
}


/* This function sets all reserved word bits to either 0 or 1  */
/* The other words in the EDH - flags, sensitivity flags */
/* are set always to the initial values, as in _mgv_init_shadow_write */
/* So, if you want to initialize EDH use following routine with */
/* 0 as argument */

int
set_all_edh_rw(uchar_t set_val)
{
   int i;
   int status = 0;

   if (!set_val) {
	_mgv_init_shadow_write();
        /*** Send the data now thru i2c bus ***/
        /* 12 is the last word */
  	status = _mgv_edh_w1(12,0x0); 
   }
   else {
	_mgv_init_shadow_write();
	/* just set bit7 and bit6 */  
        edh_w7.val = shadow_edh_iic_write[6] = 0xc0;
   	edh_w8.val = shadow_edh_iic_write[7] = 0xff;
   	edh_w9.val = shadow_edh_iic_write[8] = 0xff;
   	edh_w10.val = shadow_edh_iic_write[9] = 0xff;
   	edh_w11.val = shadow_edh_iic_write[10] = 0xff;
   	edh_w12.val = shadow_edh_iic_write[11] = 0xff;
  	status = _mgv_edh_w1(12, 0xff);
	
   }

/* Setting all reserved words was successful or failure */
   return(status); 

 } /* set_all_edh_rw */




/* This function sets a specific bit in a specific reserved word to 0 or 1  */

int
set_edh_rw(int rw, int rw_bit, uchar_t set_val)
{
   int status = 0;

   switch (rw) {
   case 1:
     switch (rw_bit) {
     case 2:
       edh_w7.bits.rw1_b2 = set_val;
       if (status = _mgv_edh_w1(7,edh_w7.val))
	 return (-1);
       break;
     case 3:
       edh_w7.bits.rw1_b3 = set_val;
       if (status = _mgv_edh_w1(7,edh_w7.val))
	 return (-1);
       break;
     case 4:
       edh_w8.bits.rw1_b4 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 5:
       edh_w8.bits.rw1_b5 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 6:
       edh_w8.bits.rw1_b6 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 7:
       edh_w8.bits.rw1_b7 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }

   case 2:
     switch (rw_bit) {
     case 2:
       edh_w8.bits.rw2_b2 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 3:
       edh_w8.bits.rw2_b3 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 4:
       edh_w8.bits.rw2_b4 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 5:
       edh_w8.bits.rw2_b5 = set_val;
       if (status = _mgv_edh_w1(8,edh_w8.val))
	 return (-1);
       break;
     case 6:
       edh_w9.bits.rw2_b6 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     case 7:
       edh_w9.bits.rw2_b7 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 3:
     switch (rw_bit) {
     case 2:
       edh_w9.bits.rw3_b2 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     case 3:
       edh_w9.bits.rw3_b3 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     case 4:
       edh_w9.bits.rw3_b4 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     case 5:
       edh_w9.bits.rw3_b5 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     case 6:
       edh_w9.bits.rw3_b6 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     case 7:
       edh_w9.bits.rw3_b7 = set_val;
       if (status = _mgv_edh_w1(9,edh_w9.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 4:
     switch (rw_bit) {
     case 2:
       edh_w10.bits.rw4_b2 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 3:
       edh_w10.bits.rw4_b3 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 4:
       edh_w10.bits.rw4_b4 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 5:
       edh_w10.bits.rw4_b5 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 6:
       edh_w10.bits.rw4_b6 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 7:
       edh_w10.bits.rw4_b7 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 5:
     switch (rw_bit) {
     case 2:
       edh_w10.bits.rw5_b2 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 3:
       edh_w10.bits.rw5_b3 = set_val;
       if (status = _mgv_edh_w1(10,edh_w10.val))
	 return (-1);
       break;
     case 4:
       edh_w11.bits.rw5_b4 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 5:
       edh_w11.bits.rw5_b5 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 6:
       edh_w11.bits.rw5_b6 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 7:
       edh_w11.bits.rw5_b7 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);
     }
   case 6:
     switch (rw_bit) {
     case 2:
       edh_w11.bits.rw6_b2 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 3:
       edh_w11.bits.rw6_b3 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 4:
       edh_w11.bits.rw6_b4 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 5:
       edh_w11.bits.rw6_b5 = set_val;
       if (status = _mgv_edh_w1(11,edh_w11.val))
	 return (-1);
       break;
     case 6:
       edh_w12.bits.rw6_b6 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     case 7:
       edh_w12.bits.rw6_b7 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 7:
     switch (rw_bit) {
     case 2:
       edh_w12.bits.rw7_b2 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     case 3:
       edh_w12.bits.rw7_b3 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     case 4:
       edh_w12.bits.rw7_b4 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     case 5:
       edh_w12.bits.rw7_b5 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     case 6:
       edh_w12.bits.rw7_b6 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     case 7:
       edh_w12.bits.rw7_b7 = set_val;
       if (status = _mgv_edh_w1(12,edh_w12.val))
	 return (-1);
       break;
     default:
       msg_printf(SUM, "ERROR - Setting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }

   } /* main switch */


/* Setting reserved bit was successful */
   return(0); 

 } /* set_edh_rw */


int
read_edh_rw(int rw, int rw_bit) {
   int status = 0;
   switch (rw) {
   case 1:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw1_b2);
     case 3:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw1_b3);
     case 4:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw1_b4);
     case 5:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw1_b5);
     case 6:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw1_b6);
     case 7:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw1_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 2:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw2_b2);
     case 3:
       if (status = _mgv_edh_r1(10,&(edh_r10.val)))
	 return (-1);
       else return(edh_r10.bits.rw2_b3);
     case 4:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw2_b4);
     case 5:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw2_b5);
     case 6:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw2_b6);
     case 7:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw2_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 3:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw3_b2);
     case 3:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw3_b3);
     case 4:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw3_b4);
     case 5:
       if (status = _mgv_edh_r1(11,&(edh_r11.val)))
	 return (-1);
       else return(edh_r11.bits.rw3_b5);
     case 6:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw3_b6);
     case 7:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw3_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 4:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw4_b2);
     case 3:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw4_b3);
     case 4:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw4_b4);
     case 5:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw4_b5);
     case 6:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw4_b6);
     case 7:
       if (status = _mgv_edh_r1(12,&(edh_r12.val)))
	 return (-1);
       else return(edh_r12.bits.rw4_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 5:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw5_b2);
     case 3:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw5_b3);
     case 4:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw5_b4);
     case 5:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw5_b5);
     case 6:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw5_b6);
     case 7:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw5_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 6:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw6_b2);
     case 3:
       if (status = _mgv_edh_r1(13,&(edh_r13.val)))
	 return (-1);
       else return(edh_r13.bits.rw6_b3);
     case 4:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw6_b4);
     case 5:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw6_b5);
     case 6:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw6_b6);
     case 7:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw6_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }
   case 7:
     switch (rw_bit) {
     case 2:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw7_b2);
     case 3:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw7_b3);
     case 4:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw7_b4);
     case 5:
       if (status = _mgv_edh_r1(14,&(edh_r14.val)))
	 return (-1);
       else return(edh_r14.bits.rw7_b5);
     case 6:
       if (status = _mgv_edh_r1(15,&(edh_r15.val)))
	 return (-1);
       else return(edh_r15.bits.rw7_b6);
     case 7:
       if (status = _mgv_edh_r1(15,&(edh_r15.val)))
	 return (-1);
       else return(edh_r15.bits.rw7_b7);
     default:
       msg_printf(SUM, "ERROR - Getting Invalid bit  RW %d RW_BIT %d\n",rw, rw_bit);
       return(-1);

     }

   } /* main switch */


/* Getting reserved bit was successful */
   return(0); 

 } /* read_edh_rw */


#endif /* IP30 */
