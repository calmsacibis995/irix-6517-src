#include "ui.h"
#include <errno.h>

void archive_db_confirm(mySession *session, char *name, int type)
{
     if ( session->textonly == 0 )
     {
	Body("<html><head><title>SGI Embedded Support Partner - ver.1.0</title>\n");
	Body("<body bgcolor=\"#E3E6D8\">\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
	RowBegin("");
		CellBegin("bgcolor=\"#C7CDB5\" width=\"15\"");
			Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
		CellEnd();
		CellBegin("bgcolor=\"#C7CDB5\"");
			Body("Archive Database\n");
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
			FormatedBody("Archive <b>%s</b> was successfully removed.",name);
		CellEnd();
        RowEnd();
	TableEnd();
	Body("</body></html>\n");
     } 
     else
     {
	Body("<html><head><title>SGI Embedded Support Partner - ver.1.0</title>\n");
	Body("<body bgcolor=\"#E3E6D8\">\n");
        Body("<pre>   Archive Database</pre>\n");
	Body("<hr width=100%>\n");
	FormatedBody("<p>Archive <b>%s</b> was successfully removed.",name);
	Body("</body></html>\n");
     }
}

void archive_list(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        const char **vector;
        int number_of_records,rec_sequence;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select arc_id,dbname,startdate,enddate from archive_list order by arc_id")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        
        if((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
	{
	        if ( session->textonly  == 0 ) 
	        {
   		   Body("Choose the database to be deleted:&nbsp;\n");
	           Body("<select name=\"ssdbs\">\n");
		   Body("<option value=0>Database Archive Name(s)\n");
		}
		else
		{
   		   Body("Choose the database to be deleted:\n");
	           Body("<p><select name=\"ssdbs\">\n");
		   Body("<option value=0>Database Archive Name(s)\n\n");
		}
		for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
		{
			vector = ssdbGetRow(error_handle,req_handle);
			if(vector)
				FormatedBody("<option value=\"%s\">%s (%s - %s)\n",vector[1],vector[1],vector[2],vector[3]);
		}
		ssdbFreeRequest(error_handle,req_handle);
		Body("</select>\n");
		Body("<p><INPUT TYPE=\"SUBMIT\" VALUE=\"   Delete Database   \">\n");
	}	
	else
	{
	        if ( session->textonly == 0 ) 
	        {
		Body("<b>There are no archives at this time.</b>\n");
		}
		else
		{
		Body("<p>There are no archives at this time.\n");
		}
	}
}

void archive_table(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        const char **vector;
        int number_of_records,rec_sequence;
	char startdate[40],event_time[16],enddate[40];


        if ( session->textonly == 0 ) 
        {
	TableBegin("border=4 cellpadding=6 cellspacing=1");
	RowBegin("align=center");
		CellBegin("colspan=2 ");
			Body("<b>Database Name</b>\n");
		CellEnd();
		CellBegin("");
			Body("<b>Start Date</b>\n");
		CellEnd();
		CellBegin("");
                        Body("<b>End  Date</b>\n");
                CellEnd();
	RowEnd();
	RowBegin("");
		CellBegin("");
			Body("<input type=\"radio\" name=\"dbname\" value=\"ssdb\" checked>\n");
		CellEnd();
		CellBegin("");
			Body("Active Database\n");
		CellEnd();
		CellBegin("");
	} 
	else 
	{
   	        Body("<p><pre>\n");
	        FormatedBody("   -------------------------------------------------------\n");
	        FormatedBody("   |     Database Name     |  Start Date  |  End  Date   |\n");
	        FormatedBody("   |-----------------------------------------------------|\n");
		FormatedBody("   |<input type=\"radio\" name=\"dbname\" value=\"ssdb\"> | Active Database |");
	}
	
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select event_start from event where event_id=1")))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			if((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
			{
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
 				     makedate(atoi(vector[0]),startdate,event_time);
				     if ( session->textonly == 0 )
					FormatedBody("%s",startdate);
				     else 
					FormatedBody("  %-10.10s  |",startdate);
				}
				else
				{
				     if ( session->textonly == 0 )
					Body("Current	\n");
				     else
				        FormatedBody("   Current    |");
				}
			}
			else
			{
				     if ( session->textonly == 0 )
					Body("Current	\n");
				     else
				        FormatedBody("   Current    |");
			}
			ssdbFreeRequest(error_handle,req_handle);			

        if ( session->textonly == 0 )
        {
		CellEnd();
		CellBegin("");
			Body("Current\n");
		CellEnd();
	RowEnd();
	}
	else
	{
         FormatedBody("   Current    |\n");
	}
	
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select arc_id,dbname,startdate,enddate from archive_list order by arc_id")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		if((number_of_records = getnumrecords(hError,error_handle,req_handle)) > 0)
		{
			for(rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
			{
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
				   if ( session->textonly == 0 )
				   {
					RowBegin("");
					CellBegin("");
						FormatedBody("<input type=\"radio\" name=\"dbname\" value=\"%s\">",vector[1]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[1]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[2]);
					CellEnd();
					CellBegin("");
						FormatedBody("%s",vector[3]);
					CellEnd();
					RowEnd();
				    }
				    else
				    {
	                                       FormatedBody("   |-----------------------------------------------------|\n");
		                               FormatedBody("   |<input type=\"radio\" name=\"dbname\" value=\"%s\"> |",vector[1]);
		                               FormatedBody(" %-15.15s |",vector[1]);
		                               
					       FormatedBody("  %-10.10s  |",vector[2]);
					       
					       FormatedBody("  %-10.10s  |\n",vector[3]);
				    }
				}
			}
			ssdbFreeRequest(error_handle,req_handle);
		}
	if ( session->textonly == 0 )
	{
	TableEnd();
	}
	else
	{
	  FormatedBody("   -------------------------------------------------------</pre>\n");
	}
}

void delete_archive(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
        ssdb_Request_Handle req_handle;
        const char **vector;
        int number_of_records,rec_sequence,common_int;
	char dbnames[80];
	
	if(!get_variable(hError,cmdp,CHARTYPE,"ssdbs",dbnames,&common_int,1000))
                return;

        /* We are going to delete archive logicaly */
        req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
        "delete from archive_list where dbname='%s'",dbnames);
	if ( req_handle == NULL )
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
	ssdbFreeRequest(error_handle,req_handle);

        /* Then physicaly */
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DROPDATABASE,
        "drop database if exists %s",dbnames)))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
	ssdbFreeRequest(error_handle,req_handle);

    
        /* Then References */
        req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
	"delete from mysql.db where Db='%s'",dbnames);    
	if ( req_handle == NULL )
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
	ssdbFreeRequest(error_handle,req_handle);

        req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE, 
        "delete from mysql.host  where Db='%s'",dbnames);
	if (req_handle == NULL ) 
        {       
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));        
                return; 
        }
	ssdbFreeRequest(error_handle,req_handle);

	system("/usr/sbin/ssdbadmin reload");
	archive_db_confirm(session,dbnames,0);
}	
