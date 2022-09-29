/*
 * cmdsrvc.c
 *	Functions for handling service commands (i.e. those that affect
 *	the MMSC itself, rather than the system at large)
 */

#include <vxworks.h>

#include <inetlib.h>
#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslib.h>
#include <tasklib.h>

#include "ffsc.h"

#include "addrd.h"
#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "misc.h"
#include "msg.h"
#include "nvram.h"
#include "oobmsg.h"
#include "remote.h"
#include "route.h"
#include "tty.h"
#include "user.h"
#include "xfer.h"


/*
 * Bitmask of things going on with OOB handling at a given point
 * in time. Take a look at ffsc.h for the bitfields. Basically,
 * we have two tasks which could be doing OOB processing at any
 * one time so we need to know what is going on (in the context
 * of another task) before we do something drastic like swapping
 * system ports.
 */
volatile unsigned int oob_handler_status;


/*
 * cmdDEBUG
 *	Change the current debug flags
 */
int
cmdDEBUG(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;

	/* Make sure the user is duly authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Bail out now if we were not selected */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If a token was found, change the debug flags & write them */
	/* out to NVRAM.					     */
	if (Token != NULL) {
		ffscDebug.Flags = strtoul(Token, NULL, 0);
		nvramWrite(NVRAM_DEBUGINFO, &ffscDebug);
	}

	/* Say what the new flags are */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK 0x%08lx",
		ffscLocal.rackid, ffscDebug.Flags);
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}


/*
 * cmdDOWNLOADER
 *	Enables or disables the SBC-FFSC serial downloader.
 */
int
cmdDOWNLOADER(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;
	int Enable;

	/* Make sure the user is duly authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Extract the next command line token and make sure it is OK */
	Token = strtok_r(NULL, " \t", CmdP);
	if (Token == NULL) {
		ffscMsg("Must specify ENABLE or DISABLE with DOWNLOADER "
			    "command");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	ffscConvertToUpperCase(Token);
	if (strcmp(Token, "ENABLE") == 0) {
		Enable = 1;
	}
	else if (strcmp(Token, "DISABLE") == 0) {
		Enable = 0;
	}
	else {
		ffscMsg("Invalid subcommand to DOWNLOADER: \"%s\"", Token);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Bail out now if we were not selected */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Do the requested action */
	if (Enable) {
		enable_downloader();
	}
	else {
		disable_downloader();
	}

	/* Record our success */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK",
		ffscLocal.rackid);
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}


/*
 * cmdFLASH
 *	Processes a "FLASH" command. 
 */
int
cmdFLASH(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Origin;
	char *SubCmd;
	int  NumRacks;
	int  OriginIsSys;
	int  Result;
	rackid_t Rack = -1;

	/* Make sure the user is duly authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Make sure only one rack is specified in the destination */
	NumRacks = destNumRacks(Dest, &Rack);
	if (NumRacks != 1) {
		ffscMsg("Attempted to FLASH %d racks",
			NumRacks);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR DEST");
		return PFC_DONE;
	}

	/* Parse any additional arguments */
	SubCmd = strtok_r(NULL, " \t", CmdP);
	if (SubCmd == NULL) {
		SubCmd = "FROM";
		Origin = "SYSTEM";
	}
	else {
		ffscConvertToUpperCase(SubCmd);

		/* There is only one subcommand, "FROM" */
		if (strcmp(SubCmd, "FROM") != 0) {
			ffscMsg("Invalid FLASH subcommand \"%s\"", SubCmd);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* FROM requires an origin as well */
		Origin = strtok_r(NULL, " \t", CmdP);
		if (Origin == NULL) {
			ffscMsg("No origin for FLASH FROM command");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}
		ffscConvertToUpperCase(Origin);
	}

	/* Make sure the origin is OK */
	if (strcmp(Origin, "SYSTEM") == 0) {
		OriginIsSys = 1;
	}
	else if (strcmp(Origin, "CONSOLE") == 0) {
		OriginIsSys = 0;
	}
	else {
		ffscMsg("Invalid origin for FLASH FROM command: \"%s\"",
			Origin);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR ARG");
		return PFC_DONE;
	}

	/* If the request originated on a remote machine, */
	/* arrange to use it for input			  */
	if (User->Type == UT_FFSC) {
		char   Command[MAX_CONS_CMD_LEN + 1];
		user_t *LocalUser;

		/* The request better be for this rack */
		if (Rack != ffscLocal.rackid) {
			ffscMsg("Remote MMSC tried to FLASH a remote rack");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* Acquire the remote console task */
		LocalUser = userAcquireRemote(User);
		if (LocalUser == NULL) {
			/* Error should already be logged */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR BUSY");
			return PFC_DONE;
		}

		/* Say what's happening if desired */
		if (ffscDebug.Flags & FDBF_ROUTER) {
			ffscMsg("Flashing firmware from remote MMSC on FD %d",
				User->InReq->FD);
		}

		/* Generate the actual command that we will send to */
		/* the console then send it.			    */
		sprintf(Command,
			"STATE FS%d M%d",
			User->InReq->FD, CONM_FLASH);
		if (userSendCommand(LocalUser,
				    Command,
				    Responses[ffscLocal.rackid]->Text)
		    != OK)
		{
			/* userSendCommand has logged the error */
			userReleaseRemote(LocalUser);
			return PFC_DONE;
		}

		/* Make sure we don't close the remote FD */
		User->InReq->FD = -1;

		/* Consider the operation a success. Note that */
		/* the console task will take care of sending  */
		/* a response to the remote side.	       */
		Responses[ffscLocal.rackid]->Text[0] = '\0';
		Result = PFC_DONE;
	}

	/* Apparently the request originated locally. If the */
	/* request is *for* a remote machine, arrange for a  */
	/* connection to it to be used for system I/O.	     */
	else if (Rack != ffscLocal.rackid) {
		int RemoteFD;

		/* If the user is already remote, don't let them */
		/* get even more remote by routing them to yet	 */
		/* another rack, the bookkeeping is too hard.	 */
		if (User->Type == UT_REMOTE) {
			ffscMsg("Trying to flash a remote machine "
				    "FROM a remote machine");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR TOOREMOTE");
			return PFC_DONE;
		}

		/* Set up the remote connection */
		RemoteFD = remStartConnectCommand(User, "FLASH", Rack, Dest);
		if (RemoteFD < 0) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR CONNECT" FFSC_ACK_SEPARATOR "%s",
				users[USER_FFSC + Rack]->OutAck->Text);
			return PFC_DONE;
		}
		else if (OriginIsSys) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE BS%d FS%d FP%d M%d",
				CONF_PROGRESS | CONF_FLASHMSG,
				RemoteFD,
				User->Console->FDs[CCFD_USER],
				CONM_COPYSYS);
			Result = PFC_DONE;
		}
		else {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE BS%d FS%d M%d",
				CONF_IGNOREEXIT | CONF_FLASHMSG | CONF_NOLOG,
				RemoteFD,
				CONM_COPYUSER);
			Result = PFC_DONE;
			routeCheckBaseIO = 1;
		}
	}

	/* Otherwise, we have a local request to enter FFSC */
	/* mode on the local rack. That's easy.		    */
	else {
		if (OriginIsSys) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE BS%d FS%d FP%d M%d",
				CONF_PROGRESS,
				User->Console->FDs[CCFD_SYS],
				User->Console->FDs[CCFD_USER],
				CONM_FLASH);
		}
		else {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE BS%d FS%d M%d",
				CONF_FLASHMSG,
				User->Console->FDs[CCFD_USER],
				CONM_FLASH);
			routeCheckBaseIO = 1;
		}

		Result = PFC_DONE;
	}

	return Result;
}


/*
 * One of the tasks started. Set the right one.
 */
static void setOOBStatusStart(user_t* User)
{

  /* upper IO6 is doing OOB IO */
  if(User->Console == consDaemon)
    oob_handler_status |= GFX_OOB_UPPER;
  
  /* lower IO6 is doing OOB IO */
  if(User->Console == consDaemon2)
    oob_handler_status |= GFX_OOB_LOWER;
}

/*
 * One of the tasks finished. Set the right one.
 */
static void setOOBStatusFinish(user_t* User)
{
  /* upper IO6 is doing OOB IO */
  if(User->Console == consDaemon)
    oob_handler_status &= ~(GFX_OOB_UPPER);
  
  /* lower IO6 is doing OOB IO */
  if(User->Console == consDaemon2)
    oob_handler_status &= ~(GFX_OOB_LOWER);
}

/*
 * Returns value > 0 if this is going on. VERY useful for when we are going to
 * swap system ports on these guys (console tasks) and we don't want to confuse them.
 */
int inOOBMessageProcessing()
{
  return ((oob_handler_status & GFX_OOB_LOWER) | (oob_handler_status & GFX_OOB_UPPER));
}



/*
 * cmdGRAPHICS_COMMAND
 *	Forward a graphics command to the display task and collect a response
 */
int
cmdGRAPHICS_COMMAND(char **CmdP, user_t *User, dest_t *Dest)
{
	char  *Token, *Ptr;
	unsigned char *DispMsg;
	unsigned char *OOBMsg;
	unsigned char *RespMsg = NULL;

	setOOBStatusStart(User);

#if 1
	/* Only the system console may issue graphics commands */
	if (User->Console != getSystemConsole() && 
	    (User->Console != consDaemon || User->Console != consDaemon2)){
	  ffscMsg("%s tried to issue graphics command", User->Name);
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR SOURCE");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}
#endif
	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no index is present, that's an error */
	if (Token == NULL) {
	  ffscMsg("No buffer address specified for GRAPHICS_COMMAND");
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* Parse the index and make sure it is OK */
	OOBMsg = (unsigned char *) strtol(Token, &Ptr, 0);
	if (*Ptr != '\0'  ||  OOBMsg == NULL) {
	  ffscMsg("Invalid value for buffer address \"%s\"", Token);
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* Make sure a legitimate address was specified */
	if (ffscTestAddress(OOBMsg) != OK) {
	  ffscMsg("Buffer address %s is not a valid address", Token);
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* As one last sanity check, make sure the message begins with */
	/* an OOB message prefix.				       */
	if (OOBMsg[0] != (unsigned char) OBP_CHAR) {
	  ffscMsg("Buffer does not contain a valid OOB message");
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* Now parse an (optional) address for the response */
	Token = strtok_r(NULL, " \t", CmdP);
	if (Token != NULL) {
		RespMsg = (unsigned char *) strtol(Token, &Ptr, 0);
		if (*Ptr != '\0'  ||  OOBMsg == NULL) {
			ffscMsg("Invalid value for response address \"%s\"",
				Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			setOOBStatusFinish(User);
			return PFC_DONE;
		}

		if (ffscTestAddress(RespMsg) != OK) {
			ffscMsg("Response address %s is not a valid address",
				Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			setOOBStatusFinish(User);
			return PFC_DONE;
		}
	}

	/* Build the command that we will send to the display task */
	msgClear(userDisplay->OutReq);
	sprintf(userDisplay->OutReq->Text, "g %d", (int) OOBMsg);
	userDisplay->OutReq->State = RS_SEND;

	/* Say what we are about to do if desired */
	if (ffscDebug.Flags & FDBF_ROUTER) {
	  ffscMsg("About to send \"%s\" to display task",
		  userDisplay->OutReq->Text);
	}

	/* Write the command to the Term task */
	if (msgWrite(userDisplay->OutReq, ffscTune[TUNE_RO_DISPLAY]) != OK) {
	  /* msgWrite should have logged the error already */
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR SEND");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* Make sure everything got written */
	if (userDisplay->OutReq->State != RS_DONE) {
	  ffscMsg("Unable to send message to display task - State=%d",
		  userDisplay->OutReq->State);
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR SEND");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* Prepare for a response from the display task */
	msgClear(userDisplay->OutAck);
	userDisplay->OutAck->State = RS_NEED_DISPACK;
	if (msgRead(userDisplay->OutAck, ffscTune[TUNE_RI_DISPLAY]) != OK  ||
	    userDisplay->OutAck->State != RS_DONE)
	{
	  ffscMsg("Failed waiting for response from display task: S=%d",
		  userDisplay->OutAck->State);
	  
	  if (userDisplay->OutAck->State == RS_ERROR) {
	    sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR BADRESP");
	  }
	  else {
	    sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR TIMEOUT (cmdFLASH)");
	  }
	  
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}
	
	/* Show the response if desired */
	if (ffscDebug.Flags & FDBF_ROUTER) {
	  ffscMsg("Router got response \"%s\" from display task",
		  userDisplay->OutAck->Text);
	}

	/* If the response is not "OK", return it verbatim */
	if (strncmp(userDisplay->OutAck->Text, "OK", 2) != 0) {
	  strcpy(Responses[ffscLocal.rackid]->Text,
		 userDisplay->OutAck->Text);
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* Parse the return argument and validate it as with input args */
	Ptr = NULL;
	Token = strtok_r(userDisplay->OutAck->Text + 2, " \t", &Ptr);
	if (Token == NULL) {
	  ffscMsg("No response address from display task");
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR DISPLAY");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	DispMsg = (unsigned char *) strtol(Token, &Ptr, 0);
	if (*Ptr != '\0'  ||  DispMsg == NULL) {
	  ffscMsg("Invalid value for display response buffer \"%s\"",
		  Token);
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR DISPLAY");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	if (ffscTestAddress(DispMsg) != OK) {
	  ffscMsg("Display response buffer %s is not a valid address",
		  Token);
	  sprintf(Responses[ffscLocal.rackid]->Text,
		  "ERROR DISPLAY");
	  setOOBStatusFinish(User);
	  return PFC_DONE;
	}

	/* If a response buffer was specified, copy the message over */
	if (RespMsg != NULL) {
		int ResponseLen;

		ResponseLen = OOBMSG_MSGLEN(DispMsg) - 1;
		bcopy(DispMsg, RespMsg, ResponseLen);
		RespMsg[ResponseLen] = oobChecksumMsg(RespMsg);
	}

	/* Return the status code with our OK response */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK %d",
		OOBMSG_CODE(DispMsg));
	setOOBStatusFinish(User);
	return PFC_DONE;
}


/*
 * cmdIPADDR
 *	Set the FFSC's IP address on the private ethernet
 */
int
cmdIPADDR(char **CmdP, user_t *User, dest_t *Dest)
{
	char NtoABuf[INET_ADDR_LEN];
	char *Token;
	identity_t NewIdentity;
	int NumRacks;

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no new IP address is present, then simple return the local one */
	if (Token == NULL) {
		if (destRackIsSelected(Dest, ffscLocal.rackid)) {
			inet_ntoa_b(ffscLocal.ipaddr, NtoABuf);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"R%ld:OK %s",
				ffscLocal.rackid, NtoABuf);
			Responses[ffscLocal.rackid]->State = RS_DONE;
		}

		return PFC_FORWARD;
	}

	/* Make sure the user is authorized to make changes */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Make sure only one rack is specified in the destination */
	NumRacks = destNumRacks(Dest, NULL);
	if (NumRacks > 1) {
		ffscMsg("Attempted to change %d rackid's with one command",
			NumRacks);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR DEST");
		return PFC_DONE;
	}

	/* If our rack is not the selected one, tell the router */
	/* to pass the command along elsewhere			*/
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Start with our original identity */
	NewIdentity = ffscLocal;

	/* Parse the new rack ID and make sure it is OK */
	if (ffscParseIPAddr(Token, &NewIdentity.ipaddr) != OK) {
		ffscMsg("Invalid IP address \"%s\"", Token);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Go set up for the new IP address */
	if (addrdUpdateLocalIdentity(&NewIdentity) != OK) {
		ffscMsg("WARNING: Something went wrong setting IP address - "
			    "conflicts may exist");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR INTERNAL");
	}
	else {
		inet_ntoa_b(ffscLocal.ipaddr, NtoABuf);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK %s",
			NtoABuf);
	}

	return PFC_DONE;
}



int 
cmdMACADDR(char **CmdP, user_t *User, dest_t *Dest)
{

  int i;
  
  if (destRackIsSelected(Dest, ffscLocal.rackid)) {

    sprintf(Responses[ffscLocal.rackid]->Text,
	    "R%ld:OK %02x:%02x:%02x:%02x:%02x:%02x OK", 
	    ffscLocal.rackid,
	    ffscMACAddr[0] & 0xff,
	    ffscMACAddr[1] & 0xff,
	    ffscMACAddr[2] & 0xff,
	    ffscMACAddr[3] & 0xff,
	    ffscMACAddr[4] & 0xff,
	    ffscMACAddr[5] & 0xff);
    Responses[ffscLocal.rackid]->State = RS_DONE;

  }

  return PFC_FORWARD;
}
	    
/*
 * cmdMEM
 *	Show current memory utilization
 */
int
cmdMEM(char **CmdP, user_t *User, dest_t *Dest)
{
  int block, block_size;
  int total_free=0, blocks=0, max_block_size=0, min_block_size=DRAM_SIZE;
  void *foo[256];


  if (destRackIsSelected(Dest, ffscLocal.rackid)) {
    /* First, use the VxWorks utility.  Unfortunatelly it is not
       very useful in a production version */

#ifndef PRODUCTION
    memShow(1);

#endif /* PRODUCTION */


    /* So now just run our own little ugly hack

       There are multiple blocks, and the blocks do not get
       combined.  So All the blocks are cycled through.
       */


    /* The outer loop cycles through blocks. It gets completed 
       when no more memory can be allocated, which means we have
       exhausted all blocks.
       */

    for (block=0; block < 256; block ++) {
      foo[block] = NULL;

      /* The inner loop tries to allocate the maximum amount within
	 a block. */

      for (block_size = 4; block_size < DRAM_SIZE; block_size += 4) {
	    
	/* if malloc successed, try a bigger amount. */
	if ((foo[block] = malloc(block_size)) == NULL) {

	  /* first check if there are no more blocks, which
	     means not even 4 bytes can be allocated*/
	      
	  if (block_size == 4)
	    break;

	      
	  /* Otherwise We're asking for too much, but the last one must
	     have worked.  So back up the size and grab the 
	     memory.  But do that carefully, to make sure it has not
	     shrunk.
	     */


	  if ((foo[block] = malloc(block_size-4)) == NULL) {
	    sprintf(Responses[ffscLocal.rackid]->Text,
		    "OK  Available memory shrunk during checking.  Try again.");
	    return PFC_DONE;
	  }
	    
	  /* do some bookkeeping */

	  total_free += (block_size-4);
	  if (max_block_size < (block_size-4))
	    max_block_size = (block_size-4);
	  if (min_block_size > (block_size-4))
	    min_block_size = (block_size-4);
	  break;
	}
	   
	free(foo[block]);
      }

#ifndef PRODUCTION 
      ffscMsg("Finished a block.  Size: %ld bytes.", (block_size-4));

#endif /* PRODUCTION */

      /* if cannot allocate 4 bytes, then there is no more memory left */

      if (block_size == 4) {
	ffscMsg("Finished all");
	blocks = block+1;
	break;
      }
    }

    for (block = 0; block < blocks; block++)
      free(foo[block]);

    if (min_block_size == DRAM_SIZE)
      min_block_size = 0;

    sprintf(Responses[ffscLocal.rackid]->Text, 
	    "R%ld:OK  Available Memory: %ld Bytes\r\nNumber of blocks: \
%ld\r\nLargest block: %ld Bytes \r\nSmallest block: \
%ld Bytes",
	    ffscLocal.rackid,
	    total_free, blocks, max_block_size, min_block_size);
    Responses[ffscLocal.rackid]->State = RS_DONE;

  }

  return PFC_FORWARD;

}


/*
 * For manufacturing: return the number of racks.
 * NOTE: user must issue "r * b *" to set the destination
 * before running if they wish to determine the total number of
 * racks available in the system.
 */
int cmdNUMRACKS(char **CmdP, user_t* User, dest_t *Dest)
{
  int nRacks = 0;
  rackid_t Rack = -1;
  nRacks = destNumRacks(Dest, &Rack);
  sprintf(Responses[ffscLocal.rackid]->Text, 
	  "%d OK", 
	  nRacks);
  return PFC_DONE;
}

/*
 * cmdPWR
 *	Intercepted version of the MSC "pwr" command. If "pwr u" or "pwr c"
 *	is specified, we will forward it to each addressed MSC as usual
 *	except that we pause for some time after sending the command to each
 *	rack so that the power doesn't all come on at once. Any other form of
 *	the MSC "pwr" command is passed along unchanged.
 */
int
cmdPWR(char **CmdP, user_t *User, dest_t *Dest)
{
	char Command[MAX_FFSC_CMD_LEN + 1];
	char *Token;
	int DidFirst;
	int NumRacks;
	int TotalDelay;
	rackid_t CurrRack;
	struct timespec Delay;

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If the token is not "u" or "c", then tell the router to treat */
	/* this like an ordinary ELSC command.				 */
	if (strcmp(Token, "u") != 0  &&  strcmp(Token, "c") != 0) {
		return PFC_NOTFFSC;
	}

	/* Determine how many bays will actually be powered up/down.   */
	/* If it's only one bay, let the router deal with this itself. */
	NumRacks = destNumRacks(Dest, NULL);
	if (NumRacks < 2) {
		return PFC_NOTFFSC;
	}

	/* Build the official command that we will send to each bay */
	sprintf(Command, "pwr %s", Token);

	/* Borrow the InAck msg_t to send a WAIT message to the console */
	/* so it doesn't time us out					*/
	if (User->Type != UT_DISPLAY) {
		TotalDelay = ffscTune[TUNE_PWR_DELAY] * (NumRacks - 1);
		sprintf(User->InAck->Text,
			"WAIT %d "
			    "Please wait %.1f secs to sequence power for %d "
			    "racks"
			    FFSC_ACK_END,
			TotalDelay,
			TotalDelay / 1000000.0, NumRacks);
		User->InAck->State = RS_SEND;

		if (msgWrite(User->InAck, ffscTune[TUNE_RO_RESP]) != OK  ||
		    User->InAck->State != RS_DONE)
		{
			ffscMsg("Unable to send WAIT message in cmdPWR");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR WAIT");
			return PFC_DONE;
		}
		msgClear(User->InAck);
	}

	/* Set up the delay time */
	Delay.tv_sec  = ffscTune[TUNE_PWR_DELAY] / 1000000;
	Delay.tv_nsec = (ffscTune[TUNE_PWR_DELAY] % 1000000) * 1000;

	/* Process each rack in order */
	DidFirst = 0;
	for (CurrRack = 0;  CurrRack < MAX_RACKS;  ++CurrRack) {

		/* Skip this rack if no bays are selected */
		if (!destRackIsSelected(Dest, CurrRack)) {
			continue;
		}

		/* Wait before actually sending the command unless this */
		/* is the first rack we are doing			*/
		if (DidFirst) {
			nanosleep(&Delay, NULL);
		}

		/* If we are on the local rack, send the command directly */
		/* to the MSC's, otherwise arrange for it to be forwarded */
		if (CurrRack == ffscLocal.rackid) {
			routeProcessELSCCommand(Command, User, Dest);
		}
		else {
			remStartRemoteCommand(User, Command, CurrRack, Dest);
		}

		/* Remember that we have done at least one rack already */
		DidFirst = 1;
	}

	/* Tell the router to wait for responses */
	return PFC_WAITRESP;
}


/*
 * cmdREBOOT_FFSC
 *	Reboot the FFSC (duh)
 */
int
cmdREBOOT_FFSC(char **CmdP, user_t *User, dest_t *Dest)
{
	/* Make sure the user is duly authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	if (destRackIsSelected(Dest, ffscLocal.rackid)) {

		/* Tell the router The End Is Near */
		routeRebootFFSC = 1;

		/* Arrange for a normal response despite it all */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK Coming Down...",
			ffscLocal.rackid);
		Responses[ffscLocal.rackid]->State = RS_DONE;
	}

	return PFC_FORWARD;
}


/*
 * cmdRESET_NVRAM
 *	Reset the contents of NVRAM to initial conditions. A useful
 *	thing to do if the NVRAM format has changed and nobody was
 *	thoughtful enough to provide for the appropriate conversion.
 */
int
cmdRESET_NVRAM(char **CmdP, user_t *User, dest_t *Dest)
{
	/* Make sure the user is duly authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	if (destRackIsSelected(Dest, ffscLocal.rackid)) {
		nvramReset();
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK %d",
			ffscLocal.rackid, NH_CURR_VERSION);
		Responses[ffscLocal.rackid]->State = RS_DONE;
	}

	return PFC_FORWARD;
}


/*
 * cmdTTY_REINIT
 *	Reinitialize the TTY ports, in case there is trouble
 */
int
cmdTTY_REINIT(char **CmdP, user_t *User, dest_t *Dest)
{
	void sysSerialHwInit(void);

	/* Make sure the user is duly authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* If our rack is not the selected one, tell the router */
	/* to pass the command along elsewhere			*/
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Wait a bit for any debug I/O to quiesce */
	taskDelay(60);

	/* Don't let anybody else run, or they might lock up a serial port */
	taskLock();

	/* Initialize all of the serial ports */
	if (ttyReInit() != OK) {
		ffscMsg("WARNING: Error reinitializing serial ports");
	}

	/* Safe for others to run now */
	taskUnlock();

	/* Everything was apparently successful */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK",
		ffscLocal.rackid);
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}
