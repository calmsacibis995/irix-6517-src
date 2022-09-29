#include "ui.h"

static const char szErrMsgEventMonDead [] = "Failed responce from event monitoring daemon\n";

void sgm_event_view(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int setup_select,key,common_int;
	char sys_id[32],common_string[2];

	if(!get_variable(hError,cmdp,INTTYPE,"setup_select",common_string,&setup_select,900))
		return;
	if (setup_select==1)
		event_description_sgm_view(hError,session,connection,error_handle,cmdp);
	else
                event_description_sgm_class(hError,session,connection,error_handle,cmdp);
}

void event_description_sgm_view(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle,req_handle1,req_handle2;
        int i,j,number_of_records,number_of_records1,number_of_records2,rec_sequence,common_int,key,row_num,balance_rows,total_pages,pageno;
	int firstpage,lastpage,bottom;
	const char **vector,**vector1,**vector2;
	char sys_id[32],common_string[2];
	
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,901))
                return;

	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsgmserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; View Current Setup\n");
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
		"select distinct event_class.class_desc,event_type.type_desc,event_type.type_id "
		"from event_class,event_type where event_type.class_id = event_class.class_id "
		"order by event_class.class_id")))
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
					CellBegin("");
						Body("<b>Member Systems</b>\n");
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
						CellBegin("");
							if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                                        "select sys_id from event_type where type_desc ='%s'",vector[1])))
							{
								sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
								return;
							}
							if ((number_of_records1 = getnumrecords(hError,error_handle,req_handle)) > 0)
							{
								for (i = 0; i < number_of_records1; i++)
								{
									vector1 = ssdbGetRow(error_handle,req_handle1);
									if(vector1)
									{
										if (!(req_handle2 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
										"select system.hostname from system where sys_id = '%s'",vector1[0])))
										{
											sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
											return;
										}
										if ((number_of_records2 = getnumrecords(hError,error_handle,req_handle)) > 0)
										{
										if (number_of_records2)
										{
											for (j = 0; j < number_of_records2; j++)
											{
												vector2 = ssdbGetRow(error_handle,req_handle2);
												if(vector2)
													FormatedBody("%s<br>",vector2[0]);
											}
											ssdbFreeRequest(error_handle,req_handle2);
										}										
										}
									}
								}
								ssdbFreeRequest(error_handle,req_handle1);
							}
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
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_DESCRIPTION_CONT?row_num=%d\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0);
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_DESCRIPTION_CONT?row_num=%d\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#cc6633\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_DESCRIPTION_CONT?row_num=%d\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        bottom = (number_of_records/100)*100;
                                        if(number_of_records%10 == 0)
                                                bottom = bottom -10;
                                        else
                                                bottom = number_of_records -(number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_DESCRIPTION_CONT?row_num=%d\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10);
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_DESCRIPTION_CONT?row_num=%d\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom);
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

void event_description_sgm_class(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle,req_handle1,req_handle2;
        int i,j,number_of_records,number_of_records1,number_of_records2,rec_sequence,row_num,balance_rows,total_pages,pageno,common_int;
	int firstpage,lastpage,bottom;
        const char **vector,**vector1,**vector2;
        char sys_id[32],common_string[2];

        if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,902))
                return;
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsgmserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SETUP &gt; Events  &gt; View Current Setup\n");
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
                "select distinct event_class.class_desc,class_id from event_class order by class_id")))
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
					CellBegin("");
                                                Body("<b>Member Systems</b>\n");
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
						CellBegin("");
							if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                                        "select sys_id from event_class where class_desc ='%s'",vector[0])))
                                                        {
                                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                                return;
                                                        }
                                                        if ((number_of_records1 = getnumrecords(hError,error_handle,req_handle)) > 0)
                                                        {
                                                                for (i = 0; i < number_of_records1; i++)
                                                                {
                                                                        vector1 = ssdbGetRow(error_handle,req_handle1);
                                                                        if(vector1)
                                                                        {
                                                                                if (!(req_handle2 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                                                                "select system.hostname from system where sys_id = '%s'",vector1[0])))
                                                                                {
                                                                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                                                        return;
                                                                                }
                                                                                if ((number_of_records2 = getnumrecords(hError,error_handle,req_handle)) >
0)
                                                                                {
                                                                                if (number_of_records2)
                                                                                {
                                                                                        for (j = 0; j < number_of_records2; j++)
                                                                                        {
                                                                                                vector2 = ssdbGetRow(error_handle,req_handle2);
                                                                                                if(vector2)
                                                                                                        FormatedBody("%s<br>",vector2[0]);
                                                                                        }
                                                                                        ssdbFreeRequest(error_handle,req_handle2);
                                                                                }
                                                                                }
                                                                        }
                                                                }
                                                                ssdbFreeRequest(error_handle,req_handle1);
                                                        }
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
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_CLASS_VIEW_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_CLASS_VIEW_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#cc6633\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_CLASS_VIEW_CONT?row_num=%d&sys_id=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        bottom = (number_of_records/100)*100;
                                        if(number_of_records%10 == 0)
                                                bottom = bottom -10;
                                        else
                                                bottom = number_of_records -(number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_CLASS_VIEW_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id);
                                         FormatedBody("<a href=\"/$sss/rg/libsgmserver~SGM_EVENT_CLASS_VIEW_CONT?row_num=%d&sys_id=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id);
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

/* Event updates in SGM */

void sgm_update_event_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int key,common_int,pageselect,ev_class;
        char sys_id[32],common_string[2];

        if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,903))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,904))
                return;

	create_help(hError,"setup_events_update_type");
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	if (!(read_java_scritps("##SGM_EV_UPDT_TYPE","##SGM_EV_UPDT_TYPE_END")))
	{
		sscError(hError,"Error reading java script for this operation\n");
		return ;
	}
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");

        /* This calls the view event configuration page. Comes from view current event setup */

	Body("<form onSubmit=\"return submitType(this)\" method=\"post\" name =ChooseType action=\"/$sss/rg/libsgmserver~SGM_EVENT_UPDATE_INFO\">\n");
	FormatedBody("<input type=\"hidden\" name=\"ev_class\" value=%d>",ev_class);
	Body("<input type=\"hidden\" name=\"multiselect\" value=1>\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                        Body("SETUP  &gt; Events &gt; Update\n");
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
                                sgm_type_list(hError,session,connection,error_handle,cmdp);
                                FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
                        CellEnd();
                RowEnd();
        TableEnd();
        Body("</form> </body></html>\n");
}

void sgm_event_update_info(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
	const char **vector;
        int number_of_records,key,n=1,common_int,pageselect,ev_class,event_type,total_actions,iterator,rec_sequence,number_of_actions[MAX_ACTIONS],flag;
        char sys_id[32],common_string[2],hostname[64],class_desc[80],type_desc[80];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,905))
		return;
        if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,906))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&event_type,907))
                return;
	        create_help(hError,"screens_setup_events_update1");
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        if (!(read_java_scritps("##SGM_EV_UPDT_INFO","##SGM_EV_UPDT_INFO_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=\"post\" name =ChooseType action=\"/$sss/rg/libsgmserver~SGM_EVENT_UPDATE_CONFIRM\" onSubmit=\"return verifyData(this)\">\n");
        FormatedBody("<input type=\"hidden\" name=\"ev_class\" value=%d>",ev_class);
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                        Body("SETUP  &gt; Events &gt; Update\n");
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
				TableBegin("border=0 cellpadding=0 cellspacing=0");
				RowBegin("");
					CellBegin("");
						Body("System name\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
					CellEnd();
					CellBegin("");
						if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
						"select hostname from system where sys_id='%s' and active=1",sys_id)))
						{
							sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
							return;
						}
						if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
						{
							vector= ssdbGetRow(error_handle,req_handle);
							if(vector)
							{
								FormatedBody("%s",vector[0]);
								strcpy(hostname,vector[0]);
							}
							else
							{
								Body("Unknown\n");
								strcpy(type_desc,"Unknown");
                                                        }
						}
						else
						{
							Body("Unknown\n");
							strcpy(type_desc,"Unknown");
						}
						ssdbFreeRequest(error_handle,req_handle);
					CellEnd();
				RowEnd();
				RowBegin("");
					CellBegin("colspan=3");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
				RowBegin("");
                                        CellBegin("");
                                                Body("Event class\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT, 
                                                "select class_desc from event_class where class_id = %d and sys_id = '%s'",ev_class,sys_id)))
						{
                                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                        return;
                                                }
                                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
                                                {
                                                        vector= ssdbGetRow(error_handle,req_handle);
                                                        if(vector)
							{
                                                                FormatedBody("%s",vector[0]);
								strcpy(class_desc,vector[0]);
                                                        }
                                                        else
                                                        {
						        	Body("Unknown\n");
								strcpy(type_desc,"Unknown");
							}
                                                }
                                                else
						{
                                                        Body("Unknown\n");
							strcpy(type_desc,"Unknown");
						}
                                                ssdbFreeRequest(error_handle,req_handle);
                                        CellEnd();
				RowEnd();
				RowBegin("valign=top");
                                        CellBegin("colspan=3");
                                                Body("&nbsp;\n");
                                        CellEnd();
                                RowEnd();
                                RowBegin("");
                                        CellBegin("");
                                                Body("Event\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT, 
                                                "select type_desc from event_type where type_id = %d and sys_id = '%s'",event_type,sys_id)))
						{       
                                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                        return;
                                                }
                                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
                                                {
                                                        vector= ssdbGetRow(error_handle,req_handle);
                                                        if(vector)
							{
                                                                FormatedBody("%s",vector[0]);
								strcpy(type_desc,vector[0]);
                                                        }
                                                        else
							{
                                                                Body("Unknown\n");
								strcpy(type_desc,"Unknown");
							}
                                                }
                                                else
						{
							Body("Unknown\n");
							strcpy(type_desc,"Unknown");
						}
                                                ssdbFreeRequest(error_handle,req_handle);
                                        CellEnd();
				RowEnd();
				TableEnd();
				Body("<p><hr><p>\n");
				TableBegin("border=0 cellpadding=0 cellspacing=0");
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select sehthrottle, sehfrequency from event_type where type_id = %d and sys_id = '%s'",event_type,sys_id)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
				{
					vector= ssdbGetRow(error_handle,req_handle);
				}
                                RowBegin("valign=top");
                                        CellBegin("");
                                                Body("Enter the number of event occurrences prior to registration with SGI Embedded Support Partner\n");
                                        CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                if (vector)
                                                        FormatedBody("<INPUT TYPE=TEXT NAME=thcount size=10 value=%d>",atoi(vector[0]));
                                        CellEnd();
                                RowEnd();
                                /* @row valign=top
                                        @cell colspan=3
                                                @ &nbsp;
                                        @endcell
                                @endrow
                                @row valign=top
                                        @cell
                                                @ Enter frequency between events before registration in seconds (multiple of 5):
                                        @endcell
                                        @cell
                                                @ &nbsp;&nbsp;
                                        @endcell
                                        @cell
                                                if(vector)
                                                        @format  "<INPUT TYPE=TEXT NAME=thfreq size=10 value=%d>" atoi(vector[1])
                                        @endcell
                                @endrow */
				TableEnd();
				Body("<p>\n");
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
                                "select action_id,action_desc from event_action where private=0")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
                                        Body("<b>There are no actions in the database.</b>\n");
                                        ssdbFreeRequest(error_handle,req_handle);
                                }
                                else
                                {
                                        Body("<p>Choose action(s) that is(are) taken as a result of this event:<p><select name=actionid multiple size=5>\n");
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
                                                                FormatedBody("<option selected value = %d> %s",atoi(vector[0]),vector[1]);
                                                        else
                                                                FormatedBody("<option value = %d> %s",atoi(vector[0]),vector[1]);
                                                }
                                        }
                                        Body("</select>\n");
                                        Body("<input type=\"hidden\" name=act_id value=\"present\">\n");
                                        ssdbFreeRequest(error_handle,req_handle);
                                }
				Body("<p>\n");
				TableBegin("border=0 cellpadding=0 cellspacing=0");
                                RowBegin("valign=top");
					CellBegin("");
						Body("<font color=\"#666633\"><b>Tip: </b></font> 	\n");
					CellEnd();
					CellBegin("");
						Body("Several actions can be selected.<br>If you cannot find an action that you need in the list above, add it by using <a href=\"/action_add.html\">SETUP: Actions: Add</a>.	\n");
					CellEnd();
				RowEnd();
				TableEnd();
					Body("<p><INPUT TYPE=\"SUBMIT\"  VALUE=\"   Accept   \"> &nbsp;&nbsp;&nbsp; <INPUT TYPE=\"button\"  VALUE=\"   Clear   \" onClick=\"clearForm(this.form)\";>\n");
					FormatedBody("<input type=\"hidden\" name=\"hostname\" value=\"%s\">",hostname);
					FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
					FormatedBody("<input type=\"hidden\" name=\"class_desc\" value=\"%s\">",class_desc);
					FormatedBody("<input type=\"hidden\" name=\"type_desc\" value=\"%s\">",type_desc);
					FormatedBody("<input type=\"hidden\" name=\"ev_class\" value=%d>",ev_class);
					FormatedBody("<input type=\"hidden\" name=\"event_type\" value=%d>",event_type);
                        CellEnd();
                RowEnd();
        TableEnd();
        Body("</form> </body></html>\n");
}

void sgm_event_update_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
	const char **vector;
	int i, actionid,number_of_records,key,common_int,total_actions,ev_class,event_type,iterator,rec_sequence,number_of_actions[MAX_ACTIONS],flag;
	int thcount, thfreq;
	char sys_id[32],common_string[2],hostname[64],class_desc[80],type_desc[80];
	char buf[1024];
	const char *update_event="UPDATE %s -1 -1 0 0 0";

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,908))
		return;
	if(!get_variable(hError,cmdp,CHARTYPE,"hostname",hostname,&common_int,909))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"class_desc",class_desc,&common_int,910))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"type_desc",type_desc,&common_int,911))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&ev_class,912))
		return;
        if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&event_type,913))
                return;
        if(!get_variable(hError,cmdp,INTTYPE,"thcount",common_string,&thcount,914))
                return;
        /* if(!get_variable(hError,cmdp,INTTYPE,"thfreq",common_string,&thfreq,915))
                return; */


	Body("<HTML><HEAD><TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                        Body("SETUP  &gt; Events &gt; Update\n");
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
                                TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
				RowBegin("valign=top");
					CellBegin("colspan=3");
						Body("The following event(s) were updated:<p><hr><p> \n");
					CellEnd();
				RowEnd();
                                RowBegin("valign=top");
					CellBegin("");
						Body("System name\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
					CellEnd();
                                        CellBegin("");
						FormatedBody("%s",hostname);
                                        CellEnd();
				RowEnd();
				RowBegin("");
                                        CellBegin("colspan=3");
						 Body("&nbsp;\n");
					CellEnd();
                                RowEnd();
                                RowBegin("valign=top");
                                        CellBegin("");
						Body("Event class\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                FormatedBody("%s",class_desc);
					CellEnd();
                                RowEnd();
                                RowBegin("valign=top ");
                                        CellBegin("colspan=3 ");
                                                 Body("&nbsp;       \n");
                                        CellEnd();
                                RowEnd();
                                RowBegin("valign=top           ");
                                        CellBegin("");
                                                Body("Event\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
                                                FormatedBody("%s",type_desc);
					 CellEnd();
                                RowEnd();
                                RowBegin("valign=top   ");
                                        CellBegin("colspan=3 ");
                                                 Body("&nbsp;       \n");
                                        CellEnd();
                                RowEnd();
				if(!(ssdbLockTable(error_handle,connection,"event_type,event_action,event_actionref")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				RowBegin("valign=top");
                                        CellBegin("");
                                                Body("Number of event occurances prior to registration with SGI Embedded Support Partner\n");
					CellEnd();
					CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
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
						FormatedBody("%d",thcount);
					CellEnd();
				RowEnd();
				/* @row valign=top  
                                        @cell colspan=3
                                                 @ &nbsp;
                                        @endcell
                                @endrow
				@row valign=top
					@cell 
						@ Frequence of event registration
					@endcell
                                        @cell
                                                @ &nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;
                                        @endcell
					@cell
						@format "%d" thfreq
					@endcell
				@endrow */
				RowBegin("valign=top");
                                        CellBegin("colspan=3 ");
                                                 Body("&nbsp;       \n");
                                        CellEnd();
                                RowEnd();
                                RowBegin("valign=top           ");
                                        CellBegin("");
                                                Body("Actions\n");
					CellEnd();
                                        CellBegin("");
                                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                                        CellEnd();
                                        CellBegin("");
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
							if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
							"select action_desc from event_action where action_id = %d",actionid)))
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
									FormatedBody("%s<br>",vector[0]);
							}
							ssdbFreeRequest(error_handle,req_handle);
						}
					}
					CellEnd();
				RowEnd();
				RowBegin("");
					CellBegin("colspan=3");
						Body("<p><hr>\n");
					CellEnd();
				RowEnd();
				TableEnd();
			CellEnd();
		RowEnd();
	TableEnd();
	if (!ssdbUnLockTable(error_handle,connection))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}

	if(snprintf(buf,1024,update_event,hostname) > 0 && 
	   configure_event(buf))
	{
	        sscError(hError, (char*) szErrMsgEventMonDead );
		return;
	}
	

	if(configure_rule("UPDATE -1"))
	{
		sscError(hError, (char*) szErrMsgEventMonDead );
		return;
	}
	Body("</body></html>\n");
}

void sgm_update_evclass_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        const char **vector;
	int common_int;
        char sys_id[32];

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,908))
                return;
	create_help(hError,"screens_setup_events_update");
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        if (!(read_java_scritps("##SGM_EV_UPDT_CLASS","##SGM_EV_UPDT_CLASS_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");

        /* This calls the view event configuration page. Comes from view current event setup */

        Body("<form onSubmit=\"return submitType(this)\" method=\"post\" name =ChooseType action=\"/$sss/rg/libsgmserver~SGM_UPDATE_EVENT_LIST\">\n");
        Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                        Body("SETUP  &gt; Events &gt; Update\n");
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
				Body("Choose a class to find an event that you want to update:<p>\n");
				sgm_class_list(hError,session,connection,error_handle,cmdp);
				FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
			CellEnd();
		RowEnd();
	TableEnd();
}
