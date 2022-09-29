/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1999, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Odsy Graphics I2C interface.
 */
#include "odsy_internals.h"

static int odsy_i2c_busy_high_loop(port_handle *);

/*
 * 1.  If we're the I2C master transmitter, and if we have just
 * received an ACK bit, we delay before sending another byte.
 *
 * 2.  If we're the I2C master receiver, and if we have just sent an
 * ACK bit, we delay before starting the receive of the next byte.
 */

/*
 * i2c_start: Generate the I2C START sequence and the slave address
 *
 * Arguments:
 *            port_handle -- contains bdata structure and tells us 
 *			     whether we are talking the option or
 *			     main i2c ports.
 *            addr -- address of I2C slave you are contacting
 *            ack --  flag of value [ODSY_I2C_ACK_ASSERT|ODSY_I2C_ACK_NULL]
 * Returns:
 *            0 on success, 1 for failure, 2 for timeout
 *            Inversion of ack expectation will generate a failure
 *
 */
int
odsy_i2c_start(port_handle *port, unsigned char addr,
	          unsigned short ack)
{
  int rc;

  ODSY_DFIFO_PSEMA(port->bdata);
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, ODSY_DFIFO_LEVEL_REQ(2))) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_TIMEOUT);
  }

  port->control_reg->bf.f_idle_bit = 1;
  ODSY_WRITE_I2C_CONTROL(port);

  DELAY(100);

  port->control_reg->bf.f_idle_bit = 0;
  ODSY_WRITE_I2C_CONTROL(port);

  DELAY(100);

  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_TIMEOUT);
  }
  ODSY_READ_I2C_CONTROL(port);

#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_i2c_start(): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif

#ifdef ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_start(): ack = %d\taddr = 0x%x\n",ack,addr);
	printf("odsy_i2c_start(): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
	printf("Recieved Word is 0x%08X\n", port->control_reg->w32);
	printf("Send_port->control_reg->w32     0x%02X\n",port->control_reg->w32 & 0xff);
	printf("Start_bit     0x%1X\n",(port->control_reg->w32 >>8 )& 0x1);
	printf("Xfer_bit      0x%1X\n",(port->control_reg->w32 >>9 )& 0x1);
	printf("Send_Ack      0x%1X\n",(port->control_reg->w32 >>10)& 0x1);
	printf("Stop_bit      0x%1X\n",(port->control_reg->w32 >>11)& 0x1);
	printf("Send_Recv_N   0x%1X\n",(port->control_reg->w32 >>12)& 0x1);
	printf("F_Stop_bit    0x%1X\n",(port->control_reg->w32 >>13)& 0x1);
	printf("F_Idle_bit    0x%1X\n",(port->control_reg->w32 >>14)& 0x1);
	printf("SDA_out_host  0x%1X\n",(port->control_reg->w32 >>15)& 0x1);
	printf("SCL_out_host  0x%1X\n",(port->control_reg->w32 >>16)& 0x1);
	printf("Slave_En      0x%1X\n",(port->control_reg->w32 >>17)& 0x1);
	printf("Slave_Addr    0x%02X\n",(port->control_reg->w32 >>18)& 0x7f);
#endif

  /* load control reg for start sequence */
  port->control_reg->bf.send_data = addr;
  port->control_reg->bf.start_bit = 1;
  port->control_reg->bf.xfer_bit = 1;
  port->control_reg->bf.send_recv_n = 1;
  port->control_reg->bf.stop_bit = 0;

#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_i2c_start(again): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif

  /* perform the PIO write to control register */
  ODSY_WRITE_I2C_CONTROL(port);

  ODSY_DFIFO_VSEMA(port->bdata);

  port->control_reg->bf.start_bit = 0;
  
  /* LOOP HERE on i2c_xfer_done bit */ 
  rc = odsy_i2c_xfer_done_loop(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_start(): completed xfer_done loop, returned %d\n",rc);
#endif

  /* did the loop timeout? */
  if(rc == ODSY_I2C_TIMEOUT)
  {
    return rc;
  }
#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  cmn_err(CE_ALERT,"Looking for ack %d\n", ack);
#endif
  rc = odsy_i2c_recv_ack(port, !ack);
  return rc;

}


#if !defined( _STANDALONE )

/*
 * i2c_stop: Generate the STOP condition.
 *
 * This function doesn't delay, but DDC2B specifies there should be a
 * delay between this stop and the next start.
 * Arguments:
 *            port_handle -- contains bdata structure and tells us 
 *			     whether we are talking the option or
 *			     main i2c ports.
 * Returns:
 *            ODSY_I2C_SUCCESS, ODSY_I2C_FAILURE, ODSY_I2C_TIMEOUT
 * 
 */

int
odsy_i2c_stop(port_handle *port)
{
  int rc;

  ODSY_DFIFO_PSEMA(port->bdata);
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, ODSY_DFIFO_LEVEL_REQ(1))) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_FAILURE);
  }

  /* set stop_bit and clear xfer_bit */
  port->control_reg->bf.f_stop_bit = 1;
  port->control_reg->bf.xfer_bit = 0;

  /* write shadow state to hw */
  ODSY_WRITE_I2C_CONTROL(port);

  ODSY_DFIFO_VSEMA(port->bdata);

  /* loop on xfer_done bit */
  rc = odsy_i2c_xfer_done_loop(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_stop(): completed xfer_done loop, returned %d\n",rc);
#endif

  /* check to see if the loop timed out or 
   * exited normaly.
   */
  if(rc == ODSY_I2C_TIMEOUT)
  {
    return rc;
  }

  port->control_reg->w32 = 0;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_stop(): control reg val should be 0: 0x%x\n",port->control_reg->w32);
#endif

  ODSY_DFIFO_PSEMA(port->bdata);
  /* read to see if transfer really stopped */
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_FAILURE);
  }
  ODSY_READ_I2C_STATUS(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_stop(): just read status reg, i2c_busy should be low",port->status_reg->w32);
#endif

  ODSY_DFIFO_VSEMA(port->bdata);

  /*
   * if i2c_busy is still high then transfer didn't stop 
   * and we should return failure
   */
  if(port->status_reg->bf.busy)
  {
    return ODSY_I2C_FAILURE;
  }

  return ODSY_I2C_SUCCESS;
}

#endif /* !defined( _STANDALONE ) */


/*
 * odsy_i2c_sendbyte: Transmit a byte of data and get acknowledge bit.
 * In all current protocols, we expect an ACK bit response.
 * Arguments:
 *            port_handle -- contains bdata structure and tells us 
 *			     whether we are talking the option or
 *			     main i2c ports.
 *	      data	  -- 8 bit data that will be sent on the line
 *	      ack	  -- whether of not we need an acknowledgement single (ODSY_I2C_ACK_ASSERT|ODSY_I2C_ACK_NULL).
 *	      stop	  -- whether or not we need a stop sequence (ODSY_I2C_STOP_ASSERT|ODSY_I2C_STOP_NULL).
 * Returns:
 *            ODSY_I2C_SUCCESS, ODSY_I2C_FAILURE, ODSY_I2C_TIMEOUT 
 */
int odsy_i2c_sendbyte(port_handle *port, unsigned char data,
	              unsigned short ack, unsigned short stop)
{
  int rc;

/*XXX-This is a hack for sim:stick something in here to make busy high */

#ifdef ODSY_SIM_KERN
  port->status_reg->bf.busy = 1;
#endif /* ODSY_SIM_KERN */

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_sendbyte(): byte=%x, checking shadow status reg 0x%x, i2c_busy should be high\n", data, port->status_reg->w32);
#endif

  /* loop on busy high */
  rc = odsy_i2c_busy_high_loop(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_sendbyte(): completed busy_high loop, returned %d\n",rc);
#endif

  /* did the loop timeout or did we exit normally */
  if(rc == ODSY_I2C_TIMEOUT)
  {
    return rc;
  }

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_sendbyte(): just read control reg: 0x%x\n",port->control_reg->w32);
#endif

  /* if a stop sequence is required we need to load the stop bit */
  if(stop == ODSY_I2C_STOP_ASSERT)
  {
    port->control_reg->bf.stop_bit = 1; /*XXX not sure if this is right */
  }
  else
  {
    port->control_reg->bf.stop_bit = 0;
  }

  /* set xfer_bit to high and load the data into the send_data field */
  port->control_reg->bf.xfer_bit = 1;
  port->control_reg->bf.send_data = data;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_sendbyte(): about to write shadow control reg to hw: 0x%x\n",port->control_reg->w32);
#endif

  ODSY_DFIFO_PSEMA(port->bdata);
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, ODSY_DFIFO_LEVEL_REQ(1))) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_TIMEOUT);
  }

  /* write the shadow state back to hw including the send data */
  ODSY_WRITE_I2C_CONTROL(port);
  ODSY_DFIFO_VSEMA(port->bdata);
  
  /* loop on xfer done */
  rc = odsy_i2c_xfer_done_loop(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_sendbyte(): completed xfer_done loop, returned %d\n",rc);
#endif

  /* did the loop timeout or did we exit normally */
  if(rc == ODSY_I2C_TIMEOUT)
  {
    return rc;
  }

  /*
   * if xfer_done loop exits properly and a stop sequence was required
   * clear the stop bit.
   */
  if(stop == ODSY_I2C_STOP_ASSERT)
  {
    port->control_reg->bf.stop_bit = 0; /*XXX not sure if this is right */
  }

  rc = odsy_i2c_recv_ack(port, !ack);
#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_i2c_sendbyte(): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif
  return rc;
}


#if !defined( _STANDALONE )

/* 
 * odsy_i2c_short_recvbyte: Master receive a byte without 
 * changing control register 
 * Arguments:
 *		port  -- handle that contains bdata structure
 *			 and tells us which port we are talking
 *			 to (MAIN or OPT).
 *		datap -- already allocated block of memory that
 *			 will hold the received data.
 * Returns:
 *            0 on success, 1 for failure, 2 for timeout
 */

int odsy_i2c_short_recvbyte(port_handle *port, unsigned char *datap)
{
  int rc;

  /* loop on xfer_done */
  rc = odsy_i2c_xfer_done_loop(port);

#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_short_recvbyte(): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_short_recvbyte(): completed xfer_done loop, returned %d\n",rc);
#endif

  /* check to see if we timed out in the above loop */
  if(rc == ODSY_I2C_TIMEOUT)
  {
    return rc;
  }

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_short_recvbyte(): look at status reg for error conditions: 0x%x\n",port->status_reg->w32);
#endif

  /* 
   * check to see if the line is busy or there have been
   * and data or clock errors.
   */
  if(!port->status_reg->bf.busy    ||
     port->status_reg->bf.scl_err  ||
     port->status_reg->bf.sda_err  )
  {
    return ODSY_I2C_FAILURE;
  }

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_short_recvbyte(): look at status reg for received data: 0x%x\n",port->status_reg->w32);
#endif

#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_short_recvbyte(again): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif

  /* place that received data into the memory already allocated */
  *datap = port->status_reg->bf.recv_data;

  return ODSY_I2C_SUCCESS;
}

/* 
 * pbj_i2c_long_recvbyte: Master receive a byte PLUS change control register 
 * Arguments:
 *		port  -- handle that contains bdata structure
 *			 and tells us which port we are talking
 *			 to (MAIN or OPT).
 *	      	ack   -- whether of not we need an acknowledgement single (ODSY_I2C_ACK_ASSERT|ODSY_I2C_ACK_NULL).
 *		datap -- already allocated block of memory that
 *			 will hold the received data.
 * Returns:
 *            ODSY_I2C_SUCCESS, ODSY_I2C_FAILURE, ODSY_I2C_TIMEOUT
 */
int odsy_i2c_long_recvbyte(port_handle *port, unsigned short ack, unsigned short stop,
		           unsigned char *datap)
{
  int rc;

  /*
   * Check to see if we need to change ack, if so read control reg
   * and change send_recv_n and put new ack on the line. If we don't
   * change the ack this just becomes a pass through and calls 
   * short_recvbyte and returns its return value.
   */
  if(ack == port->control_reg->bf.send_ack) /*XXX special hack for inverted acks!!! */
  {
    /* read the control reg into the shadow state */
#ifndef ODSY_SIM_KERN
    ODSY_DFIFO_PSEMA(port->bdata);
    if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
      ODSY_DFIFO_VSEMA(port->bdata);
      return(ODSY_I2C_FAILURE);
    }
    ODSY_READ_I2C_CONTROL(port);
    ODSY_DFIFO_VSEMA(port->bdata);
#endif /* ODSY_SIM_KERN */

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_long_recvbyte(): just read control reg: 0x%x\n",port->control_reg->w32);
#endif

#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_long_recvbyte(): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif

    /* clear the recv_n bit */
    port->control_reg->bf.send_recv_n = 0;

    /* clear the start bit */
    port->control_reg->bf.start_bit = 0;

    /* trying to load the send_ack bit as appropriate */
    if(ack == ODSY_I2C_ACK_ASSERT)
    {
      port->control_reg->bf.send_ack = 0; /* 0 is an ACK */
    }
    else
    {
      port->control_reg->bf.send_ack = 1; /* 1 is a NACK */
    }

    /* XXX special hack to see if that changes our Osc_Div value */
    port->control_reg->bf.send_data = 0;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_long_recvbyte(): writting control reg back after modifying: 0x%x\n",port->control_reg->w32);
#endif

    ODSY_DFIFO_PSEMA(port->bdata);
    if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, ODSY_DFIFO_LEVEL_REQ(1))) {
      ODSY_DFIFO_VSEMA(port->bdata);
      return(ODSY_I2C_FAILURE);
    }

    /* write the shadow state back to the hw */
    ODSY_WRITE_I2C_CONTROL(port);
    ODSY_DFIFO_VSEMA(port->bdata);

#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
  printf("odsy_long_recvbyte(again): control_reg 0x%x\tstatus_reg 0x%x\n", 
                port->control_reg->w32,port->status_reg->w32);
#endif

  }

  /* call "short_recevbyte" and return it's exit status */
  rc = odsy_i2c_short_recvbyte(port, datap);

  /* trying to load the stop_bit as appropriate */
  if(stop == ODSY_I2C_STOP_ASSERT)
  {
    port->control_reg->bf.stop_bit = 1;
  }
  else
  {
    port->control_reg->bf.stop_bit = 0;
  }

  ODSY_DFIFO_PSEMA(port->bdata);
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, ODSY_DFIFO_LEVEL_REQ(1))) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_FAILURE);
  }

  /* write the shadow state back to the hw */
  ODSY_WRITE_I2C_CONTROL(port);
  ODSY_DFIFO_VSEMA(port->bdata);

  if(stop == ODSY_I2C_STOP_ASSERT)
  {
    port->control_reg->bf.stop_bit = 0; /*XXX not sure if this is right */
  }

  if(ack == ODSY_I2C_ACK_NULL)
  {
    port->control_reg->bf.send_ack = 0; /* clear the send_ack bit */
  }

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_long_recvbyte(): returned from short_recvbyte, returned: 0x%x\n",rc);
#endif

  return rc;
}

/*
 * odsy_i2c_becomeSlaveRecv: Perform a stop and then immediately become
 * a slave receiver
 * Arguments:
 *		port  -- handle that contains bdata structure
 *			 and tells us which port we are talking
 *			 to (MAIN or OPT).
 *	      	ack   -- whether of not we need an acknowledgement single (ODSY_I2C_ACK_ASSERT|ODSY_I2C_ACK_NULL).
 *		slave_addr -- address of slave to receive from
 *		datap -- already allocated block of memory that
 *			 will hold the received data.
 * Returns:
 *            ODSY_I2C_SUCCESS, ODSY_I2C_FAILURE, ODSY_I2C_TIMEOUT
 */
int odsy_i2c_becomeSlaveRecv(port_handle *port, unsigned short ack, unsigned short slave_addr, unsigned char *datap)
{
  int rc;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_becomeSlaveRecv(): check status, busy should be high: 0x%x\n", port->status_reg->w32);
#endif

/*XXX-This is a hack for sim: do something here to make busy high */

#ifdef ODSY_SIM_KERN
  port->status_reg->bf.busy = 1;
#endif /* ODSY_SIM_KERN */

  /* check I2C bus state (it should be busy) */
  if(!(port->status_reg->bf.busy))
  {
    cmn_err(CE_ALERT,"becomeSlaveRecv: I2C status not busy\n");
    return ODSY_I2C_FAILURE;
  }

#ifndef ODSY_SIM_KERN
  ODSY_DFIFO_PSEMA(port->bdata);
  /* read the control register into the shadow state. */
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_TIMEOUT);
  }
  ODSY_READ_I2C_CONTROL(port);
  ODSY_DFIFO_VSEMA(port->bdata);
#endif /* ODSY_SIM_KERN */

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_becomeSlaveRecv(): just read control reg: 0x%x\n", port->control_reg->w32);
#endif

  /* set the f_stop_bit, xfer_bit and the slave_mode_en bit */
  port->control_reg->bf.f_stop_bit = 1;
  port->control_reg->bf.xfer_bit = 0;
  port->control_reg->bf.slave_mode_en = 1;

  /* check whether or not we need to send an ack */
  if(ack == ODSY_I2C_ACK_ASSERT)
  {
    port->control_reg->bf.send_ack = 0;
  }
  else if(ack == ODSY_I2C_ACK_NULL)
  {
    port->control_reg->bf.send_ack = 1;
  }

  /* load the slave address into the shadow state. */
  port->control_reg->bf.slave_addr = slave_addr;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_becomeSlaveRecv(): write back control reg after modifying: 0x%x\n", port->control_reg->w32);
#endif

  ODSY_DFIFO_PSEMA(port->bdata);
  if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, ODSY_DFIFO_LEVEL_REQ(1))) {
    ODSY_DFIFO_VSEMA(port->bdata);
    return(ODSY_I2C_TIMEOUT);
  }

  /* load the shadow state into the hw */
  ODSY_WRITE_I2C_CONTROL(port);
  ODSY_DFIFO_VSEMA(port->bdata);

  /* loop on xfer done to occur */
  rc = odsy_i2c_xfer_done_loop(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_becomeSlaveRecv(): completed xfer_done loop, returned %d\n",rc);
#endif

  /* check whether or not we timed out on the above loop */
  if(rc == ODSY_I2C_TIMEOUT)
  {
    return rc;
  }

  /*
   * if we saw either a clock error or data error we should
   * return failure
   */
  if(port->status_reg->bf.scl_err ||
     port->status_reg->bf.sda_err  )
  {
    return ODSY_I2C_FAILURE;
  }

  /*
   * if we make it to this point then store the received data
   * into the memory already allocated and passed in.
   */
  *datap = port->status_reg->bf.recv_data;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_becomeSlaveRecv(): received data: 0x%x\n",*datap);
#endif


  /* clear the stop bit in the shadow state */
  port->control_reg->bf.f_stop_bit = 0;

  return ODSY_I2C_SUCCESS;
}

/*
 * odsy_i2c_slave_recvbyte - Read byte while other device is controlling CLK.
 * Arguments:
 *		port   -- handle that contains bdata structure
 *			  and tells us which port we are talking
 *			  to (MAIN or OPT).
 *	      	ack    -- whether of not we need an acknowledgement single (ODSY_I2C_ACK_ASSERT|ODSY_I2C_ACK_NULL).
 *		datap  -- already allocated block of memory that
 *			  will hold the received data.
 *		buflen -- size of the data buffer
 * Returns:
 *            0 on success, 1 for failure, 2 for timeout, 3 for buffer overflow
 */

int odsy_i2c_slave_recvbyte(port_handle *port, unsigned short ack,
		   unsigned short buflen, unsigned char *datap) 
{
  int rc;
  short buffer_count = 0;

  /*XXX have no idea if this is right */
  /* attempting to determine if ack needs to change? */
  if(port->control_reg->bf.send_ack != ack)
  {
    port->control_reg->bf.send_ack = ack;
  }
   
  /* outermost loop waiting until I2C is not busy */
  while(port->status_reg->bf.busy)
  {
    /* inner loop on xfer_done */
    rc = odsy_i2c_xfer_done_loop(port);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_slave_recvbyte(): completed xfer_done loop, returned %d\n",rc);
#endif

    /* did we timeout from above loop */
    if(rc == ODSY_I2C_TIMEOUT)
    {
      return rc;
    }
  
    /* if we saw either a data or clock error, return failure */
    if(port->status_reg->bf.scl_err ||
       port->status_reg->bf.sda_err  )
    {
#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_slave_recvbyte(): data or clock error was recieved in status reg: 0x%x\n",port->status_reg->w32);
#endif
      return ODSY_I2C_FAILURE;
    }

    /*
     * if our buffer_count (number of bytes so far) is greater than
     * the size of our data buffer, invert the ack signal and return
     * buffer overflow.
     */
    if(buffer_count > buflen)
    {
#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_slave_recvbyte(): buffer_count exceded available memory: %d\n",buffer_count);
#endif
      port->control_reg->bf.send_ack ^= 0x1; /* invert the acknowledment */
      return ODSY_I2C_BUFFER_OVFLW;
    }

    ODSY_DFIFO_PSEMA(port->bdata);
    /* read the status register for the received data */
    if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
      ODSY_DFIFO_VSEMA(port->bdata);
      return(ODSY_I2C_FAILURE);
    }
    ODSY_READ_I2C_STATUS(port);
    ODSY_DFIFO_VSEMA(port->bdata);

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_slave_recvbyte(): buffer_count exceded available memory: %d\n",buffer_count);
#endif

    /* place the received data into the memory buffer */
    datap[buffer_count++] = port->status_reg->bf.recv_data;

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_slave_recvbyte(): datap: 0x%x\n",datap[buffer_count - 1]);
#endif

  }

  return ODSY_I2C_SUCCESS;
}

#endif  /* !defined( _STANDALONE ) */

/*
 * odsy_i2c_xfer_done_loop - since so many functions had the same loop code I 
 * put it into one function that does a sleep loop on the xfer_done bit. It
 * will return timeout or success as appropriate.
 * Arguments:
 *		port   -- handle that contains bdata structure
 *			  and tells us which port we are talking
 *			  to (MAIN or OPT).
 * Returns:
 *            0 on success, 1 for failure, 2 for timeout
 */
int odsy_i2c_xfer_done_loop(port_handle *port)
{
  unsigned long loop_count = 0; /* used to keep track of whether or not we've
				 * passed the loop limit.
				 */

  /* infinite loop that will be broken if we pass loop limit or xfer_done is set */
  for(;;)
  {
    loop_count++; /* keeps track of whether or not we went over LOOP_LIMIT */
    if(loop_count >= LOOP_LIMIT)
    {
      cmn_err(CE_ALERT,"I2C:timeout on xfer_done");
      cmn_err(CE_ALERT,"in xfer_done status is: 0x%x",port->status_reg->w32);
  cmn_err(CE_ALERT,"Recv_port->status_reg->w32     0x%02X\n",port->status_reg->w32& 0xff);
  cmn_err(CE_ALERT,"Recv_Ack      0x%1X\n",(port->status_reg->w32>>8 )& 0x1);
  cmn_err(CE_ALERT,"Xfer_done     0x%1X\n",(port->status_reg->w32>>9 )& 0x1);
  cmn_err(CE_ALERT,"Curr_State    0x%1X\n",(port->status_reg->w32>>10)& 0xf);
  cmn_err(CE_ALERT,"Next_State    0x%1X\n",(port->status_reg->w32>>14)& 0xf);
  cmn_err(CE_ALERT,"SDA_in        0x%1X\n",(port->status_reg->w32>>18)& 0x1);
  cmn_err(CE_ALERT,"SCL_in        0x%1X\n",(port->status_reg->w32>>19)& 0x1);
  cmn_err(CE_ALERT,"Clock_err     0x%1X\n",(port->status_reg->w32>>20)& 0x1);
  cmn_err(CE_ALERT,"Data_err      0x%1X\n",(port->status_reg->w32>>21)& 0x1);
  cmn_err(CE_ALERT,"Busy          0x%1X\n",(port->status_reg->w32>>22)& 0x1);
  cmn_err(CE_ALERT,"Abnormal_Stop 0x%1X\n",(port->status_reg->w32>>23)& 0x1);
  cmn_err(CE_ALERT,"Curr_Slave    0x%02X\n",(port->status_reg->w32>>24)& 0xf);
  cmn_err(CE_ALERT,"Next_Slave    0x%02X\n",(port->status_reg->w32>>28)& 0xf);

      return ODSY_I2C_TIMEOUT;
    }

    ODSY_DFIFO_PSEMA(port->bdata);
    /* read the status reg to see whether or not xfer_done has been set */
    if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
      ODSY_DFIFO_VSEMA(port->bdata);
      return(ODSY_I2C_FAILURE);
    }
    ODSY_READ_I2C_STATUS(port);
    ODSY_DFIFO_VSEMA(port->bdata);

    /* if xfer done has not been set put us to sleep for a bit then then continue. */
    if(!port->status_reg->bf.xfer_done)
    {
#if ODSY_I2C_LOW_LEVEL_DEBUG
      if (loop_count == 1)
	printf("odsy_i2c_xfer_done_loop(): about to enter delay period\n");
#endif
      DELAY(DELAY_WHILE_XFER_NOT_DONE);
    }
    /* but if it has been set we can exit by returning success. */
    else
    {
#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_xfer_done_loop(): successfully completed loop, exiting...\n");
#endif
      return ODSY_I2C_SUCCESS;
    }
  }
}


int odsy_i2c_busy_high_loop(port_handle *port)
{
  unsigned long loop_count = 0; /* used to keep track of whether or not we've
				 * passed the loop limit.
				 */

  /* infinite loop that will be broken if we pass loop limit or xfer_done is set */
  for(;;)
  {
    loop_count++; /* keeps track of whether or not we went over LOOP_LIMIT */
    if(loop_count >= LOOP_LIMIT)
    {
      cmn_err(CE_ALERT,"I2C:timeout on status busy");
      return ODSY_I2C_TIMEOUT;
    }

    ODSY_DFIFO_PSEMA(port->bdata);
    /* read the status reg to see whether or not xfer_done has been set */
    if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
      ODSY_DFIFO_VSEMA(port->bdata);
      return(ODSY_I2C_FAILURE);
    }
    ODSY_READ_I2C_STATUS(port);
    ODSY_DFIFO_VSEMA(port->bdata);

    /* if xfer done has not been set put us to sleep for a bit then then continue. */
    if(!port->status_reg->bf.busy)
    {
#if ODSY_I2C_LOW_LEVEL_DEBUG
      if (loop_count == 1)
	printf("odsy_i2c_busy_high_loop(): about to enter delay period\n");
#endif
      DELAY(DELAY_WHILE_XFER_NOT_DONE);
    }
    /* but if it has been set we can exit by returning success. */
    else
    {
#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_busy_high_loop(): successfully completed loop, exiting...\n");
#endif
      return ODSY_I2C_SUCCESS;
    }
  }
}

/*
 * poll until recv_ack bit in the port status register is cleared
 */
int
odsy_i2c_recv_ack(port_handle *port, unsigned short ack)
{
    unsigned long count;

    for (count = 0; count < LOOP_LIMIT; count++) {
        /*
	 * if the ack we got back was not the one we were looking for
         * (i.e. nack) then we have a failure.
         */
        if (port->status_reg->bf.recv_ack == ack) {
#ifdef ODSY_SPECIAL_I2C_LOW_LEVEL_DEBUG
            cmn_err(CE_ALERT,"Found ack %d\n",ack);
#endif 
            return ODSY_I2C_SUCCESS;
        }
#if ODSY_I2C_LOW_LEVEL_DEBUG
	if (count == 0)
            printf("odsy_i2c_recv_ack(): status reg -- 0x%x\n", port->status_reg->w32);
#endif
	DELAY(DELAY_WHILE_XFER_NOT_DONE);
	ODSY_DFIFO_PSEMA(port->bdata);
	if (ODSY_POLL_DFIFO_LEVEL(port->bdata->hw, 0)) {
	  ODSY_DFIFO_VSEMA(port->bdata);
	  return(ODSY_I2C_TIMEOUT);
	}
	ODSY_READ_I2C_STATUS(port);
	ODSY_DFIFO_VSEMA(port->bdata);
    }
    return ODSY_I2C_TIMEOUT;
}


/*
 * odsy_i2c_init_port_handle - called once for each port handle. A pointer
 * to an alread allocated port_handle should be passed along with an 
 * odsy_data stucture and a port num (MAIN or OPT). The odsy_data structure
 * pointer is stored in the handle and the correct shadow regs are chosen
 * based on the port number. 
 * Arguments:
 *		port_hndl -- handle that contains bdata structure
 *			  and tells us which port we are talking
 *			  to (MAIN or OPT).
 *		bdata  -- data structure containing hw pointer and sysreg shadow (among other things).
 *		port   -- port number (MAIN|OPT).
 * Returns:
 *            0 on success, 1 for failure
 */
int odsy_i2c_init_port_handle(port_handle *prt_hndl, odsy_data *bdata, int port)
{

#if ODSY_I2C_LOW_LEVEL_DEBUG
	printf("odsy_i2c_init_port_handle(): port number: %d\n",port);
#endif

  /* make sure we have a valid port number or else nothing will work */
  if(port >= 2)
  {
    cmn_err(CE_ALERT, "init_port_handle: Invalid port number %d\n", port);
    return ODSY_I2C_FAILURE;
  }

  /*
   * Check which port they want and set the pointer to the appropriate
   * entry in the sysreg_shadow structure
   */
  if(port == ODSY_I2C_MAIN_PORT)
  {
    prt_hndl->addr_control_reg = DBE_ADDR(PBJ_I2C_main_control);
    prt_hndl->addr_status_reg = DBE_ADDR(PBJ_I2C_main_status);
    prt_hndl->control_reg = &bdata->sysreg_shadow.pbj_i2c_main_ctrl;
    prt_hndl->status_reg  = &bdata->sysreg_shadow.pbj_i2c_main_stat;
  }
  else if(port == ODSY_I2C_OPT_PORT)
  {
    prt_hndl->addr_control_reg = DBE_ADDR(PBJ_I2C_opt_control);
    prt_hndl->addr_status_reg = DBE_ADDR(PBJ_I2C_opt_status);
    prt_hndl->control_reg = &bdata->sysreg_shadow.pbj_i2c_opt_ctrl;
    prt_hndl->status_reg  = &bdata->sysreg_shadow.pbj_i2c_opt_stat;
  }

  /* save away the bdata structure as well */
  prt_hndl->bdata = bdata;
  prt_hndl->id = port;

  ODSY_DFIFO_PSEMA(bdata);
  if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, 0)) {
    ODSY_DFIFO_VSEMA(bdata);
    return(ODSY_I2C_FAILURE);
  }
  ODSY_READ_I2C_CONTROL(prt_hndl);
  ODSY_READ_I2C_STATUS(prt_hndl);
  ODSY_DFIFO_VSEMA(bdata);

  return ODSY_I2C_SUCCESS;
}


#if !defined( _STANDALONE )

/* XXX this function should be reritten as "force idle" */

/* Have no clue whether this is correct or not */
void odsy_i2cReset(port_handle *port)
{
  int i;

  for(i = 0; i < ODSY_I2C_RESET_TOGGLE; i++)
  {
    if(odsy_i2c_stop(port) != 0)
    {
      cmn_err(CE_ALERT,"odsy_i2cReset(): Failed to reset due to bad 'stop'\n");
      return;
    }
  }
}

#endif /* !defined( _STANDALONE ) */
