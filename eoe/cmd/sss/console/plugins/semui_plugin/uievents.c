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

static const char szErrMsgEventmonDead [] = "Failed response from event monitoring daemon\n";

/* This function will provide the branching towards the three different types of reports that are
 * available for viewing current set up 
 */

void event_view_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int setup_select,key,common_int;
	char sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,300))
		return;
	if(!get_variable(hError,cmdp,INTTYPE,"setup_select",common_string,&setup_select,301))
		return;
	if (!setup_select)	
		event_view_configuration(hError,session,connection,error_handle,cmdp);
	else if (setup_select==1)
		event_description_view(hError,session,connection,error_handle,cmdp);
	else
		event_description_class(hError,session,connection,error_handle,cmdp);	
}

/*

*/
void event_view_configuration(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int key,common_int;
        char sys_id[32];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,302))
                return;
	create_help(hError,"setup_view_event");
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	if (!(read_java_scritps("##EVENT_SETUP","##EVENT_SETUP_END")))
	{
		sscError(hError,"Error reading java script for this operation\n");
		return ;
	}
	Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form onSubmit=\"return verify_submitvals()\" method=POST name =viewEvent action=\"/$sss/rg/libsemserver~EVENT_VIEW_TYPE_PAGE\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                Body("SETUP &gt; Events &gt; View Current Setup &gt; View Event\n");
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
                                Body("Choose a Class:<p>\n");
                                create_event_list(hError,session,connection,error_handle,cmdp);
				FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
				Body("<input type=\"hidden\" name=\"pageselect\" value=0>\n");
                                Body("<INPUT TYPE=\"HIDDEN\" name=\"multiselect\" VALUE=0>\n");
                        CellEnd();
                RowEnd();
        TableEnd();
	Body("</form> </body></html>\n");
}

void event_view_type_configuration(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int key,common_int,pageselect;
	char sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,303))
		return;
	if(!get_variable(hError,cmdp,INTTYPE,"pageselect",common_string,&pageselect,304))
		return;

	if (!pageselect)
		create_help(hError,"setup_view_event");
	if(pageselect==1)
		create_help(hError,"setup_events_update_type");
	if(pageselect==2)
		create_help(hError,"setup_events_update_event_actions_method2");
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	if(!pageselect)
	{
		if (!(read_java_scritps("##EVENT_TYPE_SETUP","##EVENT_TYPE_SETUP_END")))
		{
			sscError(hError,"Error reading java script for this operation\n");
			return ;
		}
	}
	else
        {
                if (!(read_java_scritps("##EVENT_UPDT_SETUP","##EVENT_UPDT_SETUP_END")))
                {
                        sscError(hError,"Error reading java script for this operation\n");
                        return ;
                }
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");

	/* This calls the view event configuration page. Comes from view current event setup */

	if(!pageselect)
	{
		Body("<form onSubmit=\"return verify_submitvals()\" method=POST name =viewEvent action=\"/$sss/rg/libsemserver~EVENT_VIEW_REPORT_PAGE\">\n");
		Body("<input type=\"hidden\" name=\"pageselect\" value=0>\n");
	}

	/* This calls the event confirmation pages from an update (single/multi) */
	if(pageselect==1)
	{
		Body("<form onSubmit=\"return submitType(this)\" method=\"post\" name =ChooseType action=\"/$sss/rg/libsemserver~EVENT_UPDT_INFO\">\n");
		Body("<input type=\"hidden\" name=\"pageselect\" value=\"\">\n");
	}
	if(pageselect==2)
        {
                Body("<form onSubmit=\"return submitType(this)\" method=\"post\" name =ChooseType action=\"/$sss/rg/libsemserver~EVENT_ACTION_AUD\">\n");
		Body("<input type=\"hidden\" name=\"submit_type\" value=1>\n");
		Body("<input type=\"hidden\" name=\"multiselect\" value=1>\n");
		Body("<input type=\"hidden\" name=\"pageselect\" value=\"\">\n");
	}
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
				if(!pageselect)
					Body("SETUP &gt; Events &gt; View Current Setup &gt; View Event\n");
				if(pageselect==1)
					Body("SETUP  &gt; Events &gt; Update	\n");
				if(pageselect==2)
					Body("SETUP  &gt; Events &gt;  Update Event Actions &gt; Add Actions to Events\n");
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
                                Body("&nbsp;<p>&nbsp;\n");
                        CellEnd();
                        CellBegin("");
                                create_type_list(hError,session,connection,error_handle,cmdp);
				FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
                        CellEnd();
                RowEnd();
        TableEnd();
        Body("</form> </body></html>\n");
}

void event_view_report_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        char sys_id[32],common_string[2];
        int i,number_of_records,rec_sequence,event_type,key,common_int,pageselect;
	int class_id,sehthrottle,sehfrequency,enabled;
	const char **vector;
	char buffer[60];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,305))
		return;
	if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&event_type,306))
		return;
	if(!get_variable(hError,cmdp,INTTYPE,"pageselect",common_string,&pageselect,307))
		return;

        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
			if(!pageselect)
				Body("SETUP &gt; Events  &gt; View Current Setup &gt; View Event\n");
			if(pageselect > 0)
				Body("SETUP &gt; Events  &gt; Update\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;<p>&nbsp;\n");
                CellEnd();
        RowEnd();
	if (pageselect > 0)
	{
		RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
			CellEnd();
			CellBegin("");
				Body("The following event(s) were updated:	\n");
			CellEnd();
		RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("&nbsp;\n");
                        CellEnd();
                        CellBegin("");
				Body("&nbsp;<hr>&nbsp;\n");
			CellEnd();
		RowEnd();
	}
        RowBegin("");
                CellBegin("");
                        Body("&nbsp;\n");
                CellEnd();
                CellBegin("");
		TableBegin("BORDER=0 CELLSPACING = 0 CELLPADDING=0");

	for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
	{
		if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
		{
			sscError(hError,"Invalid event type count parameter\n");
			return;
		}
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select event_class.class_desc,event_type.type_desc,event_type.sehthrottle,"
		"event_type.sehfrequency,event_type.enabled,event_class.class_id from event_class,event_type "
		"where event_type.type_id = %d "
		"and event_class.class_id = event_type.class_id "
		"and event_type.sys_id = '%s' and event_class.sys_id = '%s'",event_type,sys_id,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                {
                        TableEnd();
                        Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                        CellEnd();
                        RowEnd();
                        TableEnd();
                        Body("</body></html>\n");
                        ssdbFreeRequest(error_handle,req_handle);
                        return;
                }
		else
		{
			vector = ssdbGetRow(error_handle,req_handle);
			if(vector)
			{
				class_id = atoi(vector[5]);
				sehthrottle = atoi(vector[2]);
				sehfrequency = atoi(vector[3]);
				enabled = atoi(vector[4]);
				RowBegin("valign=top");
					CellBegin("");
						Body("Event Class\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("%d, %s",class_id,vector[0]);
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("");
						Body("Event \n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("%d, %s",event_type,vector[1]);
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("");
						Body("&nbsp;        \n");
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
                                        CellBegin("");
                                                Body("Event registration\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
						if (atoi(vector[4]))
							Body("Enabled\n");
						else
							Body("Disabled\n");
					CellEnd();
				RowEnd();
                                RowBegin("valign=top");
                                        CellBegin("");
                                                Body("&nbsp;\n");
                                        CellEnd();
                                RowEnd();
				RowBegin("valign=top");
					CellBegin("");
						Body("Number of events that must occur before registration begins\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[2]);
					CellEnd();
				RowEnd();
/*				@row valign=top
					@cell   
						@ &nbsp;        
					@endcell        
				@endrow
				@row valign=top
					@cell
						@ Time delay between the event and its registration
					@endcell
					@cell
						@ &nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;
					@endcell
					@cell
						@format "%s" vector[3]
					@endcell
				@endrow */
				RowBegin("valign=top");
					CellBegin("");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
			RowBegin("valign=top");
                                CellBegin("");
                                        Body("Actions for this event\n");
				CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                CellEnd();
                                CellBegin("");
				ssdbFreeRequest(error_handle,req_handle);
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select event_action.action_desc,event_actionref.action_id from event_action,event_actionref "
				"where event_action.action_id = event_actionref.action_id "
				"and event_actionref.type_id = %d "
				"and event_actionref.sys_id = '%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					Body("None\n");
					ssdbFreeRequest(error_handle,req_handle);
				}
				else
				{
					for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
					{
						vector = ssdbGetRow(error_handle,req_handle);
						if(vector)
							FormatedBody("%s<br>",vector[0]);
					}
					ssdbFreeRequest(error_handle,req_handle);
				}	
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("colspan=3");
					Body("&nbsp;<hr>&nbsp;\n");
				CellEnd();
			RowEnd();
			}
		}
	}
	TableEnd();;
	CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}

void event_description_view(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence,common_int,key,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
	const char **vector;
	char sys_id[32],common_string[2];
	
	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,308))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,309))
                return;

	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; View Current Setup &gt; View Event List\n");
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
		"select event_class.class_desc,event_type.type_desc from event_class,event_type "
		"where event_type.class_id = event_class.class_id "
		"and event_type.sys_id = '%s' and event_class.sys_id = '%s' order by event_class.class_id",sys_id,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}

		if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
		{
			TableEnd();
			Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
			CellEnd();
			RowEnd();
			TableEnd();
			Body("</body></html>\n");
			ssdbFreeRequest(error_handle,req_handle);
			return;
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
						Body("<b>Class Description</b>\n");
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
				RowBegin("valign=top");
					vector = ssdbGetRow(error_handle,req_handle);
					if (vector)
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
			CellEnd();
		}
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
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#cc6633\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_PAGE_CONT?row_num=%d&sys_id=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records -10;
					else
						bottom = number_of_records -(number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id);
                                }
                        CellEnd();
                        RowEnd();
                }
		TableEnd();
	CellEnd();
	RowEnd();
        TableEnd();
	Body("</form></body></html>\n");
}

void event_description_class(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence,row_num,balance_rows,total_pages,pageno,common_int,firstpage,lastpage,bottom;
        const char **vector;
	char sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,310))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,311))
                return;
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; View Current Setup &gt; View Classes\n");
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
                "select event_class.class_desc,class_id from event_class "
		" where event_class.sys_id = '%s' order by event_class.class_id",sys_id)))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }

		if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                {
                        TableEnd();
                        Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                        CellEnd();
                        RowEnd();
                        TableEnd();
                        Body("</body></html>\n");
                        ssdbFreeRequest(error_handle,req_handle);
                        return;
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
                                                Body("<b>Class ID</b>\n");
                                        CellEnd();
					CellBegin("");
                                                Body("<b>Class Description</b>\n");
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
							FormatedBody("%s",vector[1]);
						CellEnd();
						CellBegin("");
							FormatedBody("%s",vector[0]);
						CellEnd();
					}
                                RowEnd();
                        }
			ssdbFreeRequest(error_handle,req_handle);
                        TableEnd();;
			CellEnd();
                }
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
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_CLASS_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_CLASS_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#cc6633\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_CLASS_PAGE_CONT?row_num=%d&sys_id=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records -10;
					else
						bottom = number_of_records - (number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_CLASS_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_VIEW_CLASS_PAGE_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id);
                                }
                        CellEnd();
                        RowEnd();
                }
                TableEnd();
	CellEnd();
        RowEnd();
	TableEnd();
        Body("</form></body></html>\n");
}

void event_add_info(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence,ev_class,cust_typeid,key,common_int,class_flag,class_id,pageselect;
        const char **vector;
	char sys_id[32],custom_class_description[80],custom_type_description[80],common_string[2];

	class_flag=0;
	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,312))
                return;

	if ((key = sscFindPairByKey(cmdp,0,"ev_class")) < 0)
	{
		if(!get_variable(hError,cmdp,CHARTYPE,"custom_class_description",custom_class_description,&common_int,313))
                        return ;
	}
	else
	{	
		class_flag = 1;
		if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,314))
			return ;
	}
	
	if(!get_variable(hError,cmdp,CHARTYPE,"custom_type_description",custom_type_description,&common_int,315))
		return;

	create_help(hError,"setup_events_add");
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");

	/* Read the javascript out here */
	if (!(read_java_scritps("##EVENT_ADD_INFO","##EVENT_ADD_INFO_END")))
	{
		sscError(hError,"Error reading java script for this operation\n");
		return ;
	}	

        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<a href=\"/$sss/$nocache\"></a>\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver~EVENT_ADD_CONFIRM\" onSubmit=\"return verifyData(this)\" name=\"eventadd\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Add\n");
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
	
	if (class_flag)
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select event_class.class_desc from event_class "
		"where class_id = %d and sys_id = '%s'",ev_class,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
		{
			Body("<b>Failed to get event class description.</b>\n");
			ssdbFreeRequest(error_handle,req_handle);
			return ;
		}

	}

		TableBegin("border=0 cellpadding=0 cellspacing=0");
		RowBegin("");
			CellBegin("");
				Body("Class\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
				if(class_flag)
				{
					vector = ssdbGetRow(error_handle,req_handle);
					if (vector)
						FormatedBody("%d,  %s",ev_class,vector[0]);
					ssdbFreeRequest(error_handle,req_handle);
				}
				else
					FormatedBody("%s",custom_class_description);
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
                                Body("&nbsp;\n");
			CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("Custom event\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
				FormatedBody("%s",custom_type_description);
			CellEnd();
		RowEnd();
		TableEnd();
		CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;<hr> &nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
                        CellEnd();
			CellBegin("");
				TableBegin("border=0 cellpadding=0 cellspacing=0");
				RowBegin("valign=top");
					CellBegin("");
						Body("Choose if you want to register the event in the SGI Embedded Support Partner:\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
						Body("<INPUT TYPE=RADIO NAME=enable_event value=1 checked>\n");
					CellEnd();
					CellBegin("");
                                                Body("&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                Body("Register\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                Body("<INPUT TYPE=RADIO NAME=enable_event value=0>\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                Body("Do not register	\n");
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("colspan=9");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("");
						Body("Enter the number of events that must occur before registration begins:\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("colspan=7");
						Body("<INPUT TYPE=TEXT NAME=thcount size=10 value=1>\n");
					CellEnd();
                		RowEnd();
		/*		@row valign=top
                                        @cell colspan=9
                                                @ &nbsp;
                                        @endcell
                                @endrow
				@row valign=top
                                        @cell
                                                @ Enter the desired time delay (in multiples of 5 seconds) between the event and its registration:
					@endcell
                                        @cell
                                                @ &nbsp;&nbsp;
                                        @endcell
                                        @cell colspan=7
                                                @ <INPUT TYPE=TEXT NAME=thfreq size=10 value=0>
                                        @endcell
                                @endrow */
				RowBegin("valign=top");
                                        CellBegin("colspan=9");
                                                Body("&nbsp;\n");
                                        CellEnd();
                                RowEnd();
				RowBegin("valign=top");
                                        CellBegin("colspan=9");
                                                Body("Choose the action(s) that will be taken as a result of this event:\n");
					CellEnd();
                                RowEnd();
                                RowBegin("valign=top");
                                        CellBegin("colspan=9");
                                                Body("&nbsp;\n");
                                        CellEnd();
                                RowEnd();
                                RowBegin("valign=top");
                                        CellBegin("colspan=9");
						if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
						"select action_id,action_desc from event_action where private = 0 order by action_desc")))
						{
							sscError(hError,"Database API Error: \"%s\n\n",ssdbGetLastErrorString(error_handle));
							return;
						}

						if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
						{
							Body("<b>There are no actions in the database.</b>\n");
							ssdbFreeRequest(error_handle,req_handle);
							Body("<input type=\"hidden\" name=\"act_id\" value=\"absent\">\n");
						}
						else
						{
							Body("<select name=actionid multiple size=5>\n");
							for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
							{
								vector = ssdbGetRow(error_handle,req_handle);
								if (vector)
									FormatedBody("<option value = %d > %s",atoi(vector[0]),vector[1]);
							}
							Body("</select>\n");
							Body("<input type=\"hidden\" name=\"act_id\" value=\"present\">\n");
							ssdbFreeRequest(error_handle,req_handle);
						}
					CellEnd();
				RowEnd();
				TableEnd();
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
				TableBegin("border=0 cellpadding=0 cellspacing=0");
				RowBegin("valign=top");
					CellBegin("");
						Body("<font color=\"#666633\"><b>Tip: </b></font>\n");
					CellEnd();
					CellBegin("");
						Body("Several actions can be selected.<br>If you cannot find an action that you need in the list above, add it by using <a href=\"/action_add.html\">SETUP: Actions: Add</a>.\n");
					CellEnd();
				RowEnd();
				TableEnd();
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
				Body("<INPUT TYPE=\"SUBMIT\"  VALUE=\"   Accept   \">&nbsp;&nbsp;&nbsp;<input type=\"BUTTON\" value=\"   Clear   \" onClick=\"clearForm(this.form)\">\n");
				FormatedBody("<input type=\"hidden\" name=sys_id value=%s>",sys_id);
				if(!class_flag)
					FormatedBody("<input type=\"hidden\" name=custom_class_description value=\"%s\">",custom_class_description);
				else
					FormatedBody("<input type=\"hidden\" name=ev_class value=%d>",ev_class);
				FormatedBody("<input type=\"hidden\" name=custom_type_description value=\"%s\">",custom_type_description);
			CellEnd();
		RowEnd();
		TableEnd();
	CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}

void event_updt_info(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int i,number_of_records,rec_sequence,event_type,key,common_int,pageselect,ev_class;
        const char **vector;
	char sys_id[32],common_string[2];
	int number_of_actions[MAX_ACTIONS],total_actions,flag,iterator;

	if(!get_variable(hError,cmdp,INTTYPE,"pageselect",common_string,&pageselect,316))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,317))
                return;

	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");

	if (pageselect == 1)
	{
		if (!(read_java_scritps("##EVENT_UPDT_SINGLE","##EVENT_UPDT_SINGLE_END")))
		{
			sscError(hError,"Error reading java script for this operation\n");
			return ;
		}
		create_help(hError,"screens_setup_events_update1");
	}

	if (pageselect == 2)
	{
		if (!(read_java_scritps("##EVENT_UPDT_MULTI","##EVENT_UPDT_MULTI_END")))
                {
                        sscError(hError,"Error reading java script for this operation\n");
                        return ;
                }
		create_help(hError,"screens_setup_events_update2");
	}

        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<a href=\"/$sss/$nocache\"></a>\n");
        Body("<form onSubmit=\"return verifyData(this)\" method=POST action=\"/$sss/rg/libsemserver~EVENT_UPDT_CONFIRM\" name=\"updtEvent\">\n");
	if(pageselect==1)
	{
		if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&event_type,318))
			return;
		else
			FormatedBody("<input type=\"hidden\" name=event_type value=%d>",event_type);
	}
	if(pageselect==2)
	{
		for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
                {
                        if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
                        {
                                sscError(hError,"Invalid event type count parameter\n");
                                return;
                        }
			else
				FormatedBody("<input type=\"hidden\" name=event_type value=%d>",event_type);
		}		
	}
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Update\n");
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

	if(pageselect==1)
	{
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select event_type.type_desc,event_type.enabled,event_type.sehthrottle,"
		"event_type.sehfrequency,event_class.class_desc,event_type.class_id from event_type,event_class "
                "where event_type.type_id = %d "
		"and event_class.class_id = event_type.class_id "
                "and event_type.sys_id = '%s' and event_class.sys_id = '%s'",event_type,sys_id,sys_id)))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                }
                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                {
                        Body("Record addition was not successful.\n");
                        ssdbFreeRequest(error_handle,req_handle);
                        CellEnd();
                        RowEnd();
                        TableEnd();
                        Body("</form></body></html>\n");
                        return;
                }

		vector = ssdbGetRow(error_handle,req_handle);
		if(vector)
		{
		ev_class = atoi(vector[5]);
		TableBegin("border=0 cellpadding=0 cellspacing=0");
			RowBegin("");
                                CellBegin("");
                                        Body("Event class\n");
				CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        FormatedBody("%s",vector[4]);
				CellEnd();
                        RowEnd();
			RowBegin("");
                                CellBegin("");
                                        Body("&nbsp;\n");
				CellEnd();
                        RowEnd();
			RowBegin("");
				CellBegin("");
					Body("Event\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("");
					FormatedBody("%s",vector[0]);
				CellEnd();
			RowEnd();
		TableEnd();
		}
	}
		if(pageselect==2)
		{
			TableBegin("border=0 cellpadding=0 cellspacing=0");
				RowBegin("valign=top");
                                CellBegin("");
                                        Body("<font color=\"#cc6633\"><b>Warning: </b></font>\n");
				CellEnd();
                                CellBegin("");
                                        Body("You are changing multiple events!<br>Any change that you make to a field will affect all selected events.\n");
				CellEnd();
				RowEnd();
			TableEnd();
		}
		CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;<hr> &nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
                        CellEnd();
			CellBegin("");
				TableBegin("border=0 cellpadding=0 cellspacing=0");
				RowBegin("valign=top");
					CellBegin("");
						Body("Choose if you want to register the event in the SGI Embedded Support Partner:\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
					if(pageselect==1)
					{
						if (vector)
						{
							if(atoi(vector[1]))
								Body("<INPUT TYPE=RADIO NAME=enable_event value=1 checked>\n");
							else
								Body("<INPUT TYPE=RADIO NAME=enable_event value=1>\n");
						}
					}
					else
						Body("<INPUT TYPE=RADIO NAME=enable_event value=1>			\n");
					CellEnd();
					CellBegin("");
                                                Body("&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                Body("Register\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
					if(pageselect==1)
                                        {
						if (vector)
                                                {
							if(atoi(vector[1]))
								Body("<INPUT TYPE=RADIO NAME=enable_event value=0>\n");
							else
								Body("<INPUT TYPE=RADIO NAME=enable_event value=0 checked>\n");
						}
					}
					else
						Body("<INPUT TYPE=RADIO NAME=enable_event value=0>\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                Body("Do not register	\n");
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("colspan=9");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
				RowBegin("valign=top");
					CellBegin("");
						Body("Enter the number of events that must occur before registration begins:\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("colspan=7");
					if(pageselect==1)
					{
						if (vector)
							FormatedBody("<INPUT TYPE=TEXT NAME=thcount size=10 value=%d>",atoi(vector[2]));
					}
					else
						Body("<INPUT TYPE=TEXT NAME=thcount size=10 value=\"\">\n");
					CellEnd();
                		RowEnd();
		/*		@row valign=top
                                        @cell colspan=9
                                                @ &nbsp;
                                        @endcell
                                @endrow
				@row valign=top
                                        @cell
						@ Enter the desired time delay (in multiples of 5 seconds) between the event and its registration:
					@endcell
                                        @cell
                                                @ &nbsp;&nbsp;
                                        @endcell
                                        @cell colspan=7
					if(pageselect==1)
					{
						if(vector)
							@format  "<INPUT TYPE=TEXT NAME=thfreq size=10 value=%d>" atoi(vector[3])
					}
					else
						@ <INPUT TYPE=TEXT NAME=thfreq size=10 value="">
                                        @endcell
                                @endrow */
				TableEnd();
			if(pageselect==1)
			{
				ssdbFreeRequest(error_handle,req_handle);
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select action_id from event_actionref where type_id = %d and sys_id = '%s'",event_type,sys_id)))
				{		
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
				}
				total_actions = getnumrecords(hError,error_handle,req_handle);
				if (total_actions > 0)
				{
					for (rec_sequence = 0; rec_sequence < total_actions; rec_sequence++)
					{
						vector = ssdbGetRow(error_handle,req_handle);	
						if (vector)
							number_of_actions[rec_sequence] = atoi(vector[0]);
					}
				}	
				ssdbFreeRequest(error_handle,req_handle);
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select action_id,action_desc from event_action where private=0 order by action_desc")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					Body("<b>There are no actions in the database.</b>\n");
					Body("<input type=\"hidden\" name=act_id value=\"absent\">\n");
					ssdbFreeRequest(error_handle,req_handle);
				}
				else
				{
					Body("<p>Choose the action(s) that will be taken as a result of this event:<p><select name=actionid multiple size=5>\n");
					for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
					{
						flag = 0;
						vector = ssdbGetRow(error_handle,req_handle);
						if (vector)
						{
							common_int = atoi(vector[0]);
							for (iterator = 0; iterator < total_actions; iterator++)	
							{
								if (common_int == number_of_actions[iterator])
									flag = 1;
							}
							if (flag)
								FormatedBody("<option selected value = %d > %s",atoi(vector[0]),vector[1]);
							else
								FormatedBody("<option value = %d > %s",atoi(vector[0]),vector[1]);
						}
					}
					Body("</select>\n");
					Body("<input type=\"hidden\" name=act_id value=\"present\">\n");
					ssdbFreeRequest(error_handle,req_handle);
				}
			}

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
				TableBegin("border=0 cellpadding=0 cellspacing=0");
				RowBegin("valign=top");
					CellBegin("");
						Body("<font color=\"#666633\"><b>Tip: </b></font>\n");
					CellEnd();
					CellBegin("");
					if (pageselect==1)
						Body("You can select several actions.<br>If you cannot find an action that you need in the list above,  use <a href=\"/action_add.html\">SETUP: Actions: Add</a> to create a new action.\n");
					else
						Body("Use <a href=\"/event_update_batch.html\">SETUP: Events: Update Event Actions</a>, if you want to update actions for multiple events.\n");
					CellEnd();
				RowEnd();
				TableEnd();
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
				Body("<INPUT TYPE=\"SUBMIT\"  VALUE=\"   Accept   \">&nbsp;&nbsp;&nbsp;<input type=\"BUTTON\" value=\"   Clear   \" onClick=\"clearForm(this.form)\">\n");
				FormatedBody("<input type=\"hidden\" name=sys_id value=%s>",sys_id);
				FormatedBody("<input type=\"hidden\" name=ev_class value=%d>",ev_class);
				FormatedBody("<input type=\"hidden\" name=pageselect value=%d>",pageselect);
			CellEnd();
		RowEnd();
		TableEnd();
	CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}

void event_updt_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int i,j,number_of_records,rec_sequence,event_type,key,actionid,ev_class,common_int,enable_event,thcount,thfreq,pageselect,selvar;
	int num_actions;
	char sys_id[32],common_string[2];
	const char **vector;
	int action_array[MAX_ACTIONS];
		

	if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,352))
		return;
	if(!get_variable(hError,cmdp,INTTYPE,"pageselect",common_string,&pageselect,319))
		return;
	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,320))
                        return;
	
	if(pageselect==1)
	{
		if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&event_type,321))
			return;
		if(!get_variable(hError,cmdp,INTTYPE,"enable_event",common_string,&enable_event,322))
			return;
		if(!get_variable(hError,cmdp,INTTYPE,"thcount",common_string,&thcount,323))
			return;
		/*if(!get_variable(hError,cmdp,INTTYPE,"thfreq",common_string,&thfreq,324))
			return; */
	}
	if(pageselect==2)	
	{
		if ((key = sscFindPairByKey(cmdp,0,"enable_event")) < 0)
			enable_event = -1;
		else
		{
			if(get_variable(hError,cmdp,INTTYPE,"enable_event",common_string,&enable_event,325) < 0)
				return;
		}
		if ((selvar = get_variable_variable(hError,cmdp,INTTYPE,"thcount",common_string,&thcount,326)) < 0)
			thcount = -1;
		else if (!selvar)
			return;
		/*if((selvar = get_variable_variable(hError,cmdp,INTTYPE,"thfreq",common_string,&thfreq,327)) < 0)
			thfreq = -1;
		else if (!selvar)
			return; */
	}
	if (!(ssdbLockTable(error_handle,connection,"event_type,event_actionref,event_action")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if(pageselect==2)
	{
		for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
		{
			if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
			{
				sscError(hError,"Invalid event type count parameter\n");
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			else
			{
				if(enable_event != -1)
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
					"update event_type set enabled=%d where type_id=%d and sys_id='%s'",
					enable_event,event_type,sys_id)))		
					{
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}
					ssdbFreeRequest(error_handle,req_handle);
				}
				if(thcount != -1)
                                {
                                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
                                        "update event_type set sehthrottle=%d where type_id=%d and sys_id='%s'",
					thcount,event_type,sys_id)))               
                                        {
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
                                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                return;
                                        }
                                        ssdbFreeRequest(error_handle,req_handle);
                                }
				/*if(thfreq != -1)
                                {
                                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
                                        "update event_type set sehfrequency=%d where type_id=%d and sys_id='%s'",
					thfreq,event_type,sys_id)))
					{
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
                                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                return;
                                        }
                                        ssdbFreeRequest(error_handle,req_handle);
                                }*/

			}
		}
	}

	if(pageselect==1)
	{

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
                        "update event_type set enabled=%d,sehthrottle=%d where type_id=%d and sys_id='%s'",
                        enable_event,thcount,event_type,sys_id)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select event_actionref.action_id from event_actionref,event_action "
			"where event_actionref.action_id = event_action.action_id "
			"and event_action.private = 1 and event_actionref.type_id = %d and event_actionref.sys_id='%s'",event_type,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
		}
	
		num_actions = getnumrecords(hError,error_handle,req_handle);
		if (num_actions > 0)
		{
			for (rec_sequence = 0; rec_sequence < num_actions && rec_sequence < MAX_ACTIONS; rec_sequence++)
			{
				vector = ssdbGetRow(error_handle,req_handle);
				if (vector)
					action_array[rec_sequence] = atoi(vector[0]);
			}
		}
		ssdbFreeRequest(error_handle,req_handle);

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
		"delete from event_actionref where type_id=%d and sys_id='%s'",event_type,sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);

		for (i=0; (i = sscFindPairByKey(cmdp,i,"actionid")) >= 0;i++)
		{
			if ((sscanf(cmdp[i].value,"%d",&actionid)) < 0)
			{
				sscError(hError,"Invalid action id parameter\n");
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			else
			{
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into event_actionref values('%s',NULL,%d,%d,%d)",sys_id,ev_class,event_type,actionid)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
			}
		}

		/* Private actions are restored here. This is added for SGM tasks that are inserted like forward events */
		for (j=0; j < num_actions; j++)
		{
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into event_actionref values('%s',NULL,%d,%d,%d)",sys_id,ev_class,event_type,action_array[j])))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			ssdbFreeRequest(error_handle,req_handle);
		}
	}
	if (!ssdbUnLockTable(error_handle,connection))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if(configure_event("UPDATE localhost -1 -1 0 0 0"))
	{
		sscError(hError, (char*) szErrMsgEventmonDead );
		return;
	}
	if(configure_rule("UPDATE -1"))
        {
		sscError(hError,(char*) szErrMsgEventmonDead );
                return;
        }
	if(!emapiDeclareDaemonReloadConfig())
	{
		sscError(hError, (char*) szErrMsgEventmonDead );
		return;
	}

	event_view_report_page(hError,session,connection,error_handle,cmdp);
}


void event_add_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int i,number_of_records,rec_sequence,ev_class,cust_typeid,key,common_int,class_flag,class_id,actionid,thcount,enable_event,thfreq;
        const char **vector;
        char sys_id[32],custom_class_description[80],custom_type_description[80],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,328))
		return;
	if(!get_variable(hError,cmdp,CHARTYPE,"custom_type_description",custom_type_description,&common_int,329))     
                return; 
	if(!get_variable(hError,cmdp,INTTYPE,"enable_event",common_string,&enable_event,330))
		return;	
	if(!get_variable(hError,cmdp,INTTYPE,"thcount",common_string,&thcount,331))
		return;	
	/*if(!get_variable(hError,cmdp,INTTYPE,"thfreq",common_string,&thfreq,332))
		return;	 */

	if ((key = sscFindPairByKey(cmdp,0,"ev_class")) < 0)
        {
		class_flag = 0;
                if(!get_variable(hError,cmdp,CHARTYPE,"custom_class_description",custom_class_description,&common_int,333))
                        return ;
        }
        else
        {
                class_flag = 1;
                if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,334))
                        return ;
        }

	if (!(ssdbLockTable(error_handle,connection,"event_class,event_type,event_action,event_actionref")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if(!class_flag)
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select event_class.class_id from event_class where event_class.class_id > 7999 and event_class.sys_id = '%s'",sys_id)))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                {
                                ev_class = 8000;
                }
                else
                {
                        ssdbFreeRequest(error_handle,req_handle);
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select max(event_class.class_id) from event_class where event_class.class_id > 7999 and event_class.sys_id = '%s'",sys_id)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        vector = ssdbGetRow(error_handle,req_handle);
                        if(vector)
                                ev_class = atoi(vector[0])+1;
                }
                ssdbFreeRequest(error_handle,req_handle);
	}
	
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select event_type.type_id from event_type where type_id > 8399999 and sys_id = '%s'",sys_id)))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
			cust_typeid = 8400000;
	}
	else
	{
		ssdbFreeRequest(error_handle,req_handle);
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select max(event_type.type_id) from event_type where type_id > 8399999 and sys_id = '%s'",sys_id)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		vector = ssdbGetRow(error_handle,req_handle);
		if(vector)
			cust_typeid = atoi(vector[0])+1;
	}
	ssdbFreeRequest(error_handle,req_handle);
	if(!class_flag)
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
		"insert into event_class values('%s',%d,\"%s\")",sys_id,ev_class,custom_class_description)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		ssdbFreeRequest(error_handle,req_handle);
	}
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
	"insert into event_type values('%s',%d,%d,\"%s\",1,%d,0,-1,%d)",sys_id,cust_typeid,ev_class,
	custom_type_description,thcount,enable_event)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	ssdbFreeRequest(error_handle,req_handle);
	for (i=0; (i = sscFindPairByKey(cmdp,i,"actionid")) >= 0;i++)
	{
		if ((sscanf(cmdp[i].value,"%d",&actionid)) < 0)
		{
			sscError(hError,"Invalid action id parameter\n");
			return;
		}
		else
		{
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into event_actionref values('%s',NULL,%d,%d,%d)",sys_id,ev_class,cust_typeid,actionid)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			ssdbFreeRequest(error_handle,req_handle);		
		}
	}
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Add\n");
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
		"select event_class.class_id, event_class.class_desc,event_type.type_id,event_type.type_desc,"
		"event_type.enabled,event_type.sehthrottle,event_type.sehfrequency "
		"from event_class,event_type "
		"where event_class.class_id= %d "
		"and event_type.type_id = %d "
		"and event_class.sys_id = '%s' "
		"and event_type.sys_id = '%s' "
		"and event_type.sys_id = '%s'",ev_class,cust_typeid,sys_id,sys_id,sys_id)))	
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
		{
			Body("Record addition was not successful.\n");
			ssdbFreeRequest(error_handle,req_handle);
			CellEnd();
			RowEnd();
			TableEnd();
			Body("</form></body></html>\n");
			return;
		}
		vector = ssdbGetRow(error_handle,req_handle);
		if(vector)
		{
			TableBegin("border=0 cellspacing=0 cellpadding=0");
			RowBegin("valign=top");
				CellBegin("");
					Body("Event class\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("");
					FormatedBody("%d, %s",atoi(vector[0]),vector[1]);
				CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("colspan=3");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("");
					Body("Event\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("");
					FormatedBody("%d, %s",atoi(vector[2]),vector[3]);
				CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("colspan=3 ");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("");
					Body("Event registration with SGI Embedded Support Partner database\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("");
					if(atoi(vector[4]))
						Body("Register\n");
					else
						Body("Do not register\n");
				CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("colspan=3");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("valign=top");
				CellBegin("");
					Body("Number of events that must occur before registration\n");
				CellEnd();
				CellBegin("");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("");
					FormatedBody("%d",atoi(vector[5]));
				CellEnd();
			RowEnd();
			/*@row valign=top
				@cell colspan=3
					@ &nbsp;
				@endcell
			@endrow
			@row valign=top
				@cell
					@ Time delay between the event and its registration
				@endcell
				@cell
					@ &nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;
				@endcell
				@cell
					@format "%d secs" atoi(vector[6])
				@endcell
			@endrow */
			RowBegin("valign=top");
				CellBegin("colspan=3");
					Body("&nbsp;\n");
				CellEnd();
			RowEnd();
			ssdbFreeRequest(error_handle,req_handle);
			RowBegin("valign=top");
					CellBegin("");
						Body("Actions for this event\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
					"select event_action.action_desc,event_actionref.action_id from event_action,event_actionref "
					"where event_action.action_id = event_actionref.action_id "
					"and event_actionref.type_id = %d "
					"and event_actionref.sys_id = '%s'",cust_typeid,sys_id)))
					{
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}
					if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
					{
						Body("None\n");
					}
					else
					{
						for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
						{
							vector = ssdbGetRow(error_handle,req_handle);
							if(vector)
								FormatedBody("%s<br>",vector[0]);
						}
					}
					CellEnd();
				RowEnd();
				TableEnd();
			}
		CellEnd();
		RowEnd();
		TableEnd();
		Body("</form></body></html>	\n");
		ssdbFreeRequest(error_handle,req_handle);
		if (!ssdbUnLockTable(error_handle,connection))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		if(!emapiDeclareDaemonReloadConfig())
		{
			sscError(hError, (char*) szErrMsgEventmonDead );
			return;
		}
		if(configure_event("UPDATE localhost -1 -1 0 0 0"))
		{
			sscError(hError, (char*) szErrMsgEventmonDead );
			return;
		}
		if(configure_rule("UPDATE -1"))
		{
			sscError(hError, (char*) szErrMsgEventmonDead );
			return;
		}
}

void generate_class_list (sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle)
{
	ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence;
        const char **vector;
        char sys_id[32];

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select sys_id from system where active = 1")))
        {
                sscError(hError,"Database API Error %d: \"%s\n\n",ssdbGetLastErrorCode(error_handle),
                        ssdbGetLastErrorString(error_handle));
                return;
        }

        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                sscError(hError,"Error retrieving system information\n");
                ssdbFreeRequest(error_handle,req_handle);
                return;
        }

        vector = ssdbGetRow(error_handle,req_handle);
        if (vector)
                strcpy(sys_id,vector[0]);

        ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select class_id,class_desc from event_class where sys_id = '%s' order by class_desc",sys_id)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		Body("<b>There are no events in the database. </b>\n");
		ssdbFreeRequest(error_handle,req_handle);
		return ;
	}
	else
	{
		Body("<select name=ev_class size=5>\n");
		for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
		{
			vector = ssdbGetRow(error_handle,req_handle);
			if(vector)
				FormatedBody("<option value = %d > %s",atoi(vector[0]),vector[1]);
		}
		Body("</select>\n");
		ssdbFreeRequest(error_handle,req_handle);
	}
}

void event_update_action_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence,action_id,common_int;
        const char **vector;
        char sys_id[32],common_string[2];

        if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&action_id,335))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,336))
                return;

	create_help(hError,"setup_events_update_event_actions_method1");
        Body("<html><head><title>SGI Embedded Support Partner - ver.1.0</title>\n");
	if (!(read_java_scritps("##EVENT_UPDT_ACT_AUD","##EVENT_UPDT_ACT_AUD_END")))
	{
		sscError(hError,"Error reading java script for this operation\n");
		return ;
	}
        Body("</head><body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<a href = \"/$sss/$nocache\"></a>\n");
        Body("<form onSubmit=\"return submitType(this)\" method=POST action=\"/$sss/rg/libsemserver~EVENT_ACTION_AUD\" name=\"evactSearch\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=\"15\"");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events &gt; Update Event Actions\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2 align=right");
                        Body("<input TYPE=\"button\" onClick=\"showMap()\" Value=\"   Help   \">\n");
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
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select action_desc from event_action where action_id = %d and sys_id = '%s'",action_id,sys_id)))
                        {
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
                                Body("<p><b>There are no records for the specified time period.</b>\n");
                                CellEnd();
                                RowEnd();
                                TableEnd();
                                Body("</body></html>\n");
                                ssdbFreeRequest(error_handle,req_handle);
                                return;
                        }
                        else
                        {
                                vector = ssdbGetRow(error_handle,req_handle);
                                if(vector)
                                        FormatedBody("Action <b>%s</b> applies to the events listed below. From the following list, select one or more events whose actions you want to replace, delete, or supplement with additional actions:<p>",vector[0]);
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select distinct event_class.class_desc,event_type.type_desc,event_type.type_id "
                                "from event_class,event_type,event_actionref "
                                "where event_actionref.action_id = %d "
                                "and event_class.class_id = event_actionref.class_id "
                                "and event_type.type_id = event_actionref.type_id "
                                "order by event_class.class_id,event_type.type_id",action_id)))
                        {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
                                Body("<p><b>There are no records for the specified time period.</b>\n");
                                CellEnd();
                                RowEnd();
                                TableEnd();
                                Body("</body></html>\n");
                                ssdbFreeRequest(error_handle,req_handle);
                                return;
                        }
                        else
                        {
                                Body("<SELECT NAME=event_type multiple size=10>\n");
                                for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                                FormatedBody("<option value = %d> %s ----- %s",atoi(vector[2]),vector[0],vector[1]);
                                }
                                Body("</select>\n");
                                ssdbFreeRequest(error_handle,req_handle);
                        }
                        Body("<p><INPUT TYPE=\"SUBMIT\" VALUE=\"   Replace   \" onClick=\"findUpdt(this)\">&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"SUBMIT\" VALUE=\"   Add   \" onClick=\"findAdd(this)\">&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"SUBMIT\" value=\"   Delete   \" onClick=\"findDel(this)\">\n");
                        Body("<input type=\"hidden\" name=\"submit_type\" value=\"\">\n");
                        FormatedBody("<input type=\"hidden\" name=\"actionid\" value=\"%d\">",action_id);
                        FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
			Body("<input type=\"hidden\" name=\"multiselect\" value=\"\">\n");
                CellEnd();
	RowEnd();
        TableEnd();
        Body("</form></body></html>\n");
}

void event_action_aud(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        int submit_type;
        char common_string[2];

	if(!get_variable(hError,cmdp,INTTYPE,"submit_type",common_string,&submit_type,337))
		return;	
	if(submit_type==0)
		event_action_update(hError,session,connection,error_handle,cmdp);

	if(submit_type==1)
		event_action_add(hError,session,connection,error_handle,cmdp);

	if(submit_type==2)
		event_action_delete(hError,session,connection,error_handle,cmdp);
}

void event_action_update(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int i,event_type,common_int,action_id;
        char sys_id[32],common_string[2];
	ssdb_Request_Handle req_handle;
        const char **vector;
        int number_of_records;

        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,338))
                return;
        if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&action_id,339))
                return;

	create_help(hError,"screens_update_event_actions_update1");
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        if (!(read_java_scritps("##EVENT_UPDT_ACT_UPDLIST","##EVENT_UPDT_ACT_UPDLIST_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<form onSubmit=\"return verifyData(this)\" method=POST action=\"/$sss/rg/libsemserver~EVENT_ACTION_UPD_CONFIRM\" name=\"evactAction\">\n");
	for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
        {
                if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
                {
                        sscError(hError,"Invalid event type count parameter\n");
                        return;
                }
                else
                        FormatedBody("<input type=\"hidden\" name=\"event_type\" value=\"%d\">",event_type);
        }
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
			Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Update Event Actions &gt; Replace Action for Events\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2 align=right");
                        Body("<input TYPE=\"button\" onClick=\"showMap()\" Value=\"   Help   \">\n");
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
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select action_desc from event_action where action_id = %d",action_id)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
			{
				vector = ssdbGetRow(error_handle,req_handle);	
				if (vector)
				{
					Body("The following action is to be replaced for the selected event(s):<ul> \n");
					FormatedBody("<li>%s</ul>",vector[0]);
					Body("<p>Choose an action that you want to replace with:<p>\n");
					create_action_list(hError,session,connection,error_handle,cmdp);
					FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
					FormatedBody("<input type=\"hidden\" name=\"action_id\" value=\"%d\">",action_id);
					ssdbFreeRequest(error_handle,req_handle);	
				}
				else
				{
					ssdbFreeRequest(error_handle,req_handle);	
					sscError(hError,"Error reading action description\n");
					return;
				}
			}
			else
			{
				ssdbFreeRequest(error_handle,req_handle);	
				sscError(hError,"Error reading action description\n");
				return;
			}
                CellEnd();
        RowEnd();
        TableEnd();
        Body("</form></body></html>\n");
}

void event_action_update_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{

	int i,event_type,common_int,action_id,actionid;
        char sys_id[32],common_string[2],act_desc1[256],act_desc[256];
        ssdb_Request_Handle req_handle;
        const char **vector;
        int number_of_records;
	
	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,340))
                return;
	
	if(!get_variable(hError,cmdp,INTTYPE,"action_id",common_string,&action_id,341))
                return;
	
	if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&actionid,342))
                return;	
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE> </HEAD>\n");
	Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<form method=POST>\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Update Event Actions &gt; Update Action for Events\n");
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
			if (!(ssdbLockTable(error_handle,connection,"event_type,event_actionref,event_action")))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select action_desc from event_action where action_id = %d",action_id)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }

                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
                        {
                                vector = ssdbGetRow(error_handle,req_handle);
                                if(vector)
					strcpy(act_desc,vector[0]);
				else
					strcpy(act_desc1,"Unknown");
			}
			ssdbFreeRequest(error_handle,req_handle);
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select action_desc from event_action where action_id = %d",actionid)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }

                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
                        {
                                vector = ssdbGetRow(error_handle,req_handle);
                                if(vector)
                                        strcpy(act_desc1,vector[0]);
				else	
					strcpy(act_desc1,"Unknown");
                        }
                        ssdbFreeRequest(error_handle,req_handle);
			FormatedBody("Action <B>%s</B> was replaced by <b>%s</b> for the following events:<ul>",act_desc,act_desc1);

			
			for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
			{
				if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
				{
					sscError(hError,"Invalid event type parameter\n");
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				else
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
					"update event_actionref set action_id = %d where type_id = %d and action_id = %d and sys_id = '%s'",
					actionid,event_type,action_id,sys_id)))
					{
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}
					ssdbFreeRequest(error_handle,req_handle);
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
					"select type_desc from event_type where type_id = %d and sys_id='%s'",event_type,sys_id)))
					{
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                return;
                                        }
					if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
					{
						vector = ssdbGetRow(error_handle,req_handle);
						if(vector)
							FormatedBody("<li>%s",vector[0]);
					}
					ssdbFreeRequest(error_handle,req_handle);	
				}
			}
			Body("</ul>\n");
			if (!ssdbUnLockTable(error_handle,connection))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			if(configure_rule("UPDATE -1"))
			{
				sscError(hError, (char*) szErrMsgEventmonDead );
				return;
			}
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}
	
void event_action_add(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int i,event_type,common_int;
        char sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,343))
                return;

	create_help(hError,"screens_update_event_actions_add1");
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        if (!(read_java_scritps("##EVENT_UPDT_ACT_ADDLIST","##EVENT_UPDT_ACT_ADDLIST_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form onSubmit=\"return verifyData(this)\" method=POST action=\"/$sss/rg/libsemserver~EVENT_ACTION_ADD_CONFIRM\" name=\"evactAdd\">\n");
	for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
	{
		if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
		{
			sscError(hError,"Invalid event type count parameter\n");
			return;
		}
		else
			FormatedBody("<input type=\"hidden\" name=\"event_type\" value=\"%d\">",event_type);
	}
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Update Event Actions &gt; Add Actions to Events\n");
                CellEnd();
        RowEnd();
	RowBegin("");
		CellBegin("colspan=2");
			Body("&nbsp;\n");
		CellEnd();
        RowEnd();
	RowBegin("");
                CellBegin("colspan=2 align=right");
                        Body("<input TYPE=\"button\" onClick=\"showMap()\" Value=\"   Help   \">	\n");
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
			Body("Choose action(s) that you want to add to the selected event(s):<p>\n");
			create_action_list(hError,session,connection,error_handle,cmdp);
			FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}

void event_action_add_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        int i,j,number_of_records,action_id,common_int,event_type,class_id;
        const char **vector;
        char sys_id[32],common_string[2];

        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,344))
                return;

        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST>\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=\"15\"");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events &gt; Update Event Actions &gt; Add Actions to Events\n");
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
                        Body("The following actions has been added to the chosen event(s):<ul>\n");
			if (!(ssdbLockTable(error_handle,connection,"event_type,event_action,event_actionref")))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

                        for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
                        {
                                if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
                                {
                                        sscError(hError,"Invalid event type count parameter\n");
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                else
                                {
                                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                        "select class_id from event_type where type_id=%d and sys_id='%s'",event_type,sys_id)))
                                        {
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                return;
                                        }
                                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                        class_id = atoi(vector[0]);
                                                ssdbFreeRequest(error_handle,req_handle);
                                                for (j=0; (j = sscFindPairByKey(cmdp,j,"actionid")) >= 0;j++)                                   
                                                {       
                                                        if ((sscanf(cmdp[j].value,"%d",&action_id)) < 0)
                                                        {
								sscError(hError,"Invalid action ID parameter\n");
								if (!ssdbUnLockTable(error_handle,connection))
									sscError(hError,"Database API Error: \"%s\"\n\n",
										ssdbGetLastErrorString(error_handle));
								return;
							}
							else
							{
								if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
								"select action_id from event_actionref where action_id='%d'and type_id='%d' and sys_id='%s'",
								action_id,event_type,sys_id)))
								{
									sscError(hError,"Database API Error: \"%s\"\n\n",
											ssdbGetLastErrorString(error_handle));
									if (!ssdbUnLockTable(error_handle,connection))
										sscError(hError,"Database API Error: \"%s\"\n\n",
											ssdbGetLastErrorString(error_handle));
                                                                        return;
                                                                }
								if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)		
								{
									ssdbFreeRequest(error_handle,req_handle);
									if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
									"insert into event_actionref values('%s',NULL,%d,%d,%d)", sys_id,class_id,
									event_type,action_id)))
									{
										sscError(hError,"Database API Error: \"%s\"\n\n",
										ssdbGetLastErrorString(error_handle));
										if (!ssdbUnLockTable(error_handle,connection))
											sscError(hError,"Database API Error: \"%s\"\n\n",
												ssdbGetLastErrorString(error_handle));
										return;
									}
								}
								ssdbFreeRequest(error_handle,req_handle);
							}
						}
					}
					else
						ssdbFreeRequest(error_handle,req_handle);
				}
			}
			for (j=0; (j = sscFindPairByKey(cmdp,j,"actionid")) >= 0;j++)
			{
				if ((sscanf(cmdp[j].value,"%d",&action_id)) < 0)
				{
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					sscError(hError,"Invalid action ID parameter\n");
					return;
				}
				else
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
					"select action_desc from event_action where action_id = %d",action_id)))
					{
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}
					if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
					{
						vector = ssdbGetRow(error_handle,req_handle);
						if(vector)
							FormatedBody("<li>%s",vector[0]);
					}
					ssdbFreeRequest(error_handle,req_handle);
				}
			}
			Body("</ul>\n");
			if (!ssdbUnLockTable(error_handle,connection))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			if(configure_rule("UPDATE -1"))
			{
				sscError(hError, (char*) szErrMsgEventmonDead );
				return;
			}
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>						\n");
}

void event_action_delete(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int i,number_of_records,action_id,common_int,submit_type,event_type;
        const char **vector;
        char sys_id[32],common_string[2];

        if(!get_variable(hError,cmdp,INTTYPE,"actionid",common_string,&action_id,345))  
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,346)) 
                return; 
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
	Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=post>\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Update Event Actions &gt; Delete Action from Event(s):\n");
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
			if (!(ssdbLockTable(error_handle,connection,"event_actionref,event_action,event_type")))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select action_desc from event_action where action_id = %d",action_id)))
			{
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
			
			if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
			{
				vector = ssdbGetRow(error_handle,req_handle);
                                if(vector)
					FormatedBody("The action <b>%s</b> was removed from the following events:<ul>",vector[0]);
			}
			ssdbFreeRequest(error_handle,req_handle);
			
			for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
			{
				if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
				{
					sscError(hError,"Invalid event type count parameter\n");
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				else
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
					"delete from event_actionref where type_id=%d and action_id=%d",event_type,action_id)))
					{
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}		
					ssdbFreeRequest(error_handle,req_handle);
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
					"select type_desc from event_type where type_id = %d and sys_id = '%s'",
					event_type,sys_id)))
					{
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}
					if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
					{
						vector = ssdbGetRow(error_handle,req_handle);
						if(vector)
							FormatedBody("<li>%s",vector[0]);
					}
					ssdbFreeRequest(error_handle,req_handle);
				} 
			}
			Body("</ul>\n");
			if (!ssdbUnLockTable(error_handle,connection))
			{
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			if(configure_rule("UPDATE -1"))
			{
				sscError(hError, (char*) szErrMsgEventmonDead );
				return;
			}
	
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}

void event_delete(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{

        int submit_type;
        char common_string[2];

        if(!get_variable(hError,cmdp,INTTYPE,"submit_type",common_string,&submit_type,347))
                return;

	if (submit_type)	
		delete_event(hError,session,connection,error_handle,cmdp);
	else
		delete_class(hError,session,connection,error_handle,cmdp);
}


void delete_event(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        int i,ev_class,number_of_records,class_id,common_int;
	char sys_id[32],common_string[2];
	
        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,348))
                return;
		
	create_help(hError,"setup_events_delete1");
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        if (!(read_java_scritps("##EVENT_DELETE","##EVENT_DELETE_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	Body("<a href = \"/$sss/$nocahce\"></a>\n");
        Body("<form onSubmit=\"return verifyData(this)\" method=POST action=\"/$sss/rg/libsemserver~EVENT_DELETE_LIST\" name=\"delEvent\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Delete \n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2 align=right");
                        Body("<input TYPE=\"button\" onClick=\"showMap()\" Value=\"   Help   \">\n");
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
			create_custom_list(hError,session,connection,error_handle,cmdp);
			FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</form></body></html>\n");
}

void event_delete_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int i,number_of_records,action_id,common_int,submit_type,event_type,event_id;
        const char **vector;
        char sys_id[32],common_string[2],type_desc[80];

        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,349))
                return;
	
	Body("<html><head><title>SGI Embedded Support Partner - Ver.1.0</title></head>\n");
	Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=post>\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Delete\n");
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
			Body("The following events were deleted.<ul>\n");
		if (!(ssdbLockTable(error_handle,connection,"event_actionref,event_type,event,system_data,actions_taken")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}	

		for (i=0; (i = sscFindPairByKey(cmdp,i,"event_type")) >= 0;i++)
		{
			if ((sscanf(cmdp[i].value,"%d",&event_type)) < 0)
			{
				sscError(hError,"Invalid event type count parameter\n");
				return;
			}
			else
			{
				event_id = 0;
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select event_id from event where type_id = %d and sys_id = '%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
				{
					vector = ssdbGetRow(error_handle,req_handle);
					if(vector)
						event_id = atoi(vector[0]);
				}	
				ssdbFreeRequest(error_handle,req_handle);

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select type_desc from event_type where type_id = %d and sys_id = '%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
				{
					vector = ssdbGetRow(error_handle,req_handle);
					if(vector)
						strcpy(type_desc,vector[0]);
				}
				ssdbFreeRequest(error_handle,req_handle);

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
				"delete from event_type where type_id=%d and sys_id ='%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
			
				ssdbFreeRequest(error_handle,req_handle);
				
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
				"delete from event where type_id=%d and sys_id ='%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				ssdbFreeRequest(error_handle,req_handle);       


				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
				"delete from event_actionref where type_id=%d and sys_id ='%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				ssdbFreeRequest(error_handle,req_handle);
				if(event_id)
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE, 
					"delete from system_data where event_id=%d and sys_id ='%s'",event_id,sys_id)))      
					{       
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));       
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return; 
					}       
			
					ssdbFreeRequest(error_handle,req_handle);

					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE, 
					"delete from actions_taken where event_id=%d and sys_id ='%s'",event_id,sys_id)))    
					{       
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						if (!ssdbUnLockTable(error_handle,connection))
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
						return;
					}
					ssdbFreeRequest(error_handle,req_handle);
				}
				FormatedBody("<li>%s\n",type_desc);
			}	
		}
		if (!ssdbUnLockTable(error_handle,connection))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		if(configure_event("UPDATE localhost -1 -1 0 0 0"))
		{
			sscError(hError, (char*) szErrMsgEventmonDead );
			return;
		}
		if(configure_rule("UPDATE -1"))
		{
			sscError(hError, (char*) szErrMsgEventmonDead );
			return;
		}


		Body("</ul>\n");
		CellEnd();
	RowEnd();
	TableEnd();
	Body("</body></html>\n");
}

void delete_class(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{

	ssdb_Request_Handle req_handle;
        int i,number_of_records,action_id,common_int,ev_class,event_id,num_events;
        const char **vector;
	int event_ids[MAX_CLASSES];
        char sys_id[32],common_string[2],class_desc[80];

        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,350))
                return;
        if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,351))
                return;

	Body("<html><head><title>SGI Embedded Support Partner - Ver.1.0</title></head>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; Delete\n");
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
			if (!(ssdbLockTable(error_handle,connection,"event_class,event_actionref,event_type,event,system_data,actions_taken")))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select class_desc from event_class where class_id = %d and sys_id = '%s'",ev_class,sys_id)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			vector = ssdbGetRow(error_handle,req_handle);
			if(vector)
				strcpy(class_desc,vector[0]);
			ssdbFreeRequest(error_handle,req_handle);

			if (ev_class < 8000)
			{
				FormatedBody("The class <b>%s</b> is not a custom class and cannot be deleted.",class_desc);
				CellEnd();
				RowEnd();
				TableEnd();
				Body("</body></html>\n");
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select event_id from event where class_id = %d and sys_id = '%s'",ev_class,sys_id)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			
			if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
			{
				for (i =0; i < number_of_records; i++)
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
					event_ids[i] = atoi(vector[0]);
			}
			num_events = number_of_records;
			ssdbFreeRequest(error_handle,req_handle);

			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
			"delete from event_class where class_id=%d and sys_id ='%s'",ev_class,sys_id)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

			ssdbFreeRequest(error_handle,req_handle);

			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
			"delete from event where class_id=%d and sys_id ='%s'",ev_class,sys_id)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

			ssdbFreeRequest(error_handle,req_handle);

			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
			"delete from event_actionref where class_id=%d and sys_id ='%s'",ev_class,sys_id)))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}

			ssdbFreeRequest(error_handle,req_handle);
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,                 
			"delete from event_type  where class_id=%d and sys_id ='%s'",ev_class,sys_id)))
			{                       
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));                       
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;                 
			}                       

			ssdbFreeRequest(error_handle,req_handle);
			for (i = 0; i < num_events; i++)	
			{
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
				"delete from system_data where event_id=%d and sys_id ='%s'",event_ids[i],sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				ssdbFreeRequest(error_handle,req_handle);

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
				"delete from actions_taken where event_id=%d and sys_id ='%s'",event_ids[i],sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
			}
			FormatedBody("The class <b>%s</b> along with associated events and data has been deleted.",class_desc);
		CellEnd();
	RowEnd();
	TableEnd();
	if (!ssdbUnLockTable(error_handle,connection))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if(configure_event("UPDATE localhost -1 -1 0 0 0"))
	{
		sscError(hError, (char*) szErrMsgEventmonDead );
		return;
	}
	if(configure_rule("UPDATE -1"))
	{
		sscError(hError, (char*) szErrMsgEventmonDead );
		return;
	}
	Body("</body></html>\n");
}
