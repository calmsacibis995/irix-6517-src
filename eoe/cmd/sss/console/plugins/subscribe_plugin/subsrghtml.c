/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.5 $"

#include "subsrg.h"

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

void SUBSRGDisplayHeader(int, int, char *, char *, char *);
void SUBSRGDisplayFooter(int, int);
void SUBSRGDisplayConfirmation(char *, int, class_data_t *);
void SUBSRGDisplayError(int, int, __uint32_t, char *, ...);
void SUBSRGPrintOptionString(char *, int);
int MyStrTokR(char *, char *, char **);
int SUBSRGPrintSelectBox(char *, char *, char *, int, int);
void SUBSRGErr(__uint32_t);

/*---------------------------------------------------------------------------*/
/* SUBSRGDisplayHeader                                                       */
/*   Displays HTML Header information                                        */
/*---------------------------------------------------------------------------*/

void SUBSRGDisplayHeader(int displayhelp, 
                         int displayscript,
                         char *formname,
			 char *varname, 
			 char *action)
{

    Body("<html>\n");
    Body("<head>\n");
    Body("<title>\n");
    Body("System Support Software - Ver 1.0\n");
    Body("</title>\n");

    if ( displayhelp || (displayscript && formname && varname) ) {
        Body("  <script language=\"JavaScript\">\n");
        Body("    <!--\n");
    }

    if ( displayhelp ) {
        Body("function showMap()\n");
        Body("{\n");
        Body("var map=window.open('/help_subscribe.html', 'help',width=650,\n");
        Body("'height=620,status=yes,scrollbars=yes,resizable=yes');\n");
        Body("map.main=self;\n");
        Body("map.main.name=\"sss_main\";\n");
        Body("map.focus();\n");
        Body("}\n");
    }

    if ( displayscript && formname && varname ) {
	FormatedBody("function verifyData(%s)",formname);
	Body("{\n");
	FormatedBody("choice = %s.%s.selectedIndex;\n",formname,varname);
	Body("if(choice == -1)\n");
	Body(" {\n");
	Body("   msg1 = \"You must choose an option to proceed.\";\n");
	Body("   alert(msg1);\n");
	Body("   return false;\n");
	Body(" }\n");
	Body(" return true;\n");
	Body("}\n");
    }

    if ( displayhelp || displayscript ) {
        Body("//-->\n");
        Body("</script>\n");
    }

    Body("</head>\n");
    Body("<body bgcolor=\"#ffffcc\">\n");
    
    if ( displayhelp || (displayscript && formname && varname) ) {
        Body("<form onSubmit=\"return verifyData(this)\" method=POST \n");
        if ( action ) FormatedBody("action=\"%s\" ",action);
        FormatedBody("name=\"%s\">",formname);
    }

    TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
    RowBegin("");
      CellBegin("bgcolor=\"#cccc99\" width=15");
      Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
      CellEnd();
      CellBegin("bgcolor=\"#cccc99\"");
      Body("SETUP &gt; Events  &gt; Subscribe/Unsubscribe\n");
      CellEnd();
    RowEnd();
    RowBegin("");
      CellBegin("colspan=2");
      if ( displayhelp ) Body("&nbsp;\n");
      else Body("&nbsp;<p>&nbsp;\n");
      CellEnd();
    RowEnd();

    if ( displayhelp ) {
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
    }

    return;

}

/*---------------------------------------------------------------------------*/
/* SUBSRGDisplayFooter                                                       */
/*   Displays HTML Footer information                                        */
/*---------------------------------------------------------------------------*/

void SUBSRGDisplayFooter(int displayhelp,
			 int displayscript)
{
    TableEnd();

    if ( displayhelp || displayscript ) {
        Body("</form>\n");
    }

    Body("</body>\n");
    Body("</html>\n");

    return;
}

/*---------------------------------------------------------------------------*/
/* SUBSRGDisplayConfirmation                                                 */
/*   Displays confirmation of operation performed                            */
/*---------------------------------------------------------------------------*/

void SUBSRGDisplayConfirmation(char *host, int proc, class_data_t *myclass)
{
    class_data_t  *tmpclass = myclass;
    type_data_t   *tmptype;

    SUBSRGDisplayHeader(0, 0, NULL, NULL, NULL);

    RowBegin("");
    CellBegin("");
    CellEnd();
    CellBegin("");
      Body("The following events from \n");
      FormatedBody("host <b>%s</b> have been <i>%s</i> for class <b>%s</b><br>",host,(proc==SUBSCRIBEPROC?"subscribed":"unsubscribed"),tmpclass->class_desc);
      Body("<ul>\n");
      /*@format "<li> Class: %s\n" tmpclass->class_desc*/

      tmptype = tmpclass->type;
      while ( tmptype != NULL ) {
	  FormatedBody("<li> %s\n",tmptype->type_desc);
	  tmptype = tmptype->next;
      }
      Body("</ul>\n");
    CellEnd();
    RowEnd();

    SUBSRGDisplayFooter(0, 0);

    return;


}

/*---------------------------------------------------------------------------*/
/* SUBSRGDisplayError                                                        */
/*   Displays an error message.  If an error is passed, then it decodes      */
/*   the error message and prints it.                                        */
/*---------------------------------------------------------------------------*/

void SUBSRGDisplayError(int proc, int func, __uint32_t err, char *format, ...)
{

    va_list  args;
    char     errStr[1024];
    int      nobytes = 0;
    
    if ( format != NULL ) {
        va_start(args, format);
        nobytes = vsnprintf(errStr, sizeof(errStr), format, args);
    }

    SUBSRGDisplayHeader(0, 0, NULL, NULL, NULL);

    RowBegin("");
    CellBegin("");
    CellEnd();
    CellBegin("");
      Body("<font face=\"Arial,Helvetica\">\n");
      Body("The following error happened in \n");
      FormatedBody(" %s",(proc==SUBSCRIBEPROC?"Subscription":"UnSubscription"));
      Body("  while\n");
      switch(func)
      {
	  case GETCLASSDATA:
	    Body("retrieving Class Information : \n");
	    break;
	  case GETTYPEDATA:
	    Body("retrieving Event Information :\n");
	    break;
	  case PROCESSDATA:
	    Body("processing Event Information :\n");
	    break;
      }

      Body("<br>\n");
      Body("<ul>\n");
      if ( err ) {
	  Body("<li>\n");
	  FormatedBody("Error Number : %d",err);
	  Body("<li>\n");
          SUBSRGErr(err);
      }
      if (nobytes) {
          Body("<li>\n");
          FormatedBody("%s",errStr);
      }
      Body("</ul>\n");
      Body("</font>\n");
    CellEnd();
    RowEnd();

    SUBSRGDisplayFooter(0, 0);

    return;
}

/*---------------------------------------------------------------------------*/
/* SUBSRGPrintOptionString                                                   */
/*   Takes a string and converts all special characters to their relevent    */
/*   tags and prints an <option ..> for a select statement                   */
/*---------------------------------------------------------------------------*/

void SUBSRGPrintOptionString(char *str, int count)
{
    char ModStr[1024];
    int  j = 0, cnt = 0, i = 0;
    char *orgStr = NULL;

    if ( !str ) return;
    if ( count < 0 ) return;

    for ( j = 0; str[j] != 0; j++ ) {
	switch(str[j]) {
	    case '\"':
		i += snprintf(&ModStr[i], sizeof(ModStr)-i, "&quot;");
		break;
	    case '<':
		i += snprintf(&ModStr[i], sizeof(ModStr)-i, "&gt;");
		break;
	    case '>':
		i += snprintf(&ModStr[i], sizeof(ModStr)-i, "&lt;");
		break;
	    case '&':
		i += snprintf(&ModStr[i], sizeof(ModStr)-i, "&amp;");
		break;
	    default:
		ModStr[i++] = str[j];
		break;
	}

	if ( !orgStr ) {
	    if ( str[j] == '|' ) cnt++;
	    if ( cnt == count ) orgStr = str+j+1;
	}
    }

    ModStr[i] = '\0';

    FormatedBody("\t<option value=\"%s\"> %s\n",ModStr,orgStr);
    return;
}

/*---------------------------------------------------------------------------*/
/* MyStrTokR                                                                 */
/*   A rudimentary implementation of strtok_r function that doesn't modify   */
/*   original string                                                         */
/*---------------------------------------------------------------------------*/

int MyStrTokR(char *dptr, char *retbuf, char **nextptr)
{
    char *myptr = NULL;
    int  i = 0, k = 0;
    int  datalen = 0;

    if ( retbuf == NULL ) return(-1);

    if ( dptr == NULL && *nextptr == NULL ) return(-1);
    else if ( dptr == NULL ) myptr = *nextptr;
    else myptr = dptr;

    datalen = strlen(myptr);

    if ( datalen < 3 ) return(-1);
    else if ( *myptr != '%' || *(myptr+1) != '&' || *(myptr+2) != '^' )
	return(-1);
    else {
	myptr += 3;
	k += 3;
    }

    while ( *myptr ) {
	if ( k+2 <= datalen ) {
	    if ( *myptr == '%' && *(myptr+1) == '&' && *(myptr+2) == '^' ) {
		break;
	    } else {
		retbuf[i] = *myptr;
		i++;
		k++;
		myptr++;
	    }
	} else {
	    retbuf[i] = *myptr;
	    i++;
	    k++;
	    myptr++;
	}
    }

    retbuf[i] = '\0';
    (*nextptr) = myptr;
    return(i);
}

int SUBSRGCheckEntries(char *data, char *cmpdata, int func)
{
    char *tmpData = data;
    char Pclassdef[1024];
    int  numentries = -1;
    int  i = 0;
    char var1[20], var2[20];
    char *tmpSubData = NULL;

    if ( data == NULL ) return(-1);

    while ( (i = MyStrTokR(tmpData, Pclassdef, &tmpSubData)) >= 0 ) {
	if ( i == 0 ) continue;
	if ( func == GETCLASSDATA ) {
	    sscanf(Pclassdef, "%[0-9]|", var1);
	} else {
	    sscanf(Pclassdef, "%[0-9]|%[0-9]", var1, var2);
	}

	if ( cmpdata ) {
	    if ( strstr(cmpdata, var1) == NULL )  numentries++;
	} else {
	    numentries++;
	}
	tmpData = NULL;
    }

    return(numentries);
}

/*---------------------------------------------------------------------------*/
/* SUBSRGPrintSelectBox                                                      */
/*   Prints a select box for the given class/type data                       */
/*---------------------------------------------------------------------------*/

int SUBSRGPrintSelectBox(char *data, char *cmpdata, char *valname,
			 int  func,  int flag)
{

    char  *tmpData = data;
    char  Pclassdef[1024];
    char  *tmpSubData = NULL;
    int   datalen = 0, i = 0, j = 0, k = 0;
    char  var1[20], var2[20];
    char  junk[10];

    if ( data == NULL ) return(0);
    if ( !valname ) return(0);

    if ( func == GETCLASSDATA ) strcpy(junk, " ");
    else strcpy(junk, "multiple");

    FormatedBody("<select name=\"%s\" %s size=8>\n",valname,junk);

    if ( flag ) 
	Body("<option value=\"-1|All\">Select All\n");

    while ( (i = MyStrTokR(tmpData, Pclassdef, &tmpSubData)) >= 0 ) {
	if ( i == 0 ) continue;
	if ( func == GETCLASSDATA ) {
	    sscanf(Pclassdef, "%[0-9]|", var1);
	    if (SUBSClassCheck(atoi(var1),CLASSCANTBESUBSCRIBED)) {
		j = -1;
	    } else {
		j = 1;
	    }
	} else {
	    sscanf(Pclassdef, "%[0-9]|%[0-9]", var1, var2);
	    j = 2;
	}

	if ( cmpdata ) {
	    if ( strstr(cmpdata, var1) == NULL ) {
		SUBSRGPrintOptionString(Pclassdef, j);
	    }
	} else {
	    SUBSRGPrintOptionString(Pclassdef, j);
	}
        tmpData = NULL;
    }

    Body("</select>\n");
    Body("</font>\n");

    return(1);
}

/*---------------------------------------------------------------------------*/
/* SUBSRGErr                                                                 */
/* This is a decoder for errors generated by RPC Client/Server Task          */
/*---------------------------------------------------------------------------*/

void SUBSRGErr(__uint32_t  err)
{
    char         errString[1024];

    if ( err <= 0 ) return;

    if ( SGMStrErr(err, errString, sizeof(errString)) > 0 )
        FormatedBody("%s\n", errString);
    return;
}

