//#pragma option v  
/* i2c.c */

/*
 * contains utility routines used to support the firmware.
 */
//#include <16c74.h>
//#include <elsc.h>
//#include <i2c.h>
//#include <proto.h>


/****************************************************************************
 *																									*
 *	i2c_init() - 	Initialization code for the i2c bus.							*
 *																									*
 ****************************************************************************/
 void
 i2c_init()
 {

	/* initialize I2C Timeout count */
	 i2c_to_cnt = I2C_TIMEOUT;

	/* initialize the I2C data structures */
	cmdp.cmd_num = 0;
	cmdp.rsp_err = 0;
	cmdp.acp_int_state = ACP_BUF_EMPTY;

	
	slavep.slave_stat   = SLAVE_IDLE;
	slavep.i2c_cmd_stat = I2C_CMD_IDLE;
	slavep.byte_count   = 0;
	slavep.bytes_rec    = -1;
	slavep.bytes_used   = 0;
	slavep.sci_xfer_ct  = 0;
	slavep.chk_sum      = 0;
	slavep.slave_bus	  = 0;
	
	/* Receive queue */
	i2c_inq.c_inp 		= i2c_rxbuffer;
	i2c_inq.c_outp 	= i2c_rxbuffer;
	i2c_inq.c_len		= 0;
	i2c_inq.c_start 	= i2c_rxbuffer;
	i2c_inq.c_end 		= i2c_inq.c_start + I2CRBUFSZ;
	i2c_inq.c_maxlen 	= I2CRBUFSZ;

	/* Transmit queue */
	i2c_outq.c_inp 	= i2c_txbuffer;
	i2c_outq.c_outp 	= i2c_txbuffer;
	i2c_outq.c_len		= 0;
	i2c_outq.c_start 	= i2c_txbuffer;
	i2c_outq.c_end 	= i2c_outq.c_start + I2CTBUFSZ;
	i2c_outq.c_maxlen = I2CTBUFSZ;

	/* I2C Master Access Data Structure */
	i2c_cp.xfer_type 	= 0;		/* Bus Status */
	i2c_cp.token_cnt = -1;
	i2c_cp.i2c_add = 0;			/* i2c slave address */
	i2c_cp.i2c_sub_add = 0;		/* NVRAM byte address */
	i2c_cp.xfer_ptr = 0;			/* pointer to ELSC RAM for storage */
	i2c_cp.xfer_count = 0;		/* number of bytes to transfer */
	i2c_cp.bytes_xfered = 0;

	/* set up control regitser to load slave address into S0' */
 	writeS1(CTL_PIN);

	/* Load the ELSC slave address, (shift right by 1) */
#ifdef	SPEEDO
	if(tasks.env2.ENV2_SPEEDO_SLAVE)
		writeS0(ELSC_SLAVE_I2C_ADD >> 1);
	else
		writeS0(ELSC_I2C_ADD >> 1);
#else
	writeS0(ELSC_I2C_ADD >> 1);
#endif
	/* set up control register to load clock register */
	writeS1(CTL_PIN | CTL_ES1);

	/* set clock speed for SCL freq of 90Khz, and fclk for 8MHz */
	writeS0(0x18);

	/* now enable the serial interface and place part into idle mode */
	writeS1(CTL_PIN | CTL_ESO | CTL_ENI | CTL_ACK );

 }
 
/******************************************************************************
 *																										*
 *	i2cToken() -send a token																	*
 *																										*
 ******************************************************************************/

char
i2cToken(char tok)
{
	char	retval;
	writeS0(tok);
	writeS1(CTL_PIN | CTL_ESO | CTL_STA | CTL_ENI | CTL_ACK);
	ret_val = waitForPin();
	/* even if a timeout occurs still write the stop bit */
	writeS1(CTL_PIN | CTL_ESO | CTL_STO | CTL_ENI | CTL_ACK);
	return (ret_val);
}

/******************************************************************************
 *	i2cAccess() -	Read or write data on the i2c bus.  The data is fetched and	*
 *					written to the data structure or the i2c queue in the case 		*
 *					of pass through mode.														*
 ******************************************************************************/

char
i2cAccess()
{
	int	loop_count;
	char 	ret_val;
	char 	near *ptr;
	char 	reg;
	char	i;

	ret_val = I2C_ERROR_NONE;

	/* Request the I2C bus (assigned in sendI2cToken()), and
	 * loop until it is given to the ELSC.
	 */

	cmdp.cmd_intr.ELSC_I2C_REQ = 1;
	while (!cmdp.cmd_intr.ELSC_I2C_GRANT);
	
	i2c_cp.bytes_xfered = 0;
	ptr = i2c_cp.xfer_ptr;
		
	/* check for a bus busy and loop until free or loop count expires */
		
	loop_count = I2C_WAIT_COUNT;
	while (loop_count--) {
		reg = readS1();
		if (reg & STA_BBN)
			break;
	}
	if (loop_count <= 0) {
		ret_val = I2C_ERROR_TO_BUSY;
		goto done;
	}

	// if the sub address is non-zero, then the first load of the slave address
	// will always be a write to store the sub-address in the part.
		
	if (i2c_cp.xfer_type == I2C_NVRAM)
		writeS0( i2c_cp.i2c_add & 0xFE);
	else
 		writeS0( i2c_cp.i2c_add);

	writeS1(CTL_PIN | CTL_ESO | CTL_ENI | CTL_STA | CTL_ACK);

	/* Wait for the PIN bit to go low */
	ret_val = waitForPin();

	if (ret_val != 0)
		goto stop;

	/* Check to see if the slave acknowledged the address,  */
	reg = readS1();
		
	if (reg & STA_AD0_LRB) {
		ret_val = I2C_ERROR_NAK;
		goto stop;
	}

	// If the access is to NVRAM then the sub-address, is sent immediately
	// after the first slave address (send as a write).
	// if the access is a master-receiver, a repeated start bit is sent followed
	// by the slave address (as a read).
		
	if (i2c_cp.xfer_type == I2C_NVRAM) {
		/* load the sub address into S0 */
			
		writeS0(i2c_cp.i2c_sub_add);

		ret_val = waitForPin();
		if (ret_val != 0)
			goto stop;
			
		if (i2c_cp.i2c_add & I2C_READ) {
			// send a repeated start bit
			writeS1(CTL_ESO | CTL_STA | CTL_ACK);

			/* load the slave address into S0 */
			writeS0(i2c_cp.i2c_add);
			
			ret_val = waitForPin();
			if (ret_val != 0)
				goto stop;
		}
	}
	
	/* Check to see if mode is Master-Receiver */
	if (i2c_cp.i2c_add & I2C_READ) {
		/* if reading only one byte, set up for NAKing it */
		if (i2c_cp.xfer_count == 1)
			writeS1(CTL_ESO);
				
		/* Read slave address in s0 and ignore it */
		(void) readS0();

		for ( i = 0; i < i2c_cp.xfer_count; i++) {
			/* now look for the PIN bit to signal data received */
			ret_val = waitForPin();
			if (ret_val != 0)
					goto stop;

			/* Increment # of bytes received and look for last byte */
			i2c_cp.bytes_xfered++;

			/* If this is the last byte set neg ack */
			if (i2c_cp.bytes_xfered == (i2c_cp.xfer_count - 1))
				writeS1(CTL_ESO);

			/* read the byte */
			*ptr = readS0();
			ptr++;
		}
	}
	else { 		/* Master-Transmitter */
		while(1) {
			writeS0(*ptr);
			ret_val = waitForPin();
			if (ret_val != 0)
				goto stop;
				
			i2c_cp.bytes_xfered++;
			ptr++;

			reg = readS1();
			
			if (i2c_cp.bytes_xfered == i2c_cp.xfer_count ||
					 (reg & STA_AD0_LRB)) 
				goto stop;
		}
	}
		
stop:
	/* Send a repeated start to an invalid address to overwrite
	 * the last data sent to avoid having it being misinter-
	 * preted as a token.
	 */
	writeS1(CTL_ESO | CTL_STA | CTL_ACK);

	/* load the non-existent slave address into S0 */
	writeS0(0xfe);

	reg = waitForPin();
	/* access will have been NAKd */

	/* Send stop bit */

	writeS1(CTL_PIN | CTL_ESO | CTL_STO | CTL_ENI | CTL_ACK);

	if (i2c_cp.i2c_add & 1) 
		reg = readS0();
			
done:
	/* zero out the sub address, to ensure the next access
	 * is not mistaken for NVRAM */
	i2c_cp.i2c_xfer_type = 0;

	cmdp.cmd_intr.ELSC_I2C_GRANT = 0;

	return(ret_val);
	
}

#ifdef	SPEEDO
/**************************************************************************
 *																									*
 * i2c_master_xmit_recv -																	*
 *						Becomes bus master and tranmits data without 				*
 *						arbitration.  After transmission is complete a 				*
 *						repeated start conveting to a master receiver				*
 *						Fails on lost arbitration, bus error, or addressed as 	*
 *						slave.																	*
 *																									*
 ***************************************************************************/
char
i2c_master_xmit_recv()
{
	int	loop_count;
	char 	ret_val;
	char 	near *ptr;
	char 	reg;
	char	i;

	ret_val = I2C_ERROR_NONE;


	if (i2c_cp.i2c_add == ELSC_I2C_ADD) {

		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;

		/* request must be a speedo slave ELSC to master ELSC transfer */
		/* Wait for first appearance of token, but since we do not know
	 	* if we got to this point at the beginning of the token, once
	 	* received we will fall through the loop until the second token is received.
	 	*/

	 	/* This routine is always called with interrupts enabled, therefore they are
	 	 * disabled here and re-enabled after a start bit is sent, to ensure
	 	 * we get on the bus before the end of the time slice.
	 	 */
		while(readS0() == ELSC_SLAVE_TOKEN);

		/* Wait an entire turn of the token to appear again, but if it does not
		 *	return in 100 MS then about the transfer
		 */
		while(1) {
			sw_ms_counter = 3000;
			while(ret_val == I2C_ERROR_NONE){
				if (readS0() == ELSC_SLAVE_TOKEN)
					break;
				sw_ms_counter--;
				if (sw_ms_counter <= 0) 
					ret_val = I2C_SLAVE_TOKEN_TO;
			}
			/* Now wait with a valid token for 1/2 a msec before taking control of the bus */
			for (temp = 0; (temp < 30) && (ret_val == I2C_ERROR_NONE); temp++) {
				ext_ptr = I2C_S0;
				reg = *ext_ptr;
				if(reg != ELSC_SLAVE_TOKEN)
					break;
			}
			
			/* if we got this far, we are ready to talk on the bus, providing the
			 *	ret_val is still eqaul to zero
			 */
			break;
		}
	}

	/* abort the transfer if the ret_value contains an error code */
	if (ret_val != I2C_ERROR_NONE) {
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
		goto done_1;
	}
	
	i2c_cp.bytes_xfered = 0;
	ptr = i2c_cp.xfer_ptr;
		
	/* check for a bus busy and loop until free or loop count expires */
		
	loop_count = I2C_WAIT_COUNT;
	while (loop_count--) {
		reg = readS1();
		if (reg & STA_BBN)
			break;
	}
	if (loop_count <= 0) {
		ret_val = I2C_ERROR_TO_BUSY;
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
		goto done_1;
	}

	/* Write the Slave Address */
 	writeS0( i2c_cp.i2c_add);

	/* Send the Start bit and disable interrupts */
	writeS1(CTL_PIN | CTL_ESO | CTL_STA | CTL_ACK);

	/* in the case of a slave ELSC transfer enable interrupts after sending a start
	 * bit in an attempt to claim the bus before the end of a token period
	 */
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

	/* Wait for the PIN bit to go low */
	ret_val = waitForPin();

	if (ret_val != 0)
		goto stop_1;

	/* Check to see if the slave acknowledged the address,  */
	reg = readS1();
		
	if (reg & STA_AD0_LRB) {
		ret_val = I2C_ERROR_NAK;
		goto stop_1;
	}

	/* Transmit the data */
	while (1) {
		writeS0(*ptr);
		ret_val = waitForPin();
		if (ret_val != 0)
			goto stop_1;
				
		i2c_cp.bytes_xfered++;
		ptr++;

		reg = readS1();
			
		if (i2c_cp.bytes_xfered == i2c_cp.xfer_count)
			break; 
		else if (reg & STA_AD0_LRB) 
			goto stop_1;
	}

	/* send a repeated start bit with the read address */
	writeS1(CTL_ESO | CTL_STA | CTL_ACK);

	/* load the slave address into S0 */
	writeS0(i2c_cp.i2c_add | I2C_READ);
			
	ret_val = waitForPin();
	if (ret_val != 0)
		goto stop_1;

	/* Read slave address in s0 and ignore it */
	(void) readS0();

	/* reset the ptr and the number of bytes transfered to the original value */
	ptr = i2c_cp.xfer_ptr;
	i2c_cp.bytes_xfered = 0;

	/* the variable i becomes the response length counter, and is
	 * initialized to a positive value but will be reset after the read of the
	 * first byte of data.
	 */
	i = 5;
	while (i2c_cp.bytes_xfered <= i) {
		/* now look for the PIN bit to signal data received */
		ret_val = waitForPin();
		if (ret_val != 0)
			goto stop_1;

		/* Increment # of bytes received and look for last byte */
		i2c_cp.bytes_xfered++;

		/* If this is the last byte set neg ack */
		if (i2c_cp.bytes_xfered == (i - 1))
			writeS1(CTL_ESO);

		/* read the byte */
		*ptr = readS0();
		
		/* the first data byte is the response length */
		if (i2c_cp.bytes_xfered == 1)
			i = *ptr + 2;
		ptr++;

	}

stop_1:
	/* Send a repeated start to an invalid address to overwrite
	 * the last data sent to avoid having it being misinter-
	 * preted as a token.
	 */
	writeS1(CTL_ESO | CTL_STA | CTL_ACK);

	/* load the non-existent slave address into S0 */
	writeS0(0xfe);

	reg = waitForPin();
	/* access will have been NAKd */

	/* Send stop bit */

	writeS1(CTL_PIN | CTL_ESO | CTL_STO | CTL_ENI | CTL_ACK);

	if (i2c_cp.i2c_add & 1) 
		reg = readS0();
			
done_1:
	/* zero out the sub address, to ensure the next access
	 * is not mistaken for NVRAM */
	i2c_cp.i2c_xfer_type = 0;

	cmdp.cmd_intr.ELSC_I2C_GRANT = 0;

	return(ret_val);
	
}

#endif	//SPEEDO

/**************************************************************************
 *																									*
 * sendI2cToken -	sends a general call message to all the node i2c UARTS	*
 *						containing the CPU authorized to use the bus during the	*
 *						current time slice.													*
 *																									*
 *						This routine is called from the tick function.
 *																									*
 *						If the ELSC has data to send a Message flag is set in		*
 *						the CPU byte															*
 *																									*
 ***************************************************************************/
void
sendI2cToken()
{
	char		tokaddr;
	char		ret_val;
	
	/* Send a token if the bus is free. External bus masters must
	   use repeated starts to avoid gaps where we could briefly
	   detect bus not busy and send a token at the wrong time.
	*/

	/* first check to see if the ELSC has been granted the bus and skip if so */
	if (!cmdp.cmd_intr.ELSC_I2C_GRANT) {

		/* now make sure the ELSC i2c slave mode is idle */
		if (slavep.slave_stat == SLAVE_IDLE) {


			/* finally check to see if the bus is not in use */
			if ((readS1() & STA_BBN) != 0) {

#if 0
				/* reset the I2C timeout counter */
				i2c_to_cnt = I2C_TIMEOUT;
#endif
				/* Grant the ELSC use of the bus if it has requested it */
				if (cmdp.cmd_intr.ELSC_I2C_REQ && !cmdp.cmd_intr.ELSC_INT) {
					cmdp.cmd_intr.ELSC_I2C_REQ = 0;
					cmdp.cmd_intr.ELSC_I2C_GRANT = 1;
				}
				
				/* grant the SCI command process if requested */
				else if (cmdp.cmd_intr.SCI_CMD_REQ && !cmdp.cmd_intr.ELSC_INT) {
					cmdp.cmd_intr.SCI_CMD_REQ = 0;
					cmdp.cmd_intr.CMD_SCI = 1;
				}

				else if (cmdp.cmd_intr.TOKEN_ENABLE && !cmdp.cmd_intr.CMD_SCI ) {
					/* send the token address */
					i2c_cp.token_cnt = i2c_cp.token_cnt + 1;
					if (i2c_cp.token_cnt == MAX_CPU_CNT)
						i2c_cp.token_cnt = 0;
					tokaddr = I2C_BUS_TOKEN + i2c_cp.token_cnt;

					/* If pass thru data is waiting, or an ELSC interrupt is pending set
			 		* the message waiting flag
		 			*/

		 			/* Check for a ELSC interrupt waiting to be sent, the ELSC_INT flag
		 			 * will reset after a one second period.  Hold off sending pass through
		 			 * data until the one second period expires, to protect the message content.
		 			 */
		 			if (cmdp.cmd_intr.ELSC_INT && (cmdp.cpu_mask & (1 << i2c_cp.token_cnt))) 
		 				tokaddr |= M_FLAG;

		 			else {

						/* If no ELSC interrupt, then check for sel = Auto, meaning the M_FLAG
					 	* will be set for the last CPU that sent a line (s/b a prompt)
					 	*/
						if ((i2c_outq.c_len && cmdp.cpu_sel == SEL_AUTO) &&
						  	(i2c_cp.token_cnt == cmdp.cpu_acp))
							tokaddr |= M_FLAG;
						
						/* if auto select is not enabled then send input to the selected CPU */
						else if (i2c_outq.c_len && cmdp.cpu_sel == i2c_cp.token_cnt)
							tokaddr |= M_FLAG;
		 			}


					ret_val = i2cToken(tokaddr);
					if (ret_val != 0)
						i2c_to_cnt--;
					else
						/* reset the I2C timeout counter */
						i2c_to_cnt = I2C_TIMEOUT;

				}
			}
			/* if the slave is not idle then decrement the timeout counter */
			else
				i2c_to_cnt--;
		}
		/* if the slave is not idle then decrement the timeout counter */
		else 
			i2c_to_cnt--;
	}
	else 
		i2c_to_cnt--;
		

#ifndef	DIS_I2C_INIT
	if (i2c_to_cnt == 0)
		i2c_init();
#endif
}

#if 0
/**************************************************************************
 *																									*
 * sendI2cToken -	sends a general call message to all the node i2c UARTS	*
 *						containing the CPU authorized to use the bus during the	*
 *						current time slice.													*
 *																									*
 *						This routine is called from the tick function.
 *																									*
 *						If the ELSC has data to send a Message flag is set in		*
 *						the CPU byte															*
 *																									*
 ***************************************************************************/
void
sendI2cToken()
{
	char		tokaddr;
	
	/* Send a token if the bus is free. External bus masters must
	   use repeated starts to avoid gaps where we could briefly
	   detect bus not busy and send a token at the wrong time.
	*/

	/* first check to see if the ELSC has been granted the bus and skip if so */
	if (!cmdp.cmd_intr.ELSC_I2C_GRANT) {

		/* now make sure the ELSC i2c slave mode is idle */
		if (slavep.slave_stat == SLAVE_IDLE) {

			/* grant the SCI command process if requested */
			if (cmdp.cmd_intr.SCI_CMD_REQ) {
				cmdp.cmd_intr.SCI_CMD_REQ = 0;
				cmdp.cmd_intr.CMD_SCI = 1;
			}

			/* finally check to see if the bus is not in use */
			if ((readS1() & STA_BBN) != 0) {

				/* reset the I2C timeout counter */
				i2c_to_cnt = I2C_TIMEOUT;
				
				/* Grant the ELSC use of the bus if it has requested it */
				if (cmdp.cmd_intr.ELSC_I2C_REQ) {
					cmdp.cmd_intr.ELSC_I2C_REQ = 0;
					cmdp.cmd_intr.ELSC_I2C_GRANT = 1;
				}

				else if (cmdp.cmd_intr.TOKEN_ENABLE && !cmdp.cmd_intr.CMD_SCI ) {
					/* send the token address */
					i2c_cp.token_cnt = i2c_cp.token_cnt + 1;
					if (i2c_cp.token_cnt == MAX_CPU_CNT)
						i2c_cp.token_cnt = 0;
					tokaddr = I2C_BUS_TOKEN + i2c_cp.token_cnt;

					/* If pass thru data is waiting, or an ELSC interrupt is pending set
			 		* the message waiting flag
		 			*/

		 			/* Check for a ELSC interrupt waiting to be sent, the ELSC_INT flag
		 			 * will reset after a one second period.  Hold off sending pass through
		 			 * data until the one second period expires, to protect the message content.
		 			 */
		 			if (cmdp.cmd_intr.ELSC_INT && (cmdp.cpu_mask & (1 << i2c_cp.token_cnt))) 
		 				tokaddr |= M_FLAG;

		 			else {

						/* If no ELSC interrupt, then check for sel = Auto, meaning the M_FLAG
					 	* will be set for the last CPU that sent a line (s/b a prompt)
					 	*/
						if ((i2c_outq.c_len && cmdp.cpu_sel == SEL_AUTO) &&
						  	(i2c_cp.token_cnt == cmdp.cpu_acp))
							tokaddr |= M_FLAG;
						
						/* if auto select is not enabled then send input to the selected CPU */
						else if (i2c_outq.c_len && cmdp.cpu_sel == i2c_cp.token_cnt)
							tokaddr |= M_FLAG;
		 			}


					i2cToken(tokaddr);
				}
			}
			/* if the slave is not idle then decrement the timeout counter */
			else
				i2c_to_cnt--;
		}
		/* if the slave is not idle then decrement the timeout counter */
		else
			i2c_to_cnt--;
	}

#ifndef	DIS_I2C_INIT
	if (i2c_to_cnt == 0)
		i2c_init();
#endif
}
#endif

/******************************************************************************
*																										*
*	i2c_slave:		Interrupt handler for the I2C Slave mode of operation for 	*
*						PCF8584 I2C bus controller.  Slave mode is handled with		*
*						interrupts, where as Master mode is handled in a Polled		*
*						mode.  Therefore, if the part is not addressed as Slave		*
*						the interrupt will be ignored.										*
*																										*
*						Slave-receiver will place characters into the i2c_inq			*
*						and Slave-transmitter will remove characters from the 		*
*						i2c_rspq.																	*
*																										*
*						A count of total characters received and stored on the q		*
*						will be tracked.  If the queue is full the characters will	*
*						be read, but discarded.  The difference between the 			*
*						characters received and characters stored will relect this.	*
*																										*
*						The check sum of the received data will also be tracked		*
*																										*
*																										*
*******************************************************************************/
void
i2c_slave()
{

	unsigned ch;
//	char i2c_reg;
	int	loop_count;

	/* Reset the I2C Timeout counter */
	i2c_to_cnt = I2C_TIMEOUT;
	
	i2c_reg = readS1();
	
	if(i2c_reg & STA_AAS) {
		/* read the address and determine the data direction */
		ch = readS0();
		if (ch & 1) {
			slavep.slave_stat = SLAVE_XMIT;  //command response only
			cmdp.cmd_intr.CMD_I2C = 1;
		}

		else {
			slavep.slave_stat = SLAVE_RECV;
			slavep.bytes_rec  = 0;
			slavep.bytes_used = 0;
			slavep.chk_sum    = 0;
		}
	}
	
	else if (((i2c_reg & STA_STS) || (i2c_reg & STA_AD0_LRB))
			&& (slavep.slave_stat != SLAVE_IDLE)) {
		slavep.slave_stat = SLAVE_IDLE;	
		slavep.i2c_cmd_stat = I2C_CMD_IDLE;
		slavep.bytes_rec   = 0;
		slavep.bytes_used  = 0;
		slavep.chk_sum     = 0;
		writeS0(0);
		(void) readS1();
	}

	/* only used as part of the command response, only send if command is complete */
	else if (slavep.slave_stat == SLAVE_XMIT) {
		if(slavep.i2c_cmd_stat == I2C_CMD_DONE) {
			if (cmdp.cmd_num == GET)
				writeS0(acp_buf[acp_ptr++]);
			else
				writeS0(rsp_buf[rsp_ptr++]);
		}
		else {
			NOP();
			NOP();
		}
	}

	else if (slavep.slave_stat == SLAVE_RECV) {
		ch = readS0();

		if (slavep.bytes_rec == 0)
				slavep.byte_count = ch;
		else if (i2c_inq.c_len < I2CRBUFSZ &&
					(slavep.i2c_cmd_stat != I2C_CMD_BLOCKED)) {
			if (slavep.bytes_rec == (slavep.byte_count + 1)) {
				/* checksum, place a CR on the queue to terminate the command */
				addq(I2C_IN, CR);
			} else {
				/* Command Data */
				addq(I2C_IN, ch);
				if (slavep.bytes_rec > 4)
					slavep.bytes_used++;
			}
		} else
			slavep.i2c_cmd_stat = I2C_CMD_BLOCKED;

		/* Increment number of bytes received  */
		slavep.bytes_rec++;

		/* running check sum calculation */
		slavep.chk_sum += ch;
	}
}


/******************************************************************************
*																										*
*	waitForPin:		Routine polls PCF8584 control register for the PIN bit to 	*
*						go low, or the loop count to expire.								*
*																										*
*																										*
*******************************************************************************/
char
waitForPin()
{
	char r;
	r = 0;
	count = 0xFFFF;

	/* reset the i2c_timeout counter, this counter is reset here because if it
	   timesout or is successful it will be reset */
//	i2c_to_cnt = I2C_TIMEOUT;
	
	while (count != 0) {
		reg = readS1();
		if (!(reg & STA_PIN))
			break;
		count--;
	}
	if (count == 0) {
		r = I2C_PIN_TO;
//		i2c_init();
#ifdef	SPEEDO
			cmdp.cmd_intr.TOKEN_ENABLE = 1;
#else
		if (tasks.status.STAT_SYSPWR)
			cmdp.cmd_intr.TOKEN_ENABLE = 1;
#endif

	}


	/* check for bus related errors */
	else if (reg & STA_BER)
		r = I2C_ERROR_BUS;
	else if (reg & STA_LAB)
		r = I2C_ERROR_ARB;
		
	return(r);

}


/******************************************************************************
*																										*
*	readS0:	Reads the PCF8584 data register 												*
*																										*
*******************************************************************************/
char
readS0()
{
	
	int_state = CPUSTA.GLINTD ? 0 : 1;
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	if (int_state)
		CPUSTA.GLINTD = 0;

	ext_ptr = I2C_S0;
	reg = *ext_ptr;


	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

	return(reg);
}

/******************************************************************************
*																										*
*	readS1:	Reads the PCF8584 control register 											*
*																										*
*******************************************************************************/
char
readS1()
{
	
	int_state = CPUSTA.GLINTD ? 0 : 1;
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	if (int_state)
		CPUSTA.GLINTD = 0;
		
	ext_ptr = I2C_S1;
	reg = *ext_ptr;

	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

	return(reg);
}

/******************************************************************************
*																										*
*	writeS0:	Writes the PCF8584 data register 											*
*																										*
*******************************************************************************/
void
writeS0(reg_val)
char reg_val;
{
	
	int_state = CPUSTA.GLINTD ? 0 : 1;
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	if (int_state)
		CPUSTA.GLINTD = 0;

	ext_ptr = I2C_S0;
	*ext_ptr = reg_val;
	
	
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

}

/******************************************************************************
*																										*
*	writeS1:	Writes the PCF8584 control register											*
*																										*
*******************************************************************************/
void
writeS1(reg_val)
char reg_val;
{
	

	int_state = CPUSTA.GLINTD ? 0 : 1;
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	if (int_state)
		CPUSTA.GLINTD = 0;
	
	ext_ptr = I2C_S1;
	*ext_ptr = reg_val;
	
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
}

