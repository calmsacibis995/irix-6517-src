#pragma option v  
/* cmd.c */
/*
 * handles all commands sent to the ELSC by the RS-232 port (SCI) or the
 * I2C port.
 */
//#include <16c74.h>
//#include <elsc.h>
//#include <proto.h>

#ifdef	SPEEDO
const	char	cmd_tbl[] = {
	CR,0,0,C_RETURN,
	'a','c','p',ACP,
	'a','u','t',AUT,
	'c','l','r',CLR,
	'c','f','g',CFG,
	'd','b','g',DBG,
	'e','c','h',ECH,
	'f','a','n',FAN,
	'f','s','c',FSC,
	'g','e','t',GET,
	'h','b','t',HBT,
	'm','o','d',MOD,
	'n','i','c',NIC,
	'n','m','i',NMI,
	'n','v','m',NVM,
	'p','a','s',PAS,
	'p','w','r',PWR,
	'r','s','t',RST,
	'r','s','w',RSW,
	's','e','e',SEE,
	's','e','l',SEL,
	's','l','a',SLA,
	't','a','s',TAS,
	't','s','t',TST,
	't','m','p',TMP,
	'v','e','r',VER,
	0,0
};

#else //SPEEDO

const	char	cmd_tbl[] = {
	CR,0,0,C_RETURN,
	'a','c','p',ACP,
	'c','l','r',CLR,
	'c','f','g',CFG,
	'd','b','g',DBG,
	'd','s','c',DSC,
	'd','s','p',DSP,
	'e','c','h',ECH,
	'f','a','n',FAN,
	'f','s','c',FSC,
	'g','e','t',GET,
	'f','r','e',FRE,
	'h','b','t',HBT,
	'k','e','y',KEY,
	'm','o','d',MOD,
	'n','i','c',NIC,
	'n','m','i',NMI,
	'n','v','m',NVM,
	'p','a','s',PAS,
	'p','w','r',PWR,
	'r','s','t',RST,
	'r','s','w',RSW,
	's','e','e',SEE,
	's','e','l',SEL,
	's','l','a',SLA,
	't','a','s',TAS,
	't','s','t',TST,
	't','m','p',TMP,
	'v','e','r',VER,
	'v','l','m',VLM,
	0,0
};

#endif

/***************************************************************************
 *																									*
 *	processCmd:	This function parses commands recevied from both RS-232 		*
 *					and I2C ports																*
 *																									*
 ***************************************************************************/
void
processCmd()
{

	int count;
	
	// Clear out Response Buffer */
	for (count = 0; count < RSPBUFSZ; count++)
		rsp_buf[count] = 0;

	/* Default Response Error Status and response buffer size, will change only
	 * if an error exists or parameters are returned as part of the command.
	 */
	cmdp.rsp_err = OK;

	/* get the command number */
	getCmdNum();

	/* if the command is an i2c command, wait here
	 * until the entire command has been received, and verify the checksum.
	 * The entire command has been received once the mode switches to SLAVE_XMIT.
	 */
	if (cmdp.cmd_intr.CMD_I2C) 
		while (slavep.slave_stat != SLAVE_XMIT);

	/* check for errors before executing command */

	/* check that the byte count received is what is expected.
	 * The byte count does not include the byte-count byte or the checksum byte
	 */

	 if (cmdp.cmd_intr.CMD_I2C) {
		if ((slavep.bytes_rec - 2) != slavep.byte_count)
			cmdp.rsp_err = CLEN;

		/* Verify the checksum, it should be zero */
		else if (slavep.chk_sum != 0)
			cmdp.rsp_err = CSUM;
	 }
			
	if (cmdp.cmd_num == 0) 
		/* send bad command response */
		cmdp.rsp_err = CMD;
		
	else 
		/* parse and execute the commamd */
		parseCmd();

	/* remove all characters from queue upto and including the NL or NULL */
	if (cmdp.cmd_num != ACP)
		purgeXtraChars();

	if (cmdp.cmd_intr.CMD_SCI){
		if (cmdp.cmd_num != CFG)
			sendAcpRsp();
		cmdp.cmd_intr.CMD_SCI = 0;
	}
	else {
		if ((cmdp.cmd_num != ACP) && (cmdp.cmd_num != GET))
			sendHubRsp();
		cmdp.cmd_intr.CMD_I2C = 0;
	}

}

/***************************************************************************
 *																									*
 *	getCmdNum:	This function matches the command with a number to be used	*
 *				in a case statment by parseCmd().										*
 *				If no match is found a zero is returned.								*
 *																									*
 ***************************************************************************/
void
getCmdNum()
{

	int count;
	
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	
	/* remove the the character command mnemonic from the queue */
	if (cmdp.cmd_intr.CMD_SCI) { 
		for (count = 0; count < 4; count++) {
			cmd[count] = remq(SCI_IN);
		}
	}
	/* Hub command */		
	else {
		/* Get the command mmemonic */
		for (count = 0; count < 4; count++) 
			cmd[count] = remq(I2C_IN);
	}

	/* initialize the cmd number to be invalid */
	cmdp.cmd_num = INVALID_CMD;

	/* only look for the command number if the three digits are followed by a space or CR or
	 * the first character is a CR */
	if (cmd[3] == ' ' || cmd[3] == CR || cmd[0] == CR) {
		for (count = 0, l_ch = -1; (char) l_ch != 0; count += 2) {
			str_ptr = &cmd_tbl[count] & 0x7FFF;
			l_ch = *str_ptr;

			if ((char ) l_ch != cmd[0])
				continue;
			if ((char ) (l_ch >> 8) != cmd[1])
				continue;

			str_ptr += 1;
			l_ch = *str_ptr;

			if ((char ) l_ch != cmd[2])
				continue;

			cmdp.cmd_num = (char ) (l_ch >> 8);
			break;
		
		}
	}
		
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
}

/******************************************************************************
 *																										*
 *	parseCmd():	 Parses the incoming command based on the command number			*
 * 				 extracted from getCmdNum(), and extracts the paramaters			*
 *				 based on each command.  The appropirate routines are then			*
 *				 called to execute the command.												*
 *																										*
 *																										*
 ******************************************************************************/
void
parseCmd()
{

	int temp, temp1, temp2;
	int count;
	

	// Switch statement based on the command numbers
	switch (cmdp.cmd_num) {
		case	C_RETURN:
			/* Carriage return is only allowed in ACP Echo mode */
			if (!(cmdp.cmd_intr.CMD_SCI && cmdp.cmd_stat.CH_ECHO))
				cmdp.rsp_err = CMD;
			break;

			/* send characters to SCI output, (see Monitor()) */
		case	ACP:
			/* trigger routine in monitor.c to start sending.
			 * sci_xfer_ct is the number of bytes plus two. */
			slavep.sci_xfer_ct = slavep.bytes_used;

			/* remove two characters after the command:
			 * the cpu number, and another space.
			 */
			if (cmdp.cmd_intr.CMD_I2C) {
				CPUSTA.GLINTD = 1;
				INTSTA.INTIE = 0;
				INTSTA.PEIE = 0;
				CPUSTA.GLINTD = 0;
				temp = remq(I2C_IN);
				temp -= '0';
				(void) remq(I2C_IN);
				INTSTA.INTIE = 1;
				INTSTA.PEIE = 1;

			
				cmdp.cpu_acp = temp;
			}
			else
				cmdp.rsp_err = CMD;			
			break;
			
#ifdef	SPEEDO
		case	AUT:

			/* set up for an NVRAM Transfer */
			i2c_cp.xfer_type = I2C_NVRAM;
			i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE | I2C_READ;
			i2c_cp.i2c_sub_add = ELSC_CFG;
			i2c_cp.xfer_count = 1;
			i2c_cp.xfer_ptr = buffer;
			// Retrieve the next parameter, but first remove the space(s)
			// between the parameters
			
			cmd[0] = getNextChar(CUT_SPACE);
			 if (cmd[0] == 0) {
				// report back current autoboot setting:
				temp = i2cAccess();
			 	rsp_buf[3] = ' ';
			 	rsp_buf[4] = (buffer[0] & AUTO_BOOT_CFG) ? '1' : '0';
			}
			else if (cmd[0] == '1'){
				if (cmdp.cmd_stat.SUP_MODE  || cmdp.cmd_intr.CMD_I2C) {
					/* first read the configuration byte from NVRAM */
					temp = i2cAccess();

					/* Now set the fan hi speed bit */
						buffer[0] |= AUTO_BOOT_CFG;

					/* and now write the byte back to NVRAM */
					i2c_cp.xfer_type = I2C_NVRAM;
					i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
					temp = i2cAccess();

				}
				else
					cmdp.rsp_err = PERM;
			}
			else  if (cmd[0] == '0') {
				if (cmdp.cmd_stat.SUP_MODE || cmdp.cmd_intr.CMD_I2C) {
					/* first read the configuration byte from NVRAM */
					temp = i2cAccess();

					/* Now reset the fan hi speed bit */
					buffer[0] &= ~AUTO_BOOT_CFG;

					/* and now write the byte back to NVRAM */
					i2c_cp.xfer_type = I2C_NVRAM;
					i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
					temp = i2cAccess();
				}
				else
					cmdp.rsp_err = PERM;
			}
			else 
				cmdp.rsp_err = ARG;
			break;
#endif	//SPEEDO
			
		case	CFG:
			if (cmdp.cmd_intr.CMD_SCI) {

				/* this command reads characters from NVRAM until a terminating
				 * character is read.  Output is sent to the sci output queue
				 */

				/* the first type is set to NVRAM, the following are normal read
				 * since the address is already set in the NVRAM part.  i2cAccess
				 * resets the xfer_type after the read is complete
				 */

				i2c_cp.xfer_count = 8;
				i2c_cp.xfer_ptr = buffer;
				
				for (l_ch = 0, temp1 = 1; ((l_ch != 0x700) && temp1); l_ch += 8) {
					temp = (char) (l_ch >> 7);
					i2c_cp.i2c_add = (NVRAM_ADD + temp) | I2C_READ;
					i2c_cp.i2c_sub_add = (char) l_ch;
					i2c_cp.xfer_type = I2C_NVRAM;
					temp = i2cAccess();

					for (count = 0; count < 8; count++) {
						while(sci_outq.c_len > HIWATER);
						addq(SCI_OUT, buffer[count]);
						PIE.TXIE = 1;
						if(buffer[count] == 0) {
							temp1 = 0;
							break;
						}
					}
				}
				/* terminate response with a CR/LF */
				load_CR_NL();

			}

			else
				cmdp.rsp_err = CMD;
			break;

			/* place ELSC into the power on state */
		case	CLR:
			if (cmdp.cmd_intr.CMD_SCI && !cmdp.cmd_stat.SUP_MODE)
				cmdp.rsp_err = PERM;
			else {

#ifdef	SPEEDO
				/* clear the Supervior mode */
					cmdp.cmd_stat.SUP_MODE = 0;
#else
				/* clear the Supervior mode if the key switch is not in the Diag position*/
				if (reg_state.reg_in2.DIAG_SW_ON)
					cmdp.cmd_stat.SUP_MODE = 0;
#endif
				/* cancel outstanding Power down and Heart Beat timeouts */
				cancel_timeout(POWER_DN_TO);
				cancel_timeout(HBT_TO);
				hbt_interval = 0;

				/* turn on echo */
				cmdp.cmd_stat.CH_ECHO = 1;

#ifndef	SPEEDO
				/* Clear the Display */
				dspControl(DSP_CLEAR);
				dspControl(DSP_DISABLE_CLEAR);
				dspControl(BRT_FULL);
#endif

				/* set fan speed to normal */
				cmdp.cmd_stat.FAN_HI = 0;
				tasks.fw_logical.FWL_FAN_SPEED = 1;
			}
			break;
			

		/* debug switch settings */
		case	DBG:
			if (cmdp.cmd_intr.CMD_SCI) {

				/* set up for the NVRAM Access */
				i2c_cp.xfer_type = I2C_NVRAM;
				i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
				i2c_cp.i2c_sub_add = DEBUG_REG;
				i2c_cp.xfer_count = 2;
				i2c_cp.xfer_ptr = buffer;
				
				temp = getParam();
				
				if (temp == NO_PARAM) {
					/* read the debug byte locations */
					i2c_cp.i2c_add |= I2C_READ;
					temp = i2cAccess();

					/* convert the values to ascii */
					count = 3;
					rsp_buf[count++] = ' ';
					param_val = buffer[0];
					temp = putParam(&rsp_buf[count]);
					count += temp;
					rsp_buf[count++] = ' ';
					param_val = buffer[1];
					temp = putParam(&rsp_buf[count]);
				}
				else if (temp == PARAM_OK) {
					/* make sure permissions are OK */
					if (cmdp.cmd_stat.SUP_MODE || cmdp.cmd_intr.CMD_I2C) {
						buffer[0] = (char) param_val;
						temp = getParam();
						if (temp == NO_PARAM) 
							i2c_cp.xfer_count = 1;
						else {
							i2c_cp.xfer_count = 2;	
							buffer[1] = (char) param_val;
						}
						temp = i2cAccess();
					}
					else
					cmdp.rsp_err = PERM;
				}	
				else
					cmdp.rsp_err = ARG;
			}
			else
				cmdp.rsp_err = CMD;
			
			break;

#ifndef	SPEEDO
		case	DSC:
			// write a character to a display LED.
			cmd[0] = getNextChar(CUT_SPACE);
			if (cmd[0] >= '1' && cmd[0] <= '8') {
				cmd[0] -= '0';
				cmd[0]--;
				cmd[1] = getNextChar(CUT_SPACE);
				if (isprint(cmd[1])) 
					dspChar(cmd[0], cmd[1]);
				else
					cmdp.rsp_err = ARG;
			}
			else
				cmdp.rsp_err = ARG;
			break;
			
		case	DSP:
			// write an  eight character message to a display LED.
		
			/* get the first character, and cut the leading space */
			for (count = 0; count < 8; count++) {
				buffer[count] = getNextChar(INCLUDE_SPACE);
				temp = isprint(buffer[count]);
				if (!temp)
					break;
			}
			buffer[count] = 0;

			if (count >0)
				dspRamMsg(buffer);
			else
				cmdp.rsp_err = ARG;
	
			
			break;
#endif	//SPEEDO
			
		case	ECH:
		
			if (cmdp.cmd_intr.CMD_SCI) {
				cmd[0] = getNextChar(CUT_SPACE);
				if (cmd[0] == '1')
					cmdp.cmd_stat.CH_ECHO = 1;
				else if (cmd[0] == '0')
					cmdp.cmd_stat.CH_ECHO = 0;
				else if (cmd[0] == 0) {
					if (cmdp.cmd_stat.CH_ECHO == 1)
						cmdp.cmd_stat.CH_ECHO = 0;
					else if (cmdp.cmd_stat.CH_ECHO == 0)
						cmdp.cmd_stat.CH_ECHO = 1;
				}
				else
					// send an error indication
					cmdp.rsp_err = ARG;
				}
				else
					cmdp.rsp_err = CMD;
			break;

		case	FAN:

			/* set up for an NVRAM Transfer */
			i2c_cp.xfer_type = I2C_NVRAM;
			i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE | I2C_READ;
			i2c_cp.i2c_sub_add = ELSC_CFG;
			i2c_cp.xfer_count = 1;
			i2c_cp.xfer_ptr = buffer;
			// Retrieve the next parameter, but first remove the space(s)
			// between the parameters 
			cmd[0] = getNextChar(CUT_SPACE);
			 if (cmd[0] == 0) {
				// report back current fan setting:
			 	rsp_buf[3] = ' ';
#ifdef	SPEEDO
			 	/* first check for a fail fan */
			 	if (fan_ptr.fan_stat_1 || slave_fan_stat) {
			 		rsp_buf[4] = 'f';
			 		rsp_buf[5] = ' ';
#if 0
			 		rsp_buf[6] = fan_ptr.fan_stat_1 + '0';
#endif

			 		for(count = 0; count < 3; count++) {
			 			if(fan_ptr.fan_stat_1 & (1 << count))
			 				rsp_buf[6 + count] = '0';
			 			else
			 				rsp_buf[6 + count] = '1';
			 		}
			 		if (tasks.env2.ENV2_SPEEDO_MASTER && tasks.env2.ENV2_SPEEDO_MS_ENABLE) {
			 			rsp_buf[9] = ' ';
			 			for(count = 0; count < 3; count++) {
			 				if(slave_fan_stat & (1 << count))
			 					rsp_buf[10 + count] = '0';
			 				else
			 					rsp_buf[10 + count] = '1';
			 			}
			 		}
			 	}
				else if (reg_state.reg_out1.FAN_HI_SPEED)
					rsp_buf[4] = 'h';
				else
					rsp_buf[4] = 'n';
#else
				if (reg_state.reg_out3.FAN_HI_SPEED)
					rsp_buf[4] = 'h';
				else
					rsp_buf[4] = 'n';
#endif
			}
			else if (cmd[0] == 'h'){
				if (cmdp.cmd_stat.SUP_MODE  || cmdp.cmd_intr.CMD_I2C) {
					cmdp.cmd_stat.FAN_HI = 1;
					tasks.fw_logical.FWL_FAN_SPEED = 1;

					/* set the fan speed in NVRAM, but only if a SCI cmd */
					if (cmdp.cmd_intr.CMD_SCI) {
						/* first read the configuration byte from NVRAM */
						temp = i2cAccess();

						/* Now set the fan hi speed bit */
						buffer[0] |= FAN_HI_CFG;

						/* and now write the byte back to NVRAM */
						i2c_cp.xfer_type = I2C_NVRAM;
						i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
						temp = i2cAccess();
					}

#ifdef	SPEEDO
					if (tasks.env2.ENV2_SPEEDO_MASTER && tasks.env2.ENV2_SPEEDO_MS_ENABLE)
						master_cmd_param = SP_FAN_H;

#endif
				}
				else
					cmdp.rsp_err = PERM;
			}
			else  if (cmd[0] == 'n') {
				if (cmdp.cmd_stat.SUP_MODE || cmdp.cmd_intr.CMD_I2C) {
#ifdef	KCAR_FAN_FULL_SPEED
					if (mp_type != MP_KCAR) 
						cmdp.cmd_stat.FAN_HI = 0;
#else
					cmdp.cmd_stat.FAN_HI = 0;
#endif
					tasks.fw_logical.FWL_FAN_SPEED = 1;
					
					/* set the fan speed in NVRAM, but only if a SCI cmd */
					if (cmdp.cmd_intr.CMD_SCI) {
						/* first read the configuration byte from NVRAM */
						temp = i2cAccess();

						/* Now reset the fan hi speed bit */
						buffer[0] &= ~FAN_HI_CFG;

						/* and now write the byte back to NVRAM */
						i2c_cp.xfer_type = I2C_NVRAM;
						i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
						temp = i2cAccess();
					}
#ifdef	SPEEDO
					if (tasks.env2.ENV2_SPEEDO_MASTER && tasks.env2.ENV2_SPEEDO_MS_ENABLE)
						master_cmd_param = SP_FAN_N;

#endif
				}
				else
					cmdp.rsp_err = PERM;
			}
			else 
				cmdp.rsp_err = ARG;
			break;

#ifndef	SPEEDO
		case	FRE:
			i2c_cp.xfer_type = I2C_NVRAM;
			i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
			i2c_cp.i2c_sub_add = MP_CLK_2;
			i2c_cp.xfer_count = 1;
			i2c_cp.xfer_ptr = buffer;
			
			if (cmdp.cmd_intr.CMD_SCI) {
				temp = getParam();
				if (temp == NO_PARAM) {
					/* read the debug byte locations */
					i2c_cp.i2c_add |= I2C_READ;
					temp = i2cAccess();

					/* convert the values to ascii */
					count = 3;
					rsp_buf[count++] = ' ';
					param_val = buffer[0];
					temp = putParam(&rsp_buf[count]);
				}
				else if (temp == PARAM_OK) {
					buffer[0] = (char) param_val;
						/* make sure permissions are OK */
						if ((cmdp.cmd_intr.CMD_SCI && cmdp.cmd_stat.SUP_MODE) || cmdp.cmd_intr.CMD_I2C) {
							temp = i2cAccess();

							/* update the I2C Devices */
							if (mp_type == MP_LEGO) {
								i2c_cp.i2c_sub_add = 0;
								i2c_cp.xfer_count = 1;
								i2c_cp.i2c_add = MP_48;
								i2c_cp.xfer_ptr = buffer;
								temp = i2cAccess();
							}
						}
						else
							cmdp.rsp_err = PERM;
					}	
				}
				else
					cmdp.rsp_err = CMD;
			
			break;
		
#endif	//SPEEDO
			
		case	FSC:
			cmd[0] = getNextChar(CUT_SPACE);
			if (cmd[0] == '1')
				tasks.fw_logical.FWL_FFSC_PRESENT = 1;
			else if (cmd[0] == '0')
				tasks.fw_logical.FWL_FFSC_PRESENT = 0;
			else
				rsp_buf[3] = tasks.fw_logical.FWL_FFSC_PRESENT ? '1' : '0';
			break;

		case	GET:
			if (cmdp.cmd_intr.CMD_I2C) {
				/* if an interrupt is waiting write the first byte */
				if (cmdp.cmd_intr.ELSC_INT) {
					slavep.i2c_cmd_stat = I2C_CMD_DONE;
					acp_ptr = 1;
					writeS0(acp_buf[0]);

					/* reset the bit for the particular CPU */
					 cmdp.cpu_mask &= ~(1 << i2c_cp.token_cnt);
				}

				/* if passthrough data is waiting then send it */
				else
					sendHubInt(INT_ACP);
			}
			else
				cmdp.rsp_err = CMD;

			break;
			
		case	HBT:
				if (cmdp.cmd_stat.SUP_MODE  || cmdp.cmd_intr.CMD_I2C) {
					temp = getParam();
					
					if (temp == PARAM_OK) {
					
						if (param_val == 0) {
							/* disable the heartbeat command */
							hbt_interval = 0;
							hbt_state = HBT_READY;
							cancel_timeout(HBT_TO);
						}
		
						else if (param_val < 0x5 || (param_val > 0x258))
							/* check for parameter out of range error */
							cmdp.rsp_err = ARG;
						
						else  {
							/* set the timeout interval and start a new heartbeat */
							hbt_interval = param_val * TICKS_SEC;
							to_value = hbt_interval;
							hbt_state = HBT_TIMEOUT;
							timeout(HBT_TO);

							/* check for the cyc_timeout parameter, and if not present
							 * default to thirty seconds
							 */
							temp = getParam();
							if (temp == PARAM_OK)
								hbt_cyc_to = param_val * TICKS_SEC;
							else
								hbt_cyc_to = THIRTY_SEC;
						}
						
					}
					else {
						/* reset the timeout period */
						to_value = hbt_interval;
						timeout(HBT_TO);
					}
				}
				else
					cmdp.rsp_err = PERM;
			break;
			
#ifndef	SPEEDO
		case	KEY:
			// report back current module temperature status
			rsp_buf[3] = ' ';
#ifdef	_005
			/* Check for the key in the off state */
			if (!reg_state.reg_rb.KEY_SW_OFF) {
				rsp_buf[4] = 'o';
				rsp_buf[5] = 'f';
				rsp_buf[6] = 'f';
			}

			/* Check for the key in the DIAG state */
			else if (!reg_state.reg_in2.DIAG_SW_ON) {
				rsp_buf[4] = 'd';
				rsp_buf[5] = 'i';
				rsp_buf[6] = 'a';
				rsp_buf[7] = 'g';
			}
			/* It must be in the on position */
			else {
				rsp_buf[4] = 'o';
				rsp_buf[5] = 'n';
			}

#else
			if (!reg_state.reg_ra.KEY_SW_OFF) {
				rsp_buf[4] = 'o';
				rsp_buf[5] = 'f';
				rsp_buf[6] = 'f';
			}
			/* Check for the key in the DIAG state */
			else if (!reg_state.reg_in2.DIAG_SW_ON) {
				rsp_buf[4] = 'd';
				rsp_buf[5] = 'i';
				rsp_buf[6] = 'a';
				rsp_buf[7] = 'g';
			}
			/* It must be in the on position */
			else {
				rsp_buf[4] = 'o';
				rsp_buf[5] = 'n';
			}
#endif
			break;
#endif	//SPEEDO
			
		/* Module Number */
		case	MOD:
			i2c_cp.xfer_type = I2C_NVRAM;
			i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
			i2c_cp.i2c_sub_add = MOD_NUM;
			i2c_cp.xfer_count = 1;
			i2c_cp.xfer_ptr = buffer;
			
			if (cmdp.cmd_intr.CMD_SCI) {
				temp = getParam();
				if (temp == NO_PARAM) {
					/* read the module number from NVRAM */
					i2c_cp.i2c_add |= I2C_READ;
					temp = i2cAccess();

					/* convert the values to ascii */
					count = 3;
					rsp_buf[count++] = ' ';
					param_val = buffer[0];
					temp = putParam(&rsp_buf[count]);
				}
				else if (temp == PARAM_OK) {
					/* make sure permissions are OK */
					if ((cmdp.cmd_intr.CMD_SCI && cmdp.cmd_stat.SUP_MODE) || cmdp.cmd_intr.CMD_I2C) {
						buffer[0] = (char) param_val;
						temp = i2cAccess();
					}
					else
					cmdp.rsp_err = PERM;
				}	
				else
					cmdp.rsp_err = ARG;
			}
			else
				cmdp.rsp_err = CMD;
			
			break;


		case	NIC:
				/* fetch the PULSE delay value */
				temp = getParam();
				buffer[0] = param_val  / 5;

				/* fetch the SAMPLE delay value */
				temp |= getParam();
				buffer[1] = param_val  / 5;

				
				/* fetch the SAMPLE delay value */
				temp |= getParam();
				buffer[2] = param_val  / 5;

				if (temp == PARAM_OK) {

					rsp_buf[3] = ' ';

#ifdef	SPEEDO
					PORTB &= 0xFE;
#else
					PORTB &= 0xf7;
#endif
					
					CPUSTA.GLINTD = 1;
					/* Now disable the external and Peripheral interrupts. */
					INTSTA.INTIE = 0;
					INTSTA.PEIE = 0;
					CPUSTA.GLINTD = 0;
					
					/* Pull the NIC Pin Low */
#ifdef	SPEEDO
					DDRB = 0xE2;
					PORTB &= 0xFE;
#else
					DDRB = 0xF7;
					PORTB &= 0xf7;
#endif
					
					/* Wait for the PULSE Micro seconds */
					delayUs8Mhz(buffer[0]);

					/* Tri-state the NIC pin */
#ifdef	SPEEDO
					DDRB = 0xE3;
#else
					DDRB = 0xff;
#endif

					/* Wait for the SAMPLE Delay  */
					delayUs8Mhz(buffer[1]);

					/* Sample the NIC Pin */
					rsp_buf[4] = PORTB.NIC_MP ? '1' : '0';

					/* Wait the DELAY micro seconds */
					delayUs8Mhz(buffer[2]);
					
				}
				else 
					cmdp.rsp_err = ARG;

				INTSTA.INTIE = 1;
				INTSTA.PEIE = 1;

			break;

		case	NMI:

			/* if the system is not powered, return a command error */		
			if (!tasks.status.STAT_SYSPWR)
				cmdp.rsp_err = CMD;
				
			/* only allow the command when in Supvisor or from the I2C port */	
			else if (cmdp.cmd_intr.CMD_I2C || cmdp.cmd_stat.SUP_MODE) 
				issueNMI();
			else
				cmdp.rsp_err = PERM;
			break;

		case	NVM:

			/* temp1 will track the number of parameters, if 1 then the command is a read, if 2 the
			 * command is a write
			 */

			if (cmdp.cmd_intr.CMD_SCI) {
	 
				/* set up for the NVRAM Access */
				i2c_cp.xfer_type = I2C_NVRAM;
				i2c_cp.xfer_count = 1;
				i2c_cp.xfer_ptr = &buffer[1];
				
				/* Get the NVRAM offset */
				temp = getParam();
				
				if (temp == PARAM_OK) {
					temp1 = 1;
					i2c_cp.i2c_add = (param_val >> 8);
					/* adjust the page address for the I2C part */
					i2c_cp.i2c_add <<=1;
					
					/* Add the I2C NVRAM device address */
					i2c_cp.i2c_add |= NVRAM_ADD;
					
					/* Now load the page offset */
					i2c_cp.i2c_sub_add = (char) param_val;
				}

				/* now get the data, if the command is a write */
				temp |= getParam();
				if (temp == PARAM_OK) {
					temp1 = 2;
					buffer[1] = (char) param_val;
				}

				/* Parameter Error */
				if (temp1 == 0) {
					cmdp.rsp_err = ARG;
					goto DONE_NVR;
				}
				else if (temp1 == 1) 
					/* set up the read address */				
					i2c_cp.i2c_add |= I2C_READ;

				else if (!cmdp.cmd_stat.SUP_MODE) {
					cmdp.rsp_err = PERM;
					goto DONE_NVR;
				}
			
				/* Access NVRAM */
				(void) i2cAccess();

				/* if a read command */
				if (temp1 == 1) {
					rsp_buf[3] = ' ';
					param_val = buffer[1];
					(void) putParam(&rsp_buf[4]);
				}
			}
			else
				cmdp.rsp_err = CMD;
DONE_NVR:		
			break;

		case	PAS:
			i2c_cp.xfer_type = I2C_NVRAM;
			i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
			i2c_cp.i2c_sub_add = PASS_WD;
			i2c_cp.xfer_count = 4;
			i2c_cp.xfer_ptr = &buffer[2];
			
			if (cmdp.cmd_intr.CMD_SCI) {
				for (count = 0; count < 8; count++)
					buffer[count] = 0;
				buffer[0] = getNextChar(CUT_SPACE);
				if (buffer[0] == 0) {
#ifdef	SPEEDO
					/* clear the Supervior mode */
						cmdp.cmd_stat.SUP_MODE = 0;
#else
				/* clear the Supervior mode if the key switch is not in the Diag position*/
					if (reg_state.reg_in2.DIAG_SW_ON)
						cmdp.cmd_stat.SUP_MODE = 0;
#endif
				}
				/* before evaluating get all the characters */
				else {
					buffer[1] = getNextChar(INCLUDE_SPACE);
					if (buffer[1] != 0) {
						buffer[2] = getNextChar(INCLUDE_SPACE);
						count = 2;
					}
					if(!isspace(buffer[2]) && buffer[2] != 0) {
						count++;
						buffer[3] = getNextChar(INCLUDE_SPACE);
					}
					if(!isspace(buffer[3]) && buffer[3] != 0) {
						count++;
						buffer[4] = getNextChar(INCLUDE_SPACE);
					}
					if(!isspace(buffer[4]) && buffer[4] != 0) {
					count++;
						buffer[5] = getNextChar(INCLUDE_SPACE);
					}
					if(!isspace(buffer[5]) && buffer[5] != 0)
						count++;

					/* now evaluate the password */
					if (buffer[0] == 's' && buffer[1] == ' ') {
						if (!cmdp.cmd_stat.SUP_MODE)
							cmdp.rsp_err = PERM;
						else if (count != 6)
							cmdp.rsp_err = ARG;
						else {
							/* if we get this far set the new password */
							temp = i2cAccess();
							cmdp.cmd_stat.SUP_MODE = 1;
						}
					}

					/* set supervisor mode if the argument is correct and it compares
				 	* to the stored password
				 	*/
					else if (buffer[1] != ' ' && count == 4) {
						i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE | I2C_READ;
						i2c_cp.xfer_ptr = &buffer[4];
						temp = i2cAccess();
						if((buffer[0] == buffer[4]) && (buffer[1] == buffer[5]) &&
							(buffer[2] == buffer[6]) && (buffer[3] == buffer[7]))
							cmdp.cmd_stat.SUP_MODE = 1;
						else
							cmdp.rsp_err = ARG;
					}
					else
						cmdp.rsp_err = ARG;
				}
			}
			else
				cmdp.rsp_err = CMD;
		
			break;

		case	PWR:
			cmd[0] = getNextChar(CUT_SPACE);
			
				/******** POWER UP *********/
				if (cmd[0] == 'u') { 
					if (cmdp.cmd_stat.SUP_MODE || cmdp.cmd_intr.CMD_I2C) {
						/* Only issue a power on command if not in a powered state */
						if (!tasks.status.STAT_SYSPWR) {
							tasks.fw_logical.FWL_PWR_UP = 1;
#ifndef	SPEEDO
							dspMsg(MSG_PWR_RM);
#endif
						}
					}
					else 
						/* permission error */
						cmdp.rsp_err = PERM;
				}
				
				/******** POWER CYCLE *********/
				else if (cmd[0] == 'c'){
					/* Supervisor mode only */
					if (cmdp.cmd_stat.SUP_MODE || cmdp.cmd_intr.CMD_I2C){
						/* First check for a time delay value, and if present*/
						(void) getNextChar(INCLUDE_SPACE);
						temp = getParam();
						if (temp == PARAM_OK) {
							/* check for proper parameter range of 5 to 600 (0x258) seconds */
							if (param_val >= 5 && param_val <= 0x258) 
								tmp_count = (param_val * TICKS_SEC);
							else
								/* Argument Error */
								cmdp.rsp_err = ARG;
						}
						else if (temp == NO_PARAM)
							/* Default value */
							tmp_count = (5 * TICKS_SEC);
						
						else
							cmdp.rsp_err = ARG;
					}
					else 
						/* permission error */
						cmdp.rsp_err = PERM;
						
					if (cmdp.rsp_err == OK) {
#ifdef	SPEEDO
						speedoLED(AMBER, LED_BLINK);
#else
						dspMsg(MSG_PWR_CYCLE);
#endif
						sendHubInt(INT_PD_CYL);
						tasks.fw_logical.FWL_REMOVE_PWR = 1;
						to_value = tmp_count;
						timeout(POWER_UP_TO);
					}
				}

				/******** POWER DOWN *********/
				else if (cmd[0] == 'd'){
					/* Supervisor mode only */
					if (cmdp.cmd_stat.SUP_MODE || cmdp.cmd_intr.CMD_I2C){
						/* Only issue a power down command if in a powered state */
						if (tasks.status.STAT_SYSPWR){
							/* First check for a time delay value, and if present*/
							(void) getNextChar(INCLUDE_SPACE);
							temp = getParam();
							if (temp == PARAM_OK && (param_val > 0 && param_val <= 0x258)) {
								to_value = (param_val * TICKS_SEC);
								timeout(POWER_DN_TO);
#ifdef	SPEEDO
								speedoLED(AMBER, LED_BLINK);
#else
								dspMsg(MSG_RM_OFF);
#endif
								sendHubInt(INT_PD_RM);

							}	
							else if (temp == NO_PARAM || (temp == PARAM_OK && param_val == 0)) {
								/* if a timeout is already active, then a timeout with no parameter
								 * should shutdown immediately
								 */

								if ((TCB.power_dn_delay > 0) || (temp == PARAM_OK && param_val == 0)) {
									tasks.fw_logical.FWL_REMOVE_PWR = 1;
								}
								else {
									/*Since no existing timeouts are outstanding,
									 * then issue a soft power down
									 */
#ifdef	SPEEDO
									speedoLED(AMBER, LED_BLINK);
#else
									dspMsg(MSG_RM_OFF);
#endif
									tasks.fw_logical.FWL_PWR_DN = 1;
									sendHubInt(INT_PD_RM);
								}
							}
							
							else
								cmdp.rsp_err = ARG;
						}
					}
					else 
						/* permission error */
						cmdp.rsp_err = PERM;
				}

				/************* Power on Reset or just reset Status*************/
				else if (cmd[0] == 'q'){
					rsp_buf[3] = ' ';
					rsp_buf[4] = tasks.env1.ENV1_PON ? '1' : '0';
					tasks.env1.ENV1_PON = 0;
				}
				
				/******** POWER STATUS *********/
				else if (cmd[0] == 0) {
					/* report the power status */
					rsp_buf[3] = ' ';
					if (tasks.status.STAT_SYSPWR)
						rsp_buf[4] = 'u';
					else
						rsp_buf[4] = 'd';
				}
				
				/* invalid Argument */
				else
					cmdp.rsp_err = ARG;

			break;

		case	RST:
		
			/* if the system is not powered, return a command error */		
			if (!tasks.status.STAT_SYSPWR)
				cmdp.rsp_err = CMD;
				
			/* only allow the command when in Supvisor or from the I2C port */	
			else 	if (cmdp.cmd_intr.CMD_SCI && !cmdp.cmd_stat.SUP_MODE)
				cmdp.rsp_err = PERM;
			else 
				issueReset();
			break;
			
		case	RSW:
		
#ifdef NEW_EXT
			ext_ptr_store = DIP_SW;
			param_val = extMemRd();
#else
			CPUSTA.GLINTD = 1;
			INTSTA.INTIE = 0;
			INTSTA.PEIE = 0;
			CPUSTA.GLINTD = 0;
			ext_ptr = DIP_SW;
			param_val = *ext_ptr;
			INTSTA.INTIE = 1;
			INTSTA.PEIE = 1;
#endif
			rsp_buf[3] = ' ';
			param_val &= 0xff;
			temp = putParam(&rsp_buf[4]);
			break;

		case	SEE:
		case	SEL:
			temp1 = getNextChar(CUT_SPACE);
			temp1 = toupper(temp1);
			temp2 = getNextChar(CUT_SPACE);
			temp2 = toupper(temp2);
			
			if (temp1 == 0) {
				rsp_buf[3] = ' ';
				if (cmdp.cmd_num == SEE) {
					if (cmdp.cpu_see + 1 < 1) {	/* Compiler bug WAR */
						rsp_buf[4] = 'a';
						rsp_buf[5] = 'l';
						rsp_buf[6] = 'l';
					} else {
						rsp_buf[4] = (cmdp.cpu_see / 2) + '1';
						rsp_buf[5] = (cmdp.cpu_see & 1) + 'a';
					}
				}
				else {
					if (cmdp.cpu_sel == SEL_NONE) {	
						rsp_buf[4] = 'n';
						rsp_buf[5] = 'o';
						rsp_buf[6] = 'n';
						rsp_buf[7] = 'e';
					}
					else if(cmdp.cpu_sel == SEL_AUTO) {
						rsp_buf[4] = 'a';
						rsp_buf[5] = 'u';
						rsp_buf[6] = 't';
						rsp_buf[7] = 'o';
					}
					else {
						rsp_buf[4] = (cmdp.cpu_sel / 2) + '1';
						rsp_buf[5] = (cmdp.cpu_sel & 1) + 'a';
					}
				}
				break;
			}

			if (temp1 >= '1' && temp1 <= '4' &&
						(temp2 == 'A' || temp2 == 'B'))
				temp = (temp1 - '1') * 2 + (temp2 - 'A');
			else if ((temp1 == 'A' && temp2 == 'L') ||	/* ALL */
						(temp1 == 'N' && temp2 == 'O'))		/* NONE */
				temp = -1;
			else if (cmdp.cmd_num == SEL && (temp1 == 'A' && temp2 == 'U')) /*AUTO*/
				temp = -2;
			else
				temp = -3;

			if (temp != -3) {
				if (cmdp.cmd_num == SEE)
					cmdp.cpu_see = temp;
				if (cmdp.cmd_num == SEL || temp != -1) {
					cmdp.cpu_sel = temp;
					flush (I2C_OUT);
					cmdp.acp_int_state = ACP_BUF_EMPTY;
				}
			}
			else			
				cmdp.rsp_err = ARG;

			break;
			
		case	TAS:
			cmd[0] = getNextChar(CUT_SPACE);
			/* the response is always the old value */
			rsp_buf[3] = ' ';
			rsp_buf[4] = hub_tas;

			/* now set the test and set byte */
			if (cmd[0] != 0) {
				if (cmd[0] == '0')
					hub_tas = '0';
				else if (hub_tas == '0')
					hub_tas = cmd[0];
			}
			break;

		case	TST:
			cmd[0] = getNextChar(CUT_SPACE);
			/* the response is always the old value */

			/* now set the test and set byte */
			if (cmd[0] != 0) {
				if (cmd[0] == '1')
					mfg_tst = 1;
				else
					mfg_tst = 0;
			}
			else			
				cmdp.rsp_err = ARG;

			break;
#ifdef	SPEEDO
		case	SLA:
			/* Slave ELSC to Master ELSC command processing */

			/* The command and paramters are received here, but due to the
			 * nature of the command/response format these commands will be
			 *	processed in monitor.c
			 */
			slave_cmd_param = getNextChar(CUT_SPACE);
				if (slave_cmd_param == SLAVE_FF || slave_cmd_param == SLAVE_MFF)
					slave_fan_stat = getNextChar(CUT_SPACE);
			break;
#endif
						
		case	TMP:
			// report back current module temperature status
			rsp_buf[3] = ' ';
#ifdef	SPEEDO
			if (!reg_state.reg_ra.TMP_SYS_OT)
				rsp_buf[4] = 'o';
			else if (reg_state.reg_rb.TMP_FANS_HI)
				rsp_buf[4] = 'h';
			else
				rsp_buf[4] = 'n';
			 if (tasks.env2.ENV2_SPEEDO_MASTER && tasks.env2.ENV2_SPEEDO_MS_ENABLE) {
			 	rsp_buf[5] = ' ';
			 	rsp_buf[6] = slave_temp_stat;
			 }

#else
			if (!reg_state.reg_ra.TMP_SYS_OT)
				rsp_buf[4] = 'o';
			else if (reg_state.reg_rb.TMP_FANS_HI)
				rsp_buf[4] = 'h';
			else
				rsp_buf[4] = 'n';
#endif
			break;

		case	VER:
			// report back firmware version number
			rsp_buf[3] = ' ';
			for (temp = 0, temp1 = 4; temp < 8; temp++, temp1++) {
				if(version[temp] == 0)
					break;
				rsp_buf[temp1] = version[temp];
			}
			break;
#ifndef	SPEEDO
		case	VLM:
			// Set voltage margins
			cmd[0] = getNextChar(CUT_SPACE);
			if (cmd[0] == 0)
				cmdp.rsp_err = ARG;
			else {
				cmd[1] = getNextChar(CUT_SPACE);
				if (cmd[0] == '3') {
					if (cmd[1] == 0) {
						rsp_buf[3] = ' ';
						if (!reg_state.reg_out2.V345_MARGINH)
							rsp_buf[4] = 'h';
						else if (!reg_state.reg_out2.V345_MARGINL)
							rsp_buf[4] = 'l';
						else
							rsp_buf[4] = 'n';
					}
					else if (cmdp.cmd_stat.SUP_MODE || !cmdp.cmd_intr.CMD_SCI) {
						if (cmd[1] == 'h') {
							reg_state.reg_out2.V345_MARGINH = 0;
							reg_state.reg_out2.V345_MARGINL = 1;
						}
						else if (cmd[1] == 'l') {
							reg_state.reg_out2.V345_MARGINH = 1;
							reg_state.reg_out2.V345_MARGINL = 0;
						}
						else if (cmd[1] == 'n') {
							reg_state.reg_out2.V345_MARGINH = 1;
							reg_state.reg_out2.V345_MARGINL = 1;
						}
						else
							cmdp.rsp_err = ARG;
					}
					else
						cmdp.rsp_err = PERM;
				}
				else if (cmd[0] == '5') {
					if (cmd[1] == 0) {
						rsp_buf[3] = ' ';
						if (!reg_state.reg_out2.V5_MARGINH)
							rsp_buf[4] = 'h';
						else if (!reg_state.reg_out2.V5_MARGINL)
							rsp_buf[4] = 'l';
						else
							rsp_buf[4] = 'n';
					}
					else if (cmdp.cmd_stat.SUP_MODE || !cmdp.cmd_intr.CMD_SCI) {
						if (cmd[1] == 'h') {
							reg_state.reg_out2.V5_MARGINH = 0;
							reg_state.reg_out2.V5_MARGINL = 1;
						}
						else if (cmd[1] == 'l') {
							reg_state.reg_out2.V5_MARGINH = 1;
							reg_state.reg_out2.V5_MARGINL = 0;
						}
						else if (cmd[1] == 'n') {
							reg_state.reg_out2.V5_MARGINH = 1;
							reg_state.reg_out2.V5_MARGINL = 1;
						}
						else
							cmdp.rsp_err = ARG;
					}
					else
						cmdp.rsp_err = PERM;
				}
				else if (cmd[0] == 'v') {
					if (cmd[1] == 0) {
						rsp_buf[3] = ' ';
						if (reg_state.reg_out2.RMT_MARGINH)
							rsp_buf[4] = 'h';
						else if (reg_state.reg_out2.RMT_MARGINL)
							rsp_buf[4] = 'l';
						else
							rsp_buf[4] = 'n';
					}
					else if (cmdp.cmd_stat.SUP_MODE || !cmdp.cmd_intr.CMD_SCI) {
						if (cmd[1] == 'h') {
							reg_state.reg_out2.RMT_MARGINH = 1;
							reg_state.reg_out2.RMT_MARGINL = 0;
						}
						else if (cmd[1] == 'l') {
							reg_state.reg_out2.RMT_MARGINH = 0;
							reg_state.reg_out2.RMT_MARGINL = 1;
						}
						else if (cmd[1] == 'n') {
							reg_state.reg_out2.RMT_MARGINH = 0;
							reg_state.reg_out2.RMT_MARGINL = 0;
						}
						else
							cmdp.rsp_err = ARG;
					}
					else
						cmdp.rsp_err = PERM;
				}
				else
					cmdp.rsp_err = ARG;
			}
				
			/* the voltage margin register is always updated here to conserve ROM */
			ext_ptr_store = OUTP2;
			extMemWt(reg_state.reg_out2);
			break;
#endif	//SPEEDO
			
	}
}

/***************************************************************************
 *																									*
 *	getNextChar:  Retrieves the next parameter from the queue for command	*
 *					processing.  Space(s) are removed and ignored if indicated 	*
 *					by the input parameter.  CR/LF are chopped.					   *
 *																									*
 ***************************************************************************/
unsigned char
getNextChar(cut_space)
int	cut_space;					/* flag for cutting leading spaces */
{
	// Retrieve the next parameter, but first remove the space(s)
	// between the parameters
	
	char ch;
	int	near *qp;						/* pointer to queue */
	
	if (cmdp.cmd_intr.CMD_SCI) {
		
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ch = remq(SCI_IN);
		if (cut_space) {
			while(ch == ' ') 
				ch = remq(SCI_IN);
		}
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
	}

	else {
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ch = remq(I2C_IN);
		if (cut_space) {
			while(ch == ' ') 
				ch = remq(I2C_IN);
		}
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
	}

	if (ch == CR || ch == NL)
		ch = 0;

	return(ch);
}


/******************************************************************************
 *																										*
 *	purgeXtraChars:  	Retrieves the xtra junk characters from the queu upto		*
 *					and including the next CR, NULL, or NL.								*
 *																										*
 ******************************************************************************/
void
purgeXtraChars()
{
	/* first check the last character on the queue for a terminating
	 * character, if not present, keep pulling characters off the queue until
	 * a zero is returned indicating the end of a command or an empty queue.
	 */

	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	
	if (cmdp.cmd_intr.CMD_SCI)
		flush(SCI_IN);
	else
		flush(I2C_IN);
		
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

}

/******************************************************************************
 *																										*
 *	getParam() - 		Fetches up to four hex characters from the input queue	*
 *							and converts to a "long" (16 bits).  Stores result in		*
 *							the global param_val.												*
 *						 																				*
 ******************************************************************************/

char
getParam()
{
	char 	ret_val;
	char 	ch;
	int 	i;
	
	ret_val = PARAM_OK;
	param_val = 0;

	for (i = 0; i < 4; i++) {
		ch = getNextChar(INCLUDE_SPACE);

		if (ch >= 'a' && ch <= 'f')
			param_val = param_val * 16 + (ch - 'a' + 10);
		else if (ch >= 'A' && ch <= 'F')
			param_val = param_val * 16 + (ch - 'A' + 10);
		else if (ch >= '0' && ch <= '9')
			param_val = param_val * 16 + (ch - '0'     );
		else
			break;
	}

	if (i == 0)
		ret_val = NO_PARAM;

	return(ret_val);
}

/******************************************************************************
 *																										*
 *	putParam() - 		Converts a value into up to four hex characters.			*
 *																										*
 *					The input value is located in the long param_val					*
 *					and the result is stored in an array pointed to by the input	*
 *					parameter.																		*
 *						 																				*
 ******************************************************************************/

char
putParam(ptr)
char near * ptr;
{
	int count;
	char ph, pl, ch;
	
	count = 0;

	ph = (char) (param_val >> 8);
	pl = (char) (param_val & 0xff);
	
	if (ph > 0xf) {
		ch = convertXtoA(ph, HIGH);
		ptr[count] = ch;		/* N.B. Can't use autoinc here! */
		count++;
	}

	if (ph) {
		ch = convertXtoA(ph, LOW);
		ptr[count] = ch;
		count++;
	}

	if (ph != 0 || pl > 0xf) {
		ch = convertXtoA(pl, HIGH);
		ptr[count] = ch;
		count++;
	}
	
	ch = convertXtoA(pl, LOW);
	ptr[count] = ch;
	count++;

	return(count);
}

/******************************************************************************
 *																										*
 *	convertXtoA() - 	Converts one nibble of a byte of hex to ascii.				*
 *																										*
 *							Error checking is not done.										*
 *																										*
 *						 																				*
 ******************************************************************************/
char
convertXtoA(c, nibble)
char	c;							/* value to be converted */
int	nibble;					/* nibble to be converted */
{
	char x;
	int i;

	if (nibble == LOW)
		c &= 0x0F;
	else
		c >>= 4;
		
	if ((c >= 0) && (c <= 9))
		x = c + '0';
	else if ((c >= 0xA) && (c <= 0xF)) 
		x = (c - 0xA) + 'a';
	return(x);
}

/******************************************************************************
 *																										*
 *	sendHubInt() -  Sends an interrupt from the ELSC to the Hub.  The 			*
 *						 Includes Charater passing from the serial port to the Hub	*
 *						 in pass-through mode and informing the hub of ELSC warings	*
 *						 and alarms.
 *																										*
 ******************************************************************************/
void
sendHubInt(acp_int)
char	 *acp_int;				/* interrupt to send */
{
	
	int i, j;
	int count;


	/* No node cards in a KCAR chassis */
	if (mp_type == MP_KCAR)
		return;

		
	/* needed to remove an internal compiler flag */
	str_ptr = acp_int & 0x7FFF;

	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;

	/* fill in the constant portion of the string */
	for (count = 1;; str_ptr += 1, count++) {
		
		/* read the two character word from program space */
		l_ch = *str_ptr;
		
		/* transfer the first byte if non zero */
		if ((char ) l_ch  == 0)
			break;
		acp_buf[count] = (char ) l_ch;
		count++;

		/* transfer the second byte if non zero */
		if (((char ) (l_ch >> 8) + 1)  == 1) 	// the +1 is due to a bug setting the CC
			break;
		acp_buf[count] = (char ) (l_ch >> 8);

	}
		

	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

#ifndef	SPEEDO
	/* fill slot number for POK failure */
	if(acp_int == INT_POK) {
		acp_buf[count++] = ' ';
		acp_buf[count++] = pok_slot_no + '0';
	}
#endif
	
	/* fill in data for the queue for pass through */
	if (acp_int == INT_ACP) {
		/* store the CPU number */
		acp_buf[count++] = cmdp.cpu_sel + '0';
		acp_buf[count++] = ' ';
		
		/* get a snap shot of the number of characters in the queue */
		j = i2c_outq.c_len;
		for(i = 0; (i < j) && (count < ACPBUFSZ); i++, count++) {
			CPUSTA.GLINTD = 1;
			INTSTA.INTIE = 0;
			INTSTA.PEIE = 0;
			CPUSTA.GLINTD = 0;
			
			acp_buf[count] = remq(I2C_OUT);
			
			INTSTA.INTIE = 1;
			INTSTA.PEIE = 1;
		}
	}
	/* store the byte count */
	acp_buf[0] = count - 1;
	
	/* Calculate the checksum */
	acp_buf[count] = 0;
	for (i = 0; i < count; i++) 
		acp_buf[count] += acp_buf[i];
	acp_buf[count] = 0 - acp_buf[count];


	/* if this is pass through data, the slave I2C is waiting, so write the first byte*/
	if (acp_int == INT_ACP) {
		slavep.i2c_cmd_stat = I2C_CMD_DONE;
		acp_ptr = 1;
		writeS0(acp_buf[0]);
	}
	else {

#ifdef	SPEEDO
		/* print out the interrupt message on the serial port */
		addq(SCI_OUT, CNTL_T);
		addq(SCI_OUT, CR);
		addq(SCI_OUT, NL);
		addq(SCI_OUT, 'M');
		addq(SCI_OUT, 'S');
		addq(SCI_OUT, 'C');
		addq(SCI_OUT, ':');
		addq(SCI_OUT, ' ');
		PIE.TXIE = 1;
		for (i = 0; i < acp_buf[0]; i++) {
			addq(SCI_OUT, acp_buf[i + 1]);
			PIE.TXIE = 1;
		}
		addq(SCI_OUT, CNTL_T);
		addq(SCI_OUT, CR);
		PIE.TXIE = 1;
#endif
		
		/* toggle Panic Interrupt */
		if (mp_type != MP_KCAR) {
			reg_state.reg_out1.PANIC_INTR = 0;
			ext_ptr_store = OUTP1;
			extMemWt(reg_state.reg_out1);

			to_value = MSEC_100;
			reg_state.reg_out1.PANIC_INTR = 1;
			ext_ptr_store = OUTP1;
			extMemWt(reg_state.reg_out1);

		}


		/* the general call routine will send the message waiting flag */
		/* set a time for the message waiting to expire */
		cmdp.cmd_intr.ELSC_INT = 1;
		cmdp.cpu_mask = 0xFF;
		to_value = ELSC_INT_DELAY;
		timeout(ELSC_INT_TO);
	}
		
}

#ifdef	SPEEDO
/******************************************************************************
 *																										*
 *	elscToElsc() - Sends an I2C command from  ELSC to the ELSC in a 4P Speedo	*
 *						configuration.  The ELSC whether a SLAVE/MASTER ELSC must	*
 *						send the command as a I2C master transmitter.					*
 *																										*
 ******************************************************************************/
char
elscToElsc(ptr)
char	*ptr;
{
	
	int i, j;
	int count;
	
	/* needed to remove an internal compiler flag */
	str_ptr = ptr & 0x7FFF;

	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;

	/* fill in the constant portion of the string */
	for (count = 1;; str_ptr += 1, count++) {
		
		/* read the two character word from program space */
		l_ch = *str_ptr;
		
		/* transfer the first byte if non zero */
		if ((char ) l_ch  == 0)
			break;
		buffer[count] = (char ) l_ch;
		count++;

		/* transfer the second byte if non zero */
		if (((char ) (l_ch >> 8) + 1)  == 1) 	// the +1 is due to a bug setting the CC
			break;
		buffer[count] = (char ) (l_ch >> 8);

	}
		

	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

	/* if the command is from a SLAVE ELSC load the parameter character(s) */
	if (ptr == SLAVE_CMD) {
		buffer[count] = slave_cmd_param;
		count++;
		if ((slave_cmd_param == SLAVE_FF) || (slave_cmd_param == SLAVE_MFF)) {
			buffer[count] = ' ';
			count++;
			buffer[count] = fan_ptr.fan_stat_1;
			count++;
		}
	}

	/* store the byte count */
	buffer[0] = count - 1;
	
	/* Calculate the checksum */
	buffer[count] = 0;
	for (i = 0; i < count; i++) 
		buffer[count] += buffer[i];
	buffer[count] = 0 - buffer[count];

	i2c_cp.xfer_type = 0;
	if (ptr == SLAVE_CMD) 
		i2c_cp.i2c_add = ELSC_I2C_ADD;
	else
		i2c_cp.i2c_add = ELSC_SLAVE_I2C_ADD;
	i2c_cp.i2c_sub_add = 0;
	i2c_cp.xfer_count = count + 1;
	i2c_cp.xfer_ptr = buffer;
	i = i2c_master_xmit_recv();

	return(i);
	
}

#endif
/******************************************************************************
 *																										*
 *	sendAcpRsp() - 	Starts a response for a Hub initiated command				*
 *																										*
 *						 Response buffer (rsp_buff) must be preloaded inlcuding		*
 *						 byte counts																*
 *						 																				*
 ******************************************************************************/
void
sendAcpRsp() {
	int count;

	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;

	/* Precede response with a Control-T */

	addq(SCI_OUT, CNTL_T);

	if (cmdp.rsp_err != OK) {
		addq(SCI_OUT, 'e');
		addq(SCI_OUT, 'r');
		addq(SCI_OUT, 'r');
		addq(SCI_OUT, ' ');

		if (cmdp.rsp_err == CSUM) {
			addq(SCI_OUT, 'c');
			addq(SCI_OUT, 's');
			addq(SCI_OUT, 'u');
			addq(SCI_OUT, 'm');
		}
		else if (cmdp.rsp_err == PERM) {
			addq(SCI_OUT, 'p');
			addq(SCI_OUT, 'e');
			addq(SCI_OUT, 'r');
			addq(SCI_OUT, 'm');
		}
		else if (cmdp.rsp_err == CMD) {
			addq(SCI_OUT, 'c');
			addq(SCI_OUT, 'm');
			addq(SCI_OUT, 'd');
		}
		else if (cmdp.rsp_err == ARG) {
			addq(SCI_OUT, 'a');
			addq(SCI_OUT, 'r');
			addq(SCI_OUT, 'g');
		}
	}
	else {
		addq(SCI_OUT, 'o');
		addq(SCI_OUT, 'k');

		/* load parameter characters, if any, into the output queue */
		for (count = 3;
				((count < RSPBUFSZ) && (rsp_buf[count] != 0));
				count++) {
			temp = rsp_buf[count];
			addq(SCI_OUT, temp);
		}
	}
	
	/* terminate response with a CR/LF */
	load_CR_NL();

	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
}

/******************************************************************************
 *																										*
 *	sendHubRsp() -  Starts a response for a Hub initiated command.					*
 *																										*
 *						 The response buffer is loaded with return parameters 		*
 *						 starting at the fourth location (location 0-3 reserved for *
 *						 the string "OK ").  The routine then calculates the byte	*
 *						 count, and checksum.  All responses are terminated with		*
 *						 a null (0).																*
 *						 																				*
 ******************************************************************************/
void
sendHubRsp() {
	int count;
	
	if (cmdp.rsp_err != OK) {
		rsp_buf[1] = 'e';
		rsp_buf[2] = 'r';
		rsp_buf[3] = 'r';
		rsp_buf[4] = ' ';
		if (cmdp.rsp_err == CSUM) {
			rsp_buf[5] = 'c';
			rsp_buf[6] = 's';
			rsp_buf[7] = 'u';
			rsp_buf[8] = 'm';
		}
		else if (cmdp.rsp_err == CMD) {
			rsp_buf[5] = 'c';
			rsp_buf[6] = 'm';
			rsp_buf[7] = 'd';
		}
		else if (cmdp.rsp_err == ARG) {
			rsp_buf[5] = 'a';
			rsp_buf[6] = 'r';
			rsp_buf[7] = 'g';
		}
	}
	else {
		/* because of optional parameter byte count must be set by the calling routine */
		rsp_buf[1] = 'o';
		rsp_buf[2] = 'k';
		rsp_buf[3] = ' ';
	}

	
	/* Calculate the checksum */
	slavep.chk_sum = 0;

	/* calculate byte count, including response parameters */
	for (count = 1; count < RSPBUFSZ; count++) {
		if (rsp_buf[count] == 0)
			break;
	}
	slavep.byte_count = count - 1;
	rsp_buf[0] = slavep.byte_count;

	for (count = 0; count < (slavep.byte_count + 1); count++) 
		slavep.chk_sum += rsp_buf[count];
	slavep.chk_sum = 0 - slavep.chk_sum;
	
	/* store the checksum on the queue */
	rsp_buf[count] = slavep.chk_sum;

	/* if the response is on hold then load the first byte */
	if (slavep.slave_stat == SLAVE_XMIT) {
		slavep.i2c_cmd_stat = I2C_CMD_DONE;
		rsp_ptr = 1;
		writeS0(rsp_buf[0]);
	}
}

