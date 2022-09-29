#ifdef IP30
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include "uif.h"
#include "libsc.h"
#include "libsk.h"
#include "sys/mgrashw.h"
#include "iset.diag"
#include "ide_msg.h"

#include "mgras_diag.h"

#define TDMA_FORMAT_CTL     ((__paddr_t)&(mgbase->tdma_format_ctl) - (__paddr_t)mgbase)
#define TDMA_ADDR	((__paddr_t)&(mgbase->tdma_addr) - (__paddr_t)mgbase)
#define VDMA_ADDR	((__paddr_t)&(mgbase->vdma_addr) - (__paddr_t)mgbase)

extern void COMPARE64(char *str, int addr, __uint64_t exp, __uint64_t rcv,
        unsigned int mask, __uint32_t *errors);

extern uchar_t _mgras_vsync ;


#define READ_MORE_RECS  1
#define LAST_REC        0

#define DMA_STALL 2
#define DMA_ABORT 1
#define DMA_START 0


uchar_t no_get_chunk = 0;

/*
_compare_formats:
   pio == PIO_OP:
    reads from the fifo depending on the format and compares

  pio == DMA_OP:
   compares the recv and exp values and update the recv ptr depending
   on the format
*/

_check_for_valid_format (uchar_t out_format, uchar_t in_format)
{

    if ((out_format == TEX_RGBA8_OUT) || (out_format == TEX_SHORT_OUT) ||
       (out_format == TEX_RGBA12_OUT) ) ;
	else
	 	{ msg_printf(ERR, "Invalid Format\n");  return (0); }

	if ((in_format == TEX_RGBA16_IN) || (in_format == TEX_SHORT_IN) ||
		(in_format == TEX_RGBA10_IN) || (in_format == TEX_ABGR8_IN) ||
		(in_format == TEX_RGBA8_IN) ) ;
	else
		{ msg_printf(ERR, " Invalid In format\n"); return (0); }

	return (1);
}
	
	

__uint32_t 
_compare_formats ( char *str, __uint64_t exp, 
                              __uint64_t *recv, uchar_t pio ,
                              __uint32_t *errors)
{

    __uint64_t temp64 = 0, mask = 0, buf = 0;

	msg_printf(DBG, " Entering the compare format function\n");

	*errors = 0;
	switch (tdma_format.bits.video_in_format) {

      case TEX_SHORT_IN :
       mask = (tdma_format.bits.tex_out_format == TEX_SHORT_OUT) ? 
                     0x0000ffff0000ffff : 0x0 ;
	  msg_printf(DBG, " TEX_SHORT_IN and TEX_SHORT_OUT\n" );
	  msg_printf(DBG,"  exp 0x%llx\n", exp); 
       if (pio ) {
	  msg_printf(DBG, " Reading the FIFO 1word using PIO\n");
          temp64 = (mgbase->vdma_fifo)  ;
          msg_printf(DBG,"FIFO 1ST WORD 0x%llx ", temp64 );

	  msg_printf(DBG, " Reading the FIFO 2word using PIO\n");
          *recv =  (mgbase->vdma_fifo)  ;
          msg_printf(DBG,"FIFO 2ND WORD 0x%llx\n", *recv );
       }
       else {
          temp64 = *recv++;
       }
	   buf =    ((temp64 & 0xffff00000000) << 16) | 
                    ((temp64 & 0xffff) << 32)         |
                    ((*recv  & 0xffff00000000) >> 16) | 
                     (*recv & 0xffff) ;

          msg_printf(DBG,"after shift: exp 0x%llx recv 0x%llx mask 0x%llx\n",
			 exp, buf, mask);
#if 0
         COMPARE64(str,(__uint64_t)&(mgbase->vdma_fifo), exp, buf, mask, errors) ;
#else
	if ( (exp & mask) != (buf & mask) ) {
	  (*errors)++;
	  msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n", 
		     exp, buf, mask);
	}
#endif

              if (*errors) return (*errors);

       break;

      case TEX_RGBA16_IN :

	 msg_printf(DBG, " TEX_RGBA16_IN\n");
	  if (tdma_format.bits.tex_out_format == TEX_RGBA8_OUT) {
	 	msg_printf(DBG, " TEX_RGBA8_OUT\n");
		mask = 0x00ff0ff000ff0ff0 ;
		temp64 = ((exp & 0xff00000000000000) >> 20) |
			 ((exp & 0x0000ff0000000000) << 8)  |
			 ((exp & 0x00000000ff000000) >> 20) |
			 ((exp & 0x000000000000ff00) << 8) ;
	  }
	  else {
	 	msg_printf(DBG, " TEX_RGBA12_OUT\n");
		mask = 0x00ffffff00ffffff;
		temp64 = ((exp & 0xfff0000000000000) >> 20) |
			 ((exp & 0x0000fff000000000) << 8)  |
			 ((exp & 0x00000000fff00000) >> 20) |
			 ((exp & 0x000000000000fff0) << 8) ;

	  };

	  msg_printf(DBG,"  exp 0x%llx\n", exp); 
	  msg_printf(DBG,"  After shifting 0x%llx\n", temp64); 


        if (pio ) {
          *recv =  (mgbase->vdma_fifo)  ;
          msg_printf(DBG, "fifo word 0x%llx\n", *recv );
       }else
		  msg_printf(DBG, "  rcv 0x%llx\n", *recv );

#if 0
           COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ;
#else
        if ( (temp64 & mask) != (*recv & mask) ) {
          (*errors)++;
          msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
                    temp64,  *recv, mask);
	}
#endif

           if (*errors) return (*errors);
        break;      

      case TEX_ABGR8_IN :
           msg_printf(DBG, " TEX_ABGR8_IN\n"  );

          mask = 0x00ff0ff000ff0ff0 ;

           msg_printf(DBG, "exp 0x%llx\n", exp );

           temp64 = (exp & 0x000000ff00000000) << 4 |
                    (exp & 0x0000ff0000000000) << 8 |
                    (exp & 0x00ff000000000000) >> 44 |
                    (exp & 0xff00000000000000) >> 40 ;
           msg_printf(DBG, "after shifting exp 0x%llx\n", temp64 );

            if (pio) {
                  *recv =  (mgbase->vdma_fifo)  ;
                  msg_printf(DBG, "fifo 1st word 0x%llx\n", *recv );
            }

                msg_printf(DBG, "exp 0x%llx recv 0x%llx mask 0x%llx\n", temp64,
*recv, mask);

#if 0
               COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ;

#else
        if ( (temp64 & mask) != (*recv & mask) ) {
          (*errors)++;
          msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
                   temp64, *recv,  mask);
        }
#endif
                   if (*errors) return (*errors);


               temp64 = (exp & 0x00000000000000ff) << 36  |
                        (exp & 0x000000000000ff00) << 40  |
                        (exp & 0x0000000000ff0000) >> 12  |
                        (exp & 0x00000000ff000000) >> 8 ;

           msg_printf(DBG, "after shifting exp 0x%llx\n", temp64 );

               if (pio) {
                  *recv =  mgbase->vdma_fifo & SIXTYFOURBIT_MASK ;
                  msg_printf(DBG, "fifo 2nd word 0x%llx\n", *recv );
                }
                else
                  recv++;
#if 0
               COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ;

#else
        if ( (temp64 & mask) != (*recv & mask) ) {
          (*errors)++;
          msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
                   temp64, *recv,  mask);
        }
#endif
               if (*errors) return (*errors);
               break;


      case TEX_RGBA8_IN :
           msg_printf(DBG, " TEX_RGBA8_IN\n "  );

	  mask = 0x00ff0ff000ff0ff0 ;

           msg_printf(DBG, "exp 0x%llx\n", exp );

           temp64 = (exp & 0xff00000000000000) >> 20 |
                    (exp & 0x00ff000000000000) |
                    (exp & 0x0000ff0000000000) >> 36 |
                    (exp & 0x000000ff00000000) >> 16 ;  
           msg_printf(DBG, "after shifting exp 0x%llx\n", temp64 );

            if (pio) {
          	  *recv =  (mgbase->vdma_fifo)  ;
          	  msg_printf(DBG, "fifo 1st word 0x%llx\n", *recv );
            } else
          	  msg_printf(VRB, "word 0x%llx\n", *recv );


#if 0
               COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ;

#else
        if ( (temp64 & mask) != (*recv & mask) ) {
          (*errors)++;
          msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
                   temp64, *recv,  mask);
        }
#endif
                   if (*errors) return (*errors);


               temp64 = (exp & 0x00000000ff000000) << 12  |
                        (exp & 0x0000000000ff0000) << 32  |
                        (exp & 0x000000000000ff00) >> 4   |
                        (exp & 0x00000000000000ff) << 16 ;  

           msg_printf(DBG, "after shifting exp 0x%llx\n", temp64 );

               if (pio) {
                  *recv =  mgbase->vdma_fifo & SIXTYFOURBIT_MASK ;
          	  msg_printf(DBG, "fifo 2nd word 0x%llx\n", *recv );
		} 
		else
                  recv++;
#if 0
               COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ;

#else
        if ( (temp64 & mask) != (*recv & mask) ) {
          (*errors)++;
          msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
                   temp64, *recv,  mask);
        }
#endif
               if (*errors) return (*errors);
               break;

        case TEX_RGBA10_IN :

          	  msg_printf(DBG, " TEX_RGBA10_IN\n" );

	  if (tdma_format.bits.tex_out_format == TEX_RGBA8_OUT) {
	          mask = 0x00ff0ff000c00ff0;
          	  msg_printf(DBG, " TEX_RGBA8_OUT\n" );
	  }
	  else {
	          mask = 0x00ffcffc00c00ffc;
          	  msg_printf(DBG, " TEX_RGBA12_OUT\n" );
	       }
          	  msg_printf(DBG, "exp 0x%llx\n", exp );

		  temp64 = (exp & 0xffc0000000000000) >> 20 | 
		           (exp & 0x003ff00000000000) << 2  | 
		           (exp & 0x00000ffc00000000) >> 32 | 
		           (exp & 0x0000000300000000) >> 10 ; 

          	  msg_printf(DBG, "after shifting first word 0x%llx\n",temp64);

                 if (pio) {
                  *recv =  mgbase->vdma_fifo ;
          	  msg_printf(DBG, "fifo 1st word 0x%llx\n", *recv );
		}


#if 0
                COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ; 
#else
		if ( (temp64 & mask) != (*recv & mask) ) {
		  (*errors)++;
		  msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
			  temp64, *recv,  mask);
		}

#endif
               if (*errors) return (*errors);

		  temp64 = (exp & 0x00000000ffc00000) << 12 | 
		           (exp & 0x00000000003ff000) << 34 | 
		           (exp & 0x0000000000000ffc)   | 
		           (exp & 0x0000000000000003) << 22 ; 

          	  msg_printf(DBG, "after shifting 2nd word 0x%llx\n", temp64 );

                if (pio) {
                  *recv =  mgbase->vdma_fifo ;
          	  msg_printf(DBG, "fifo 2nd word 0x%llx\n", *recv );
		} else
		    recv++;

#if 0
                COMPARE64(str, (__uint64_t)&mgbase->vdma_fifo, temp64, *recv, mask, errors) ; 
#else
		if ( (temp64 & mask) != (*recv & mask) ) {
		  (*errors)++;
		  msg_printf(ERR,"  exp 0x%llx recv 0x%llx mask 0x%llx\n",
			  temp64, *recv,  mask);
		}

#endif

                 if (*errors) return (*errors);
	     	    break;
                 default:
                      break;
	  }

  return(0);
}

void
_mgras_report_format ( uchar_t in, uchar_t out)
{
    switch ( in ) {
        case TEX_RGBA16_IN :
            msg_printf(SUM, "TEX_RGBA16_IN and ");
            break;
        case TEX_SHORT_IN :
            msg_printf(SUM, "TEX_SHORT_IN and ");
            break;
        case TEX_RGBA10_IN :
            msg_printf(SUM, "TEX_RGBA10_IN and ");
            break;
        case TEX_ABGR8_IN :
            msg_printf(SUM, "TEX_ABGR8_IN and ");
            break;
        case TEX_RGBA8_IN :
            msg_printf(SUM, "TEX_RGBA8_IN and ");
            break;
        default:
            break;
    }

    switch (out) {
        case TEX_SHORT_OUT :
            msg_printf(SUM, "TEX_SHORT_OUT\n");
            break;
        case TEX_RGBA8_OUT :
            msg_printf(SUM, "TEX_RGBA8_OUT\n");
            break;
        case TEX_RGBA12_OUT :
            msg_printf(SUM, "TEX_RGBA12_OUT\n");
            break;
        default:
            break;
    }
}

__uint32_t 
mgras_rtpio_loop ( int argc, char **argv )
{

	__uint32_t i, temp , errors = 0, value;
    __uint32_t in_format, out_format, bad_arg = 0;

   errors = 0;

   /* default format setup */ 

    in_format = TEX_SHORT_IN ; out_format = TEX_SHORT_OUT ;

    argc--; argv++; /* Skip Test Name */

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 'i' :
                                if (argv[0][2]=='\0') { /* has white space */
                                        atobu(&argv[1][0], &in_format);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atobu(&argv[0][2], &in_format);
                                }
			msg_printf(DBG, " in_format 0x%x\n", in_format);
                break;
            case 'o' :
                                if (argv[0][2]=='\0') { /* has white space */
                                        atobu(&argv[1][0], &out_format);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atobu(&argv[0][2], &out_format);
                                }
			msg_printf(DBG, " out_format 0x%x\n", out_format);
                break;
             default:
                  bad_arg++;
                break;
       }
        argc--; argv++;
    }

    if (bad_arg) {
        msg_printf(SUM, " Usage: -i <in_format> -o <out_format>\n");
        msg_printf(SUM, " <in_format>\n");
	msg_printf(SUM,"                      0 -RGBA 8bit\n");
	msg_printf(SUM,"                      1 -ABGR 8bit\n");
	msg_printf(SUM,"                      2 -RGBA 10bit\n");
	msg_printf(SUM,"                      3 -short 16bit\n");
	msg_printf(SUM,"                      4 -RGBA 16bit\n");
        msg_printf(SUM, " <out_format>\n");
	msg_printf(SUM,"                      0 -RGBA 12bit\n");
	msg_printf(SUM,"                      1 -RGBA 8bit\n");
	msg_printf(SUM,"                      2 -short 16bit\n");
        return (1);
    }

   msg_printf(DBG, "in format 0x%x out_format 0x%x\n", in_format, out_format );
	if (!(_check_for_valid_format (out_format, in_format)))
	return (1);
	
	
	/* store the existing value in the tdma_format to some location */  
    temp = mgbase->tdma_format_ctl & THIRTYTWOBIT_MASK ;
    tdma_format.value = temp;

   /* set the diag mode, video input format and output format */


#if 1
     tdma_format.bits.diag_mode = 1;
     tdma_format.bits.tex_out_format = (out_format & 0x3);
     tdma_format.bits.video_in_format = (in_format & 0x7);
#else
   value = ((out_format & 0x3) << 4 ) | ((in_format & 0x7) << 1 ) |
			(1 << 16) ;
#endif

   mgbase->tdma_format_ctl = 
	(THIRTYTWOBIT_MASK) & (tdma_format.value) ;	

	us_delay(1000);

#if 1 /* Test if dma_abort helps */
	mgbase->vdma_ctl = 0x2 ; /* clear the dma_abort bit */
	msg_printf(DBG, " setting the DMA abort bit\n");
#endif

	us_delay(1000);

   recv_32 = (mgbase->tdma_format_ctl) & THIRTYTWOBIT_MASK; 	
   msg_printf(DBG, "tdma_format_ctl 0x%x\n",  recv_32);

/**************** PATTERN TEST ****************/

   msg_printf(DBG, " Addr tdma_fifo 0x%llx\n", 
               ((__uint64_t) &(mgbase->tdma_fifo)));

   for ( i = 0 ; i <  ((SIZE_DMA_WR_DATA/8)   ) ; i++) 
     mgbase->tdma_fifo = SIXTYFOURBIT_MASK & DMA_wr_data[i];

   	us_delay(1000); 

   for ( i = 0 ; i <  ((SIZE_DMA_WR_DATA/8)  ) ; i++)
       _compare_formats("PATTERN TEST: RTPIO LOOP", DMA_wr_data[i], &recv_64, PIO_OP , &errors);

  	if (errors) goto  _errors;


/********** done with PATTERN TEST *************/

#if 1

/********  do a walking 1s on the fifo ***********/

   for ( i = 0, exp_64 = 1; i < 64 ;  i++, exp_64 <<=1) {
     mgbase->tdma_fifo  = exp_64 ;

   	us_delay(1000); 

     _compare_formats("Walk1s: rtpio loop", exp_64, &recv_64, PIO_OP , &errors);
     if (errors) goto  _errors;
   }

/********  done walking 1s on the fifo ***********/

/********** do a walking 0s *******************/

   exp_64 = 0xfffffffffffffffe; 
   for (i = 0 ; i < 64; i++, exp_64 = ((exp_64 <<=1) | 1)) {
        mgbase->tdma_fifo  = exp_64 ;

       us_delay(1000); 

      _compare_formats("Walk0s: rtpio loop",exp_64, &recv_64, PIO_OP , &errors);
     if (errors) goto  _errors;
   }

/********  done walking 0s on the fifo ***********/

     /* we are done with the test: store the old value into the register now */
		mgbase->tdma_format_ctl =  temp;
        tdma_format.value = temp;
#endif 

	mgbase->tdma_format_ctl =   temp;
	msg_printf(SUM, " PIO Diag Mode Passed in ");
	_mgras_report_format (in_format, out_format);

	return (0);

_errors:
    msg_printf(ERR, " PIO Diag Mode Failed in ");
	_mgras_report_format (in_format, out_format);

     /* we are done with the test: store the old value into the register now */
	HQ_WR(mgbase->tdma_format_ctl, THIRTYTWOBIT_MASK, temp);
        tdma_format.value = temp;

       return (1); 

}

/*
_fill_buffers: fills from from_buffer to to_buffer using the
byte_len of both buffers 
*/

void
_fill_buffers (uchar_t *to, uchar_t *from,  
              __uint32_t to_len, __uint32_t from_len)
{
 __uint32_t   i, j;

  for (i = 0, j = 0; i < to_len ; i++, j = i%from_len ) {
   *(to+i) = *(from+j) ;
	}
  
#if 0
	/*added some unique value at the end of the buffer */
	*(__uint64_t *)(to+i-8) = 0xdeadbeefdeadbeef;		
#endif
}

void 
rtdma2dcb_setup (uchar_t *ptr2data, __uint32_t byte_len,
                            realdma_words_t *dma_tbls,
                            vdma_tdma_addr_t *dma_addr)
{
    uchar_t *start_addr ;
    realdma_words_t *buf;
    __uint32_t records2xfer,  i, val;

   msg_printf(DBG, " Entering rtdma2dcb_setup function.\n");
   /* we need to double word align the dma_tbls */
   buf = (realdma_words_t *)(((__paddr_t)dma_tbls + 64) & (~0x7f)) ; 

    /* let us setup the dma_addr fields. we do DMA to the DCB */
    dma_addr->bits.dma_data_gbr =    0;
    dma_addr->bits.dma_tbl_gbr =     0;
    dma_addr->bits.dma_data_dev_id = HEART_ID ; /*heart ID from RACER/IP30.h*/
    dma_addr->bits.dma_tbl_dev_id =  HEART_ID ;
    dma_addr->bits.dma_tbl_addr =    K1_TO_PHYS(buf);
    HQ_WR(mgbase->tdma_addr, THIRTYTWOBIT_MASK,  tdma_addr.value) ;

    HQ_RD(mgbase->tdma_addr, THIRTYTWOBIT_MASK,  val) ;
    msg_printf(DBG, " tdma_addr 0x%x\n" , val) ;

    start_addr = ptr2data;

    /* we assume that the data is less that 64K */
    buf->word1.bits.data_len = byte_len;
    buf->word2.bits.vsync_ctl = 0;
    buf->word1.bits.data_start_addr = K1_TO_PHYS(start_addr);
    buf->word2.bits.next_tbl_addr = 0;
    buf->word2.bits.more_records = LAST_REC; /*last table  */
    
   msg_printf(DBG, " Leaving rtdma2dcb_setup function.\n");
}


void 
rtdma_records_setup (__uint32_t *ptr2data, __uint32_t byte_len ,
                                realdma_words_t *dma_tbls, 
                                vdma_tdma_addr_t *dma_addr)
{
   __uint32_t records2xfer,  i, val = 0, remain_bytes;
   uchar_t *start_addr ;
   realdma_words_t *buf;

   msg_printf(DBG, " Entering the rtdma_records_setup function.\n");
   /* we need to double word align the dma_tbls */
   buf = (realdma_words_t *)(((__paddr_t)dma_tbls + 64) & (~0x7f)) ; 

   msg_printf(DBG, " address of buf 0x%llx\n", buf);
     
    /* let us setup the dma_addr fields . We do only mem2mem dma's*/
#if 1
    dma_addr->bits.dma_data_gbr =    0;
    dma_addr->bits.dma_tbl_gbr =     0;
    dma_addr->bits.dma_data_dev_id = HEART_ID ; /*heart ID from RACER/IP30.h*/
    dma_addr->bits.dma_tbl_dev_id =  HEART_ID ;
    dma_addr->bits.dma_tbl_addr =    K1_TO_PHYS(buf);
#else
    dma_addr->value = (0 << 57) | (0 << 56) | 
			(HEART_ID << 52) | (HEART_ID << 48) | (K1_TO_PHYS(buf));
#endif

    start_addr = (uchar_t *)ptr2data ;
    msg_printf(DBG, " address of start_addr 0x%llx\n", start_addr);

    msg_printf(DBG, " dma_addr->value 0x%llx\n", dma_addr->value);

    msg_printf(DBG, " byte_len 0x%x MAX-DMA_SIZE 0x%x\n",
			byte_len, MAX_RTDMA_SIZE);
	remain_bytes = byte_len %  MAX_RTDMA_SIZE ;

	if (remain_bytes) {
    	msg_printf(DBG, " remain_bytes %d\n", remain_bytes);

		buf->word1.bits.data_len = remain_bytes;
		buf->word1.bits.data_start_addr =  K1_TO_PHYS(start_addr);
		buf->word2.bits.next_tbl_addr = K1_TO_PHYS(buf+1) ;
		buf->word2.bits.more_records = READ_MORE_RECS;
	  	buf->word2.bits.vsync_ctl  = 0x0;
		start_addr = start_addr + remain_bytes;
		byte_len = byte_len - remain_bytes ;
		buf++;
		val++;
	}

    records2xfer = byte_len / MAX_RTDMA_SIZE ;
    msg_printf(DBG, " records2xfer %d\n", records2xfer);

       for ( i = 0 ; i < records2xfer ; i++ , buf++) {

		val++;
          buf->word1.bits.data_len = MAX_RTDMA_SIZE;

          buf->word1.bits.data_start_addr = K1_TO_PHYS(start_addr);

          buf->word2.bits.next_tbl_addr = K1_TO_PHYS(buf+1) ;

          buf->word2.bits.more_records = READ_MORE_RECS ; 

	  buf->word2.bits.vsync_ctl  = 0x0;

          start_addr = start_addr + MAX_RTDMA_SIZE ;
	   }

	if (val == 1)
	(buf-val)->word2.bits.vsync_ctl  = 0x3;
	else {
	(buf-val)->word2.bits.vsync_ctl  = 0x1;
	(buf-1)->word2.bits.vsync_ctl  = 0x2;
	}

        (buf-1)->word2.bits.next_tbl_addr  = 0;
        (buf-1)->word2.bits.more_records = LAST_REC;  /* last table */

	flush_cache();

    msg_printf(DBG, " dma_addr->value 0x%llx\n", dma_addr->value);
   msg_printf(DBG, " Leaving the rtdma_records_setup function.\n");
}

__uint32_t mgras_rtdma_loop ( int argc, char **argv )
{

    __uint32_t value, i, temp , errors = 0, bad_arg = 0;
    __uint32_t dma_len = 0x1, mode = 2;  /* initial dma length is 4K */
	uchar_t no_diag_mode = 0;
	__uint64_t val64;
	
  __paddr_t    pgMask = ~0x0;

   /* default format setup */

    in_format = TEX_RGBA16_IN ; out_format = TEX_RGBA12_OUT ;
    argc--; argv++; /* Skip Test Name */

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 'p' :
                 if (argv[0][2]=='\0') { /* has white space */
                        atob(&argv[1][0], (int*)&dma_len);
                        argc--; argv++;
                  }
                  else { /* no white space */
                      atob(&argv[0][2], (int*)&dma_len);
                  }
                break;
			case 't' :	
					no_diag_mode = 1;
				break;
			case 'm' :
				if (argv[0][2]=='\0') { /* has white space */
                        atob(&argv[1][0], (int*)&mode);
						argc--; argv++;
				}
				else {
					atob(&argv[0][2], (int*)&mode);
				}
			  break;

             default:
                bad_arg++;
                break;
       }
    }

    if (bad_arg) {
       msg_printf(SUM,"Usage: -p <#pages>\n");
       return(1);
    }


   /* convert #pages to #bytes */
    dma_len = dma_len * MAX_RTDMA_SIZE;

    /* store the initial value of tdma_format_ctl in temp */
    HQ_RD(mgbase->tdma_format_ctl, THIRTYTWOBIT_MASK, temp);
    tdma_format.value = temp;

    /* initialize the value field of all the registers */
    tdma_format.value = (mgbase->tdma_format_ctl) & THIRTYTWOBIT_MASK;
    tdma_cntl.value   = (mgbase->tdma_ctl) & THIRTYTWOBIT_MASK;
    tdma_addr.value   = (mgbase->tdma_addr) & THIRTYTWOBIT_MASK;

    /* set the diag mode, video input format and output format */

	if (!no_diag_mode)
       tdma_format.bits.diag_mode = 1;

     tdma_format.bits.tex_out_format = (out_format & 0x3);
     tdma_format.bits.video_in_format = (in_format & 0x7);

    mgbase->tdma_format_ctl = (THIRTYTWOBIT_MASK) & (tdma_format.value) ;

   	us_delay(100000); 

    mgbase->vdma_ctl = BIT(DMA_ABORT) ; /* clear the dma_abort bit */
    msg_printf(DBG, " setting the DMA abort bit\n");

   	us_delay(100000); 

   recv_32 = (mgbase->tdma_format_ctl) & THIRTYTWOBIT_MASK;
   msg_printf(VRB, "tdma_format_ctl 0x%x\n",  recv_32);

    msg_printf(VRB, "  tdma_format  0x%x\n", mgbase->tdma_format_ctl);

    /* turn off the Interrupts, set_enable, clear_flag */
    mgbase->flag_intr_enab_clr = (THIRTYTWOBIT_MASK & (~0x0));
    mgbase->flag_enab_set =  
             (THIRTYTWOBIT_MASK & (HQ_FLAG_TDMA_DONE | HQ_FLAG_VDMA_DONE));  

    mgbase->flag_clr_priv =  (THIRTYTWOBIT_MASK & (~0x0)) ;

	flush_cache();

    msg_printf(DBG, " flag_intr_enab_clr 0x%x\n", mgbase->flag_intr_enab_clr);
    msg_printf(DBG, " flag_enab_set 0x%x\n", mgbase->flag_enab_set);
    msg_printf(DBG, " flag_clr_priv 0x%x\n", mgbase->flag_clr_priv);

    DMA_WRTile_notaligned = (__uint32_t *)get_chunk(2*DMABUFSIZE); 
	DMA_RDTile_notaligned = (DMA_WRTile_notaligned + DMABUFSIZE);
	if (DMA_WRTile_notaligned == 0) {
		msg_printf(ERR,"Could not allocate %d bytes.\n",2*DMABUFSIZE);
		return 1;
	}

     pgMask = PAGE_MASK(pgMask);

     DMA_RDTile = (__uint32_t *)
     ((((__paddr_t)DMA_RDTile_notaligned) + 0x1000) & pgMask);
     msg_printf(DBG, "DMA_RDTile 0x%x\n", DMA_RDTile);

     DMA_WRTile = (__uint32_t *)
     ((((__paddr_t)DMA_WRTile_notaligned) + 0x1000) & pgMask);
     msg_printf(DBG, "DMA_WRTile 0x%x\n", DMA_WRTile);


     /* 
     we now have a RD buffer and WR buffer 
     so fill the buffers with patterns.
     currently we live with our own patterns. later we shall
     move the patterns from mgras to gamera. 
     */ 
     _fill_buffers((uchar_t *)DMA_WRTile, (uchar_t *)DMA_wr_data , 
                         dma_len, SIZE_DMA_WR_DATA);


     /* setup the dma tables now in the system memory.  */
	if ((mode == _WRONLY_) || (mode == _RDWR_) ) {
     rtdma_records_setup ( DMA_WRTile, dma_len,
                                tbl_ptr_send,
                                &tdma_addr);
        value = tdma_addr.value ;
        msg_printf(DBG, "tdma_addr 0x%llx\n", value );
        mgbase->tdma_addr =  tdma_addr.value ;
     /* kick off the texture dma */
        tdma_cntl.bits.dma_start = 1 ;
        mgbase->tdma_ctl = tdma_cntl.value ;
	}

	flush_cache();

	if ((mode == _RDONLY_) || (mode == _RDWR_) ) {
     rtdma_records_setup ( DMA_RDTile, dma_len,
                                tbl_ptr_recv,
                                &vdma_addr);
        value = vdma_addr.value ;
        msg_printf(DBG, "vdma_addr 0x%llx\n", value );
        mgbase->vdma_addr = vdma_addr.value ;
     /* kick off the texture dma */
        vdma_cntl.bits.dma_start = 1;
        mgbase->vdma_ctl = vdma_cntl.value;
	}

	flush_cache();

	us_delay(1000000);	
#if 1
	HQ3_PIO_RD(TDMA_FORMAT_CTL, THIRTYTWOBIT_MASK, value);
    msg_printf(DBG, "tdma_format_ctl 0x%x\n",value);

	HQ3_PIO_RD(TDMA_ADDR, SIXTYFOURBIT_MASK, val64);
    msg_printf(DBG, "tdma_addr 0x%llx\n", val64 );

	HQ3_PIO_RD(VDMA_ADDR, SIXTYFOURBIT_MASK, val64);
    msg_printf(DBG, "vdma_addr 0x%llx\n", val64 );

#endif
    /* wait for TDMA done : */
   for ( i = 0; i < 100000; i++) {
	  HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, THIRTYTWOBIT_MASK, recv_32);
      msg_printf(DBG, " flag_set_priv 0x%x\n", recv_32 );
      if (recv_32 & HQ_FLAG_TDMA_DONE) break; /* exit the loop */
   }
   if ( i == 100000) {  /* dma not done, ERR occured */
       msg_printf(ERR, " TDMA NOT DONE. FLAG NOT SET\n");
       goto _errors;
   }


   /* wait for VDMA done : */
   for ( i = 0; i < 100000; i++) {
	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, THIRTYTWOBIT_MASK, recv_32);
       msg_printf(DBG, " flag_set_priv 0x%x\n", recv_32 );
      if (recv_32 & HQ_FLAG_VDMA_DONE) break ; /* exit the loop */
   }
   if ( i == 100000)  { /* dma not done, ERR occured */
        msg_printf(ERR, " VDMA NOT DONE. FLAG NOT SET\n");
       goto _errors;
   }

	if  (mode == _RDWR_)  {
	/* check the buffers using __uint64_t type */
        for ( i = 0; i < (dma_len) ; i+=8) {
	    	msg_printf(DBG, " i %d\n", i);
            _compare_formats("RTDMA loopback", 
		    		*((__uint64_t *)(((uchar_t *)DMA_WRTile) + i)), 
		    		((__uint64_t *)(((uchar_t *)DMA_RDTile) + i)), 
		    		DMA_OP, &errors) ;

            if (errors) goto _errors;
        }
	}

/*   restore the initial value of tdma_format_ctl from temp */
    HQ_WR(mgbase->tdma_format_ctl, THIRTYTWOBIT_MASK, temp);
    tdma_format.value = temp;
   
    	msg_printf(SUM, " Real Time DMA diag mode Passed\n");
    return (0);

_errors:
    	msg_printf(ERR, " Failed DMA\n");
		msg_printf(SUM, " flag_set_priv 0x%x\n", mgbase->flag_set_priv);
		msg_printf(SUM, " Status 0x%x : DMA active is bit #12\n", 
							mgbase->status);
    	errors++; 

    return(errors);
}

__uint32_t 
mgras_te_load_bit_test ()
{

    __uint32_t i, temp , value;

    /* store the existing value in the tdma_format to some location */
	temp = mgbase->tdma_format_ctl ;
    tdma_format.value = temp;

#if 1
    tdma_format.bits.diag_mode = 1;
    tdma_format.bits.vsync_cntl = 1;
    mgbase->tdma_format_ctl = tdma_format.value ;

   	us_delay(10000); 

    mgbase->vdma_ctl = BIT(DMA_ABORT) ; /* clear the dma_abort bit */
    msg_printf(VRB, " setting the DMA abort bit\n");

   	us_delay(10000); 

#endif /* initialize the dma channel */

    /* a write to the top page of the tfifo will deactivate the te_load_cnlt */
	mgbase->tdma_fifo = 0xdeadbeefdeadbeef;
   
    /* read from the vfifo and check if the te_load_bit is deactivated */
		exp_64 = mgbase->vdma_fifo ;
	msg_printf(DBG, " exp 0x%llx\n", exp_64);

	if ( exp_64 & 0x01000000  ) {
		msg_printf(SUM, " te load bit not deactivated\n" );
		goto _errors ;
	} else
		msg_printf(SUM, " te load bit deactivated\n" ); 

    /* a write to last page of the tfifo will activate the te_load_cnlt */
		mgbase->tdma_fifo_pad[0x4000] = 0xdeadbeefdeadbeef ; 
  
    /* read from the vfifo and check if the te_load_bit is activated */
       exp_64 = mgbase->vdma_fifo ; 
		msg_printf(DBG, " exp 0x%llx\n", exp_64);

	if ( exp_64 & 0x01000000 ) 
		msg_printf(SUM, " te load bit activated\n" ); 
	else {
		msg_printf(SUM, " te load bit not activated\n" );
		goto _errors ;
	}

	msg_printf(SUM, " te_load_bit_test Passed\n");
	mgbase->tdma_format_ctl = temp;
	return (0);

_errors:
	msg_printf(ERR, " te_load_bit_test Failed\n");
	mgbase->tdma_format_ctl = temp;
	return (1);
}


__uint32_t 
mgras_vc3_dcbdma ( uchar_t *data, __uint32_t byte_len)
{
   __uint32_t i, value;

    /* turn off the Interrupts, set_enable, clear_flag */
    mgbase->flag_intr_enab_clr = (THIRTYTWOBIT_MASK & (~0x0));
    mgbase->flag_enab_set =
             (THIRTYTWOBIT_MASK & (HQ_FLAG_TDMA_DONE | HQ_FLAG_VDMA_DONE));

    mgbase->flag_clr_priv =  (THIRTYTWOBIT_MASK & (~0x0)) ;

     tdma_format.bits.video_in_format = TEX_DCB_IN ;
     mgbase->tdma_format_ctl = tdma_format.value;

   us_delay(100000); 

    mgbase->vdma_ctl = BIT(DMA_ABORT) ; /* clear the dma_abort bit */
    msg_printf(VRB, " setting the DMA abort bit\n");

   us_delay(10000); 

     recv_32 = (mgbase->tdma_format_ctl) & THIRTYTWOBIT_MASK;
     msg_printf(VRB, "tdma_format_ctl 0x%x\n",  recv_32);

     rtdma_records_setup((__uint32_t *)data, byte_len, tbl_ptr_send, &tdma_addr );

     /* write the tdma_addr.value to the tdma_addr regs */
     mgbase->tdma_addr = tdma_addr.value ;
 
     /* kick off the DCB dma */
     tdma_cntl.bits.dma_start = 1 ;
     mgbase->tdma_ctl =  tdma_cntl.value ;

	us_delay(10000);	

    /* wait for TDMA done : */
   for ( i = 0; i < 100000; i++) {
      recv_32 = mgbase->flag_set_priv   ;
      msg_printf(VRB, "flag_set_priv 0x%x\n", recv_32 ); 
      if (recv_32 & HQ_FLAG_TDMA_DONE) break; /* exit the loop */
   }
   if ( i == 100000)  /* dma not done, ERR occured */
    msg_printf(ERR, " TDMA2DCB DMA NOT DONE. FLAG NOT SET\n");
     
   return (0);

 
}

__uint32_t
_mgras_rttexture_setup(uchar_t in_format,uchar_t out_format, __uint32_t d_len,
			__uint32_t *in_buf)
{ 

	__uint32_t value, i, rcv_32;

    tdma_format.bits.te_stall_en_mask = 1; 
    tdma_format.bits.tex_out_format = (out_format & 0x3);
    tdma_format.bits.video_in_format = (in_format & 0x7);
    mgbase->tdma_format_ctl = (THIRTYTWOBIT_MASK) & (tdma_format.value) ;

    us_delay(10); 
    msg_printf(DBG, "  tdma_format  0x%x\n", mgbase->tdma_format_ctl);

    mgbase->vdma_ctl = BIT(DMA_ABORT) ; /* clear the dma_abort bit */
    msg_printf(DBG, " setting the DMA abort bit\n");

    us_delay(10); 

    rcv_32 = (mgbase->tdma_format_ctl) & THIRTYTWOBIT_MASK;
    msg_printf(DBG, "tdma_format_ctl 0x%x\n",  rcv_32);


    /* turn off the Interrupts, set_enable, clear_flag */
    mgbase->flag_intr_enab_clr = (THIRTYTWOBIT_MASK & (~0x0));
    msg_printf(DBG, " flag_intr_enab_clr 0x%x\n", mgbase->flag_intr_enab_clr);

    mgbase->flag_enab_set =
             (THIRTYTWOBIT_MASK & (HQ_FLAG_TDMA_DONE | HQ_FLAG_VDMA_DONE));
    msg_printf(DBG, " flag_enab_set 0x%x\n", mgbase->flag_enab_set);

    mgbase->flag_clr_priv =  (THIRTYTWOBIT_MASK & (~0x0)) ;
    msg_printf(DBG, " flag_clr_priv 0x%x\n", mgbase->flag_clr_priv);

    /*
     we now have a RD buffer and WR buffer
     so fill the buffers with patterns.
     currently we live with our own patterns. later we shall
     move the patterns from mgras to gamera.
    */

    rtdma_records_setup ( in_buf, d_len, tbl_ptr_send, &tdma_addr);

    msg_printf(DBG, "tdma_addr 0x%llx\n", tdma_addr.value );
    mgbase->tdma_addr =  tdma_addr.value ;

    msg_printf(DBG, " Kicking off the RT DMA\n");
    /* kick off the texture dma */
    tdma_cntl.bits.dma_start = 1 ;
    mgbase->tdma_ctl = tdma_cntl.value ;
    
    /* wait for TDMA done : */
    for ( i = 0; i < 100000; i++) {
      	rcv_32 = mgbase->flag_set_priv  & THIRTYTWOBIT_MASK ;
      	if (rcv_32 & HQ_FLAG_TDMA_DONE) break; /* exit the loop */
      	us_delay(100);
    }
    if ( i == 100000) {  /* dma not done, ERR occured */
   	msg_printf(ERR, " TDMA NOT DONE. FLAG NOT SET 0x%x\n", rcv_32);
        goto _errors;
    }

    msg_printf(DBG, " RT DMA Write done\n");	
    return (0);	

_errors:
	return (1);	

}

#endif
