/*
 * Copyright 1992-1998 Silicon Graphics, Inc.
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

/*
 * Internal Routines.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sss/ssdbapi.h>
#include <sss/ssdberr.h>
#include "ssdbidata.h"
#include "ssdbiutil.h"
#include "ssdbosdep.h"


/* ----------------- ssdbiu_process_SELECT_Results ---------------------- */
int ssdbiu_process_SELECT_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ MYSQL_FIELD *field;
  int i;
  MYSQL *mysql = &con->mysql;
  int retcode = 0; /* Error retcode */

  if((req->res = mysql_store_result(mysql)) != 0)
  { req->row_strlength    = 0;
    req->row_bytelength   = 0;
    req->num_rows         = mysql_num_rows(req->res);
    req->columns          = mysql_num_fields(req->res);
    req->cur_field_number = 0;
    req->cur_row_number   = 0;
    req->current_row_ptr  = 0;

    if(req->num_rows && req->columns)
    { /* Here we ONLY collect information about results */
      if(ssdbiu_allocRequestFieldInfo(req,req->columns))
      {	mysql_data_seek(req->res,0);
        mysql_field_seek(req->res,0);
        for(i = 0;i < req->columns;i++)
        { if((field = mysql_fetch_field(req->res)) == 0) break;
          if((req->field_info_ptr[i].byte_len = ssdbiu_getFieldByteSize(field->type,&req->field_info_ptr[i].Fpconvert)) <= 0)
           req->field_info_ptr[i].byte_len = field->max_length;
          req->field_info_ptr[i].mysql_type   = field->type;	
	  req->field_info_ptr[i].len          = field->length;
	  req->field_info_ptr[i].maxlen       = field->max_length;
	  req->row_strlength                  += field->length;
	  req->row_bytelength                 += req->field_info_ptr[i].byte_len;
	  strncpy(req->field_info_ptr[i].name, field->name,SSDB_MAX_FIELDNAME_SIZE);	
	  req->field_info_ptr[i].name[SSDB_MAX_FIELDNAME_SIZE] = 0;
	}
	mysql_field_seek(req->res,0);
	if(field)
	{ if((req->current_row_ptr = mysql_fetch_row(req->res)) != 0) retcode++; /* Success */
	}
      }
    }
    else { if(!req->num_rows) retcode++; }

    if(!retcode)
    { if(req->res)
      {	mysql_free_result(req->res);  
	req->res            = 0;
	req->row_strlength  = 0;
	req->row_bytelength = 0;
	req->num_rows       = 0;
	req->columns        = 0;
      }
    }
  } /* end of if((req->res = mysql_store_result(mysql)) != 0) */
  return retcode;
}
/* ------------------ ssdbiu_process_AFFECTED_rows ---------------------- */
static int ssdbiu_process_AFFECTED_rows(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ MYSQL *mysql = &con->mysql;
  int retcode = 0;
  if(mysql) { req->num_rows = mysql->affected_rows; retcode++; }
  return retcode;
}
/* ----------------- ssdbiu_process_Delete_Results ---------------------- */
int ssdbiu_process_DELETE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ----------------- ssdbiu_process_INSERT_Results ---------------------- */
int ssdbiu_process_INSERT_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ---------------- ssdbiu_process_UPDATE_Results ----------------------- */
int ssdbiu_process_UPDATE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ----------------- ssdbiu_process_SHOW_Results ------------------------ */
int ssdbiu_process_SHOW_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_SELECT_Results(con,req);
}
/* ---------------- ssdbiu_process_REPLACE_Results ---------------------- */
int ssdbiu_process_REPLACE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* --------------- ssdbiu_process_DROPTABLE_Results --------------------- */
int ssdbiu_process_DROPTABLE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ---------------- ssdbiu_process_CREATE_Results ----------------------- */
int ssdbiu_process_CREATE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ---------------- ssdbiu_process_DESCRIBE_Results --------------------- */
int ssdbiu_process_DESCRIBE_Results (ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_SELECT_Results(con,req);
}
/* ------------------ ssdbiu_process_LOCK_Results ----------------------- */
int ssdbiu_process_LOCK_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ------------------ ssdbiu_process_UNLOCK_Results --------------------- */
int ssdbiu_process_UNLOCK_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ------------------ ssdbiu_process_ALTER_Results ---------------------- */
int ssdbiu_process_ALTER_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
/* ------------- ssdbiu_process_DROPDATABASE_Results -------------------- */
int ssdbiu_process_DROPDATABASE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ return ssdbiu_process_AFFECTED_rows(con,req);
}
