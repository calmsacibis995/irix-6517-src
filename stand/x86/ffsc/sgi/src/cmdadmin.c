/*
 * cmdadmin.c
 *	Functions for handling administrative commands (mostly those
 *	that deal with information or authority)
 */

#include <vxworks.h>

#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "ffsc.h"

#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "elsc.h"
#include "log.h"
#include "misc.h"
#include "msg.h"
#include "remote.h"
#include "route.h"
#include "user.h"


extern part_info_t* partInfo; /* in elsc.c */
  

/* Token tables */
#define PRINTENV_ALL	1
#define PRINTENV_DFLT	2
#define PRINTENV_SECRET 3
static tokeninfo_t PrintEnvOptions[] = {
	{ "ALL",	PRINTENV_ALL },
	{ "DEFAULT",	PRINTENV_DFLT },
	{ "DFLT",	PRINTENV_DFLT },
	{ "SECRET",	PRINTENV_SECRET },
	{ NULL,		0 },
};

static tokeninfo_t AuthorityLevels[] = {
	{ "BASIC",	AUTH_BASIC },
	{ "SUPERVISOR",	AUTH_SUPERVISOR },
	{ "SERVICE",	AUTH_SERVICE },
	{ "MSC",	-1 },		/* Special case */
	{ "ELSC",	-1 },
	{ NULL,		0 },
};

#define PASSWORD_SET		1
#define PASSWORD_SETMMSC	2
#define PASSWORD_UNSET		3
static tokeninfo_t PasswordSubCmds[] = {
	{ "SET",	PASSWORD_SET },
	{ "SETMMSC",	PASSWORD_SETMMSC },
	{ "UNSET",	PASSWORD_UNSET },
	{ NULL,		0 },
};


/*
 * cmdAUTHORITY
 *	Set or modify the user's authority level
 */
int
cmdAUTHORITY(char **CmdP, user_t *User, dest_t *Dest)
{
	char *LevelStr;
	char *SpecifiedPassword;
	int  Authority;

	/* First, parse the authority level */
	LevelStr = strtok_r(NULL, " \t", CmdP);

	/* No authority level at all simply means show the current level */
	if (LevelStr == NULL) {
		char LevelName[20];

		ffscIntToToken(AuthorityLevels,
			       User->Authority,
			       LevelName,
			       20);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK %s",
			LevelName);
		return PFC_DONE;
	}

	/* Convert the authority level token to an actual value */
	ffscConvertToUpperCase(LevelStr);
	if (ffscTokenToInt(AuthorityLevels, LevelStr, &Authority) != OK  ||
	    Authority == -1)
	{
		ffscMsg("Invalid authority level \"%s\"", LevelStr);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Parse the password if present */
	SpecifiedPassword = strtok_r(NULL, " \t", CmdP);
	if (SpecifiedPassword == NULL) {
		SpecifiedPassword = "";
	}

	/* If the user is "upgrading" their authority, validate the */
	/* password (if one is needed)				    */
	if (Authority > User->Authority  &&
	    User->Type != UT_DISPLAY)
	{
		char *RequiredPassword;

		/* Determine the new level's password (if any) */
		if (Authority >= AUTH_SERVICE) {
			RequiredPassword = userPasswords.Service;
		}
		else if (Authority >= AUTH_SUPERVISOR) {
			RequiredPassword = userPasswords.Supervisor;
		}
		else {
			RequiredPassword = "";
		}

		/* Make sure the passwords match */
		if (strncmp(SpecifiedPassword,
			    RequiredPassword,
			    MAX_PASSWORD_LEN) != 0)
		{
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR PASSWORD");
			return PFC_DONE;
		}
	}

	/* Update the user information with the new authority level */
	User->Authority = Authority;
	userUpdateSaveInfo(User);

	/* Return successfully */
	sprintf(Responses[ffscLocal.rackid]->Text, "OK");
	return PFC_DONE;
}


/*
 * cmdHELP
 *	Display help in various forms
 */
int
cmdHELP(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Topic;
	int  i;
	pagebuf_t *HelpBuf;

	/* Look for the next token */
	Topic = strtok_r(NULL, " \t", CmdP);
	if (Topic != NULL) {
		ffscConvertToUpperCase(Topic);
	}

	/* Set up a pager buffer to hold the help info. The size of */
	/* the buffer may need tweaking as time goes on.	    */
	HelpBuf = ffscCreatePageBuf(16*1024);
	if (HelpBuf == NULL) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR STORAGE");
		return PFC_DONE;
	}
	ffscPrintPageBuf(HelpBuf, "");

	/* Proceed according to the topic */
	if (Topic == NULL) {
		char LineBuf[81];
		int CurrCol;
		int NotFirst;

		/* Print the names of all commands */

		ffscPrintPageBuf(HelpBuf,
				 "Help is available on the following "
				     "commands:\r\n");

		NotFirst = 0;
		CurrCol  = 1;
		LineBuf[0] = '\0';
		for (i = 0;  cmdList[i].Name != NULL;  ++i) {
			int NameLen;

			/* Skip secret commands */
			if (cmdList[i].Help == NULL) {
				continue;
			}

			/* Append a comma if necessary */
			if (NotFirst) {
				strcat(LineBuf, ", ");
				CurrCol += 2;
			}
			else {
				NotFirst = 1;
			}

			/* Go on to the next line if necessary */
			NameLen = strlen(cmdList[i].Name);
			if (CurrCol + NameLen + 2 > 80) {
				ffscPrintPageBuf(HelpBuf, LineBuf);
				LineBuf[0] = '\0';
				CurrCol = 1;
			}

			/* Add the command name to the list */
			strcat(LineBuf, cmdList[i].Name);
			CurrCol += NameLen;
		}

		ffscPrintPageBuf(HelpBuf,
				 "%s\r\n\r\nFor more info on a command, type "
				     "HELP <cmd>",
				 LineBuf);
	}

	else if (strcmp(Topic, "ALL") == 0) {

		/* Want help for all topics */
		for (i = 0;  cmdList[i].Name != NULL;  ++i) {

			/* Skip secret commands */
			if (cmdList[i].Help == NULL) {
				continue;
			}

			/* Add the current command to the list */
			ffscPrintPageBuf(HelpBuf, cmdList[i].Help);
		}
	}

	else {
		/* Show help for a single topic */
		cmdinfo_t *Info;

		/* Look for the command */
		Info = NULL;
		for (i = 0;  cmdList[i].Name != NULL;  ++i) {
			if (strcmp(Topic, cmdList[i].Name) == 0) {
				Info = &cmdList[i];
				break;
			}
		}

		/* If the command is not found (or is secret), complain */
		if (Info == NULL  ||  cmdList[i].Help == NULL) {
			ffscPrintPageBuf(HelpBuf,
					 "Cannot find help for command %s",
					 Topic);
		}

		/* Otherwise, print the help text */
		else {
			ffscPrintPageBuf(HelpBuf, cmdList[i].Help);
		}
	}

	/* Ask the console to page the resulting output for us */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK;STATE FS%d FB%d M%d",
		getSystemPortFD(), (int) HelpBuf, CONM_PAGER);

	return PFC_DONE;
}


/*
 * cmdNOP
 *	Do nothing (useful for waking up the router process if necessary)
 */
int
cmdNOP(char **CmdP, user_t *User, dest_t *Dest)
{
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK",
		ffscLocal.rackid);
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}

/*
 * cmdPAS
 *	Intercepted version of the MSC "pas" command. If "pas s <pw>" is
 *	specified, convert it to the MMSC command "PASSWORD SET MSC <pw>",
 *	otherwise pass it through to the MSC as-is.
 */
int
cmdPAS(char **CmdP, user_t *User, dest_t *Dest)
{
	char Command[MAX_FFSC_CMD_LEN + 1];
	char *Token;

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If the token is not simply "s", then tell the router to treat */
	/* this like an ordinary ELSC command.				 */
	if (strcmp(Token, "s") != 0) {
		return PFC_NOTFFSC;
	}

	/* Parse the actual password */
	Token = strtok_r(NULL, " \t", CmdP);
	if (Token == NULL) {
		ffscMsg("No password specified for \"pas s\"");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Generate an FFSC command from the pieces */
	sprintf(Command, "PASSWORD SET MSC %s", Token);

	/* Go run the FFSC command recursively */
	return cmdProcessFFSCCommand(Command, User, Dest);
}


/*
 * cmdPASSWORD
 *	Set one of the various passwords
 */
int
cmdPASSWORD(char **CmdP, user_t *User, dest_t *Dest)
{
	bayid_t CurrBay;
	char *NewPassword;
	char *SubCmdString;
	char *WhichPasswordString;
	int  Propagate;
	int  SubCmd;
	int  WhichPassword;

	/* Parse the subcommand */
	SubCmdString = strtok_r(NULL, " \t", CmdP);
	if (SubCmdString == NULL) {
		ffscMsg("No subcommand specified for PASSWORD");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	ffscConvertToUpperCase(SubCmdString);
	if (ffscTokenToInt(PasswordSubCmds, SubCmdString, &SubCmd) != OK) {
		ffscMsg("Invalid subcommand \"%s\" for PASSWORD",
			SubCmdString);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	if (SubCmd == PASSWORD_SET) {
		Propagate = 1;
	}
	else {
		Propagate = 0;
	}

	/* Determine which password is to be changed */
	WhichPasswordString = strtok_r(NULL, " \t", CmdP);
	if (WhichPasswordString == NULL) {
		ffscMsg("Password to be changed not specified for PASSWORD "
			    "%s",
			SubCmdString);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	ffscConvertToUpperCase(WhichPasswordString);
	if (ffscTokenToInt(AuthorityLevels,
			   WhichPasswordString,
			   &WhichPassword) != OK  ||
	    WhichPassword == AUTH_BASIC)
	{
		ffscMsg("\"%s\" is not a valid password to %s",
			WhichPasswordString, SubCmdString);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* If the password is being set, obtain the new password */
	if (SubCmd == PASSWORD_SET  ||  SubCmd == PASSWORD_SETMMSC) {
		NewPassword = strtok_r(NULL, " \t", CmdP);
		if (NewPassword == NULL  ||  strlen(NewPassword) < 1) {
			ffscMsg("No new password specified for PASSWORD %s",
				SubCmdString);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}
	}
	else {
		NewPassword = "";
	}

	/* Now that we have validated the command syntax, bail out if */
	/* we are not one of The Selected.			      */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Make sure the user is authorized to change this password */
	if ((WhichPassword == -1  &&  User->Authority < AUTH_SUPERVISOR)  ||
	    (WhichPassword > 0  &&  User->Authority < WhichPassword))
	{
		ffscMsg("User %s not authorized to change %s password",
			User->Name, WhichPasswordString);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR PERMISSION");
		return PFC_DONE;
	}

	/* Proceed according to the selected password */
	switch (WhichPassword) {

	    /* -1 means change the MSC password */
	    case -1:
		for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
			if (destLocalBayIsSelected(Dest, CurrBay)) {
				elscSetPassword(CurrBay,
						NewPassword,
						Propagate);
			}
			else if (destAllBaysSelected(Dest, ffscLocal.rackid)) {
				sprintf(ELSC[CurrBay].OutAck->Text, "OFFLINE");
				ELSC[CurrBay].OutAck->State = RS_DONE;
			}
		}
		break;

	    /* Supervisor password */
	    case AUTH_SUPERVISOR:
		strncpy(userPasswords.Supervisor,
			NewPassword,
			MAX_PASSWORD_LEN);
		userUpdatePasswords();

		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK",
			ffscLocal.rackid);
		Responses[ffscLocal.rackid]->State = RS_DONE;

		break;

	    /* Service password */
	    case AUTH_SERVICE:
		strncpy(userPasswords.Service,
			NewPassword,
			MAX_PASSWORD_LEN);
		userUpdatePasswords();

		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK",
			ffscLocal.rackid);
		Responses[ffscLocal.rackid]->State = RS_DONE;

		break;

	    /* Unknown password type */
	    default:
		ffscMsg("Don't know how to change password type %d",
			WhichPassword);

		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:ERROR INTERNAL",
			ffscLocal.rackid);
		Responses[ffscLocal.rackid]->State = RS_DONE;

		break;
	}

	return PFC_FORWARD;
}


/*
 * cmdPRINTENV
 *	Display some or all tuneable variables
 */
int
cmdPRINTENV(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;
	char Options[80];
	int  NumRacks;
	int  PrintAll = 0;
	int  PrintDflt = 0;
	int  PrintSecret = 0;
	pagebuf_t *PageBuf;
	rackid_t  Rack;

	/* Count how many racks were specified */
	NumRacks = destNumRacks(Dest, &Rack);
	if (NumRacks != 1) {
		ffscMsg("Must specify only one rack with PRINTENV");
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR DEST");
		return PFC_DONE;
	}

	/* Parse any option tokens */
	Options[0] = '\0';
	for (Token = strtok_r(NULL, " \t", CmdP);
	     Token != NULL;
	     Token = strtok_r(NULL, " \t", CmdP))
	{
		int TokenNum;

		ffscConvertToUpperCase(Token);
		if (ffscTokenToInt(PrintEnvOptions, Token, &TokenNum) != OK) {
			ffscMsg("Invalid PRINTENV option \"%s\"", Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		switch (TokenNum) {

		    case PRINTENV_ALL:
			PrintAll = 1;
			break;

		    case PRINTENV_DFLT:
			PrintDflt = 1;
			break;

		    case PRINTENV_SECRET:
			PrintSecret = 1;
			break;

		    default:
			ffscMsg("Don't know option %s (token %d)",
				Token, TokenNum);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR NOTAVAILABLE");
			return PFC_DONE;
		}

		/* Remember each option in case we forward to a remote FFSC */
		strcat(Options, Token);
		strcat(Options, " ");
	}

	/* If the user is local but the requested rack is not, then we */
	/* need to ask the remote rack for its environment var info    */
	if (User->Type != UT_FFSC  &&  Rack != ffscLocal.rackid) {
		char Command[MAX_FFSC_CMD_LEN + 1];
		int  DataLen;
		int  RemoteFD;

		/* Set up the remote command */
		sprintf(Command, "PRINTENV %s", Options);

		/* Fire up the connection request */
		RemoteFD = remStartDataRequest(User,
					       Command,
					       Rack,
					       Dest,
					       &DataLen);
		if (RemoteFD < 0) {
			/* Error msgs & responses already taken care of */
			return PFC_DONE;
		}

		/* Allocate a pagebuf for the incoming data */
		PageBuf = ffscCreatePageBuf(DataLen);
		if (PageBuf == NULL) {
			close(RemoteFD);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR STORAGE");
			return PFC_DONE;
		}

		/* Go read the data */
		if (remReceiveData(RemoteFD, PageBuf->Buf, DataLen) != OK) {
			/* Error messages & responses already taken care of */
			ffscFreePageBuf(PageBuf);
			return PFC_DONE;
		}

		/* Update the pagebuf info */
		PageBuf->DataLen = DataLen;

		/* We have successfully obtained the data */
		close(RemoteFD);
	}

	/* Otherwise, write local log info into a pagebuf */
	else {
		int i;
		int NumPrinted;

		/* Allocate a paged output buffer, guessing at the size */
		PageBuf = ffscCreatePageBuf(4096);
		if (PageBuf == NULL) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR STORAGE");
			return PFC_DONE;
		}

		/* Consider each of the known environment variables */
		NumPrinted = 0;
		for (i = 0;  i < NUM_TUNE;  ++i) {
			char ValueStr[20];

			/* If the value is the same as the default, */
			/* skip it unless ALL was specified *and*   */
			/* it isn't secret (unless secret is OK)    */
			if (ffscTune[i] == ffscTuneInfo[i].Default) {
				if (!PrintAll) {
					continue;
				}
				if ((ffscTuneInfo[i].Flags & TIF_SECRET) &&
				    !PrintSecret)
				{
					continue;
				}

				ValueStr[0] = '\0';
			}

			/* Otherwise, generate a string containing the value */
			else {
				sprintf(ValueStr, "%d", ffscTune[i]);
			}

			/* Generate a line for this variable */
			if (PrintDflt) {
				ffscPrintPageBuf(PageBuf,
						 "%s=%s   (default=%d)",
						 ffscTuneInfo[i].Name,
						 ValueStr,
						 ffscTuneInfo[i].Default);
			}
			else {
				ffscPrintPageBuf(PageBuf,
						 "%s=%s",
						 ffscTuneInfo[i].Name,
						 ValueStr);
			}

			/* Remember how many lines we have printed */
			++NumPrinted;
		}

		/* If we didn't print any values at all, say so */
		if (NumPrinted < 1) {
			ffscPrintPageBuf(PageBuf, "<NONE>");
		}
	}

	/* If the request was from a remote machine, send the data over */
	/* to it.							*/
	if (User->Type == UT_FFSC) {

		/* Make sure that local info was really requested */
		if (Rack != ffscLocal.rackid) {
			ffscMsg("Remote MMSC tried to PRINTENV for remote "
				    "MMSC");

			ffscFreePageBuf(PageBuf);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* Go send the data over to the other side */
		if (remSendData(User, PageBuf->Buf, PageBuf->DataLen) != OK) {
			/* remSendData should have set up a response, etc. */
			return PFC_DONE;
		}

		/* Consider the operation a success. Note that the */
		/* remote side will take care of the response.     */
		Responses[ffscLocal.rackid]->Text[0] = '\0';
	}

	/* Otherwise, hand the log info off to the pager */
	else {

		/* Log the data manually, since it won't be logged */
		/* by the pager					   */
		if (User->Console->UserOutBuf->Log != NULL) {
			logWrite(User->Console->UserOutBuf->Log,
				 PageBuf->Buf,
				 PageBuf->DataLen);
		}

		/* Indicate success and request PAGER mode */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE FB%d FS%d M%d",
			(int) PageBuf, getSystemPortFD(), CONM_PAGER);
	}

	return PFC_DONE;
}


/*
 * cmdVER
 *	Show the current FFSC firmware revision
 */
int
cmdVER(char **CmdP, user_t *User, dest_t *Dest)
{
	/* Only print the version number if our rack was selected */
	if (destRackIsSelected(Dest, ffscLocal.rackid)) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:MMSC %s",
			ffscLocal.rackid, ffscVersion);
		Responses[ffscLocal.rackid]->State = RS_DONE;
	}

	return PFC_FORWARD;
}

/* 
 * cmdARIBER
 *  Show which MMSC is the master right now.  Note that it's called
 *  arbiter so that the users won't be scared by the terminology.
 */

int
cmdARBITER(char **CmdP, user_t *User, dest_t *Dest)
{ 

  if (destRackIsSelected(Dest, ffscLocal.rackid)) {
    /* if there is no master, just say so */
    if (ffscMaster == NULL) {
      sprintf(Responses[ffscLocal.rackid]->Text,
	      "R%ld:OK No arbiter has been elected.",
	      ffscLocal.rackid);
      Responses[ffscLocal.rackid]->State = RS_DONE;
    }

    else {
      /* If there is one, print out its rackid */
      sprintf(Responses[ffscLocal.rackid]->Text,
	      "R%ld:OK %ld", ffscLocal.rackid,
	      ffscMaster->rackid);
      Responses[ffscLocal.rackid]->State = RS_DONE;
    }
  }

    
  return PFC_FORWARD;
}
	  

/*
 * cmdPARTLST
 *    List partition information currently stored in memory.
 */

int cmdPARTLST(char ** CmdP, user_t *User, dest_t *Dest)
{
  char * str = NULL;
  part_info_t* p = partInfo;
  int numparts;
  pagebuf_t *PageBuf;
  int numracks;
  rackid_t Rack;

  
  numracks = destNumRacks(Dest, &Rack);
  if (numracks != 1) {
    ffscMsg("Must specify only one rackid with PARTLST");
    sprintf(Responses[ffscLocal.rackid]->Text,
	    "ERROR DEST");
    return PFC_DONE;
  }


  /* If the user is local but the requested rack is not, then we
   * need to ask the remote rack for its partition info
   */

  if (User->Type != UT_FFSC && Rack != ffscLocal.rackid) {
    int RemoteFD, DataLen;

    /* Request a connection */

    RemoteFD = remStartDataRequest(User,
				   "PARTLST",
				   Rack,
				   Dest,
				   &DataLen);
    if (RemoteFD < 0) {
      /* Error messages & responses already taken care of */
      return PFC_DONE;
    }

    /* Allocate a pagebuf for the incoming data */

    if ((PageBuf = ffscCreatePageBuf(DataLen)) == NULL) {
      ffscMsg("Unable to allocate pagebug (length %d) for PARTLST",
	      DataLen);
      close(RemoteFD);
      sprintf(Responses[ffscLocal.rackid]->Text,
	      "ERROR STORAGE");
      return PFC_DONE;
    }

    /* Go read the data */
    if (remReceiveData(RemoteFD, PageBuf->Buf, DataLen) != OK) {
      /* Error messages and responses already taken care of */
      ffscFreePageBuf(PageBuf);
      return PFC_DONE;
    }
    
    /* Update the pagebuf info.  Gross abstraction violation :) */

    PageBuf->DataLen = DataLen;

    /* We have succesfully obtained the data */
    close(RemoteFD);
  }

  /* Otherwise, write local partition list int a pagebuf */
  else {

    
    


    /* Mutex enter  */
    lock_part_info();
    
    if(!partInfo){

      if ((PageBuf = ffscCreatePageBuf(256)) == NULL) {
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK ERROR STORAGE");
	unlock_part_info();
	return PFC_DONE;
      }

      ffscPrintPageBuf(PageBuf, "No partition information available");

      unlock_part_info();

    } else {


    

      /* The right way to do this is to use the pager, since we can
       * potentially have a lot of partitions.
       */

      /* first, figure out how many partitions we have */
      numparts = num_parts(partInfo);

      /* and then asssume that each partition will take at most
	 265 characters */
      if ((PageBuf = ffscCreatePageBuf(numparts * 256)) == NULL) {
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK ERROR STORAGE",
		ffscLocal.rackid);
	unlock_part_info();
	return PFC_DONE;
      }

    
      /* and now stuff the partition information into the buffer */
      while(p != NULL){
	str = get_part_info(&p);
	ffscPrintPageBuf(PageBuf, str);
	free(str);
	p = p->next;
      }
    
      /* since we're not gonna update the information at this point, 
       * release the lock.
       */
      unlock_part_info();

    }
  }
  

  /* If the request was from a remote machine, send the date over
   * to it.
   */

  if (User->Type == UT_FFSC) {

    /* Make sure the local info was really requested */
    if (Rack != ffscLocal.rackid) {
      ffscMsg("Remote MMSC tried to get PARTLST for a remote MMSC");
      ffscFreePageBuf(PageBuf);
      sprintf(Responses[ffscLocal.rackid]->Text,
	      "ERROR DEST");
      return PFC_DONE;

    }

    /* go send the data over to the other side */
    if (remSendData(User, PageBuf->Buf, PageBuf->DataLen) != OK) {
      /* error messages should be taken care of by now */
      return PFC_DONE;
    }
    ffscFreePageBuf(PageBuf);
    Responses[ffscLocal.rackid]->Text[0] = '\0';
  }

  /* Otherwise, have the pager spew out the information */
  else { 
 
    sprintf(Responses[ffscLocal.rackid]->Text,
	    "OK;STATE FS%d FB%d M%d",
	    getSystemPortFD(),
	    (int) PageBuf,
	    CONM_PAGER);
  }

  return PFC_DONE;


}


/*
 * cmdPARTGET
 * gets the module list of a partition set.
 */
int cmdPARTGET(char ** CmdP, user_t *User, dest_t *Dest)
{
  char tmp[128];
  int partid, located = 0;
  char* str = NULL, *tok = NULL;  
  part_info_t* p = partInfo;
  int NumRacks;
  pagebuf_t *PageBuf;
  rackid_t Rack;

  /* Count how many racks were specified */
  NumRacks = destNumRacks(Dest, &Rack);

  if (NumRacks != 1) {
    ffscMsg("Must specify only one rack with PARTGET");
    sprintf(Responses[ffscLocal.rackid]->Text,
	    "ERROR DEST");
    return PFC_DONE;
  }
  
  /* If the user is local but the requested rack is not, then we */
  /* need to ask the remote rack for its partition info          */
  if (User->Type != UT_FFSC && Rack != ffscLocal.rackid) {
    char Command[MAX_FFSC_CMD_LEN + 1];
    int  DataLen;
    int  RemoteFD;

    /* Set up the remote comman */
    if (*CmdP != NULL) {
      sprintf(Command, "PARTGET %s", *CmdP);
    } else {
      sprintf(Command, "PARTGET");
    }

    /* And fire up the connection request */

    RemoteFD = remStartDataRequest(User,
				   Command,
				   Rack,
				   Dest,
				   &DataLen);
    if (RemoteFD < 0) {
      /* Error msgs & responses already taken care of */
      return PFC_DONE;
    }


    PageBuf = ffscCreatePageBuf(DataLen);
    if (PageBuf == NULL) {
      ffscMsg("Unable to allocate pagebuf for PARTGET");
      close(RemoteFD);
      sprintf(Responses[ffscLocal.rackid]->Text,
	      "ERROR STORAGE");
      return PFC_DONE;
    }

    /* Go read the data */
    if (remReceiveData(RemoteFD, PageBuf->Buf, DataLen) != OK) {
      /* Error messages & responses already taken care of */
      ffscFreePageBuf(PageBuf);
      return PFC_DONE;
    }

    /* Update the pagebuf info */
    PageBuf->DataLen = DataLen;

    /* We have successfully obtained the data */
    close(RemoteFD);
  }
  
  /* Otherwise, write the local partition info into a pagebuf */

  else {
    
    int numparts;
    


    /* Mutex enter  */
    lock_part_info();
    
    Responses[ffscLocal.rackid]->Text[0] = '\0';
    
    if(!partInfo){

      if ((PageBuf = ffscCreatePageBuf(256)) == NULL) {
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK ERROR STORAGE");
	unlock_part_info();
	return PFC_DONE;
      }

      ffscPrintPageBuf(PageBuf, "No partition information availabe");

      /* Mutex exit */
      unlock_part_info();

    } else {
    

      /* The right way to do this is to use the pager, since we can
       * potentially have a lot of partitions.
       */
      
      /* first, figure out how many partitions we have */
      numparts = num_parts(partInfo);
      
      /* and then asssume that each partition will take at most
	 265 characters */
      if ((PageBuf = ffscCreatePageBuf(numparts * 256)) == NULL) {
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK ERROR STORAGE",
		ffscLocal.rackid);
	unlock_part_info();
	return PFC_DONE;
      }

      for(tok = strtok_r(NULL," \t",CmdP);
	  tok != NULL && p != NULL;
	  tok = strtok_r(NULL," \t",CmdP)){
	
	if(tok){ /* Partid given, printout info on just that one */
	  partid = atoi(tok);

	  if ((partid == 0) && !(tok[0] == '0')) {
	    sprintf(tmp,
		    "Invalid partition id: %s\r\n",
		    tok);
	    ffscPrintPageBuf(PageBuf, tmp);
	  }

	  else {
	    
	    str = get_part_info_on_partid(&p,partid);
	    if(str){
	      strcpy(tmp, str);
	      located++;
	    }
	    else {
	      sprintf(tmp,"Invalid partition id: %s\r\n",tok);    
	    }
	  
	    ffscPrintPageBuf(PageBuf, tmp);
	    free(str);

	  }
	}
      }
    
      if(!located){

	p = partInfo;

	while(p != NULL){
	  str = get_part_info(&p);
	  ffscPrintPageBuf(PageBuf, str);
	  free(str);
	  p = p->next;
	}
      }
    
    
      /* Mutex exit */
      unlock_part_info();
    

    }
  }


  /* If the request was from a remote machine, send the data over */
  /* to it.							*/
  if (User->Type == UT_FFSC) {

    /* Make sure that local info was really requested */
    if (Rack != ffscLocal.rackid) {
      ffscMsg("Remote MMSC tried to PARTGET for "
	      "a remote MMSC");
      
      ffscFreePageBuf(PageBuf);

      sprintf(Responses[ffscLocal.rackid]->Text,
	      "ERROR DEST");
      return PFC_DONE;
    }

  
    /* Go send the data over to the other side */
    if (remSendData(User, PageBuf->Buf, PageBuf->DataLen) != OK) {
      /* remSendData should have set up a response, etc. */
      return PFC_DONE;
    }
    /* Consider the operation a success. Note that the */
    /* remote side will take care of the response.     */
    Responses[ffscLocal.rackid]->Text[0] = '\0';
  }

  else {

    sprintf(Responses[ffscLocal.rackid]->Text,
	    "OK;STATE FS%d FB%d M%d",
	    getSystemPortFD(), 
	    (int) PageBuf, 
	    CONM_PAGER);
  }

  return PFC_DONE;

}
	    




/*
 * cmdPARTCLR
 * clear a partition mapping.
 */
int cmdPARTCLR(char ** CmdP, user_t *User, dest_t *Dest)
{
  int argCount, partId = 0, goodArgs = 0;
  char *tok;
  char* str = NULL;
  char tmp[128];
  part_info_t* p = partInfo;

  if (destRackIsSelected(Dest, ffscLocal.rackid)) {
    /* Mutex enter  */
    lock_part_info();
  
    Responses[ffscLocal.rackid]->Text[0] = '\0';
  
    if(!partInfo) {
      sprintf(Responses[ffscLocal.rackid]->Text,
	      "R%ld:OK No partition information available.",
	      ffscLocal.rackid);
      Responses[ffscLocal.rackid]->State = RS_DONE;
      unlock_part_info();
      return PFC_FORWARD; /* Send to other rack(s) */
    }
      
    bzero(Responses[ffscLocal.rackid]->Text,
	  Responses[ffscLocal.rackid]->MaxLen);
  
    sprintf(Responses[ffscLocal.rackid]->Text,
	    "R%ld:OK ",
	    ffscLocal.rackid); 
  
    /* Process each argument and validate input */
    for(argCount = 0, tok = strtok_r(NULL," \t",CmdP);
	(tok != NULL) && (argCount <= 2);
	tok = strtok_r(NULL," \t",CmdP),argCount++){

      if((argCount == 0) && tok){
	partId = atoi(tok);       /* Partition ID to name */
	if ((partId != 0) || (tok[0] == '0')) /* make sure it's a number */
	  goodArgs++;
	break;
      }

    }
    
    /* Arguments possibly valid, find partition id and remove it */
    if(argCount == 0 && goodArgs) {
      str = get_part_info_on_partid(&p, partId);
      if (str) {
	remove_partition_id(&partInfo, partId);
      }
      else {
	sprintf(tmp, "Invalid partition id: %s", tok);
	strcat(Responses[ffscLocal.rackid]->Text, tmp);
      }
    }
	       
    else
      strcat(Responses[ffscLocal.rackid]->Text,
	     "Syntax Error");
  
    Responses[ffscLocal.rackid]->State = RS_DONE;
  
  /* Mutex exit */
    unlock_part_info();
  }
  
  return PFC_FORWARD;
}


/*
 * cmdPARTSET
 * set a partition mapping.
 * NOTE: this information should not need updating but may in some
 * cases.
 * Syntax: partset ID MOD0 ... MODN
 * NOTE: MODN is the console module (last module given treated as console).
 */
int cmdPARTSET(char ** CmdP, user_t *User, dest_t *Dest)
{
  char tmp[128];
  int modList[256];
  int i,argCount, partId = 0, goodArgs = 0;
  char *tok;
  /*   part_info_t* p = partInfo;   */

  if  (destRackIsSelected(Dest, ffscLocal.rackid)) {

    bzero(Responses[ffscLocal.rackid]->Text,
	  Responses[ffscLocal.rackid]->MaxLen);
  

    sprintf(Responses[ffscLocal.rackid]->Text,
	    "R%ld:OK ",
	    ffscLocal.rackid); 

    /* Process each argument and validate input */
    for(argCount = 0, tok = strtok_r(NULL," \t",CmdP);
	(tok != NULL);tok = strtok_r(NULL," \t",CmdP),argCount++){

      if((argCount == 0) && tok) {
	partId = atoi(tok);       /* Partition ID to create */
	if ((partId == 0) && !(tok[0] == '0')) { /* this wasn't a number */
	  sprintf(tmp, "Invalid partition id: %s", tok);
	  strcat(Responses[ffscLocal.rackid]->Text, tmp);
	  Responses[ffscLocal.rackid]->State = RS_DONE;
	  return PFC_FORWARD;
	}
      }
      if((argCount >= 1) && tok){
	modList[argCount-1] = atoi(tok);
	if ((modList[argCount-1] == 0) && (tok[0] != '0')) {
	  /* this wasn't a number */
	  strcat(Responses[ffscLocal.rackid]->Text, "Syntax Error");
	  Responses[ffscLocal.rackid]->State = RS_DONE;
	  return PFC_FORWARD;
	}	  
	goodArgs++;
      }
    }
  
    argCount--; /* discount first argument (partition id)*/

  /* Arguments possibly valid, find partition id and assign name to it */
    if(argCount >= 1 && goodArgs){
      for(i = 0; i < argCount; i++){
	bzero(tmp,128);
	/* NOTE: last module input is assumed to be the console module. */
	if (i == (argCount-1)){
	  sprintf(tmp,"P %d M %d C", partId,modList[i]);
	  update_partition_info(&partInfo,tmp);
	}
	else{
	  sprintf(tmp,"P %d M %d", partId,modList[i]);
	  update_partition_info(&partInfo,tmp);
	}
      }
    }
    else {
      strcat(Responses[ffscLocal.rackid]->Text, "Syntax Error");
      Responses[ffscLocal.rackid]->State = RS_DONE;
      return PFC_FORWARD;
    }

    
    Responses[ffscLocal.rackid]->State = RS_DONE;

  }   
  return PFC_FORWARD;
 
}





/*
 * cmdPARTNAM
 * names the partition something useful.
 */
int cmdPARTNAM(char ** CmdP, user_t *User, dest_t *Dest)
{
  char tmp[128];
  int argCount, partId = 0, found = 0, goodArgs = 0;
  char *tok;
  part_info_t* p = partInfo;

  if (destRackIsSelected(Dest, ffscLocal.rackid)) {  
    /* Mutex enter  */
    lock_part_info();
    
    Responses[ffscLocal.rackid]->Text[0] = '\0';
    
    if(!partInfo){
      
      sprintf(tmp,"No partition mappings available.\r\n");
      strcat(Responses[ffscLocal.rackid]->Text,tmp); 
      Responses[ffscLocal.rackid]->State = RS_DONE;
      
      /* Mutex exit */
      unlock_part_info();
      return PFC_DONE;
    }
    
    bzero(Responses[ffscLocal.rackid]->Text,
	  Responses[ffscLocal.rackid]->MaxLen);
    
    sprintf(tmp,"OK\r\n");
    strcat(Responses[ffscLocal.rackid]->Text,tmp); 
    
    /* Process each argument and validate input */
    for(argCount = 0, tok = strtok_r(NULL," \t",CmdP);
	(tok != NULL) && (argCount <= 2);
	tok = strtok_r(NULL," \t",CmdP),argCount++){
      if((argCount == 0) && tok)
	partId = atoi(tok);       /* Partition ID to name */
      if((argCount == 1) && tok){
	sprintf(tmp,"%s",tok);    /* Name to assign to ID */
	goodArgs++;
	break;
      }
    }
    
    /* Arguments possibly valid, find partition id and assign name to it */
    if(argCount == 1 && goodArgs)
      while(p != NULL){
	if(p->partId == partId){
	  if(p->name)
	    free(p->name);
	  p->name = (char*)malloc(strlen(tmp)+1);
	  sprintf(p->name, "%s", tmp);
	  found++;
	break;
	} 
	p = p->next;
      }
    
    /* Bad argument count, or invalid partition id*/
    if(argCount != 1 || !goodArgs){
    sprintf(tmp,"Usage: partname id name");
    }
    else if(!found){
      sprintf(tmp,"partition id %d is invalid",partId);
    } 
    /* ArgCount == 1 && found */
    strcat(Responses[ffscLocal.rackid]->Text,tmp); 
    
    Responses[ffscLocal.rackid]->State = RS_DONE;
    
    /* Mutex exit */
    unlock_part_info();
    
    return PFC_DONE;
  }
  return PFC_FORWARD;
}






