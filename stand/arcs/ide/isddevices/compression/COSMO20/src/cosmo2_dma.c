/*
 * dma_setup.c
 *
 * dma setup routines
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */


#include "cosmo2_defs.h"
#include "sys/param.h"


#define rounddown(X,Y)  (((X) / (Y)) * (Y))

__uint32_t DespTblSize =  5;

#define XFER_WIDTH 8

static char fbmkr_mask[] = {0x7f, 0x7e, 0x7c, 0x78, 0x70, 0x60, 0x40, 0x00};
static char lbmkr_mask[] = {0x7f, 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f};

extern __uint32_t timeOut ;

extern __uint64_t cosmo2brbase;
extern __uint32_t cos2_EnablePioMode( void );
extern __uint32_t cos2_DisEnablePioMode (void);
extern void mcu_WRITE (__uint32_t, UBYTE);

extern void Reset_SCALER_UPC(UWORD );
extern  void Reset_050_016_FBC_CPC(UWORD );
extern __uint64_t dma_send_data[DMA_DATA_SIZE], dma_recv_data[DMA_DATA_SIZE];


cosmo2DMAtable_t d_table;

__uint64_t SetWaterMark( __uint32_t x) 
{
return ( (__uint64_t)(x) | ((__uint64_t)(x) << 16) | \
			( (__uint64_t)(x) << 32) | ((__uint64_t)(x) << 48) ) ;
}

void cosmo2_dma_initialize_fifos_channel(__uint32_t level,
                                 UBYTE channel )
{
    __uint64_t recv ;
    cos2_EnablePioMode( );

    recv = SetWaterMark(level) ;
    cgi1_write_reg(recv,  cosmobase + cgi1_fifo_fg_af_o(channel) );
	recv = cgi1_read_reg(  cosmobase + cgi1_fifo_fg_af_o(channel) );
    msg_printf(DBG, " AF Water mark Level %x  %llx \n", channel, recv);

    recv = SetWaterMark(level-1) ;
    cgi1_write_reg(recv,  cosmobase + cgi1_fifo_tg_ae_o(channel) );
	recv = cgi1_read_reg(  cosmobase + cgi1_fifo_tg_ae_o(channel) );
    msg_printf(DBG, " AE Water mark Level %x  %llx \n", channel, recv);

    cos2_DisEnablePioMode ( );

}

extern cosmo2_mod(__uint32_t, __uint32_t);

void
cosmo2_1to2_dma_swap ( __uint64_t *to, __uint64_t *from, __uint32_t str)
{
	__uint32_t i, j;
#if 0
	__uint32_t dma_bytes ;
	dma_bytes =  sizeof(__uint64_t)*DMA_DATA_SIZE;

    for ( j = 0  ; j < dma_bytes  ;  j++) {
		i = cosmo2_mod((str+j),dma_bytes) ;
        *(((UBYTE *)to) + j)  = *(((UBYTE *)from) + i);
	}
#else
    for ( j = 0  ; j < DMA_DATA_SIZE ;  j++)
        *(to+j) = *(from + j) ;
#endif

}

__uint32_t dma_setup ( UBYTE chnl, UBYTE dir, __uint64_t *dma_data, 
					__uint32_t len, __uint32_t level)
{

	__uint64_t dma_control, overview;
	__uint32_t i,  Num_Bytes, chunks_2_xfer, Q_words;
	__paddr_t	str_addr, end_addr, phys_addr ;
	__uint32_t	mask, imask;

#if !defined(IP30)
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#else

#endif
	/* static sankar added this on jan 26 */ __uint32_t chunky[4][512];

	overview = cgi1_read_reg(cosmobase + cgi1_overview_o) ; 
	msg_printf(DBG, " Initial overview reg %llx \n", overview );
		/* enable dma channel */
	dma_control  = cgi1_read_reg(cosmobase + cgi1_dma_control_o) ; 
	msg_printf(DBG, "Initial dma ctrl reg %llx \n", dma_control );

#if 0
	switch (chnl) {
	case CHANNEL_0:
		 cgi1_write_reg(cgi1_RESET_0, cosmobase + cgi1_dma_control_o) ; 
		break;
	case CHANNEL_1:
		 cgi1_write_reg(cgi1_RESET_1, cosmobase + cgi1_dma_control_o) ; 
        break;
	case CHANNEL_2:
		 cgi1_write_reg(cgi1_RESET_2, cosmobase + cgi1_dma_control_o) ; 
        break;
	case CHANNEL_3:
		 cgi1_write_reg(cgi1_RESET_3, cosmobase + cgi1_dma_control_o) ; 
        break;
		default:
		break;
	};
#endif 

	switch (chnl) {
	case CHANNEL_0:
		dma_control &= ~cgi1_ENABLE_0;
		if (dir) 
			dma_control |= cgi1_TOGIO_0 ;
		else
			dma_control &= ~cgi1_TOGIO_0;

		imask = cgi1_CH0_I0 ;  
		break ;

	case CHANNEL_1:
		dma_control &= ~cgi1_ENABLE_1;
		if (dir ) 
			dma_control |= cgi1_TOGIO_1 ;
		 else
			dma_control &= ~cgi1_TOGIO_1;

		imask = cgi1_CH1_I0 ;  
		break ;

	case CHANNEL_2:
        dma_control &= ~cgi1_ENABLE_2;
        if (dir )
         dma_control |= cgi1_TOGIO_2 ;
        else
         dma_control &= ~cgi1_TOGIO_2;

		imask = cgi1_CH2_I0 ;  

		break ;

	case CHANNEL_3:
        dma_control &= ~cgi1_ENABLE_3;
        if (dir )
          dma_control |= cgi1_TOGIO_3 ;
        else
          dma_control &= ~cgi1_TOGIO_3;

		imask = cgi1_CH3_I0 ;  

		break ;
	default:
		msg_printf(DBG,"Illegal chnnl no %d chnnl ranges from 0-4\n", chnl);
		break ;
	}

	cgi1_write_reg(dma_control, cosmobase + cgi1_dma_control_o)  ; 
	msg_printf(DBG, " dma ctrl reg after reset DTP %llx \n", dma_control );

    /* enable the required channel */
     switch (chnl) {
        case CHANNEL_0:
        dma_control |= cgi1_ENABLE_0 ;
		dma_control &= ~cgi1_DTP_RESET_0 ;
        break;

        case CHANNEL_1:
        dma_control |= cgi1_ENABLE_1 ;
		dma_control &= ~cgi1_DTP_RESET_1 ;
        break;

        case CHANNEL_2:
        dma_control |= cgi1_ENABLE_2 ;
		dma_control &= ~cgi1_DTP_RESET_2 ;
        break;

        case CHANNEL_3:
        dma_control |= cgi1_ENABLE_3 ;
		dma_control &= ~cgi1_DTP_RESET_3 ;
        break;

        default :
        break;
    }

	cgi1_write_reg(dma_control, cosmobase + cgi1_dma_control_o)  ; 
	msg_printf(DBG, " dma ctrl reg after Enable %llx \n", dma_control );


		/* set interrupt */
	cgi1_write_reg(imask, cosmobase+cgi1_interrupt_status_o); 

	mask = cgi1_read_reg(cosmobase+cgi1_interrupt_mask_o); 
	mask &= ~imask ;
	cgi1_write_reg(mask, cosmobase+cgi1_interrupt_mask_o); 

	mask = cgi1_read_reg(cosmobase+cgi1_interrupt_mask_o); 
	msg_printf(DBG, " intr mask reg %x \n", mask );


	Q_words = (len % XFER_WIDTH) ? (len / XFER_WIDTH) + 1 : (len / XFER_WIDTH);
	chunks_2_xfer =
		(Q_words % level) ? (Q_words / level) + 1 : (Q_words / level) ;

	msg_printf(DBG, " chunks_2_xfer 0x%x \n" , chunks_2_xfer);
	str_addr = (__paddr_t) dma_data ;

        if (chunks_2_xfer == 1) {
				Num_Bytes = len ;
        		end_addr = str_addr + Q_words * XFER_WIDTH ; 
				msg_printf(DBG, "len %x Q_words %x chunks_2_xfer %x \n", 
							len, Q_words, chunks_2_xfer ) ;
				msg_printf(DBG, "	str_addr 0x%x end_addr 0x%x num_bytes 0x%x \n", 
									str_addr, end_addr, Num_Bytes);
		}
        else 
			{
				Num_Bytes = level * XFER_WIDTH ;
        		end_addr = str_addr + Num_Bytes  ;
				msg_printf(DBG, "len %x Q_words %x chunks_2_xfer %x \n", 
							len, Q_words, chunks_2_xfer ) ;
				msg_printf(DBG, "	str_addr 0x%x end_addr 0x%x num_bytes %x \n", 
									str_addr, end_addr, Num_Bytes);
			}

	/* 
	Get chunks_2_xfer + 1 memory 
	*/

	d_table.dma_entries = (cosmo2DMAentry_t *) chunky[chnl]; 
		if (d_table.dma_entries == (cosmo2DMAentry_t *) NULL) {
			msg_printf(DBG, "ERROR in malloc-ing the descriptor tables\n");
			return -1 ;
		}

	/* 
	We need to check if d_table.dma_entries is oct-aligned aligned
	If not, we need to add 8 bytes and align it to oct boundary
	*/


 	d_table.dma_entries = (cosmo2DMAentry_t *)
             ( ( (__paddr_t) d_table.dma_entries + 127 ) & (~0x7f) ) ;

	msg_printf(DBG, "tbl entries  =  0x%x\n", (__paddr_t)d_table.dma_entries);
	for  (i = 0; i  < chunks_2_xfer ; i++) {
		msg_printf (DBG, " entering iteration %d \n", i) ;
#ifdef IP30
		d_table.dma_entries[i].start_addr = (__uint32_t)kv_to_bridge32_dirmap((void *)cosmo2brbase, (__psunsigned_t)str_addr);
		msg_printf (DBG, "  cosmo2brbase 0x%x \n", cosmo2brbase) ;
		msg_printf (DBG, "  d_table.dma_entries[i].start_addr 0x%x \n", (__uint32_t) (d_table.dma_entries[i].start_addr)) ;

#else
			d_table.dma_entries[i].start_addr = K1_TO_PHYS(str_addr) ;
#endif
		
			d_table.dma_entries[i].ent.ent.i0 =  0;
			d_table.dma_entries[i].ent.ent.i1 =  0;
			d_table.dma_entries[i].ent.ent.i2 =  0;
			d_table.dma_entries[i].ent.ent.i3 =  0;

			d_table.dma_entries[i].ent.ent.eox = 0;

			d_table.dma_entries[i].ent.ent.first_b_mkr = 
				fbmkr_mask[str_addr % 8] ;

			d_table.dma_entries[i].ent.ent.last_b_mkr =
					lbmkr_mask[end_addr % 8] ;

			msg_printf(DBG, "fbmrkr %x ", 
				d_table.dma_entries[i].ent.ent.first_b_mkr  & 0x7f  );
			msg_printf(DBG, "lbmrkr %x \n", 
				d_table.dma_entries[i].ent.ent.last_b_mkr & 0x7f );

			d_table.dma_entries[i].ent.ent.t_count = 
				(roundup((unsigned) end_addr, 8) 
				- rounddown((unsigned) str_addr, 8)) / 8 ;

			msg_printf(DBG, "gio cycle count  0x%x \n", 
		(__uint32_t)(d_table.dma_entries[i].ent.ent.t_count & 0xfff));


               msg_printf(DBG, "len %x Q_words %x chunks_2_xfer %x \n", 
           			                 len, Q_words, chunks_2_xfer ) ;
               msg_printf(DBG, "str_addr 0x%x end_addr 0x%x num_bytes 0x%x \n", 
                                    str_addr, end_addr, Num_Bytes);

			str_addr = end_addr ;
			len = len - Num_Bytes ;

        		if ((chunks_2_xfer - i) == 1)  {
           		     Num_Bytes = len ;
           		     end_addr = str_addr + Q_words * XFER_WIDTH ; 
	       		}
   		   		  else {
           	    	Num_Bytes = level * XFER_WIDTH ;
           	    	end_addr = str_addr + Num_Bytes  ;
#if 0
           	    	end_addr = str_addr + Num_Bytes - XFER_WIDTH  ;
#endif
           		}
			

		msg_printf (DBG, " completing chunk_2_xfer %d \n\n", i) ;
		}

		i-- ;
		d_table.dma_entries[i].ent.ent.eox = 1 ;
		d_table.dma_entries[i].ent.ent.i0 =  1 ;

		msg_printf(DBG, "d_table.dma_entries[%d] = 0x%x \n", i, d_table.dma_entries[i].ent.all); 

		d_table.phys_addr = (int *)(d_table.dma_entries) ;
		msg_printf(DBG, "d_table.phys_addr 0x%x\n", d_table.phys_addr);

#ifdef IP30
		phys_addr = kv_to_bridge32_dirmap((void *)cosmo2brbase, (__psunsigned_t)d_table.phys_addr);
#else
		phys_addr = K1_TO_PHYS (d_table.phys_addr);
#endif
		msg_printf(DBG, "phys_addr 0x%x \n", phys_addr );


		msg_printf(DBG, " should Kick off DMA \n") ;

		flush_cache();
#if 1

		/* assign the address to dtblptr */
		cgi1_write_reg(phys_addr, cosmobase+cgi1_tpo[chnl] ); 
#endif

		flush_cache();

		return(1);

}

extern __uint64_t dma_data[ DMA_DATA_SIZE ] ;

void PRINT_CGI1_REGS ( void )  ;


void PRINT_CGI1_REGS ( void ) 
{
		unsigned long recvl ;
			int i;
            recvl = cgi1_read_reg(cosmobase + cgi1_overview_o);
            msg_printf(SUM, "  cgi1_overview_o %x \n",  recvl);
            recvl = cgi1_read_reg(cosmobase + cgi1_dma_control_o);
            msg_printf(DBG, "  cgi1_dma_control_o %x \n",  recvl);
            recvl = cgi1_read_reg(cosmobase + cgi1_interrupt_status_o);
            msg_printf(DBG, "  cgi1_interrupt_status_o %x \n",  recvl);
            recvl = cgi1_read_reg(cosmobase + cgi1_interrupt_mask_o);
            msg_printf(DBG, "  cgi1_interrupt_mask_o %x \n",  recvl);
            recvl = cgi1_read_reg(cosmobase + cgi1_tpo[CHANNEL_0]);
            msg_printf(DBG, "  DTP 0 %x \n",  recvl);
            recvl = cgi1_read_reg(cosmobase + cgi1_tpo[CHANNEL_1]);
            msg_printf(DBG, "  DTP 1 %x \n",  recvl);

			i=0;
			do{
			msg_printf(DBG, "i=%d a=%x ",i, d_table.dma_entries[i].start_addr);
            msg_printf(DBG, "c=%x \n",
				(__uint32_t)(d_table.dma_entries[i].ent.ent.t_count & 0xfff));
			i++;
			} while(d_table.dma_entries[i-1].ent.ent.eox==0);

            return ;
}

volatile unsigned char *lio_isr0 ; 

UBYTE CheckInterrupt ( void)
{
	__paddr_t mask , i, value;
	unsigned long status ;

			timeOut = 0xffff ;
#if !defined(IP30)
        do {
            timeOut-- ;
        } while (!((*lio_isr0) & LIO_GIO_1) && (timeOut));
        if (!timeOut) {
            PRINT_CGI1_REGS ();
            msg_printf(ERR, " Time Out occured %x \n", timeOut );
			timeOut = 0xff ;

        	*lio_isr0 = 0x0;
            status = cgi1_read_reg(cosmobase + cgi1_interrupt_status_o);
            return  0;
        }

#else
		/* cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o); */	
		status = cgi1_read_reg(cosmobase + cgi1_overview_o);
		msg_printf(DBG, "  cgi1_overview_o 0x%x \n",  status);

        i = 0xffff;
        do {
        i--;
        BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
        if (value & 0x2) { msg_printf(DBG, " DMA DONE %d \n", i); break;}
        } while ( i > 0);

        msg_printf(DBG, " Bridge inter status 0x%x After DMA kick off \n", value);
        value = cgi1_read_reg(cosmobase+ cgi1_interrupt_status_o);
        msg_printf(DBG, " inter status 0x%x \n", value);

        if (i == 0)
        {
        msg_printf(SUM, " DMA Timed-Out : \n");
        msg_printf(SUM, " inter status 0x%x \n", value);
        return (0);
        }

#endif
            status = cgi1_read_reg(cosmobase + cgi1_interrupt_status_o);
            msg_printf(DBG, "  cgi1_interrupt_status_o %x \n",  status);
            mask = cgi1_read_reg(cosmobase + cgi1_interrupt_mask_o);
			status=status& ~mask;
            msg_printf(DBG, " masked cgi1_interrupt_status_o %x \n",  status);
			cgi1_write_reg(status, cosmobase+cgi1_interrupt_status_o );

		return ( 1 );
}

__uint64_t  recv_data1[DMA_DATA_SIZE+(2*128/8)];
__uint64_t  dma_data_all[DMA_DATA_SIZE+(2*128/8)];



__uint32_t
cosmo2_recv_dma_data(UBYTE chnl, UBYTE dir, __uint64_t *dma, __uint32_t len)
{

    static UBYTE i = 1, j = 1;
    __uint64_t  *recv, *dma_al, value;

#if !defined(IP30)

	lio_isr0 = K1_LIO_0_ISR_ADDR;
     *lio_isr0 = 0x0;
#else
        cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o);
        us_delay(1000);
        BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
        msg_printf(DBG, " Bridge inter status 0x%x Before DMA kick off \n",value);
#endif

    if (dir == FROM_GIO) {
    dma_al = (__uint64_t *)(((((__uint64_t )dma_data_all)+127)>>7) <<7);
    cosmo2_1to2_dma_swap(dma_al, dma, 0);
    dma_setup(chnl, dir, dma_al, len, DMA_DATA_SIZE);
    } else {
    recv = (__uint64_t *)(((((__uint64_t)recv_data1)+127)>>7) <<7);
    dma_setup(chnl, dir, recv, len, DMA_DATA_SIZE);
    }


    if (i == 0) {
    msg_printf(SUM, " dma_data_all %x dma_al %x buffer size %x \n",
        (__paddr_t) dma_data_all, (__paddr_t) dma_al, (DMA_DATA_SIZE+(2*128/8)));
    i = 1;
    }

    if ((j == 0)    && (dir == TO_GIO)) {
    msg_printf(SUM, " recv_data1 %x recv %x buffer size %x \n",
        (__paddr_t) recv_data1, (__paddr_t) recv, (DMA_DATA_SIZE+(2*128/8)));
    j = 1;
    }

#if 0
    if (!CheckInterrupt ( )) {
     /*                       *lio_isr0 = 0x0; */
                            G_errors++;
                            return (1);
                            }
#endif
    if (dir == TO_GIO)
    cosmo2_1to2_dma_swap(dma, recv, 0);


/*     *lio_isr0 = 0x0;*/
	return (0);
}


UBYTE cos2_dma0()
{
    int i = 0;
   	__uint64_t value = 0; 

		G_errors = 0;
        Reset_050_016_FBC_CPC(CHANNEL_0);
        Reset_SCALER_UPC (CHANNEL_0 );
#if IP30
		/* clear the status register */
		cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o);
		us_delay(1000);
		BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
		msg_printf(DBG, " Bridge inter status 0x%x Before DMA kick off \n", value);

        cosmo2_recv_dma_data (CHANNEL_0, FROM_GIO, dma_data,
            DMA_DATA_SIZE*sizeof(__uint64_t)  ) ;
     /*   if (!CheckInterrupt ( )) return (0); */

        mcu_WRITE (UPC_FLOW_CONTROL + VIDCH1_UPC_BASE, 0xa1 );

		i = 0xffff;
		do {
		i--;
		BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
		if (value & 0x2) { msg_printf(DBG, " DMA DONE %d \n", i); break;}
		} while ( i > 0);

		msg_printf(DBG, " Bridge inter status 0x%x After DMA kick off \n", value);
		value = cgi1_read_reg(cosmobase+ cgi1_interrupt_status_o);	
		msg_printf(DBG, " inter status 0x%x \n", value);

		if (i == 0)
		{
		msg_printf(SUM, " DMA Timed-Out : \n");
		msg_printf(SUM, " inter status 0x%x \n", value);
		return (0);
		}
#endif
     cos2_EnablePioMode( );

        for ( i = 0; i < DMA_DATA_SIZE; i++) {
		msg_printf(DBG, " cos2_dma0 %d  \n", i);
           dma_recv_data[i] = cgi1_read_reg (cosmobase+cgi1_fifo_rw_o (CHANNEL_1));
		}
     cos2_DisEnablePioMode( );

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
                if (dma_recv_data[i] != dma_data[i] ) {
            msg_printf(DBG, "ERROR recv %llx exp %llx \n",
                            dma_recv_data[i], dma_data[i] );
            G_errors++;
        }

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n");
            return (1);
            }
}

UBYTE cos2_dma_to1()
{
    int i = 0;
   
#if !defined(IP30) 
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#else
	volatile unsigned char *lio_isr0 = 0x0;
#endif

        Reset_050_016_FBC_CPC(CHANNEL_0);
        Reset_SCALER_UPC (CHANNEL_0 );
        cosmo2_recv_dma_data (CHANNEL_1, FROM_GIO, dma_data,
            DMA_DATA_SIZE*sizeof(__uint64_t)  ) ;
        if (!CheckInterrupt ( )) return (0);

        mcu_WRITE (UPC_FLOW_CONTROL + VIDCH1_UPC_BASE, 0xa1 );

     cos2_EnablePioMode( );
        for ( i = 0; i < DMA_DATA_SIZE; i++)
           dma_recv_data[i] = cgi1_read_reg (cosmobase+cgi1_fifo_rw_o (CHANNEL_0));
     cos2_DisEnablePioMode( );

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
            msg_printf(SUM, "recv %llx exp %llx \n",
                dma_recv_data[i], dma_data[i] );

	return (1);

}

UBYTE cos2_dma10()
{

    int i = 0;
	__uint64_t value= 0;
    

	G_errors = 0;
	


    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

        Reset_050_016_FBC_CPC(CHANNEL_0);
        Reset_SCALER_UPC (CHANNEL_0 );
#if IP30
        /* clear the status register */
        cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o);
        us_delay(1000);
        BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
        msg_printf(DBG, " Bridge inter status 0x%x Before DMA kick off \n", value);

        cosmo2_recv_dma_data ( CHANNEL_1, FROM_GIO, dma_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        
        i = 0xffff;
        do {
        i--;
        BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
        if (value & 0x2) { msg_printf(DBG, " DMA DONE %d \n", i); break;}
        } while ( i > 0);

        msg_printf(DBG, " Bridge inter status 0x%x After DMA kick off \n", value);
        value = cgi1_read_reg(cosmobase+ cgi1_interrupt_status_o);
        msg_printf(DBG, " inter status 0x%x \n", value);

        if (i == 0)
        {
        msg_printf(SUM, " DMA Timed-Out : \n");
        msg_printf(SUM, " inter status 0x%x \n", value);
        return (0);
        }

#endif
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        mcu_WRITE (UPC_FLOW_CONTROL + VIDCH1_UPC_BASE, 0xa0 );

#if IP30
        /* clear the status register */
        cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o);
        us_delay(1000);
        BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
        msg_printf(DBG, " Bridge inter status 0x%x Before DMA kick off \n", value);

        cosmo2_recv_dma_data ( CHANNEL_0, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        
        i = 0xffff;
        do {
        i--;
        BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff, value);
        if (value & 0x2) { msg_printf(DBG, " DMA DONE %d \n", i); break;}
        } while ( i > 0);

        msg_printf(DBG, " Bridge inter status 0x%x After DMA kick off \n", value);
        value = cgi1_read_reg(cosmobase+ cgi1_interrupt_status_o);
        msg_printf(DBG, " inter status 0x%x \n", value);

        if (i == 0)
        {
        msg_printf(SUM, " DMA Timed-Out : \n");
        msg_printf(SUM, " inter status 0x%x \n", value);
        return (0);
        }

#endif
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
                if (dma_recv_data[i] != dma_data[i] ) {
            msg_printf(DBG, "ERROR recv %llx exp %llx \n",
                            dma_recv_data[i], dma_data[i] );
				G_errors++;
		}

		
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n");
            return (1);
            }



}


UBYTE cos2_dma01()
{

    int i = 0;

	G_errors = 0;

    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

        Reset_050_016_FBC_CPC(CHANNEL_0);
        Reset_SCALER_UPC (CHANNEL_0 );

        cosmo2_recv_dma_data ( CHANNEL_0, FROM_GIO, dma_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        mcu_WRITE (UPC_FLOW_CONTROL + VIDCH1_UPC_BASE, 0xa1 );

        cosmo2_recv_dma_data ( CHANNEL_1, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
                if (dma_recv_data[i] != dma_data[i] ) {
            msg_printf(DBG, "ERROR recv %llx exp %llx \n",
                            dma_recv_data[i], dma_data[i] );
			G_errors++;
		}

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n");
            return (1);
            }



}



UBYTE cos2_dma32()
{

    int i = 0;
    

	G_errors = 0;

    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

        Reset_050_016_FBC_CPC(CHANNEL_1);
        Reset_SCALER_UPC (CHANNEL_1 );



        cosmo2_recv_dma_data ( CHANNEL_3, FROM_GIO, dma_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        mcu_WRITE (UPC_FLOW_CONTROL + VIDCH2_UPC_BASE, 0xa0 );


        cosmo2_recv_dma_data ( CHANNEL_2, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
                if (dma_recv_data[i] != dma_data[i] ) {
            msg_printf(SUM, "ERROR recv %llx exp %llx \n",
                			dma_recv_data[i], dma_data[i] );
			G_errors++ ;
		}

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n");
            return (1);
            }

}




UBYTE cos2_dma23()
{
    int i = 0;
    

	G_errors = 0;


    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

        Reset_050_016_FBC_CPC(CHANNEL_1);
        Reset_SCALER_UPC (CHANNEL_1 );

        cosmo2_recv_dma_data( CHANNEL_2, FROM_GIO, dma_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        mcu_WRITE(UPC_FLOW_CONTROL + VIDCH2_UPC_BASE, 0xa1 );

        cosmo2_recv_dma_data( CHANNEL_3, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
                if (dma_recv_data[i] != dma_data[i] ) {
            msg_printf(DBG, "ERROR recv %llx exp %llx \n",
                			dma_recv_data[i], dma_data[i] );
			G_errors++;
		}

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n");
            return (1);
            }


}

cosmo2_dma_arb(  int argc, char **argv  )
{
    int i = 0, size;
    

    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

    G_errors = 0;
    Reset_050_016_FBC_CPC(CHANNEL_0);
    Reset_SCALER_UPC (CHANNEL_0 );

     argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 's' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &size);
                    argc--; argv++;
                } else {
                     atob(&argv[0][2], (int *) &size);
                }

            size = DMA_DATA_SIZE / size ;

            break;
            default:
            msg_printf(SUM, " Usage: cos2_fifo_lb -c chnl \n");
            break;
        }
        argc--; argv++;
    }

     cos2_EnablePioMode( );
        for ( i = 0; i < DMA_DATA_SIZE; i++)
            cgi1_write_reg (dma_data[i],cosmobase+cgi1_fifo_rw_o (CHANNEL_0));
     cos2_DisEnablePioMode( );

        mcu_WRITE (UPC_FLOW_CONTROL + VIDCH1_UPC_BASE, 0xa1 );

        cosmo2_recv_dma_data ( CHANNEL_1, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
        
        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n"); return (0);
            }

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++) {
            msg_printf(SUM, "i %d recv %llx exp %llx \n", i,
                            dma_recv_data[i], dma_data[i] );
                if (dma_recv_data[i] != dma_data[i] ) {
                    msg_printf(DBG, "ERROR i %d recv %llx exp %llx \n", i,
                                            dma_recv_data[i], dma_data[i] );
                    G_errors++;
            }
        }

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n"); return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n"); return (1);
            }
}


UBYTE cos2_dma1( int argc, char **argv )
{
	int i = 0, size;

	for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
		dma_recv_data[i] = 0 ; 

	G_errors = 0;
	Reset_050_016_FBC_CPC(CHANNEL_0);
	Reset_SCALER_UPC (CHANNEL_0 );

     argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 's' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &size);
                    argc--; argv++;
                } else {
                     atob(&argv[0][2], (int *) &size);
                }

			size = DMA_DATA_SIZE / size ;

            break;
            default:
            msg_printf(SUM, " Usage: cos2_fifo_lb -c chnl \n");
            break;
        }
        argc--; argv++;
    }

	 cos2_EnablePioMode( );
		for ( i = 0; i < DMA_DATA_SIZE; i++)
			cgi1_write_reg (dma_data[i],cosmobase+cgi1_fifo_rw_o (CHANNEL_0));
	 cos2_DisEnablePioMode( );

		mcu_WRITE (UPC_FLOW_CONTROL + VIDCH1_UPC_BASE, 0xa1 );

		cosmo2_recv_dma_data ( CHANNEL_1, TO_GIO, dma_recv_data, 
			DMA_DATA_SIZE*sizeof(__uint64_t) ) ;

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n"); return (0);
            }
		

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++) {
            msg_printf(SUM, "i %d recv %llx exp %llx \n", i, 
                            dma_recv_data[i], dma_data[i] );
                if (dma_recv_data[i] != dma_data[i] ) {
            		msg_printf(DBG, "ERROR i %d recv %llx exp %llx \n", i, 
                   							dma_recv_data[i], dma_data[i] );
            		G_errors++;
        	}
		}

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n"); return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n"); return (1);
            }
}


test_dma ()
{
    int i = 0;

	__uint64_t *ptr_64;
    G_errors = 0;


    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

        Reset_050_016_FBC_CPC(CHANNEL_1);
        Reset_SCALER_UPC (CHANNEL_1 );

		ptr_64 = (__uint64_t *) get_chunk(256);
		cosmo2_1to2_dma_swap(ptr_64, dma_data, 0);
		
        cosmo2_recv_dma_data( CHANNEL_2, FROM_GIO, ptr_64,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        mcu_WRITE(UPC_FLOW_CONTROL + VIDCH2_UPC_BASE, 0xa1 );

        cosmo2_recv_dma_data( CHANNEL_3, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
                if (dma_recv_data[i] != ptr_64[i] ) {
            msg_printf(DBG, "ERROR recv %llx exp %llx \n",
                            dma_recv_data[i], ptr_64[i] );
            G_errors++;
        }

        if (G_errors) {
            msg_printf(SUM, "DMA Failed \n");
            return (0);
            }
        else {
            msg_printf(SUM, "DMA passed \n");
            return (1);
            }


}

int cosmo2_iter = 0;

void cosmo2_dma_tog( int argc, char **argv )
{
    int i = 0 ;
    int chnl ;


    for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
        dma_recv_data[i] = 0 ;

     argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 'c' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &chnl);
                    argc--; argv++;
                } else {
                     atob(&argv[0][2], (int *) &chnl);
                }
            break;
            default:
            msg_printf(SUM, " Usage: cosmo2_dma_tog -c chnl \n");
            break;
        }
        argc--; argv++;
    }


        cosmo2_recv_dma_data ( chnl, TO_GIO, dma_recv_data,
            DMA_DATA_SIZE*sizeof(__uint64_t) ) ;

            msg_printf(SUM, "cosmo2_iter %d \n", cosmo2_iter   );
		cosmo2_iter++;

        for ( i = 0 ; i < DMA_DATA_SIZE ; i++)
            msg_printf(SUM, " i %d recv %llx \n", i, dma_recv_data[i]  );
        
}

void cosmo2_setfifolevels ()
{
cosmo2_dma_initialize_fifos_channel (DMA_DATA_SIZE, 0);
cosmo2_dma_initialize_fifos_channel (DMA_DATA_SIZE, 1);
cosmo2_dma_initialize_fifos_channel (DMA_DATA_SIZE, 2);
cosmo2_dma_initialize_fifos_channel (DMA_DATA_SIZE, 3);
}
