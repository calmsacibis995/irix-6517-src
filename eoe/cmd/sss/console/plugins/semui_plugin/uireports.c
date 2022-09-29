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

/* Some Lynx related static  variables  */

static const char szErrDateFormat  [] = "\"%s\" parameter must be in mm/dd/yyyy format. Please enter date correctly and try again.";
static const char szErrMonthFormat [] = "Invalid month specified in \"%s\" parameter. Please enter correct month and try again.";
static const char szErrDayFormat   [] = "Invalid day specified in \"%s\" parameter. Please enter correct day and try again.";
static const char szErrDatesOrder  [] = "\"Start Date\" is greater then \"End Date\"";
static const char szActionRepTitle1[] = "SYSTEM INFORMATION &gt; Actions Taken &gt; All Actions Taken";
static const char szActionRepTitle2[] = "SYSTEM INFORMATION &gt; Actions Taken &gt; Actions Taken for Specific Event";
static const char szEventRepTitle1 [] = "SYSTEM INFORMATION &gt; Events Registered &gt; Specific System Event";
static const char szEventRepTitle2 [] = "SYSTEM INFORMATION &gt; Events Registered &gt; System Events by Class";
static const char szEventRepTitle3 [] = "SYSTEM INFORMATION &gt; Events Registered &gt; All System Events";

/* ************************************ */

void event_report_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int key,event_select;
	char common_string[2];
	if(!get_variable(hError,cmdp,INTTYPE,"event_select",common_string,&event_select,400))
		return;

	if (!event_select)
		event_report_by_host(hError,session,connection,error_handle,cmdp);
	else 
		event_reports_byclass(hError,session,connection,error_handle,cmdp,event_select);
}

void event_actions_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	int key,event_select;
	char common_string[2];
	if(!get_variable(hError,cmdp,INTTYPE,"event_select",common_string,&event_select,401))
		return;

	if (!event_select)
		event_action_report(hError,session,connection,error_handle,cmdp);
	else 
		event_reports_byclass(hError,session,connection,error_handle,cmdp,event_select);
}


void display_error_html ( mySession *session, const char *szTitle,  const char *szError  )
{
   if ( session->textonly == 0 )
   {
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\"  vlink=\"#333300\" link=\"#333300\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        FormatedBody("%s",szTitle);
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;<p>&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("");
                        FormatedBody("%s",szError);
                CellEnd();
                CellBegin("");
   }
   else
   {
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
        FormatedBody("<pre>   %s</pre>",szTitle);
        Body("<hr width=100%>\n");
        FormatedBody("<p>%s",szError);
   }       
}


int IsStrBlank (char *szStr)
{
   int  len;
   
   if ( szStr == NULL ) 
        return 1;
   
   len = strlen ( szStr );
   if ( len == 0)
        return 1;
        
   if ( strspn ( szStr, " \t\r\n") == len )
        return 1;
        
   return 0;     
}


/****************************************************************************/
/* The purpose of this function is to validate date format                  */
/* it is supposed to be like this mm/dd/yyyy with white spaces also allowed */
/****************************************************************************/

int  IsValidDateFormat ( char * szDate, mySession *session, const char *szTitle, const char *szParam )
{
    int   m, d, y;
    char  szErrorText[256];
    
    if ( 3 != sscanf ( szDate, "%d/%d/%d", &m, &d, &y ))
    {
         snprintf ( szErrorText, sizeof(szErrorText), szErrDateFormat, szParam );
         display_error_html ( session, szTitle, szErrorText );
         return 0;
    }
         
    if ( m <=0 || m > 12 )
    {
         snprintf ( szErrorText, sizeof(szErrorText), szErrMonthFormat, szParam );
         display_error_html ( session, szTitle, szErrorText );
         return 0;
    }     
         
    if ( d <=0 || d > 31 )
         goto DayError;
         
    if ( m == 4 || m == 6 || m == 9 || m == 11 )
    {
        if ( d > 30 )
          goto DayError;
    }
    
    if ( m == 2 )
    {
        if ( d > 29 )
             goto DayError;
             
        if ( d == 29 )
          if ( y%4  != 0 )
             goto DayError;
    } 
    
    return 1;
    
  DayError:  
    snprintf ( szErrorText, sizeof(szErrorText), szErrDayFormat, szParam );
    display_error_html ( session, szTitle, szErrorText );
    return 0;
}

int IsDateVarValid ( const char *szVarName, CMDPAIR *cmdp , mySession *session, const char * szTitle, const char *szParamName  )
{
    int idx;
    
    idx = sscFindPairByKey ( cmdp, 0, szVarName );
    if ( idx >= 0 )
    {
       if ( IsStrBlank (cmdp[idx].value))
       {
          char szErr[128];
          sprintf ( szErr, "Parameter \"%s\" is empty or missing", szParamName );
          display_error_html ( session, szTitle, (const char *) szErr   );
          return 0;
       }
       return IsValidDateFormat ( cmdp[idx].value, session, szTitle, szParamName );
    }
    return 1; /* this error will be handled by farther processing */
}

void event_action_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle,req_handle1;
        int number_of_records,rec_sequence,key,typeid,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
        long typeval;
	time_t start_time,end_time;
	const char **vector,**vector1;
	char sys_id[32],startdate[40],enddate[40],dateout[16],event_time[16],common_string[2];

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp , session, szActionRepTitle1, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp , session, szActionRepTitle1, "End  date"  ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,402))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,403))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,404))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,405))
                return;

        start_time = maketime(startdate,START);
        end_time   = maketime(enddate,END);

        if ( end_time < start_time )
        {
          display_error_html ( session, szActionRepTitle1, szErrDatesOrder ); 
          return;
        }

        if ( session->textonly == 0 )
        {
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\"  vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SYSTEM INFORMATION &gt; Actions Taken &gt; All Actions Taken\n");
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
         }
         else
         {
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
        FormatedBody("<pre>   %s</pre>",szActionRepTitle1);
        Body("<hr width=100%>\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
         }       
                
	drop_temp_table(hError,connection,error_handle);
        if(!generate_system_info(hError,session,sys_id,connection,error_handle))
                return;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_CREATE,
        "create table temp(action_time bigint,action_args varchar(255),class_desc varchar(128),"
	"type_desc varchar(128),event_id int,type_id int,action_desc varchar(80))")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        ssdbFreeRequest(error_handle,req_handle);

        if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select dbname from archive_list")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        if ((number_of_records = getnumrecords(hError,error_handle,req_handle1)) > 0)
        {
                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                {
                        vector1 = ssdbGetRow(error_handle,req_handle1);
                        if (vector1)
                        {
				if(!(ssdbLockTable(error_handle,connection,
				"temp,%s.actions_taken,%s.event_class,%s.event_type,%s.event,%s.event_action,%s.system",
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0])))
				{
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        drop_temp_table(hError,connection,error_handle);
                                        return;
                                }

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into temp select %s.actions_taken.action_time,%s.actions_taken.action_args,"
				"%s.event_class.class_desc,%s.event_type.type_desc,%s.event.event_id,%s.event.type_id,"
				"%s.event_action.action_desc "
				"from %s.actions_taken,%s.event_class,%s.event_type,%s.event,%s.event_action,%s.system "
				"where %s.actions_taken.event_id = %s.event.event_id "
				"and %s.actions_taken.action_id = %s.event_action.action_id "
				"and %s.event.class_id = %s.event_class.class_id "
				"and %s.event.type_id = %s.event_type.type_id "
				"and %s.event.event_start >= %d "
				"and %s.event.event_end <= %d "
				"and %s.system.sys_id = '%s' "
				"and %s.event.sys_id = '%s' "
				"and %s.event_type.sys_id = '%s' "
				"and %s.event_class.sys_id = '%s' "
				"and %s.actions_taken.sys_id = '%s' "
				"and %s.event_action.private = 0 and %s.system.active=1",vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				start_time,vector1[0],end_time,vector1[0],sys_id,vector1[0],sys_id,vector1[0],sys_id,vector1[0],sys_id,
				vector1[0],sys_id,vector1[0],vector1[0])))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					drop_temp_table(hError,connection,error_handle);
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (!ssdbUnLockTable(error_handle,connection))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
			}
		}
	}
	ssdbFreeRequest(error_handle,req_handle1);
	if(!(ssdbLockTable(error_handle,connection,"temp,actions_taken,event_class,event_type,event,event_action,system")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
	"insert into temp select actions_taken.action_time,actions_taken.action_args,"
	"event_class.class_desc,event_type.type_desc,event.event_id,event.type_id,event_action.action_desc "
	"from actions_taken,event_class,event_type,event,event_action,system "
	"where actions_taken.event_id = event.event_id "
	"and actions_taken.action_id = event_action.action_id "
	"and event.class_id = event_class.class_id "
	"and event.type_id = event_type.type_id "
	"and event.event_start >= %d "
	"and event.event_end <= %d "
	"and system.sys_id = '%s' "
	"and event.sys_id = '%s' "
	"and event_type.sys_id = '%s' "
	"and event_class.sys_id = '%s' "
	"and actions_taken.sys_id = '%s' "
	"and event_action.private = 0 and system.active=1",start_time,end_time,sys_id,sys_id,sys_id,sys_id,sys_id)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}

	ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select * from temp")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                if ( session->textonly == 0 )
                {
                TableEnd();
                Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                CellEnd();
                RowEnd();
                TableEnd();
                Body("</form></body></html>\n");
                } 
                else
                { /* Lynx */
                Body("<p>There are no records for the specified time period.</form></body></html>\n");
                }
                ssdbFreeRequest(error_handle,req_handle);
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        else
        {
                if ( session->textonly == 0 )
                {
			RowBegin("");
                                CellBegin("COLSPAN=3");
                                        Body("&nbsp;\n");
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("<b>Class of Reports </B>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>All Actions</b>\n");
                                CellEnd();
                        RowEnd();
                TableEnd();
		Body("<br><hr>\n");
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
                TableBegin("BORDER=0 CELLSPACING = 0 CELLPADDING=0");
                RowBegin("");
                        CellBegin("align=right");
                        
                }
                else
                {
                        Body("<p><pre>   Class of Reports     :  All Actions</pre> \n");
                }        
                
                        total_pages = number_of_records/10;
                        if (number_of_records%10)
                                total_pages++;
                        pageno = row_num/10;
                        pageno++;

                if ( session->textonly == 0 )
                {                        
                        FormatedBody("Page %d of %d",pageno,total_pages);
                CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("");
                TableBegin("BORDER=4 CELLSPACING = 1 CELLPADDING=6");
                RowBegin("ALIGN=CENTER");
                        CellBegin("");
                                Body("<b>No.</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Event Class</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Event Description</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Event ID</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Action Description</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Action Taken</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Time of Action</b>\n");
                        CellEnd();
                RowEnd();
                }
                else
                {
                  FormatedBody("<p><pre>   Page %d of %d\n",pageno,total_pages);
                  FormatedBody("  |----------------------------------------------------------------------|\n");
                  FormatedBody("  | No. | Time of Action        | Event Class                            |\n");
                  FormatedBody("  | Event  Description                                        | Event ID |\n");
                  FormatedBody("  | Action Description                                                   |\n");
                  FormatedBody("  | Action Taken                                                         |\n");
                }
		balance_rows = row_num + 10;
                if (balance_rows > number_of_records)
                        balance_rows = number_of_records;
		ssdbRequestSeek(error_handle,req_handle,row_num,0);
                vector = ssdbGetRow(error_handle,req_handle);
                for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
                {
			if (vector)
			{
			        if ( session->textonly == 0 )
			        {
				RowBegin("valign=top");
					CellBegin("");
                                                FormatedBody("%d",rec_sequence+1);
                                        CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[2]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[3]);
					CellEnd();
					CellBegin("");
						typeval = strtol(vector[5],(char**)NULL,10);
						FormatedBody("0x%X",typeval);
					CellEnd();
					CellBegin("");
                                                FormatedBody("%s",vector[6]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[1]);
					CellEnd();
					CellBegin("");
						makedate(atoi(vector[0]),dateout,event_time);
						FormatedBody("%s  %s",dateout,event_time);
					CellEnd();
				RowEnd();
				}
				else
				{ /* Lynx */
  				  typeval = strtol(vector[5],(char**)NULL,10);
 				  makedate(atoi(vector[0]),dateout,event_time);
                                  FormatedBody("  |----------------------------------------------------------------------|\n");
                                  FormatedBody("  |%-5d| %-10.10s %-10.10s | %-39.39s|\n",rec_sequence+1,dateout,event_time,vector[2]);
                                  FormatedBody("  | %-58.58s|0x%-08X|\n",vector[3],typeval);
                                  FormatedBody("  | %-69.69s|\n",vector[6]);
                                  FormatedBody("  | %-69.69s|\n",vector[1]);
				}
			}
			vector = ssdbGetRow(error_handle,req_handle);
                }
        }
        
        
        if ( session->textonly == 0 )
        {
	RowEnd();
        TableEnd();
        } 
        else
        {
          FormatedBody ( "  ------------------------------------------------------------------------</pre>\n");
        }


        if ( session->textonly == 0 )
        {        
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
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id,startdate,enddate);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,startdate,enddate);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records -10;
					else
						bottom = number_of_records - (number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,startdate,enddate);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id,startdate,enddate);
                                }
                        CellEnd();
                        RowEnd();
                }
		TableEnd();
		CellEnd();
        RowEnd();
        TableEnd();
        
        Body("</form> </body></html>\n");
        } 
        else
        { /* Lynx */
                if(total_pages > 1)
                {
                        firstpage = ((pageno-1)/10)*10+1;
                        lastpage  = firstpage+9;
                        if (lastpage >= total_pages)
                            lastpage = total_pages;
                        if (firstpage > 1)
                        {
                            FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;&lt;</a>&nbsp;&nbsp; ",0,sys_id,startdate,enddate);
                            FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;</a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,startdate,enddate);
                        }

                        for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                        {
                            if (rec_sequence == pageno)
                                FormatedBody("<b>%d</b>&nbsp;&nbsp;",rec_sequence);
                            else
                                FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
                        }
                        if (lastpage < total_pages)
                        {
                            if(number_of_records%10 == 0)
                                 bottom = number_of_records -10;
			    else
				 bottom = number_of_records - (number_of_records%10);
				 
                            FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;    </a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,startdate,enddate);
                            FormatedBody("<a href=\"/$sss/rg/libsemserver~ACTION_TAKEN_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;&gt;</a>&nbsp;&nbsp; ",bottom,sys_id,startdate,enddate);
                        }
	          	Body("<hr width=100%>\n");
		}
		Body("<p><a href=\"/index_sem.txt.html\">Return on Main Page</a>\n");
        	Body("</form></body></html>\n");
        }
        ssdbFreeRequest(error_handle,req_handle);
	drop_temp_table(hError,connection,error_handle);
}

void event_action_specific_report(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle,req_handle1;
        int number_of_records,rec_sequence,key,typeid,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom,event_type;
        long typeval;
	time_t start_time,end_time;
	const char **vector,**vector1;
	char sys_id[32],startdate[40],enddate[40],dateout[16],event_time[16],common_string[2];

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp , session, szActionRepTitle2, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp , session, szActionRepTitle2, "End  date"  ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,406))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,407))
                return;
	if(!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,408))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,409))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&event_type,410))
                return;

	start_time = maketime(startdate,START);
        end_time = maketime(enddate,END);
        
        if ( end_time < start_time )
        {
          display_error_html ( session, szActionRepTitle2, szErrDatesOrder ); 
          return;
        }

        if ( session->textonly == 0 )
        {
	Body("<HTML> <HEAD> <TITLE>Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\"  vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SYSTEM INFORMATION &gt; Actions Taken &gt; Actions Taken for Specific Event\n");
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
                
        }
        else
        {
	Body("<HTML> <HEAD> <TITLE>Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
        FormatedBody("<pre>   %s</pre>",szActionRepTitle2);
        Body("<hr width=100%>\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
        }        

        if(!generate_system_info(hError,session,sys_id,connection,error_handle))
                return;

	drop_temp_table(hError,connection,error_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_CREATE,
        "create table temp(action_time bigint,action_args varchar(255),class_desc varchar(128),"
        "type_desc varchar(128),event_id int,type_id int,action_desc varchar(80))")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        ssdbFreeRequest(error_handle,req_handle);
        if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select dbname from archive_list")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        
       
        if ((number_of_records = getnumrecords(hError,error_handle,req_handle1)) > 0)
        {
                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                {
                        vector1 = ssdbGetRow(error_handle,req_handle1);
                        if (vector1)
                        {
				if(!(ssdbLockTable(error_handle,connection,
                                "temp,%s.actions_taken,%s.event_class,%s.event_type,%s.event,%s.event_action,%s.system",
                                vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0])))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        drop_temp_table(hError,connection,error_handle);
                                        return;
                                }

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into temp select %s.actions_taken.action_time,%s.actions_taken.action_args,"
				"%s.event_class.class_desc,%s.event_type.type_desc,%s.event.event_id,%s.event.type_id,"
				"%s.event_action.action_desc from %s.actions_taken,%s.event_class,%s.event_type,"
				"%s.event,%s.event_action,%s.system "
				"where %s.actions_taken.event_id = %s.event.event_id "
				"and %s.actions_taken.action_id = %s.event_action.action_id "
				"and %s.event.type_id = %d "
				"and %s.event.class_id = %s.event_class.class_id "
				"and %s.event.type_id = %s.event_type.type_id "
				"and %s.event.event_start >= %d "
				"and %s.event.event_end <= %d "
				"and %s.system.sys_id = '%s' "
				"and %s.event.sys_id = '%s' "
				"and %s.event_type.sys_id = '%s' "
				"and %s.event_class.sys_id = '%s' "
				"and %s.actions_taken.sys_id = '%s' "
				"and %s.event_action.private = 0 and %s.system.active=1",vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],event_type,vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],start_time, vector1[0],end_time,vector1[0], sys_id,vector1[0],sys_id,vector1[0],
				sys_id,vector1[0],sys_id,vector1[0],sys_id,vector1[0],vector1[0])))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					drop_temp_table(hError,connection,error_handle);
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (!ssdbUnLockTable(error_handle,connection))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
			}
		}
	}
	ssdbFreeRequest(error_handle,req_handle1);

	if(!(ssdbLockTable(error_handle,connection,"temp,actions_taken,event_class,event_type,event,event_action,system")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                drop_temp_table(hError,connection,error_handle);
                return;
        }

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
	"insert into temp select actions_taken.action_time,actions_taken.action_args,"
	"event_class.class_desc,event_type.type_desc,event.event_id,event.type_id,event_action.action_desc "
	"from actions_taken,event_class,event_type,event,event_action,system "
	"where actions_taken.event_id = event.event_id "
	"and actions_taken.action_id = event_action.action_id "
	"and event.type_id = %d "
	"and event.class_id = event_class.class_id "
	"and event.type_id = event_type.type_id "
	"and event.event_start >= %d "
	"and event.event_end <= %d "
	"and system.sys_id = '%s' "
	"and event.sys_id = '%s' "
	"and event_type.sys_id = '%s' "
	"and event_class.sys_id = '%s' "
	"and actions_taken.sys_id = '%s' "
	"and event_action.private = 0 and system.active=1",event_type,start_time,end_time,sys_id,sys_id,sys_id,sys_id,sys_id)))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select * from temp")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }


	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                if ( session->textonly == 0 )
                {
                TableEnd();
                Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                CellEnd();
                RowEnd();
                TableEnd();
                Body("</form></body></html>\n");
                } 
                else
                { /* Lynx */
                Body("<p>There are no records for the specified time period.</form></body></html>\n");
                }
                ssdbFreeRequest(error_handle,req_handle);
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        else
        {
                if ( session->textonly == 0 )
                {
			RowBegin("");
                                CellBegin("COLSPAN=3");
                                        Body("&nbsp;\n");
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("<b>Class of Reports </B>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>All Actions Taken for Specific Event</b>\n");
                                CellEnd();
                        RowEnd();
                TableEnd();
		Body("<br><hr>\n");
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
                TableBegin("BORDER=0 CELLSPACING = 0 CELLPADDING=0");
                RowBegin("");
                        CellBegin("align=right");
                }
                else
                {
                        Body("<p><pre>   Class of Reports     :  All Actions Taken for Specific Event</pre> \n");
                }
                
                        total_pages = number_of_records/10;
                        if (number_of_records%10)
                                total_pages++;
                        pageno = row_num/10;
                        pageno++;
                
                if ( session->textonly == 0 )
                {        
                        
                        FormatedBody("Page %d of %d",pageno,total_pages);
                CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("");
                TableBegin("BORDER=4 CELLSPACING = 1 CELLPADDING=6");
                RowBegin("ALIGN=CENTER");
                        CellBegin("");
                                Body("<b>No.</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Event Class</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Event Description</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Event ID</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Action Description</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Action Taken</b>\n");
                        CellEnd();
                        CellBegin("");
                                Body("<b>Time of Action</b>\n");
                        CellEnd();
                RowEnd();
                }
                else
                {
                  FormatedBody("<p><pre>   Page %d of %d\n",pageno,total_pages);
                  FormatedBody("  |----------------------------------------------------------------------|\n");
                  FormatedBody("  | No. | Time of Action        | Event Class                            |\n");
                  FormatedBody("  | Event  Description                                        | Event ID |\n");
                  FormatedBody("  | Action Description                                                   |\n");
                  FormatedBody("  | Action Taken                                                         |\n");
                }
                
		balance_rows = row_num + 10;
                if (balance_rows > number_of_records)
                        balance_rows = number_of_records;
		ssdbRequestSeek(error_handle,req_handle,row_num,0);
                vector = ssdbGetRow(error_handle,req_handle);
                for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
                {
			if (vector)
			{
				if ( session->textonly == 0 )
				{
				RowBegin("valign=top");
					CellBegin("");
                                                FormatedBody("%d",rec_sequence+1);
                                        CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[2]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[3]);
					CellEnd();
					CellBegin("");
						typeval = strtol(vector[5],(char**)NULL,10);
						FormatedBody("0x%X",typeval);
					CellEnd();
					CellBegin("");
                                                FormatedBody("%s",vector[6]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[1]);
					CellEnd();
					CellBegin("");
						makedate(atoi(vector[0]),dateout,event_time);
						FormatedBody("%s  %s",dateout,event_time);
					CellEnd();
				RowEnd();
				}
				else
				{
				  typeval = strtol(vector[5],(char**)NULL,10);
 				  makedate(atoi(vector[0]),dateout,event_time);
                                  FormatedBody("  |----------------------------------------------------------------------|\n");
                                  FormatedBody("  |%-5d| %-10.10s %-10.10s | %-39.39s|\n",rec_sequence+1,dateout,event_time,vector[2]);
                                  FormatedBody("  | %-58.58s|0x%-08X|\n",vector[3],typeval);
                                  FormatedBody("  | %-69.69s|\n",vector[6]);
                                  FormatedBody("  | %-69.69s|\n",vector[1]);
				}
			}
			vector = ssdbGetRow(error_handle,req_handle);
                }
        }
        
        if ( session->textonly == 0 )
        {
	RowEnd();
        TableEnd();
        }
        else
        {
          FormatedBody ( "  ------------------------------------------------------------------------</pre>\n");
        }
        
        if ( session->textonly == 0 )
        {
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
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"fisrt page\"></a>&nbsp;&nbsp; ",0,sys_id,startdate,enddate,event_type);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,startdate,enddate,event_type);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,startdate,enddate,event_type,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records -10;
					else
						bottom = number_of_records - (number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,startdate,enddate,event_type);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id,startdate,enddate,event_type);
                                }
                        CellEnd();
                        RowEnd();
                }
		TableEnd();
		CellEnd();
        RowEnd();
        TableEnd();
        Body("</form> </body></html>\n");
        }
        else
        {
                if(total_pages > 1)
                {
                       firstpage = ((pageno-1)/10)*10+1;
                       lastpage  = firstpage+9;
                       if (lastpage >= total_pages)
                           lastpage = total_pages;
                                if (firstpage > 1)
                       {
                                FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\">&lt;&lt;</a>&nbsp;&nbsp; ",0,sys_id,startdate,enddate,event_type);
                                FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\">&lt;    </a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,startdate,enddate,event_type);
                       }

                       for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                       {
                                if (rec_sequence == pageno)
                                       FormatedBody("<b>%d</b>&nbsp;&nbsp;",rec_sequence);
                                else
                                       FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,startdate,enddate,event_type,rec_sequence);
                       }
                       if (lastpage < total_pages)
                       {
                                if(number_of_records%10 == 0)
                                       bottom = number_of_records -10;
	 		        else
				       bottom = number_of_records - (number_of_records%10);
				       
                                FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\">&gt;    </a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,startdate,enddate,event_type);
                                FormatedBody("<a href=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s&event_type=%d\">&gt;&gt;</a>&nbsp;&nbsp; ",bottom,sys_id,startdate,enddate,event_type);
                       }
          	       Body("<hr width=100%>\n");
		}
		Body("<p><a href=\"/index_sem.txt.html\">Return on Main Page</a>\n");
        	Body("</form></body></html>\n");
        }
        ssdbFreeRequest(error_handle,req_handle);
	drop_temp_table(hError,connection,error_handle);
}

/* This report is a report for a particular type of event */
void event_report_type_page(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	char sys_id[32],startdate[40],enddate[40],common_string[2];
	char tmpdate[40];
	int key,common_int,event_select;
	time_t  start_time;
	time_t  end_time;

	if (!get_variable(hError,cmdp,INTTYPE,"event_select",common_string,&event_select,414))
                return;

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp , session, (event_select == 3) ? szActionRepTitle2 : szEventRepTitle1, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp , session, (event_select == 3) ? szActionRepTitle2 : szEventRepTitle1, "End date"  ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,411))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,412))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,413))
                return;

	if(event_select == 3)
                create_help(hError,"specific_action_taken1");
        else
                create_help(hError,"screens_specific_sys_event2");

        /* we are going to copy date staff in separate buffer cause maketime use strtok function internaly */
        strcpy ( tmpdate, startdate );
        start_time = maketime(tmpdate,START);
        
        strcpy ( tmpdate, enddate );
        end_time   = maketime(tmpdate,END);
        
        if ( end_time < start_time )
        {
          display_error_html ( session, (event_select == 3) ? szActionRepTitle2 : szEventRepTitle1, szErrDatesOrder ); 
          return;
        }

	if ( session->textonly == 0 )
	{
	Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
	if (!(read_java_scritps("##EVENTREPORTSPECIFIC","##EVENTREPORTSPECIFIC_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }

        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
	if(event_select == 3)
	{
		FormatedBody("<form method=POST name=\"specificEvent\" action=\"/$sss/rg/libsemserver~EVENT_ACTION_SPECIFIC_REPORT?sys_id=%s&ev_start_time=%s&ev_end_time=%s&\" onSubmit=\"return verify_submitvals();\">",sys_id,startdate,enddate);
		Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
	}
	else
	{
		FormatedBody("<form method=POST name=\"specificEvent\" action=\"/$sss/rg/libsemserver~EVENT_REPORT_BY_TYPE_HOST?sys_id=%s&ev_start_time=%s&ev_end_time=%s&\" onSubmit=\"return verify_submitvals();\">",sys_id,startdate,enddate);
                Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
	}
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
				if(event_select == 3)
					Body("SYSTEM INFORMATION &gt; Actions Taken &gt; Actions Taken for Specific Event\n");
				else
					Body("SYSTEM INFORMATION &gt; Events Registered &gt; Specific System Event\n");
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
				create_type_list(hError,session,connection,error_handle,cmdp);	
				Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
			CellEnd();
		RowEnd();
        TableEnd();
	Body("</form></body></html>\n");
	} 
	else 
	{

	Body("<HTML> <HEAD> <TITLE>Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
	if(event_select == 3)
		FormatedBody("<pre>   %s</pre>",szActionRepTitle2);
	else
		FormatedBody("<pre>   %s</pre>",szEventRepTitle1);
	Body("<hr width=100%>\n");

        FormatedBody ( "<form method=POST name=\"specificEvent\" action=\"/$sss/rg/libsemserver~%s?sys_id=%s&ev_start_time=%s&ev_end_time=%s&\" >",
                        (event_select == 3) ? "EVENT_ACTION_SPECIFIC_REPORT" : "EVENT_REPORT_BY_TYPE_HOST",
                         sys_id ,
                         startdate, 
                         enddate );
        Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
	create_type_list(hError,session,connection,error_handle,cmdp);	
	Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
	Body("</form></body></html>\n");
	}
}	

void event_reports_byclass(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp,int event_select)
{
	char startdate[40], enddate[40],sys_id[32], tmpdate[40];
	int key,common_int;
        const char *pTitle = "SYSTEM INFORMATION &gt;"; /* Just default title */
	time_t  start_time;
	time_t  end_time;

        if (event_select == 1)
	     pTitle = szEventRepTitle1;
	     
        if (event_select == 2)
	     pTitle = szEventRepTitle2;
	     
        if (event_select == 3)
	     pTitle = szActionRepTitle2;

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp, session, pTitle, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp, session, pTitle, "End date"   ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,415))
                return;
	if (!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,416))
		return;
	if (!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,417))
                return;

        /* we are going to copy date staff in separate buffer cause maketime use strtok function internaly */
        strcpy ( tmpdate, startdate );
        start_time = maketime(tmpdate,START);
        
        strcpy ( tmpdate, enddate );
        end_time   = maketime(tmpdate,END);
        
        if ( end_time < start_time )
        {
          display_error_html ( session, pTitle, szErrDatesOrder ); 
          return;
        }
        
	if(event_select == 1)
		create_help(hError,"screens_specific_sys_event1");
	if(event_select == 2)
		create_help(hError,"screens_sys_event_class");
	if(event_select == 3)
		create_help(hError,"specific_action_taken");


    if ( session->textonly == 0 )
    {
        Body("<HTML><HEAD><TITLE>Embedded Support Partner - ver.1.0</TITLE>\n");
        if (!(read_java_scritps("##EVENTREPORTBYCLASS","##EVENTREPORTBYCLASS_END")))
        {
                sscError(hError,"Error reading java script for this operation\n");
                return ;
        }
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
	if (event_select == 2)
	{
		Body("<form onSubmit=\"return verify_submitvals()\" method=POST name = EventsByClass action=\"/$sss/rg/libsemserver~EVENT_REPORT_BY_CLASS_HOST\">\n");
		Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
	}
	else if (event_select == 3)
        {
                Body("<form onSubmit=\"return verify_submitvals()\" method=POST name = EventsByClass action=\"/$sss/rg/libsemserver~EVENT_REPORT_TYPE_PAGE\">\n");
                Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
		Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
                FormatedBody("<input type=\"hidden\" name=\"event_select\" value=%d>",event_select);
        }
	else
	{
		Body("<form onSubmit=\"return verify_submitvals()\" method=POST name = EventsByClass action=\"/$sss/rg/libsemserver~EVENT_REPORT_TYPE_PAGE\">\n");
		Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
                FormatedBody("<input type=\"hidden\" name=\"event_select\" value=%d>",event_select);
	}
	
	FormatedBody("<input type=\"hidden\" name=\"ev_start_time\" value=\"%s\">",startdate);
	FormatedBody("<input type=\"hidden\" name=\"ev_end_time\" value=\"%s\">",enddate);
	FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
	
	
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
				if (event_select == 1)
					Body("SYSTEM INFORMATION &gt; Events Registered &gt; Specific System Event\n");
				if (event_select == 2)
					Body("SYSTEM INFORMATION &gt; Events Registered &gt; System Events by Class\n");
				if (event_select == 3)
					Body("SYSTEM INFORMATION &gt; Actions Taken &gt; Actions Taken for Specific Event\n");
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
				Body("Choose an event class:<p>\n");
				create_event_list(hError,session,connection,error_handle,cmdp);
			CellEnd();
		RowEnd();
        TableEnd();
   }
   else 
   { /* Lynx */
        Body("<HTML> <HEAD> <TITLE>Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
	FormatedBody("<pre>   %s</pre>",pTitle);
	Body("<hr width=100%>\n");

	if (event_select == 2)
	{
		Body("<form method=POST name = EventsByClass action=\"/$sss/rg/libsemserver~EVENT_REPORT_BY_CLASS_HOST\">\n");
		Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
	}
	else if (event_select == 3)
        {
                Body("<form method=POST name = EventsByClass action=\"/$sss/rg/libsemserver~EVENT_REPORT_TYPE_PAGE\">\n");
                Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
		Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
                FormatedBody("<input type=\"hidden\" name=\"event_select\" value=%d>",event_select);
        }
	else
	{
		Body("<form method=POST name = EventsByClass action=\"/$sss/rg/libsemserver~EVENT_REPORT_TYPE_PAGE\">\n");
		Body("<input type=\"hidden\" name=\"multiselect\" value=0>\n");
                FormatedBody("<input type=\"hidden\" name=\"event_select\" value=%d>",event_select);
	}
	FormatedBody("<input type=\"hidden\" name=\"ev_start_time\" value=\"%s\">",startdate);
	FormatedBody("<input type=\"hidden\" name=\"ev_end_time\" value=\"%s\">",enddate);
	FormatedBody("<input type=\"hidden\" name=\"sys_id\" value=\"%s\">",sys_id);
					
	Body("<p>Choose an event class:<p>\n");
	create_event_list(hError,session,connection,error_handle,cmdp); 
   }     
}

void event_report_by_type_host(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle,req_handle1;
        int number_of_records,rec_sequence,key,typeid,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
        long typeval;
        const char **vector,**vector1;
        char sys_id[32],startdate[40],enddate[40],dateout[16],event_time[16],common_string[2];
        time_t start_time,end_time;

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp , session, szEventRepTitle1, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp , session, szEventRepTitle1, "End date"  ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,418))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,419))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,420))
                return;
        if(!get_variable(hError,cmdp,INTTYPE,"event_type",common_string,&typeid,421))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,422))
                return;

	start_time = maketime(startdate,START);
        end_time   = maketime(enddate,END);

        if ( end_time < start_time )
        {
          display_error_html ( session, szEventRepTitle1, szErrDatesOrder ); 
          return;
        }

        if ( session->textonly == 0 )
        {
        Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
        Body("</HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
	Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin("");
                CellBegin("bgcolor=\"#cccc99\" width=15");
                        Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("bgcolor=\"#cccc99\"");
                        Body("SYSTEM INFORMATION &gt; Events Registered &gt; Specific System Event\n");
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
        }
        else
        {
        Body("<HTML> <HEAD> <TITLE>Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
        FormatedBody("<pre>   %s</pre>",szEventRepTitle1);
        Body("<hr width=100%>\n");
        Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
	Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
        }        
                
	drop_temp_table(hError,connection,error_handle);
        if(!generate_system_info(hError,session,sys_id,connection,error_handle))
                return;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select event_class.class_desc, event_type.type_desc from event_class,event_type "
	"where event_type.type_id = %d and event_class.class_id = event_type.class_id "
	"and event_type.sys_id = '%s' and event_class.sys_id = '%s'",typeid,sys_id,sys_id)))
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                if ( session->textonly == 0 )
                {
                TableEnd();
                Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                CellEnd();
                RowEnd();
                TableEnd();
                Body("</form></body></html>\n");
                } 
                else
                { /* Lynx */
                Body("<p>There are no records for the specified time period.</form></body></html>\n");
                }
                ssdbFreeRequest(error_handle,req_handle);
                return;
        }
	else
	{
  	   if ( session->textonly == 0 )
	   {
                vector = ssdbGetRow(error_handle,req_handle);
		if (vector)
		{
				RowBegin("");
					CellBegin("COLSPAN=3");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
				RowBegin("");
					CellBegin("");
						Body("<b>Class of Event</B>\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("<b>%s</b>",vector[0]);
					CellEnd();
				RowEnd();
				RowBegin("");
					CellBegin("");
						Body("<b>Event Description</B>\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("<b>%s</b>",vector[1]);
					CellEnd();
				RowEnd();
				RowBegin("");
					CellBegin("");
						Body("<b>Event ID</b>\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("<b>0x%X<b>",typeid);
					CellEnd();
				RowEnd();
		}
		TableEnd();
	    }   
	    else
	    { /* Lynx */
                vector = ssdbGetRow(error_handle,req_handle);
		if (vector)
		{
		FormatedBody("<p><pre>   Class of Event       : %s</pre>",vector[0]);
		FormatedBody("<pre>   Event Description    : %s</pre>",vector[1]);
		FormatedBody("<pre>   Event ID             : 0x%X</pre>",typeid);
		}
	    }
	}

	ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_CREATE,
	"create table temp(hostname varchar(64),class_desc varchar(128),type_desc varchar(128),event_start bigint,"
	"event_end bigint,event_count int,class_id int,type_id int,event_id int)")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }	
	ssdbFreeRequest(error_handle,req_handle);
	if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select dbname from archive_list")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle)); 
		drop_temp_table(hError,connection,error_handle);
                return; 
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle1)) > 0)
	{
		for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
		{
			vector1 = ssdbGetRow(error_handle,req_handle1);
			if (vector1)
			{	
				if(!(ssdbLockTable(error_handle,connection,
				"temp,%s.event_class,%s.event_type,%s.event,%s.system",vector1[0],vector1[0],vector1[0],vector1[0])))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        drop_temp_table(hError,connection,error_handle);
                                        return;
                                }

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into ssdb.temp select %s.system.hostname,%s.event_class.class_desc, %s.event_type.type_desc,"
				"%s.event.event_start,%s.event.event_end,%s.event.event_count,%s.event.class_id,%s.event.type_id,"
				"%s.event.event_id "
				"from %s.system,%s.event_class,%s.event_type,%s.event "
				"where %s.event.type_id = %d "
				"and %s.event.event_start >= %d "
				"and %s.event.event_end <= %d "
				"and %s.event_class.class_id = %s.event.class_id "
				"and %s.event.type_id = %s.event_type.type_id "
				"and %s.event.sys_id = '%s' "
				"and %s.event_class.sys_id = '%s' "
				"and %s.event_type.sys_id = '%s' "
				"and %s.system.sys_id = '%s' and %s.system.active=1",
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],typeid,vector1[0],start_time,vector1[0],
				end_time,vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],sys_id,vector1[0],sys_id,vector1[0],
				sys_id,vector1[0],sys_id,vector1[0])))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					drop_temp_table(hError,connection,error_handle);
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (!ssdbUnLockTable(error_handle,connection))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
			}
		}
	}
	ssdbFreeRequest(error_handle,req_handle1);	
	if(!(ssdbLockTable(error_handle,connection,"temp,event_class,event_type,event,system")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
	"insert into temp select system.hostname,event_class.class_desc, event_type.type_desc,"
	"event.event_start,event.event_end,event.event_count,event.class_id,event.type_id,"
	"event.event_id "
	"from system,event_class,event_type,event "
	"where event.type_id = %d "
	"and event.event_start >= %d "
	"and event.event_end <= %d "
	"and event_class.class_id = event.class_id "
	"and event.type_id = event_type.type_id "
	"and event.sys_id = '%s' "
	"and event_class.sys_id = '%s' "
	"and event_type.sys_id = '%s' "
	"and system.sys_id = '%s' and system.active=1",
	typeid,start_time,end_time,sys_id,sys_id,sys_id,sys_id)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}
	ssdbFreeRequest(error_handle,req_handle);	

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select * from temp")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                if ( session->textonly == 0 )
                {
                	Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                	CellEnd();
                	RowEnd();
                	TableEnd();
                	Body("</form></body></html>\n");
                } 
                else
                {
                	Body("<p>There are no records for the specified time period.\n");
                	Body("</form></body></html>\n");
                }
                ssdbFreeRequest(error_handle,req_handle);
		drop_temp_table(hError,connection,error_handle);
                return;
        }
	else
        {
		if ( session->textonly == 0 )
		{
			Body("<p><hr>\n");
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
                }
                else
                {
                        	total_pages = number_of_records/10;
                        	if (number_of_records%10)
                                	total_pages++;
                        	pageno = row_num/10;
                        	pageno++;
                        	FormatedBody("<p>Page %d of %d",pageno,total_pages);
                } 	
                
		if ( session->textonly == 0 )
		{
		RowBegin("");
                        CellBegin("");
                TableBegin("BORDER=4 CELLSPACING = 1 CELLPADDING=6");
                        RowBegin("ALIGN=CENTER");
                                CellBegin("");
                                        Body("<b>No.</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>First Event Occurrence</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>Last Event Occurrence</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>Event Count</b>\n");
                                CellEnd();
                        RowEnd();
                 }
                 else
                 {
                    Body("<pre>\n");
                    FormatedBody("  |----------------------------------------------------------------------|\n");
                    FormatedBody("  | No. | First Event Occurrence | Last Event Occurrence  | Event Count  |\n");
                 }
                        
			balance_rows = row_num + 10;
                        if (balance_rows > number_of_records)
                                balance_rows = number_of_records;
			ssdbRequestSeek(error_handle,req_handle,row_num,0);
			vector = ssdbGetRow(error_handle,req_handle);
                        for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
                        {
				if(vector)
				{
					if ( session->textonly == 0 )
					{
					RowBegin("valign=top");
						CellBegin("");
                                                        FormatedBody("%d",rec_sequence+1);
                                                CellEnd();
						CellBegin("");
							makedate(atoi(vector[3]),dateout,event_time);
							FormatedBody("%s  %s",dateout,event_time);
						CellEnd();
						CellBegin("");
							makedate(atoi(vector[4]),dateout,event_time);
							FormatedBody("%s  %s",dateout,event_time);
						CellEnd();
						CellBegin("align=center");
							FormatedBody("%s",vector[5]);
						CellEnd();
					RowEnd();
					}
					else
					{
                                           FormatedBody ( "  |----------------------------------------------------------------------|\n");
					   FormatedBody ( "  |%-5d|", rec_sequence+1 );
					   
					   makedate(atoi(vector[3]),dateout,event_time);
					   FormatedBody ( " %10.10s  %10.10s |", dateout, event_time );
					   
					   makedate(atoi(vector[4]),dateout,event_time);
					   FormatedBody ( " %10.10s  %10.10s |", dateout, event_time );
					   
					   FormatedBody ( " %10s   |\n", vector[5] );
					}
				}
                                vector = ssdbGetRow(error_handle,req_handle);
                        }
                if ( session->textonly == 0 )
                {        
		TableEnd();
                } 
                else
                {
                FormatedBody ( "  ------------------------------------------------------------------------</pre>\n");
                }
        }
        
        
        if ( session->textonly == 0 )
        {
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
					 FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",typeid,0,sys_id,startdate,enddate);
					 FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",typeid,(firstpage-11)*10,sys_id,startdate,enddate);
				}

				for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
				{
					if (rec_sequence == pageno)
						FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
					else
						 FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",typeid,10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
				}
				if (lastpage < total_pages)
				{
					if(number_of_records%10 == 0)
						bottom = number_of_records -10;	
					else
						bottom = number_of_records - (number_of_records%10);
					 FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",typeid,(firstpage+9)*10,sys_id,startdate,enddate);
					 FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",typeid,bottom,sys_id,startdate,enddate);
				}
			CellEnd();
			RowEnd();
		}
		TableEnd();
		CellEnd();
        RowEnd();
        TableEnd();
        Body("</form> </body></html>\n");
        }
        else
        { /* Lynx */

		if(total_pages > 1)
		{
			firstpage = ((pageno-1)/10)*10+1;
			lastpage  = firstpage+9;
			if (lastpage >= total_pages)
				lastpage = total_pages;
			if (firstpage > 1) 
			{
				FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;&lt;</a>&nbsp;&nbsp; ",typeid,0,sys_id,startdate,enddate);
				FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;    </a>&nbsp;&nbsp; ",typeid,(firstpage-11)*10,sys_id,startdate,enddate);
			}

			for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
			{
				if (rec_sequence == pageno)
					FormatedBody("<b>%d</b>&nbsp;&nbsp;",rec_sequence);
				else
					FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",typeid,10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
			}
			if (lastpage < total_pages)
			{
				if(number_of_records%10 == 0)
					bottom = number_of_records -10;	
				else
					bottom = number_of_records - (number_of_records%10);
				FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;</a>&nbsp;&nbsp; ",typeid,(firstpage+9)*10,sys_id,startdate,enddate);
				FormatedBody("<a href=\"/$sss/rg/libsemserver~TYPEREPORT_CONT?event_type=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;&gt;</a>&nbsp;&nbsp; ",typeid,bottom,sys_id,startdate,enddate);
			}
	          Body("<hr width=100%>\n");
		}
	Body("<p><a href=\"/index_sem.txt.html>Return on Main Page</a>\n");
        Body("</form> </body></html>\n");
        }
        
        
        ssdbFreeRequest(error_handle,req_handle);
	drop_temp_table(hError,connection,error_handle);
}

void event_report_by_class_host(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle,req_handle1;
        int number_of_records,rec_sequence,key,classid,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
        long typeval;
        const char **vector,**vector1;
        char sys_id[32],startdate[40],enddate[40],dateout[16],event_time[16],common_string[2];
        time_t start_time,end_time;

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp , session, szEventRepTitle2, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp , session, szEventRepTitle2, "End date"  ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,423))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,424))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,425))
                return;
        if(!get_variable(hError,cmdp,INTTYPE,"ev_class",common_string,&classid,426))
                return;
	if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,427))
                return;

	start_time = maketime(startdate,START);
        end_time = maketime(enddate,END);
        

        if ( end_time < start_time )
        {
          display_error_html ( session, szEventRepTitle2, szErrDatesOrder ); 
          return;
        }
        
        if ( session->textonly == 0 )
        {
		Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
		Body("</HEAD>\n");
		Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
		Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
		Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
		TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
		RowBegin("");
			CellBegin("bgcolor=\"#cccc99\" width=15");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("bgcolor=\"#cccc99\"");
				Body("SYSTEM INFORMATION &gt; Events Registered &gt; System Events by Class\n");
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
        }
        else
        {
		Body("<HTML> <HEAD> <TITLE>Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
		Body("<body bgcolor=\"#ffffcc\">\n");
		FormatedBody("<pre>   %s</pre>",szEventRepTitle2);
		Body("<hr width=100%>\n");
		Body("<form method=POST action=\"/$sss/rg/libsemserver\">\n");
		Body("<input type=\"hidden\" name=\"row_num\" value=0>\n");
        }        
                
	drop_temp_table(hError,connection,error_handle);
	if(!generate_system_info(hError,session,sys_id,connection,error_handle))
                return;
	
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select class_desc from event_class where class_id = %d and sys_id = '%s'",classid,sys_id)))
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                if ( session->textonly == 0 )
                {
                Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                CellEnd();
                RowEnd();
                TableEnd();
                Body("</form></body></html>\n");
                } 
                else
                { /* Lynx */
                Body("<p>There are no records for the specified time period.</form></body></html>\n");
                }
                ssdbFreeRequest(error_handle,req_handle);
                return;
        }
	else
	{
   		vector = ssdbGetRow(error_handle,req_handle);	
		if (vector)
		{
                	if ( session->textonly == 0 )
                	{
				RowBegin("");
					CellBegin("COLSPAN=3");
						Body("&nbsp;\n");
					CellEnd();
				RowEnd();
				RowBegin("");
					CellBegin("");
						Body("<b>Class of Event</B>\n");
					CellEnd();
					CellBegin("");
						Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
					CellEnd();
					CellBegin("");
						FormatedBody("<B>%s</B>",vector[0]);
					CellEnd();
				RowEnd();
			}
			else
			{
				FormatedBody("<p><pre>   Class of Event       : %s</pre>\n",vector[0]);
			}		
		}
		if ( session->textonly == 0 )
		{
		TableEnd();
		}
		ssdbFreeRequest(error_handle,req_handle);
	}


	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_CREATE,
        "create table temp(hostname varchar(64),class_desc varchar(128),type_desc varchar(128),event_start bigint,"
	"event_end bigint,event_count int,class_id int,type_id int,event_id int)")))
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        ssdbFreeRequest(error_handle,req_handle);
	if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select dbname from archive_list")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        if ((number_of_records = getnumrecords(hError,error_handle,req_handle1)) > 0)
        {
                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                {
                        vector1 = ssdbGetRow(error_handle,req_handle1);
                        if (vector1)
                        {
				if(!(ssdbLockTable(error_handle,connection,
				"temp,%s.event_class,%s.event_type,%s.event,%s.system",vector1[0],vector1[0],vector1[0],vector1[0])))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        drop_temp_table(hError,connection,error_handle);
                                        return;
                                }

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into ssdb.temp select %s.system.hostname,%s.event_class.class_desc,%s.event_type.type_desc,"
				"%s.event.event_start,%s.event.event_end,%s.event.event_count,%s.event.class_id,%s.event.type_id,%s.event.event_id "
				"from %s.system,%s.event_class,%s.event_type,%s.event "
				"where %s.event_class.class_id = %d "
				"and %s.event.event_start >= %d "
				"and %s.event.event_end <= %d "
				"and %s.event_class.class_id = %s.event.class_id "
				"and %s.event.type_id = %s.event_type.type_id "
				"and %s.event.sys_id = '%s' "
				"and %s.event_class.sys_id = '%s' "
				"and %s.event_type.sys_id = '%s' "
				"and %s.system.sys_id = '%s' and %s.system.active=1",
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],classid,vector1[0],start_time,vector1[0],end_time,vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],sys_id,vector1[0],sys_id,vector1[0],sys_id,vector1[0],sys_id,
				vector1[0])))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					drop_temp_table(hError,connection,error_handle);
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (!ssdbUnLockTable(error_handle,connection))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
			}
                }
        }
        ssdbFreeRequest(error_handle,req_handle1);     
	if(!(ssdbLockTable(error_handle,connection,"temp,event_class,event_type,event,system")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
	"insert into temp select system.hostname,event_class.class_desc, event_type.type_desc,"
	"event.event_start,event.event_end,event.event_count,event.class_id,event.type_id,event.event_id "
	"from system,event_class,event_type,event "
	"where event_class.class_id = %d "
	"and event.event_start >= %d "
	"and event.event_end <= %d "
	"and event_class.class_id = event.class_id "
	"and event.type_id = event_type.type_id "
	"and event.sys_id = '%s' "
	"and event_class.sys_id = '%s' "
	"and event_type.sys_id = '%s' "
	"and system.sys_id = '%s' and system.active = 1",classid,start_time,end_time,sys_id,sys_id,sys_id,sys_id)))
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        ssdbFreeRequest(error_handle,req_handle);
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select * from temp order by event_id")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
                if ( session->textonly == 0 )
                {
                Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
                CellEnd();
                RowEnd();
                TableEnd();
                Body("</form></body></html>\n");
                } 
                else
                { /* Lynx */
                Body("<p>There are no records for the specified time period.</form></body></html>\n");
                }
                ssdbFreeRequest(error_handle,req_handle);
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        else
        {
        	if ( session->textonly == 0 )
        	{ 
                Body("<p><hr>\n");
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
				CellBegin("");
                                        Body("<b>No.</b>\n");
                                CellEnd();
				CellBegin("");
					Body("<b>Event Description</b>\n");
				CellEnd();
				CellBegin("");
					Body("<b>Event ID</b>\n");
				CellEnd();
				CellBegin("");
					Body("<b>First Event Occurrence</b>\n");
				CellEnd();
				CellBegin("");
					Body("<b>Last Event Occurrence</b>\n");
				CellEnd();
				CellBegin("");
					Body("<b>Event Count</b>\n");
				CellEnd();
			RowEnd();
		}
		else
		{
                total_pages = number_of_records/10;
                if (number_of_records%10) 
                    total_pages++;
                pageno = row_num/10;
                pageno++;
                
                FormatedBody("<p>Page %d of %d",pageno,total_pages);
                Body("<pre>\n");
                FormatedBody("  |----------------------------------------------------------------------|\n");
                FormatedBody("  | No. | Event Description                             |   Event ID     |\n");
                FormatedBody("  |     | First Event Occurrence | Last Event Occurrence|   Event Count  |\n");
                }
			
			balance_rows = row_num + 10;
			if (balance_rows > number_of_records)
				balance_rows = number_of_records;
			ssdbRequestSeek(error_handle,req_handle,row_num,0);
			vector = ssdbGetRow(error_handle,req_handle);
			for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
			{
			   if (vector)
			   {
				if ( session->textonly == 0 )
				{
					RowBegin("valign=top");
						CellBegin("");
                                                        FormatedBody("%d",rec_sequence+1);
						CellEnd();
						CellBegin("");
							FormatedBody("%s",vector[2]);
						CellEnd();
						CellBegin("");
							typeval = strtol(vector[7],(char**)NULL,10);
							FormatedBody("0x%X",typeval);
						CellEnd();
						CellBegin("");
							makedate(atoi(vector[3]),dateout,event_time);
							FormatedBody("%s  %s",dateout,event_time);
						CellEnd();
						CellBegin("");
							makedate(atoi(vector[4]),dateout,event_time);
							FormatedBody("%s  %s",dateout,event_time);
						CellEnd();
						CellBegin("align=center");
							FormatedBody("%s",vector[5]);
						CellEnd();
					RowEnd();
				}
				else
				{    /* Lynx */
				     char  date_first[16], date_last[16];
				     char  time_first[16], time_last[16];
				    
				     typeval = strtol(vector[7],(char**)NULL,10);
  				     makedate(atoi(vector[3]),date_first,time_first);
 				     makedate(atoi(vector[4]),date_last ,time_last );
 				     
                                     FormatedBody( "  |----------------------------------------------------------------------|\n");
                                     FormatedBody( "  |%-5d| %-46.46s|   0x%-08X   |\n", rec_sequence+1, vector[2], typeval  );
                                     FormatedBody( "  |     | %-10.10s  %-8.8s   | %-10.10s  %-8.8s |   %-12s |\n",
                                                      date_first, time_first, date_last, time_last, vector[5] );
                                                     
				}
					
			   }
			   vector = ssdbGetRow(error_handle,req_handle);
			}
                if ( session->textonly == 0 )
                {        
		TableEnd();
                } 
                else
                {
                FormatedBody ( "  ------------------------------------------------------------------------</pre>\n");
                }
		ssdbFreeRequest(error_handle,req_handle);
        }
        
 	if ( session->textonly == 0 )
 	{
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
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",classid,0,sys_id,startdate,enddate);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",classid,(firstpage-11)*10,sys_id,startdate,enddate);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",classid,10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records -10;
					else
						bottom = number_of_records - (number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",classid,(firstpage+9)*10,sys_id,startdate,enddate);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",classid,bottom,sys_id,startdate,enddate);
                                }
                        CellEnd();
                        RowEnd();
                }
		TableEnd();
	CellEnd();
        RowEnd();
        TableEnd();
        Body("</form> </body></html>\n");
        }
        else
        { /* Lynx */
		if(total_pages > 1)
                {
                     firstpage = ((pageno-1)/10)*10+1;
                     lastpage  = firstpage+9;
                     if (lastpage >= total_pages)
                         lastpage = total_pages;
                     if (firstpage > 1)
                     {
                        FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;&lt;</a>&nbsp;&nbsp; ",classid,0,sys_id,startdate,enddate);
                        FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;    </a>&nbsp;&nbsp; ",classid,(firstpage-11)*10,sys_id,startdate,enddate);
                     }

                     for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                     {
                        if (rec_sequence == pageno)
                            FormatedBody("<b>%d</b>&nbsp;&nbsp;",rec_sequence);
                        else
                            FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",classid,10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
                     }
                     if (lastpage < total_pages)
                     {
                        if(number_of_records%10 == 0)
                           bottom = number_of_records -10;
			else
	    		   bottom = number_of_records - (number_of_records%10);
	    		   
                        FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;    </a>&nbsp;&nbsp; ",classid,(firstpage+9)*10,sys_id,startdate,enddate);
                        FormatedBody("<a href=\"/$sss/rg/libsemserver~CLASSREPORT_CONT?ev_class=%d&row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;&gt;</a>&nbsp;&nbsp; ",classid,bottom,sys_id,startdate,enddate);
                     }
	          Body("<hr width=100%>\n");
		}
	        Body("<p><a href=\"/index_sem.txt.html\">Return on Main Page</a>\n");
                Body("</form> </body></html>\n");
        }
	ssdbFreeRequest(error_handle,req_handle);
	drop_temp_table(hError,connection,error_handle);
}


void event_report_by_host(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle,req_handle1;
        int number_of_records,rec_sequence,key,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
	long typeval;
        const char **vector,**vector1;
	char sys_id[32],startdate[40],enddate[40],dateout[16],event_time[16],common_string[2];
	time_t start_time,end_time;

        if ( session->textonly != 0 )
        {
         if ( !IsDateVarValid    ( "ev_start_time", cmdp , session, szEventRepTitle3, "Start date" ))
               return ;
         if ( !IsDateVarValid    ( "ev_end_time"  , cmdp , session, szEventRepTitle3, "End date"  ))
               return ;
        }  

	if(!get_variable(hError,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,428))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_start_time",startdate,&common_int,429))
                return;
        if(!get_variable(hError,cmdp,CHARTYPE,"ev_end_time",enddate,&common_int,430))
                return;
        if(!get_variable(hError,cmdp,INTTYPE,"row_num",common_string,&row_num,431))
                return;

	start_time = maketime(startdate,START);
	end_time   = maketime(enddate,END);

	
        if ( end_time < start_time )
        {
          display_error_html ( session, szEventRepTitle3, szErrDatesOrder ); 
          return;
        }
     
        if ( session->textonly == 0 )
        {
	Body("<HTML><HEAD><TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\"  vlink=\"#333300\" link=\"#333300\">\n");
	Body("<form method = post action=\"/$sss/rg/libsemserver\">\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
	RowBegin("");
		CellBegin("bgcolor=\"#cccc99\" width=15");
			Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
		CellEnd();
		CellBegin("bgcolor=\"#cccc99\"");
			Body("SYSTEM INFORMATION &gt; Events Registered &gt; All System Events\n");
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
	}
	else
	{ /* Lynx */
	Body("<HTML><HEAD><TITLE>Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\">\n");
	FormatedBody("<pre>   %s</pre>",szEventRepTitle3);
	Body("<hr width=100%>\n");
	Body("<form method = post action=\"/$sss/rg/libsemserver\">\n");
	}	
		
	drop_temp_table(hError,connection,error_handle);

	if(!generate_system_info(hError,session, sys_id,connection,error_handle))
		return;


	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_CREATE,
        "create table temp(hostname varchar(64),sys_serial varchar(16),sys_type int,ip_addr varchar(16),"
        "class_desc varchar(128),type_desc varchar(128),event_start bigint,event_end bigint,event_count int,class_id int,type_id int)")))
        {
                sscError(hError,"1Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        ssdbFreeRequest(error_handle,req_handle);
        if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select dbname from archive_list")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
        if ((number_of_records = getnumrecords(hError,error_handle,req_handle1)) > 0)
        {
                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                {
                        vector1 = ssdbGetRow(error_handle,req_handle1);
                        if (vector1)
                        {
				if(!(ssdbLockTable(error_handle,connection,
				"temp,%s.event_class,%s.event_type,%s.event,%s.system",vector1[0],vector1[0],vector1[0],vector1[0])))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        drop_temp_table(hError,connection,error_handle);
                                        return;
                                }
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into temp select %s.system.hostname,%s.system.sys_serial,%s.system.sys_type,%s.system.ip_addr, "
				"%s.event_class.class_desc,%s.event_type.type_desc,"
				"%s.event.event_start,%s.event.event_end,%s.event.event_count,%s.event.class_id,%s.event.type_id "
				"from %s.event_class,%s.event_type,%s.event,%s.system "
				"where %s.event.type_id = %s.event_type.type_id "
				"and %s.event.class_id = %s.event_class.class_id "
				"and %s.event.event_start >= %d "
				"and %s.event.event_end <= %d "
				"and %s.event.sys_id = '%s' "
				"and %s.event_class.sys_id = '%s' "
				"and %s.event_type.sys_id = '%s' "
				"and %s.system.sys_id = '%s' and %s.system.active = 1",
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
				start_time,vector1[0],end_time,vector1[0],sys_id,vector1[0],sys_id,vector1[0],sys_id,vector1[0],sys_id,
				vector1[0])))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					drop_temp_table(hError,connection,error_handle);
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (!ssdbUnLockTable(error_handle,connection))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
			}
                }
        }
	ssdbFreeRequest(error_handle,req_handle1);
	if(!(ssdbLockTable(error_handle,connection,"temp,event_class,event_type,event,system")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
	"insert into temp select system.hostname,system.sys_serial,system.sys_type,system.ip_addr, "
	"event_class.class_desc, event_type.type_desc,"
	"event.event_start,event.event_end,event.event_count,event.class_id,event.type_id "
	"from event_class,event_type,event,system "
	"where event.type_id = event_type.type_id "
	"and event.class_id = event_class.class_id "
	"and event.event_start >= %d "
	"and event.event_end <= %d "
	"and event.sys_id = '%s' "
	"and event_class.sys_id = '%s' "
	"and event_type.sys_id = '%s' "
	"and system.sys_id = '%s' and system.active = 1",start_time,end_time,sys_id,sys_id,sys_id,sys_id)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
		return;
	}
	ssdbFreeRequest(error_handle,req_handle);

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select * from temp")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		drop_temp_table(hError,connection,error_handle);
                return;
        }
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
	    if ( session->textonly == 0 )
	    {
		TableEnd();
                Body("<p><hr><p><b>There are no records for the specified time period.</b>\n");
		CellEnd();
		RowEnd();
		TableEnd();
		Body("</form></body></html>\n");
	    }
	    else
	    {
                Body("<p>There are no records for the specified time period.\n");
		Body("</form></body></html>\n");
	    }
		ssdbFreeRequest(error_handle,req_handle);
		drop_temp_table(hError,connection,error_handle);
		return;
	}
	else
        {
	    if ( session->textonly == 0 )
	    {
                        RowBegin("");
                                CellBegin("COLSPAN=3");
                                        Body("&nbsp;\n");
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("<b>Class of Event</B>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>All events</b>\n");
                                CellEnd();
                        RowEnd();
                TableEnd();
            } 
            else    
            { /* Lynx */
              Body("<p><pre>   Class of Event       : All events</pre>\n");
            }
                
                
	    if ( session->textonly == 0 )
	    {
                Body("<br><hr>\n");
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
				CellBegin("");
					Body("<b>No.</b>\n");
				CellEnd();
                                CellBegin("");
                                        Body("<b>Event Class</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>Event Description</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>Event ID</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>First Occurrence</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>Last Occurrence</b>\n");
                                CellEnd();
                                CellBegin("");
                                        Body("<b>Event Count</b>\n");
                                CellEnd();
                        RowEnd();
            } 
            else    
            {   /* Lynx */
            
                total_pages = number_of_records/10;
                if (number_of_records%10) 
                    total_pages++;
                pageno = row_num/10;
                pageno++;
                
                FormatedBody("<p>Page %d of %d",pageno,total_pages);
                FormatedBody("<pre>  |----------------------------------------------------------------------|</pre>");
                FormatedBody("<pre>  | No. | Event Class                     |  First Occurrence | Event ID |</pre>");
                FormatedBody("<pre>  |     | Event Description               |  Last  Occurrence |  Count   |</pre>");
                FormatedBody("<pre>  |----------------------------------------------------------------------|</pre>");
            }

		balance_rows = row_num + 10;
		if (balance_rows > number_of_records)
			balance_rows = number_of_records;
		ssdbRequestSeek(error_handle,req_handle,row_num,0);
                vector = ssdbGetRow(error_handle,req_handle);
                for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
                {
			if(vector)
			{
			        if ( session->textonly == 0 )
			        {
				RowBegin("valign=top");
					CellBegin("");
						FormatedBody("%d",rec_sequence+1);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[4]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[5]);
					CellEnd();
					CellBegin("");
						typeval = strtol(vector[10],(char**)NULL,10);
						FormatedBody("0x%X",typeval);
					CellEnd();
					CellBegin("");
						makedate(atoi(vector[6]),dateout,event_time);
						FormatedBody("%s  %s",dateout,event_time);

					CellEnd();
					CellBegin("");
						makedate(atoi(vector[7]),dateout,event_time);
						FormatedBody("%s  %s",dateout,event_time);
					CellEnd();
					CellBegin("align=center");
						FormatedBody("%s",vector[8]);
					CellEnd();
				RowEnd();
				}
				else
				{    /* Lynx */
				     char  date_first[16], date_last[16];
				     char  time_first[16], time_last[16];
				    
				     typeval = strtol(vector[10],(char**)NULL,10);
  				     makedate(atoi(vector[6]),date_first,time_first);
 				     makedate(atoi(vector[7]),date_last ,time_last );
				     
                                     FormatedBody ( "<pre>  |%-5d| %-32.32s|%-10.10s %-8.8s|0x%-08X|</pre>",
                                                     rec_sequence+1, vector[4], date_first, time_first, typeval );
                                                     
                                     FormatedBody ( "<pre>  |     | %-32.32s|%-10.10s %-8.8s|%9.9s |</pre>",
						     vector[5], date_last, time_last, vector[8] );
                                     FormatedBody ( "<pre>  |----------------------------------------------------------------------|</pre>");
				}
			}
                        vector = ssdbGetRow(error_handle,req_handle);
                }
		ssdbFreeRequest(error_handle,req_handle);
        } 

       if ( session->textonly == 0 )
       {
	RowEnd();
	TableEnd();
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
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"first page\"></a>&nbsp;&nbsp; ",0,sys_id,startdate,enddate);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,startdate,enddate);
                                }

                                for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                                {
                                        if (rec_sequence == pageno)
                                                FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                        else
                                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
                                }
                                if (lastpage < total_pages)
                                {
                                        if(number_of_records%10 == 0)
                                                bottom = number_of_records - 10;
					else
						bottom = number_of_records - (number_of_records%10);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,startdate,enddate);
                                         FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",bottom,sys_id,startdate,enddate);
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
       else
       { /* Lynx */
                if(total_pages > 1)
                {
                        firstpage = ((pageno-1)/10)*10+1;
                        lastpage  = firstpage+9;
                        if (lastpage >= total_pages)
                                lastpage = total_pages;
                        if (firstpage > 1)
                        {
                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;&lt;</a>&nbsp;&nbsp; ",0,sys_id,startdate,enddate);
                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&lt;</a>&nbsp;&nbsp; ",(firstpage-11)*10,sys_id,startdate,enddate);
                        }

                        for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
                        {
                                if (rec_sequence == pageno)
                                        FormatedBody("<b>%d</b></font>&nbsp;&nbsp;",rec_sequence);
                                else
                                        FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
                        }
                        if (lastpage < total_pages)
                        {
                                if(number_of_records%10 == 0)
                                        bottom = number_of_records - 10;
				else
					bottom = number_of_records - (number_of_records%10);
                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;</a>&nbsp;&nbsp; ",(firstpage+9)*10,sys_id,startdate,enddate);
                                 FormatedBody("<a href=\"/$sss/rg/libsemserver~EVREPORT_CONT?row_num=%d&sys_id=%s&ev_start_time=%s&ev_end_time=%s\">&gt;&gt;</a>&nbsp;&nbsp; ",bottom,sys_id,startdate,enddate);
                        }
	          Body("<hr width=100%>\n");
		}
	Body("<p><a href=\"/index_sem.txt.html\">Return on Main Page</a>\n");
	Body("</form></body></html>\n");
       }	
       ssdbFreeRequest(error_handle,req_handle);
       drop_temp_table(hError,connection,error_handle);
}

void drop_temp_table(sscErrorHandle hError,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{
	int number_of_records,rec_sequence;
	const char **vector5,**vector6;
	ssdb_Request_Handle req_handle5,req_handle6;	
	if (!ssdbUnLockTable(error_handle,connection))
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
        if (!(req_handle5 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SHOW,
        "show tables")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        if ((number_of_records = getnumrecords(hError,error_handle,req_handle5)) > 0)
        {
                for (rec_sequence=0;rec_sequence < number_of_records; rec_sequence++)
                {
                        vector5 = ssdbGetRow(error_handle,req_handle5);
                        if (vector5)
                        {
                                if(!strcmp(vector5[0],"temp"))
                                {
                                        if (!(req_handle6 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DROPTABLE,
                                        "drop table temp")))
                                        {
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                                return;
                                        }
                                        ssdbFreeRequest(error_handle,req_handle6);
                                }
                        }
                }
        }
        ssdbFreeRequest(error_handle,req_handle5);
}

int generate_system_info(sscErrorHandle hError, mySession *session, char *sys_id,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle)
{
        int number_of_records;
        const char **vector;
	ssdb_Request_Handle req_handle;
        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select * from system where sys_id = '%s' and active=1",sys_id)))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return 0;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
             if ( session->textonly == 0 )
             {
                Body("<b>The System for this serial number does not exist.</b>\n");
             } 
             else 
             {
                Body("<p>The System for this serial number does not exist.\n");
             }
		ssdbFreeRequest(error_handle,req_handle);
                return 0;
        }
        else
        {   
             if ( session->textonly == 0 )
             {
                TableBegin("BORDER=0 CELLSPACING=0 CELLPADDING=0");
                vector = ssdbGetRow(error_handle,req_handle);
		if (vector)
		{
                        RowBegin("");
                                CellBegin("");
                                        Body("System name\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        FormatedBody("%s",vector[4]);
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("System ID\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        FormatedBody(sys_id);
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("System serial number\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        FormatedBody("%s",vector[3]);
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("System IP type\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        FormatedBody("IP%s",vector[2]);
                                CellEnd();
                        RowEnd();
                        RowBegin("");
                                CellBegin("");
                                        Body("System IP address\n");
                                CellEnd();
                                CellBegin("");
                                        Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
                                CellEnd();
                                CellBegin("");
                                        FormatedBody("%s",vector[5]);
                                CellEnd();
                        RowEnd();
		} /* End of vector */
	     } 
	     else
	     {  /* Lynx */
                vector = ssdbGetRow(error_handle,req_handle);
		if (vector)
		{
                  FormatedBody ( "<pre>   System name          : %s</pre>"  , vector[4] );
                  FormatedBody ( "<pre>   System ID            : %s</pre>"  , sys_id    );
                  FormatedBody ( "<pre>   System serial number : %s</pre>"  , vector[3] );
                  FormatedBody ( "<pre>   System IP type       : IP%s</pre>", vector[2] );
                  FormatedBody ( "<pre>   System IP address    : %s</pre>"  , vector[5] );
		} 
	     }
		
	ssdbFreeRequest(error_handle,req_handle);
        }
        return 1;
}

void makedate (time_t timeval, char *dateout, char *event_time)
{
        struct tm *timeptr;
        time_t timein = timeval;
        timeptr = localtime(&timein);
        strftime(dateout,20,"%m/%d/%Y",timeptr);
        strftime(event_time,20,"%X",timeptr);
}

long maketime(const char *dateval1,int startend)
{
        time_t timevalue;
        struct tm timestruct;
        struct tm *timeptr;
        char dateval[40];
        int i;
        char *p;
        strcpy (dateval,dateval1);
        p = strtok(dateval,"/");
        i = atoi(p);
        i = i - 1;
        timestruct.tm_mon = i;
        p = strtok(NULL,"/");
        timestruct.tm_mday = atoi(p);
        p = strtok(NULL,"/");
        i = atoi(p);
        i = i - 1900;
        timestruct.tm_year = i;
	timestruct.tm_isdst = -1;
        if (!startend)
        {
                timestruct.tm_sec = 00;
                timestruct.tm_min = 00;
                timestruct.tm_hour = 00;
        }
        else
        {
                timestruct.tm_sec = 60;
                timestruct.tm_min = 59;
                timestruct.tm_hour = 23;
        }
        timevalue =  mktime(&timestruct);
        return (timevalue);
}
