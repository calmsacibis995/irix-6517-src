/*
 * Copyright 1992-1999 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include "ui.h"

int verify_action_uid(char *actuname)
{ struct passwd *ptr;
  uid_t act_uid = 0;
  setpwent();
  while ((ptr = getpwent()) != NULL)
  { if (strcasecmp(actuname, ptr->pw_name) == 0)
    { if (act_uid == ptr->pw_uid)
      { endpwent(); return 0;}
      endpwent();
      return 1;
    }
  }
  endpwent();
  return 2;
}


/* This function reads from the database and brings up the different parameters
   for the select action
*/

static const char szErrMsgEventMonDead[] = "Event monitoring daemon is not running\n";

void action_parameters(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence;
        const char **vector;
	char common_string[2];
        int action_id,key,multiselect,common_int;

	create_help(hError,"setup_actions_update");
	if(!get_variable(hError,cmdp,INTTYPE,"multiselect",common_string,&multiselect,100))
		return;
	if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&action_id,101))
		return;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select action_id,timeoutval,dsmthrottle,dsmfrequency,action,retrycount,userstring,action_desc "
		"from event_action where action_id = '%d'",action_id)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		Body("<b>There are no actions in the database.</b>\n");
		ssdbFreeRequest(error_handle,req_handle);
		return ;
	}

	/* Create the java pieces here */
	Body("<HTML>\n");
	Body("<HEAD>	\n");
	Body("<TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");

	if (!(read_java_scritps("##ACTION_PARAM","##ACTION_PARAM_END")))
	{
		ssdbFreeRequest(error_handle,req_handle);
		sscError(hError,"Error reading java script for this operation\n");
		return ;
	}

	Body("</HEAD>\n");
	Body("<body bgcolor=\"#ffffcc\">\n");
	Body("<a href=\"/$sss/$nocache\"></a>\n");
	Body("<form onSubmit=\"return verifyData(this)\" method=POST name =action_updt action=\"/$sss/rg/libsemserver~UPDTACTCONFIRM\">\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
		RowBegin("");
			CellBegin("bgcolor=\"#cccc99\" width=15");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("bgcolor=\"#cccc99\"");
				Body("SETUP &gt; Actions &gt; Update\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("colspan=2");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("align=right colspan=2");
				Body("<input TYPE=button onClick=\"showMap()\" Value=\"   Help   \">	\n");
			CellEnd();
		RowEnd();
		RowBegin("");
                        CellBegin("colspan=2 ");
                                Body("&nbsp;        \n");
                        CellEnd();
                RowEnd();
	RowBegin("");
		CellBegin("");
                	Body("&nbsp;\n");
                CellEnd();
		CellBegin("");
	vector = ssdbGetRow(error_handle,req_handle);
	TableBegin("border=0 cellpadding=0 cellspacing=0");
		RowBegin("valign=top");
			CellBegin("");
				Body("Action description:\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;\n");
                        CellEnd();
			CellBegin("colspan=2");
				FormatedBody("%s",vector[7]);
			CellEnd();
                RowEnd();
		RowBegin("valign=top");
			CellBegin("");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("Actual action command string:\n");
                        CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
                                FormatedBody("<INPUT TYPE=TEXT NAME=act_str size=30 value = \"%s\">",vector[4]);
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("&nbsp;&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("valign=top");
			CellBegin("");
				Body("Enter a username to execute the action:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
				FormatedBody("<INPUT TYPE=TEXT NAME=user_name size=10 value = %s>",vector[6]);
			CellEnd();
		RowEnd();
		RowBegin("valign=top");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("valign=top");
			CellBegin("");
				Body("Enter action timeout (in multiples of 5)\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
				FormatedBody("<INPUT TYPE=TEXT NAME=action_timeout size=10 value =%d> seconds",atoi(vector[1]));
			CellEnd();
		RowEnd();
		RowBegin("valign=top");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("valign=top");
			CellBegin("");
				Body("Enter the number of times that the event must be registered before an action will be taken:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
				FormatedBody("<INPUT TYPE=TEXT NAME=dsmthrottle size=10 value =%d>",atoi(vector[2]));
			CellEnd();
		RowEnd();
	RowBegin("valign=top");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
	RowBegin("valign=top");
		CellBegin("");
			Body("Enter the number of retry times (up to 23; more than 4 not recommended):\n");
		CellEnd();
		CellBegin("");
			Body("&nbsp;&nbsp;\n");
		CellEnd();
		CellBegin("");
			FormatedBody("<INPUT TYPE=TEXT NAME=retryt size=10 value =%d>",atoi(vector[5]));
		CellEnd();
	RowEnd();
	ssdbFreeRequest(error_handle,req_handle);
	TableEnd();
	Body("<p><INPUT TYPE=\"SUBMIT\" VALUE=\"   Accept   \">&nbsp;&nbsp;&nbsp;<input type=\"BUTTON\" value=\"   Clear   \" onClick=\"clearForm(this.form)\">\n");
	FormatedBody("<input type=\"hidden\" name =\"actionid\" value = %d>",action_id);
	FormatedBody("<input type=\"hidden\" name =\"multiselect\" value = 1>");
	CellEnd();
	RowEnd();
	TableEnd();
	Body("</FORM>\n");
	Body("</BODY>\n");
	Body("</HTML>\n");
}

/* This function is dual purpose. With multiselect 1, the function is used for
   update and with multiselect 0, the function is used to add a new action 
*/ 

void update_action_params(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
	const char **vector;
	char user_name[32],action_description[80],action_name[255],sys_id[32],common_string[2],act_str[255];
	int number_of_records,actionid, key,action_timeout,dsmthrottle,dsmfrequency,retryt,multiselect,common_int,last_id;

	if(!get_variable(hError,cmdp,INTTYPE,"multiselect",common_string,&multiselect,102))
                return;
	if (multiselect)
		if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&actionid,103))
                return;

	if(!get_variable(hError,cmdp,INTTYPE,"action_timeout",common_string,&action_timeout,104))
                return;

	if(!get_variable(hError,cmdp,INTTYPE,"dsmthrottle",common_string,&dsmthrottle,105))
                return;

	/*if(!get_variable(hError,cmdp,INTTYPE,"dsmfrequency",common_string,&dsmfrequency,106))
                return; */

	if(!get_variable(hError,cmdp,INTTYPE,"retryt",common_string,&retryt,107))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"user_name",user_name,&common_int,108))
		return;
        else
	{ if(verify_action_uid(user_name) != 1)
	  { Body("<html><head><title>Embedded Support Partner</title></head>\n");
	    Body("<body bgcolor=\"#ffffcc\">\n");
	    Body("<a href=\"/$sss/$nocache\"></a>\n");
            TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                    CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                    CellEnd();
                    CellBegin("bgcolor=\"#cccc99\"");
                        if(multiselect)
                            Body("SETUP &gt; Actions &gt; Update &gt; <b>ERROR</b>\n");
                        else
                            Body("SETUP &gt; Actions &gt; Add &gt; <b>ERROR</b>\n");
                    CellEnd();
                RowEnd();
                RowBegin("");
                    CellBegin("colspan=2");
                        Body("&nbsp;<p>&nbsp;\n");
                    CellEnd();
                RowEnd();
                RowBegin("");
                    CellBegin("");
                        Body("&nbsp;\n");
                    CellEnd();
                    CellBegin("");
                        if(verify_action_uid(user_name) == 0)
			    Body("<b>Username is invalid. Action cannot be executed by root.</b>\n\n");
          		if(verify_action_uid(user_name) == 2)
			    FormatedBody("<b>Username \"%s\" is not valid.</b>\n",user_name);
		    CellEnd();
		RowEnd();
	    TableEnd();
            Body("</BODY></HTML>\n");
	    return;
          }
	}

	if (!multiselect)
	{
		if(!get_variable(hError,cmdp,CHARTYPE,"action_description",action_description,&common_int,110))
			return;
		if(!get_variable(hError,cmdp,CHARTYPE,"action_name",action_name,&common_int,111))
                        return;
		if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,112))   
                        return; 
	}
	
	/* lock the table */

	if(!(ssdbLockTable(error_handle,connection,"event_action")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }


	/* Update fucntion here. Get additional parameter for update of actions */
	if (multiselect)
	{
		if(!get_variable(hError,cmdp,CHARTYPE,"act_str",act_str,&common_int,109))
			return;

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update event_action set dsmthrottle=%d,retrycount=%d,timeoutval=%d,userstring=\"%s\",action=\"%s\""
		" where action_id = %d",dsmthrottle,retryt,action_timeout,user_name,act_str,actionid)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
	}
	/* Insert a new action */
	else
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
		"insert into event_action values ('%s',NULL,'',%d,0,\"%s\",%d,%d,\"%s\",\"%s\",0,0)",
		sys_id,dsmthrottle,action_description,retryt,action_timeout,user_name,action_name)))
		{
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
        }
	
	ssdbFreeRequest(error_handle,req_handle);
	if (!ssdbUnLockTable(error_handle,connection))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}

	/* Let the event manager know that there was a configuration change */

	if(configure_rule("UPDATE -1"))
	{
		ssdbFreeRequest(error_handle,req_handle);
		sscError(hError, (char*) szErrMsgEventMonDead );
		return;
	}

	/* Select the new values from the db for the confirmation screen.
	   In the case of insertion of new action, get the last inserted
	   id() as the event_action id's are automatically generated
	*/
	if (multiselect)
        {
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select action_desc,action,userstring,timeoutval,dsmthrottle,dsmfrequency,retrycount "
		"from event_action where action_id = %d",actionid)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
	}
	else
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select last_insert_id()")))
		{
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		
		if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
		{
			Body("<b>Last inserted ID read failed</b>\n");
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			ssdbFreeRequest(error_handle,req_handle);
			return ;
		}

		vector = ssdbGetRow(error_handle,req_handle);
		if(vector)
		{
			last_id = atoi(vector[0]);
			ssdbFreeRequest(error_handle,req_handle);
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select action_desc,action,userstring,timeoutval,dsmthrottle,dsmfrequency,retrycount "
			"from event_action where action_id = %d",last_id)))
			{
				sscError(hError,"Database API Error: \"%s\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
		}
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                Body("<b>There are no actions in the database.</b>\n");
                ssdbFreeRequest(error_handle,req_handle);
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return ;
        }
	else
	{
	Body("<HTML>\n");
        Body("<HEAD>\n");
        Body("<TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	 Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
        Body("<form onSubmit=\"return verifyData(this)\" method=POST name =action_updt action=\"\">\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
		RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
				if(multiselect)
					Body("SETUP &gt; Actions &gt; Update\n");
				else
					Body("SETUP &gt; Actions &gt; Add\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;<p>&nbsp;\n");
                        CellEnd();
                RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("&nbsp;\n");
                CellEnd();
                CellBegin("");
	vector = ssdbGetRow(error_handle,req_handle);
        TableBegin("border=0 cellpadding=0 cellspacing=0");
                RowBegin("valign=top");
			CellBegin("");
				Body("Action description:\n");
			CellEnd();
			CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
			CellBegin("colspan=2");
				FormatedBody("%s",vector[0]);
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("valign=top");
                        CellBegin("");
                                Body("Action command string\n");
                        CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("colspan=2");
                                FormatedBody("%s",vector[1]);
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("A username to execute the action\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("colspan=2");
                                FormatedBody("%s",vector[2]);
			CellEnd();
                RowEnd();
		RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("Action timeout\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
                        CellBegin("colspan=2");
                                FormatedBody("%d seconds",atoi(vector[3]));
			CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("Number of times the event must be registered before an action will be taken\n");
			CellEnd();
			CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("colspan=2");
                                FormatedBody("%d",atoi(vector[4]));
                        CellEnd();
		RowEnd();
		/*@row
                        @cell
                                @ &nbsp;
                        @endcell
                @endrow
		@row
                        @cell
                                @ Delay time between the event and an action
			@endcell
                        @cell   
                                @ &nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;
			@endcell        
                        @cell colspan=2 
                                @format "%d seconds" atoi(vector[5])
			@endcell
		@endrow	*/
		RowBegin("");
                        CellBegin("");
                                Body("&nbsp;        \n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("Retry times\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
                        CellBegin("colspan=2         ");
                                FormatedBody("%d",atoi(vector[6]));
			CellEnd();
                RowEnd();
	ssdbFreeRequest(error_handle,req_handle);	
	TableEnd();
	CellEnd();
	RowEnd();
        TableEnd();
        Body("</FORM>\n");
        Body("</BODY>\n");
        Body("</HTML>\n");
	}
}

void delete_action(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
	int number_of_records,key,deletebutton,action_id,common_int;
	char actionbuffer[80], sys_id[32],common_string[2];

	if (!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&action_id,113))
		return;
 	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,114))
		return; 
	if(!get_variable(hError,cmdp,CHARTYPE,"action_desc",actionbuffer,&common_int,115))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"proceeddel",common_string,&deletebutton,116))
                return;

	if (deletebutton)
	{
		if (!(ssdbLockTable(error_handle,connection,"actions_taken,event_action,event_actionref")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
			"delete from event_actionref where action_id = %d and sys_id = '%s'",action_id,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                        "delete from actions_taken where action_id = %d and sys_id = '%s'",action_id,sys_id)))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
			"delete from event_action where action_id = %d and sys_id = '%s'",action_id,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);

		if (!ssdbUnLockTable(error_handle,connection))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}

		if(configure_rule("UPDATE -1"))
		{
			sscError(hError, (char*) szErrMsgEventMonDead );
			return;
		}
	}
	Body("<HTML>\n");
        Body("<HEAD>\n");
        Body("<TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
	Body("<a href=\"/$sss/$nocache\"></a>\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                Body("SETUP &gt; Actions &gt; Delete\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
			CellEnd();
			CellBegin("");
			if (deletebutton)
			{
				Body("The following action has been deleted from the SGI Embedded Support Partner database:\n");
				FormatedBody("<ul><li>%s</ul>",actionbuffer);
			}
			else
				Body("Request for deletion cancelled.\n");
			CellEnd();
		RowEnd();
	TableEnd();
	Body(" </form> </body> </html>\n");
}

int deletelistset(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int key,number_of_records,rec_sequence,action_id,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
        const char **vector;
	char actionbuffer[80],sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,117))
                return 0;
	if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&action_id,118))
		return 0;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,119))
		return 0;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select action_desc from event_action where action_id = %d and sys_id = '%s'",action_id,sys_id)))
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return 0;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		Body("There are no records for this operation\n");
		ssdbFreeRequest(error_handle,req_handle);
                return (number_of_records);
	}
	

	vector = ssdbGetRow(error_handle,req_handle);
	if (vector)
		strcpy (actionbuffer, vector[0]);
	else 
		strcpy (actionbuffer,"Unknown");
	ssdbFreeRequest(error_handle,req_handle);
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select DISTINCT event_type.type_desc from event_type,event_actionref "
		"where event_actionref.action_id = %d "
		"and event_actionref.type_id = event_type.type_id "
		"and event_actionref.sys_id = '%s' "
		"order by event_type.type_id",action_id,sys_id))) 
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return 0;
        }

	
	number_of_records = getnumrecords(hError,error_handle,req_handle);
        create_help (hError,"setup_actions_delete");
	
	Body("<HTML>\n");
        Body("<HEAD>\n");
        Body("<TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");

        if (!(read_java_scritps("##DELETELISTSET","##DELETELISTSET_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
		ssdbFreeRequest(error_handle,req_handle);
                return 0;
        }

        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<form method=POST name =action_updt action=\"/$sss/rg/libsemserver~DELETEACTION\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                Body("SETUP &gt; Actions &gt; Delete\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("align=right colspan=2");
                                Body("<input TYPE=button onClick=\"showMap()\" Value=\"   Help   \">\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("&nbsp;\n");
                CellEnd();

		CellBegin("");
			if (number_of_records)	
			{

				FormatedBody("The following events will be affected as a result of <b>%s</b> action deletion:",actionbuffer);
				Body("<p>\n");
				TableBegin("BORDER=0 CELLSPACING = 0 CELLPADDING=0");
				RowBegin("");
					CellBegin("align=right");
					total_pages = number_of_records/10;
					if (number_of_records%10)
						total_pages++;
					pageno = row_num/10;
					pageno++;
					FormatedBody("Page %d of %d",pageno,total_pages);
				CellEnd();
				RowEnd();
				RowBegin("");
				CellBegin("");
				TableBegin("border=4 cellpadding=6 cellspacing=1");
				RowBegin("align=center");
					CellBegin("");
						Body("<b>No</b>\n");
					CellEnd();
					CellBegin("");
						Body("<b>Event Description</b>\n");
					CellEnd();
				RowEnd();
					balance_rows = row_num + 10;
					if (balance_rows > number_of_records)
						balance_rows = number_of_records;
					ssdbRequestSeek(error_handle,req_handle,row_num,0);
					for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
					{
						RowBegin("");
							vector = ssdbGetRow(error_handle,req_handle);
							if (vector)
							{
								CellBegin("");
									FormatedBody("%d",rec_sequence);
								CellEnd();
								CellBegin("");
									FormatedBody("%s",vector[0]);
								CellEnd();
							}
						RowEnd();
					}
					TableEnd();
					CellEnd();
					RowEnd();
					RowBegin("");
					CellBegin("");
						Body("&nbsp;\n");
					CellEnd();
					RowEnd();
					if(total_pages > 1)
					{
						RowBegin("");
						CellBegin("align=center");
							firstpage = ((pageno-1)/10)*10+1;
							lastpage  = firstpage+9;
							if (lastpage >= total_pages)
								lastpage = total_pages;
							if (firstpage > 1)
							{
								 FormatedBody("<a href=\"/$sss/rg/libsemserver~deletelistset?row_num=%d&sys_id=%s&actionid=%d\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id,action_id);
								 FormatedBody("<a href=\"/$sss/rg/libsemserver~deletelistset?row_num=%d&sys_id=%s&actionid=%d\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,action_id);
							}

							for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
							{
								if (rec_sequence == pageno)
									FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
								else
									 FormatedBody("<a href=\"/$sss/rg/libsemserver~deletelistset?row_num=%d&sys_id=%s&actionid=%d\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,action_id,rec_sequence);
							}
							if (lastpage < total_pages)
							{
								if(number_of_records%10 == 0)
									bottom = number_of_records -10;
								else
									bottom = number_of_records -(number_of_records%10);
								 FormatedBody("<a href=\"/$sss/rg/libsemserver~deletelistset?row_num=%d&sys_id=%s&actionid=%d\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,action_id);
								 FormatedBody("<a href=\"/$sss/rg/libsemserver~deletelistset?row_num=%d&sys_id=%s&actionid=%d\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id,action_id);
							}
						CellEnd();
						RowEnd();
					}

				TableEnd();
				Body("<p>Would you like to proceed with the deletion?\n");
			}
			else
			{	
				FormatedBody("There are no events for the action <b>%s</b>. Would you like to proceed with the deletion?",actionbuffer);
			}
			Body("<p>\n");
			Body("<input type=\"SUBMIT\"  value=\" Proceed with deletion \" onClick=\"proceed(this)\">\n");
			Body("&nbsp;&nbsp;&nbsp;\n");
			Body("<input type=\"SUBMIT\" value=\"   Stop deletion   \" onClick=\"canceldel(this)\">\n");
			FormatedBody("<input type=\"hidden\" name= action_desc value =\"%s\">",actionbuffer);
			FormatedBody("<input type=\"hidden\" name= actionid value = %d>",action_id);
			FormatedBody("<input type=\"hidden\" name= sys_id value =\"%s\">",sys_id);
			Body("<input type=\"hidden\" name= proceeddel value = \"\"> \n");
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</form> </body> </html>	\n");
	ssdbFreeRequest(error_handle,req_handle);
	return 1;
}

void view_action_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int key,number_of_records,rec_sequence,multiselect,action_select,common_int,pageno;
        const char **vector;
        char actionbuffer[80],sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,120))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"action_select",common_string,&action_select,121))
		return;

	if(!action_select)
		create_help(hError,"screens_action_setup");

	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	if(!action_select)
	{
		if (!(read_java_scritps("##ACTIONSETUP","##ACTIONSETUP_END")))
		{
			sscError(hError,"Error reading java script for this operation\n");
			return ;
		}
	}
	Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        if (!action_select)
                Body("<form onSubmit=\"return verify_submitvals()\" method=POST name=Action action=\"/$sss/rg/libsemserver~ACTION_REPORT_BY_ACTION\">\n");
	else
		Body("<form onSubmit=\"return verify_submitvals()\" method=POST action=\"/$sss/rg/libsemserver~ACTION_LIST\">\n");
	
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
				if (!action_select)
					Body("SETUP &gt; Actions &gt; View Current Setup &gt; View Action Setup \n");
				else
					Body("SETUP &gt; Actions &gt; View Current Setup &gt; View Available Actions List\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		if(!action_select)
		{
			RowBegin("");
				CellBegin("align=right colspan=2");
					Body("<input TYPE=button onClick=\"showMap()\" Value=\"   Help   \">\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("colspan=2");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("&nbsp;<p>&nbsp;\n");
                        CellEnd();
                        CellBegin("");
                                Body("Choose an action whose description you want to view:<p>\n");
				create_action_list(hError,session,connection,error_handle,cmdp);
			CellEnd();
                RowEnd();
		}
		else
		{
			RowBegin("");
                        CellBegin("");
                                Body("&nbsp;<p>&nbsp;\n");
                        CellEnd();
                        CellBegin("");
				view_action_list(hError,session,connection,error_handle,cmdp);
				FormatedBody("<input TYPE=\"hidden\" name=sys_id value=\"%s\">",sys_id);
			CellEnd();
			RowEnd();
		}
        TableEnd();
        Body("</form></body></html>\n");
}
void action_report_by_action(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int key,number_of_records,rec_sequence,multiselect,actionid,common_int;
        const char **vector;
	char common_string[2];

	if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&actionid,122))
		return;

        Body("<HTML><HEAD><TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></head>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Actions &gt; View Current Setup &gt; View Action Setup\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;<p>&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("&nbsp;\n");
                CellEnd();
                CellBegin("");
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select action_desc,action,userstring,timeoutval,dsmthrottle,dsmfrequency,retrycount "
                "from event_action where action_id = %d",actionid)))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                Body("<B>There are no actions in the database.</B>\n");
                ssdbFreeRequest(error_handle,req_handle);
                return ;
        }
        else
        {
		TableBegin("border=0 cellpadding=0 cellspacing=0");
		vector = ssdbGetRow(error_handle,req_handle);
		if(vector)
		{
			RowBegin("valign=top");
				CellBegin("");
					Body("Action command string\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("colspan=2");
					FormatedBody("%s",vector[1]);
				CellEnd();
			RowEnd();
			
			RowBegin("");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("");
					Body("Action description:\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("colspan=2");
					FormatedBody("%s",vector[0]);
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("Execute this action as\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("colspan=2");
					FormatedBody("%s",vector[2]);
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("Action timeout\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("colspan=2");
					FormatedBody("%d seconds",atoi(vector[3]));
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("Number of times the event must be registered before an action will be taken\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("colspan=2");
					FormatedBody("%d",atoi(vector[4]));
				CellEnd();
			RowEnd();
			/*@row
				@cell
					@ &nbsp;
				@endcell
			@endrow
			@row
				@cell
					@ Delay time between the event and an action
				@endcell
				@cell
					@ &nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;
				@endcell
				@cell colspan=2
					@format "%d seconds" atoi(vector[5])
				@endcell
			@endrow */
			RowBegin("");
				CellBegin("");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("Retry times\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("colspan=2");
					FormatedBody("%d",atoi(vector[6]));
				CellEnd();
			RowEnd();
		}
		ssdbFreeRequest(error_handle,req_handle);
		TableEnd();
		CellEnd();
		RowEnd();
		TableEnd();
		Body("</FORM></BODY></HTML>\n");
        }
}

void view_action_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	
	ssdb_Request_Handle req_handle;
	int key,number_of_records,rec_sequence,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
        const char **vector;
	char sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,123))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,124))
                return;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select action_desc,action from event_action where private=0 and sys_id = '%s'",sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		Body("<b>There are no actions in the database.</b>\n");
		ssdbFreeRequest(error_handle,req_handle);
		return ;
	}
	else
	{
		TableBegin("BORDER=0 CELLSPACING = 0 CELLPADDING=0");
                RowBegin("");
                        CellBegin("align=right");
                        total_pages = number_of_records/10;
                        if (number_of_records%10)
                                total_pages++;
                        pageno = row_num/10;
                        pageno++;
                        FormatedBody("Page %d of %d",pageno,total_pages);
                CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("");
		TableBegin("BORDER=4 CELLSPACING = 1 CELLPADDING=6");
		RowBegin("ALIGN=CENTER");
				CellBegin("align=right");
					Body("<b>No.</b>\n");
				CellEnd();
				CellBegin("");
					Body("<b>Action Description</b>\n");
				CellEnd();
				CellBegin("");
					Body("<b>Action Command String</b>\n");
				CellEnd();
		RowEnd();
		balance_rows = row_num + 10;
		if (balance_rows > number_of_records)
			balance_rows = number_of_records;
		ssdbRequestSeek(error_handle,req_handle,row_num,0);
		for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
		{
			RowBegin("valign=top");
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
					CellBegin("");
						FormatedBody("%d",rec_sequence+1);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[0]);
					CellEnd();
					CellBegin("");
                                                FormatedBody("%s",vector[1]);
                                        CellEnd();
				}
			RowEnd();
		}
		ssdbFreeRequest(error_handle,req_handle);
		TableEnd();;
	}
	CellEnd();
	RowEnd();
	RowBegin("");
		CellBegin("");
			Body("&nbsp;\n");
		CellEnd();
	RowEnd();
                if(total_pages > 1)
                {
                        RowBegin("");
                        CellBegin("align=center");
                                firstpage = ((pageno-1)/10)*10+1;
                                lastpage  = firstpage+9;
                                if (lastpage >= total_pages)
                                        lastpage = total_pages;
                                if (firstpage > 1)
                                {
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_VIEW_CONT?row_num=%d&sys_id=%s&action_select=1\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_VIEW_CONT?row_num=%d&sys_id=%s&action_select=1\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_VIEW_CONT?row_num=%d&sys_id=%s&action_select=1\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records -10;
					else
						bottom = number_of_records - (number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_VIEW_CONT?row_num=%d&sys_id=%s&action_select=1\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_VIEW_CONT?row_num=%d&sys_id=%s&action_select=1\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id);
                                }
                        CellEnd();
                        RowEnd();
                }
	TableEnd();
}
