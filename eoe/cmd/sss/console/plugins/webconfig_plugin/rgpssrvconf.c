/* --------------------------------------------------------------------------- */
/* -                             RGPSSRVCONF.C                               - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
/* $Revision: 1.9 $ */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <malloc.h>

#include <ssdbapi.h>
#include <ssdberr.h>
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include "sscShared.h"
#include "sscErrors.h"

#if 0
#define LVA_XCODE_INCLUDE 1 /* Include xcode support */
#endif
/* --------------------------------------------------------------------------- */
#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif
/* --------------------------------------------------------------------------- */
#define MYVERSION_MAJOR    1          /* Report Generator PlugIn Major Version number */
#define MYVERSION_MINOR    1          /* Report Generator PlugIn Minor Version number */
#define MAXNUMBERS_OF_KEYS 778        /* Max size of Pair array */
/* --------------------------------------------------------------------------- */
#define RGP_DEFAULTSSDB_DBNAME  "ssdb"       /* Default Database name */
#define RGP_DEFAULTSSDB_TABLE   "ssrv_ipaddr"/* Default "IP filter info" Table name */
#define RGP_DEFAULTSSDB_KTABLE  "key_table"  /* Default "Key" Table name */
#define RGP_SSDBCLIENT_NAME     ""           /* SSDB Connect Client name */
#define RGP_SSDBCLIENT_PASSWORD ""           /* SSDB Connect Client password */
#define LOCAL_SSDB_NEWCLIENTFLAGS (SSDB_CLIFLG_ENABLEDEFUNAME|SSDB_CLIFLG_ENABLEDEFPASWD) /* Create new SSDB client flags */
#define LOCAL_SSDB_NEWCONNECTFLAGS (SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT|SSDB_CONFLG_DEFAULTHOSTNAME) /* Create new SSDB connection flags */
/* --------------------------------------------------------------------------- */
/* Local "Session" structure */
typedef struct sss_s_MYSESSION {
  unsigned long signature;            /* sizeof(mySession) */
  struct sss_s_MYSESSION *next;       /* next pointer */
  int textonly;                       /* text mode only flag */
  int argc;                           /* argc (command string) */
  char **argv;                        /* argv (command string) */
} mySession;
/* --------------------------------------------------------------------------- */
static const char myLogo[]                     = "ESP WEB Server Configuration plugin (legalov@sgi.com)";
static const char szVersion[]                  = "Version";            /* Obligatory attribute name: What current version of this plugin */
static const char szTitle[]                    = "Title";              /* Obligatory attribute name: What real name of this plugin */
static const char szThreadSafe[]               = "ThreadSafe";         /* Obligatory attribute name: Is this plugin threadsafe */
static const char szUnloadable[]               = "Unloadable";         /* Obligatory attribute name: Is this plugin unlodable */
static const char szUnloadTime[]               = "UnloadTime";         /* Obligatory attribute name: What unload time for this plugin */
static const char szAcceptRawCmdString[]       = "AcceptRawCmdString"; /* Obligatory attribute name: how plugin will process cmd parameters */

static const char szDatabaseName[]             = RGP_DEFAULTSSDB_DBNAME;     /* DB name */
static const char szDbTableName[]              = RGP_DEFAULTSSDB_TABLE;      /* "IP filter info" Table name */
static const char szDbKeyTableName[]           = RGP_DEFAULTSSDB_KTABLE;     /* "Key" Table name */

static char szServerNameErrorPrefix[]          = "ESP WEB Server Config PlugIn Error: %s";
static char szServerNameErrorPrefix0[]         = "ESP WEB Server Config PlugIn Error: ";
static char szPercent4s[]                      = "%s%s%s%s";
static char szPercent3xs[]                     = "%s%s '%s'";
static const char szInvalidActionStr1[]        = "Invalid \'action\' string format - need variable name or file name as additional argument(s)";
static const char szErrorMsgIncorrectSess[]    = "Incorrect session id in rgpgGenerateReport()";
static const char szDestroyHTMLGen[]           = "Can't destroy the HTML generator";
static const char szErrMsgCantAllocErrHandle[] = "Can't alloc SSDB error handle";
static const char szErrMsgCantAllocCliHandle[] = "Can't alloc SSDB client handle";
static const char szErrMsgCantOpenConnection[] = "Can't open SSDB connection handle";
static const char szErrMsgExecRequest[]        = "Exec SSDB request error";
static const char szErrMsgGetNumRec[]          = "SSDB Get number of records error";
static const char szErrMsgGetNumCol[]          = "SSDB Get number of columns error";
static const char szErrMsgGetNexField[]        = "SSDB Get Next Field error";
static const char szErrMsgSetROAttr[]          = "Attempt to set read only attribute in rgpgSetAttribute()";
static const char szErrMsgIncorrectArgs[]      = "Incorrect argument(s)";
static const char szErrMsgCantOpenHtmlGen[]    = "Can't open the HTML generator";
static const char szErrMsgNoMemory[]           = "No memory to create session in rgpgCreateSesion()";
static const char szCantFindArgsBegin[]        = "Can't find '";
static const char szCantFindArgsEnd[]          = "' in argument(s) list";
static const char szIncorrectFileNameInReqStr[]= "Incorrect file name in request string";
static const char szInvalidResourcePath[]      = "Invalid resource path in 'switcher' module";
static const char szCantOpenFile[]             = "Can't open file";
static const char szCantFindFile[]             = "Can't find file";
static const char szReadFileError[]            = "Read file error";
static const char szCantProcessFile[]          = "Can't process file from request command string.";
static const char szErrMsgUnknownReqType[]     = "Unknown request type: <b>'%s'</b><br>";
static const char szIncorrectKeyString[]       = "Incorrect 'Key' string";
static const char szIncorrectUsername[]        = "Incorrect User name!<br>";
static const char szIncorrectNewPassword[]     = "Incorrect 'New' Password!<br>";
static const char szIncorrectOldPassword[]     = "Incorrect 'Old' Password!<br>";
static const char szUsernameChanged[]          = "UserName successfuly changed";
static const char szPasswordChanged[]          = "Password successfuly changed";
static char szUsageInfo[]                      = "Usage:<br>\n\
/$sss/rg[s]/%s~&lt;request_method&gt;[~&lt;request_argument(s)&gt;]<br>\n\
where:<br>\n\
&lt;request_method&gt; - \"websrvport\"|\"websrvident\"|\"websrvtitle\"|\"websrvversion\"|\"getwhiteiplist\"|\"getdarkiplist\"|<br>\n\
\"websrvport\" - Get SGI Adjustable WEB server port number<br>\n\
\"websrvident\" - Get SGI Adjustable WEB server identification string<br>\n\
\"websrvtitle\" - Get SGI Adjustable WEB server title string<br>\n\
\"websrvversion\" - Get SGI Adjustable WEB server version string<br>\n\
\"getwhiteiplist\" - Get WEB server \"white\" IP address list<br>\n\
\"getdarkiplist\" - Get WEB server \"dark\" IP address list<br>\n\
\"delwhiteip\"|\"deldarkip\"|\"addwhiteip\"|\"adddarkip\"|\"setuname\"|\"setpasswd\"<br>\n\
\"delwhiteip\" - Delete \"white\" ip address<br>\n\
delwhiteip~&lt;IP_addr_variable_name&gt;~conform_page_begin_html~conform_page_end_html<br>\n\
\"deldarkip\" - Delete \"dark\" ip address<br>\n\
deldarkip~&lt;IP_addr_variable_name&gt;~conform_page_begin_html~conform_page_end_html<br>\n\
\"addwhiteip\" - Add \"white\" ip address<br>\n\
addwhiteip~&lt;IP_addr_variable_name&gt;~conform_page_begin_html~conform_page_end_html<br>\n\
\"adddarkip\" - Add \"dark\" ip address<br>\n\
adddarkip~&lt;IP_addr_variable_name&gt;~conform_page_begin_html~conform_page_end_html<br>\n\
\"setuname\" - Change \"user name\"<br>\n\
\"setpasswd\" - Change \"password\"<br>\n\
</P><BR><ADDRESS><A HREF=\"mailto:legalov@sgi.com\">legalov@sgi.com</A></ADDRESS>";
/* --------------------------------------------------------------------------- */
static const char szPageInvalidNewUsername[]       = "<HTML><HEAD><TITLE>Invalid Username</TITLE></HEAD>\
<H1>Invalid Username</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
You enter incorrect <B>\"UserName\"</B> or it is too short.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szUserAgent[]                = "User-Agent";
static const char szLynx[]                     = "Lynx";
static char szVersionStr[64];

static pthread_mutex_t seshFreeListmutex;
static pthread_mutex_t ipaddrlist_mutex;
static const char szUnloadTimeValue[]         = "60";  /* Unload time for this plug in (sec.) */
static const char szThreadSafeValue[]         = "1";   /* "Thread Safe" flag value - this plugin is thread safe */
static const char szUnloadableValue[]         = "1";   /* "Unloadable" flag value - RG core might unload this plugin from memory */
static const char szAcceptRawCmdStringValue[] = "1";   /* RG server accept "raw" command string like key=value&key2=value&... */

static int volatile mutex_inited      = 0;   /* seshFreeListmutex mutex inited flag */
static mySession *sesFreeList         = 0;   /* Session free list */

/* ----------------------- make_sockaddr ----------------------------------- */
static int make_sockaddr(struct sockaddr_un *s_saddr,const char *sock_path)
{ int addrlen = 0;
  if(s_saddr)
  { memset((char *)s_saddr,0,sizeof(struct sockaddr_un));
    if(sock_path) strcpy(s_saddr->sun_path,sock_path);
    s_saddr->sun_family = AF_UNIX;
    addrlen = strlen(s_saddr->sun_path) + sizeof(s_saddr->sun_family);
  }
  return addrlen;
}
/* -------------------------- closeSocket ---------------------------------- */
void closeSocket(int s)
{ for(;;) if(!close(s) || (errno != EINTR)) break;
}
/* ------------------------- declare_reload -------------------------------- */
static void declare_reload(void)
{ struct sockaddr_un su_addr;
  char sockname[256],buf[8];
  int j,s;
  if(sscGetSharedString(3,sockname,sizeof(sockname)))
  { if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) >= 0)
    { buf[0] = 'L'; buf[1] = 'V'; buf[2] = 'A'; buf[3] = 'R';
      for(j = 0;j < 2;j++)
      { if(sendto(s,buf,4,0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,sockname)) == 4) break;
        sleep(1);
      }
      closeSocket(s);
    }
  }
}
/* -------------------------- xsEncodeBuf ------------------------------------ */
static int xsEncodeBuf(const char *in,char *out,int sizeto,unsigned long key)
{ unsigned char c;
  int i,j,size,retcode = 0;

  size = in ? strlen(in) : 0;

  if(size >= 0 && size <= 0xffff && out && sizeto >= ((size*2)+4))
  { retcode = sprintf(out,"%04lX",(unsigned long)size);
    out += retcode;
    key ^= (0x65193EA5+(size*size));
    if(size & 1)
    { for(i = 0;i < size;i++)
      { c = (unsigned char)in[i] ^ ((unsigned char)((key >> ((i&0xf)+(i&0x7)+(i&0x3)))+(size-i)));
        j = sprintf(out,"%02X",(int)(c&0xff)); retcode += j; out += j;
      }
    }
    else
    { for(i = 0;i < size;i++)
      { c = (unsigned char)in[i] ^ ((unsigned char)((key >> ((i&0xf)+(i&0x7)+(i&0x2)))+(size+i)));
        j = sprintf(out,"%02X",(int)(c&0xff)); retcode += j; out += j;
      }
    }
  }
  return retcode;
}
/* -------------------------- xsDecodeBuf ------------------------------------ */
static int xsDecodeBuf(const char *frombuf,char *out,int sizeto,unsigned long key)
{ char *in,c;
  int size,i,j;

  if(((in  = (char*)frombuf) == 0) || (strlen(frombuf) < 4) || !out) return 0;
  for(i = 0;i < 4;i++) if(!isxdigit(in[i])) return 0;
  c = in[4]; in[4] = 0; i = sscanf(in,"%x",&size); in[4] = c; in += 4;
  if(i != 1 || size > sizeto) return 0;
  for(i = 0;i < (size*2);i++) if(!isxdigit(in[i])) return 0;

  key ^= (0x65193EA5+(size*size));

  if(size & 1)
  { for(i = 0;i < size;i++)
    { c = in[2]; in[2] = 0; sscanf(in,"%x",&j); in[2] = c; in += 2;
      out[i] = (char)((unsigned char)(j&0xff) ^ ((unsigned char)((key >> ((i&0xf)+(i&0x7)+(i&0x3)))+(size-i))));
    }
  }
  else
  { for(i = 0;i < size;i++)
    { c = in[2]; in[2] = 0; sscanf(in,"%x",&j); in[2] = c; in += 2;
      out[i] = (char)((unsigned char)(j&0xff) ^ ((unsigned char)((key >> ((i&0xf)+(i&0x7)+(i&0x2)))+(size+i))));
    }
  }
  out[i] = 0;
  return size;
}
/* -------------------- deleteLocalHTMLGenerator ----------------------------- */
static void deleteLocalHTMLGenerator(sscErrorHandle hError)
{ if(deleteMyHTMLGenerator()) sscError(hError,szServerNameErrorPrefix,szDestroyHTMLGen);
}
/* ----------------------------- rgpgInit ------------------------------------ */
int RGPGAPI rgpgInit(sscErrorHandle hError)
{ /* Try initialize "free list of mySession" mutex */
  if(pthread_mutex_init(&seshFreeListmutex,0))
  { sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex \"seshFreeListmutex\"");
    return 0; /* error */
  }
  if(pthread_mutex_init(&ipaddrlist_mutex,0))
  { pthread_mutex_destroy(&seshFreeListmutex);
    sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex \"ipaddrlist_mutex\"");
    return 0; /* error */
  }

  mutex_inited++;
#ifdef INCLUDE_TIME_DATE_STRINGS
#ifdef LVA_XCODE_INCLUDE
  snprintf(szVersionStr,sizeof(szVersionStr),"%d.%d(x) %s %s",MYVERSION_MAJOR,MYVERSION_MINOR,__DATE__,__TIME__);
#else
  snprintf(szVersionStr,sizeof(szVersionStr),"%d.%d %s %s",MYVERSION_MAJOR,MYVERSION_MINOR,__DATE__,__TIME__);
#endif
#else
#ifdef LVA_XCODE_INCLUDE
  snprintf(szVersionStr,sizeof(szVersionStr),"%d.%d(x)",MYVERSION_MAJOR,MYVERSION_MINOR);
#else
  snprintf(szVersionStr,sizeof(szVersionStr),"%d.%d",MYVERSION_MAJOR,MYVERSION_MINOR);
#endif
#endif
  return 1; /* success */
}
/* ----------------------------- rgpgDone ------------------------------------ */
int RGPGAPI rgpgDone(sscErrorHandle hError)
{ mySession *s;

  if(mutex_inited)
  { pthread_mutex_destroy(&seshFreeListmutex);
    pthread_mutex_destroy(&ipaddrlist_mutex);
    mutex_inited = 0;
    while((s = sesFreeList) != 0)
    { sesFreeList = s->next;
      free(s);
    }
    return 1; /* success */
  }
  return 0; /* error - not inited */
}
/* -------------------------- rgpgCreateSesion ------------------------------- */
rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError)
{  mySession *s;
   pthread_mutex_lock(&seshFreeListmutex);
   if((s = sesFreeList) != 0) sesFreeList = s->next;
   pthread_mutex_unlock(&seshFreeListmutex);
   if(!s) s = (mySession*)malloc(sizeof(mySession));
   if(!s) 
   { sscError(hError,szServerNameErrorPrefix,szErrMsgNoMemory);
     return 0;
   }
   memset(s,0,sizeof(mySession));
   s->signature = sizeof(mySession);
   return (rgpgSessionID)s;
}
/* ------------------------- rgpgDeleteSesion -------------------------------- */
void RGPGAPI rgpgDeleteSesion(sscErrorHandle hError,rgpgSessionID _s)
{ mySession *s = (mySession *)_s;
  if(!s || s->signature != sizeof(mySession))
  { sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgDeleteSesion()");
  }
  else
  { pthread_mutex_lock(&seshFreeListmutex);
    s->next = sesFreeList; sesFreeList = s; s->signature = 0;
    pthread_mutex_unlock(&seshFreeListmutex);
  }
}
/* ------------------------ rgpgGetAttribute --------------------------------- */
char *RGPGAPI rgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session, const char *attributeID, const char *extraAttrSpec,int *typeattr)
{  char *s;

   if(typeattr) *typeattr = RGATTRTYPE_STATIC;
        if(!strcasecmp(attributeID,szVersion))            s = (char*)szVersionStr;
   else if(!strcasecmp(attributeID,szTitle))              s = (char*)myLogo; 
   else if(!strcasecmp(attributeID,szUnloadTime))         s = (char*)szUnloadTimeValue;
   else if(!strcasecmp(attributeID,szThreadSafe))         s = (char*)szThreadSafeValue;
   else if(!strcasecmp(attributeID,szUnloadable))         s = (char*)szUnloadableValue;
   else if(!strcasecmp(attributeID,szAcceptRawCmdString)) s = (char*)szAcceptRawCmdStringValue;
   else 
   { sscError(hError,szServerNameErrorPrefix,"No such attribute");
     sscError(hError,"'%s' in rgpgGetAttribute()",attributeID);
     return 0;
   }
   if(!s) sscError(hError,szServerNameErrorPrefix,"No memory in rgpgGetAttribute()");
   return s;
}
/* --------------------- rgpgFreeAttributeString ------------------------------ */
void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,char *attrString,int restype)
{  if(((restype == RGATTRTYPE_SPECIALALLOC) || (restype == RGATTRTYPE_MALLOCED)) && attrString) free(attrString);
}
/* ------------------------ rgpgSetAttribute --------------------------------- */
void RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value)
{  mySession *sess = (mySession *)session;

   if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle) ||
      !strcasecmp(attributeID,szThreadSafe) || !strcasecmp(attributeID,szUnloadable) ||
      !strcasecmp(attributeID,szUnloadTime))
   { sscError(hError,szServerNameErrorPrefix,szErrMsgSetROAttr);
     return;
   }

   if(!strcasecmp(attributeID,szUserAgent) && value) 
   { sess->textonly = (strcasecmp(value,szLynx) == 0) ? 1 : 0;
   }
#if 0
   if(!sess || sess->signature != sizeof(mySession))
   { sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGetAttribute()");
     return;
   }
#endif
}
/* ---------------------- getipaddroptionlist -------------------------------- */
static void getipaddroptionlist(sscErrorHandle hErr,mySession *sess,int enabled)
{ int i,rows_cnt,col_cnt,err;
  char ip_string[256];
  ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;

  pthread_mutex_lock(&ipaddrlist_mutex); /* begin of critical section */

  if((hError = ssdbCreateErrorHandle()) != 0)
  { if((hClient = ssdbNewClient(hError,RGP_SSDBCLIENT_NAME,RGP_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) != 0)
    { if((hConnect = ssdbOpenConnection(hError,hClient,NULL,szDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) != 0)
      { if((hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_SELECT,"select ipaddrstr from %s where enabled %s",szDbTableName,enabled ? "> 0" : "<= 0")) != 0)
	{ rows_cnt = ssdbGetNumRecords(hError,hRequest);
          if(ssdbGetLastErrorCode(hError) == SSDBERR_SUCCESS)
	  { col_cnt = ssdbGetNumColumns(hError,hRequest);
            if((ssdbGetLastErrorCode(hError) == SSDBERR_SUCCESS) && (col_cnt == 1))
	    { for(err = (i = 0);i < rows_cnt;i++)
              { if(!ssdbGetNextField(hError,hRequest,ip_string,sizeof(ip_string)-1)) { err++; break; }
                FormatedBody("<option value=\"%s\">%s\n",ip_string,ip_string);
              } 
              if(err) sscError(hErr,szServerNameErrorPrefix,szErrMsgGetNexField);
	    }
            else sscError(hErr,szServerNameErrorPrefix,szErrMsgGetNumCol);
	  }
          else sscError(hErr,szServerNameErrorPrefix,szErrMsgGetNumRec);
	  ssdbFreeRequest(hError,hRequest);
	}
 	else sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest);
	ssdbCloseConnection(hError,hConnect);
      }
      else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantOpenConnection);
      ssdbDeleteClient(hError,hClient);
    }
    else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocCliHandle);
    ssdbDeleteErrorHandle(hError);    
  }
  else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocErrHandle);

  pthread_mutex_unlock(&ipaddrlist_mutex); /* end of critical section */
}
/* ----------------------- testFileExist ------------------------------------ */
static int testFileExist(sscErrorHandle hErr,const char *fname)
{ struct stat statbuf;
  char tmp[2049];
  int i,retcode = 0;

  if(fname && fname[0])
  { if((i = sscGetSharedString(2,tmp,sizeof(tmp))) != 0)
    { if(tmp[i-1] != '/' || tmp[i-1] != '\\') strcat(tmp,"/");
      strcat(tmp,fname);
      if(stat(tmp,&statbuf) || !S_ISREG(statbuf.st_mode)) sscError(hErr,"%s%s \"%s\"",szServerNameErrorPrefix0,szCantFindFile,tmp);
      else retcode++;
    }
  }
  else sscError(hErr,szServerNameErrorPrefix,szIncorrectFileNameInReqStr);
  return retcode;
}
/* --------------------- addFileToOutputStream ------------------------------- */
static int addFileToOutputStream(sscErrorHandle hErr,const char *fname)
{ struct stat statbuf;
  char tmp[2049];
  FILE *fp;
  int i,j,retcode = 0;
  if(fname && fname[0])
  { if((i = sscGetSharedString(2,tmp,sizeof(tmp))) != 0)
    { if(tmp[i-1] != '/' || tmp[i-1] != '\\') strcat(tmp,"/");
      strcat(tmp,fname);
      if(stat(tmp,&statbuf) || !S_ISREG(statbuf.st_mode) || (fp = fopen(tmp,"rb")) == NULL)
      { sscError(hErr,"%s%s \"%s\"",szServerNameErrorPrefix0,szCantOpenFile,tmp);
      }
      else 
      { for(i = (int)statbuf.st_size;i;i -= j)
        { if((j = (sizeof(tmp)-1)) > i) j = i;
          if(fread(tmp,1,j,fp) != j) break;
          tmp[j] = 0;
          Body(tmp);
        }
        if(i) sscError(hErr,szServerNameErrorPrefix,szReadFileError);
        else retcode++;
      }
    }
    else sscError(hErr,szServerNameErrorPrefix,szInvalidResourcePath);
  }
  else sscError(hErr,szServerNameErrorPrefix,szIncorrectFileNameInReqStr);
  return retcode;
}
/* ------------------------- delipaddrlist ----------------------------------- */
static int delipaddrlist(sscErrorHandle hErr,mySession *sess,int enabled,CMDPAIR *cmdp)
{ int i,action_cnt;
  ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;
  char *valuename,*bfname,*efname;

  if(sess->argc < 5) return 1;

  valuename = sess->argv[2];
  bfname    = sess->argv[3];
  efname    = sess->argv[4];

  if(!sscGetPairCounterByKey(cmdp,valuename))
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,valuename,szCantFindArgsEnd);
    return 0;
  }

  if(!testFileExist(hErr,bfname) || !testFileExist(hErr,efname)) return 0;

  if(!addFileToOutputStream(hErr,bfname))
  { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,bfname);
    return 0;
  }

  action_cnt = 0;

  pthread_mutex_lock(&ipaddrlist_mutex); /* begin of critical section */

  if((hError = ssdbCreateErrorHandle()) != 0)
  { if((hClient = ssdbNewClient(hError,RGP_SSDBCLIENT_NAME,RGP_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) != 0)
    { if((hConnect = ssdbOpenConnection(hError,hClient,NULL,szDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) != 0)
      { UListBegin(0);
        for(i = 0;(i = sscFindPairByKey(cmdp,i,valuename)) >= 0;i++)
        { if((hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_DELETE,
                         "delete from %s where ipaddrstr=\"%s\" and enabled %s",szDbTableName,cmdp[i].value,enabled ? "> 0" : "<= 0")) != 0)
	  { FormatedBody("<li>%s\n",cmdp[i].value);
            ssdbFreeRequest(hError,hRequest);
            action_cnt++;
	  }
 	  else sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest);
        }
        UListEnd();
        ssdbCloseConnection(hError,hConnect);
      }
      else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantOpenConnection);
      ssdbDeleteClient(hError,hClient);
    }
    else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocCliHandle);
    ssdbDeleteErrorHandle(hError);    
  }
  else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocErrHandle);

  if(!addFileToOutputStream(hErr,efname)) sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,efname);

  pthread_mutex_unlock(&ipaddrlist_mutex); /* end of critical section */

  if(action_cnt) declare_reload();

  return 0;
}
/* ------------------------- addipaddrlist ----------------------------------- */
static int addipaddrlist(sscErrorHandle hErr,mySession *sess,int enabled,CMDPAIR *cmdp)
{ int i,row_cnt,err,action_cnt;
  ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;
  char *valuename,*bfname,*efname;

  if(sess->argc < 5) return 1;

  valuename = sess->argv[2];
  bfname    = sess->argv[3];
  efname    = sess->argv[4];

  if(!sscGetPairCounterByKey(cmdp,valuename))
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,valuename,szCantFindArgsEnd);
    return 0;
  }
  
  if(!testFileExist(hErr,bfname) || !testFileExist(hErr,efname)) return 0;

  action_cnt = 0;

  if(!addFileToOutputStream(hErr,bfname))
  { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,bfname);
    return 0;
  }

  pthread_mutex_lock(&ipaddrlist_mutex); /* begin of critical section */

  if((hError = ssdbCreateErrorHandle()) != 0)
  { if((hClient = ssdbNewClient(hError,RGP_SSDBCLIENT_NAME,RGP_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) != 0)
    { if((hConnect = ssdbOpenConnection(hError,hClient,NULL,szDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) != 0)
      { UListBegin(0);
        for(i = (err = 0);(i = sscFindPairByKey(cmdp,i,valuename)) >= 0 && !err;i++)
        { if((hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_SELECT,
                         "select ipaddrstr from %s where ipaddrstr=\"%s\" and enabled %s",
                         szDbTableName,cmdp[i].value,enabled ? "> 0" : "<= 0")) != 0)
	  { row_cnt = ssdbGetNumRecords(hError,hRequest);
            ssdbFreeRequest(hError,hRequest);
            if(!row_cnt)
            { if((hRequest = ssdbSendRequestTrans(hError,hConnect,SSDB_REQTYPE_INSERT,szDbTableName,
                             "insert into %s values(\"%s\",%d)",szDbTableName,cmdp[i].value,enabled)) == 0)
              { sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest); err++;
              }
              else
              { ssdbFreeRequest(hError,hRequest);
                FormatedBody("<li>%s\n",cmdp[i].value);
                action_cnt++;
              }
            }
            else
            { FormatedBody("<li>%s (already exist)\n",cmdp[i].value);
            }
	  }
 	  else { sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest); err++; }
        }
        UListEnd();
        ssdbCloseConnection(hError,hConnect);
      }
      else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantOpenConnection);
      ssdbDeleteClient(hError,hClient);
    }
    else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocCliHandle);
    ssdbDeleteErrorHandle(hError);    
  }
  else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocErrHandle);

  if(!addFileToOutputStream(hErr,efname)) sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,efname);

  pthread_mutex_unlock(&ipaddrlist_mutex); /* end of critical section */

  if(action_cnt) declare_reload();

  return 0;
}
/* -------------------------- setuname --------------------------------------- */
static int setuname(sscErrorHandle hErr,mySession *sess,CMDPAIR *cmdp)
{ char tmpstring[512],buffer[1024];
  ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;
  unsigned long key;
  int oldidx,newidx,err,action_cnt;
  char *conformpage,*conformpageend,*errpagefname;

  if(sess->argc < 7) return 1;

  conformpage    = sess->argv[4];
  conformpageend = sess->argv[5];
  errpagefname   = sess->argv[6];

  if((oldidx = sscFindPairByKey(cmdp,0,sess->argv[2])) < 0)
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,sess->argv[2],szCantFindArgsEnd);
    return 0;
  }
  if((newidx = sscFindPairByKey(cmdp,0,sess->argv[3])) < 0)
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,sess->argv[3],szCantFindArgsEnd);
    return 0;
  }
  if(!cmdp[newidx].value || !cmdp[newidx].value[0] || !cmdp[oldidx].value || !cmdp[oldidx].value[0])
  { Body(szPageInvalidNewUsername);
    return 0;
  }

  if(!testFileExist(hErr,conformpage) || !testFileExist(hErr,conformpageend) || !testFileExist(hErr,errpagefname)) return 0;

  action_cnt = (err = 0);

  key = sscGetSharedLong(3);

  pthread_mutex_lock(&ipaddrlist_mutex); /* begin of critical section */

  if((hError = ssdbCreateErrorHandle()) != 0)
  { if((hClient = ssdbNewClient(hError,RGP_SSDBCLIENT_NAME,RGP_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) != 0)
    { if((hConnect = ssdbOpenConnection(hError,hClient,NULL,szDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) != 0)
      { /* Read old username from database */
        if((hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_SELECT,"select value from %s where keyname=\"userentry\"",szDbKeyTableName)) != 0)
        { if(ssdbGetNumRecords(hError,hRequest) != 0)
          { if(ssdbGetNextField(hError,hRequest,tmpstring,sizeof(tmpstring)-1))
            { if(!xsDecodeBuf(tmpstring,buffer,sizeof(buffer)-1,key) || strcmp(cmdp[oldidx].value,buffer))
              { if(!addFileToOutputStream(hErr,errpagefname))
                { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,errpagefname);
                  sscError(hErr,szServerNameErrorPrefix,szIncorrectUsername);
                }
                err++;
              }
            }
            else { sscError(hErr,szServerNameErrorPrefix,szErrMsgGetNexField); err++; }
          }
          else { sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest); err++; }
          ssdbFreeRequest(hError,hRequest);

          if(!err && !strcmp(buffer,cmdp[newidx].value))
          { if(!addFileToOutputStream(hErr,conformpage))
            { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,conformpage);
              err++;
            }
            else
            { Body(buffer);
              if(!addFileToOutputStream(hErr,conformpageend))
              { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,conformpageend);
                err++;
              }
            }
          }
          else
          { if(!err) /* Scramble new user name */
            { if(!xsEncodeBuf(cmdp[newidx].value,buffer,sizeof(buffer)-1,key))
              { if(!addFileToOutputStream(hErr,errpagefname))
                { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,errpagefname);
                  sscError(hErr,szServerNameErrorPrefix,szIncorrectUsername);
                }
                err++;
              }
            }

            /* Set new username */
            if(!err && (hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_UPDATE,"update %s set value=\"%s\" where keyname=\"userentry\"",szDbKeyTableName,buffer)) != 0)
            { if(!addFileToOutputStream(hErr,conformpage))
              { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,conformpage);
                sscError(hErr,szServerNameErrorPrefix,szUsernameChanged);
              }
              else
              { FormatedBody("<B>%s</B>",cmdp[newidx].value);
                if(!addFileToOutputStream(hErr,conformpageend))
                { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,conformpageend);
                  sscError(hErr,szServerNameErrorPrefix,szUsernameChanged);
                }
              }
              ssdbFreeRequest(hError,hRequest);
              action_cnt++;
            }
          }
        } /* end of old username request */
        else sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest);
        ssdbCloseConnection(hError,hConnect);
      }
      else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantOpenConnection);
      ssdbDeleteClient(hError,hClient);
    }
    else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocCliHandle);
    ssdbDeleteErrorHandle(hError);    
  }
  else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocErrHandle);

  pthread_mutex_unlock(&ipaddrlist_mutex); /* end of critical section */

  if(action_cnt) declare_reload();

  return 0;
}
/* -------------------------- setpasswd -------------------------------------- */
static int setpasswd(sscErrorHandle hErr,mySession *sess,CMDPAIR *cmdp)
{ char *conformpage,*newpwderr,*oldpwderr;
  int oldpwdidx,new1pwdidx,new2pwdidx,action_cnt,err;
  char tmpstring[512],buffer[1024];
  ssdb_Error_Handle hError;
  ssdb_Client_Handle hClient;
  ssdb_Connection_Handle hConnect;
  ssdb_Request_Handle hRequest;
  unsigned long key;
 
  if(sess->argc < 8) return 1;
  conformpage    = sess->argv[5];
  newpwderr      = sess->argv[6];
  oldpwderr      = sess->argv[7];

  if((oldpwdidx = sscFindPairByKey(cmdp,0,sess->argv[2])) < 0)
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,sess->argv[2],szCantFindArgsEnd);
    return 0;
  }
  if((new1pwdidx = sscFindPairByKey(cmdp,0,sess->argv[3])) < 0)
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,sess->argv[3],szCantFindArgsEnd);
    return 0;
  }
  if((new2pwdidx = sscFindPairByKey(cmdp,0,sess->argv[4])) < 0)
  { sscError(hErr,szPercent4s,szServerNameErrorPrefix0,szCantFindArgsBegin,sess->argv[4],szCantFindArgsEnd);
    return 0;
  }

  if(!testFileExist(hErr,conformpage) || !testFileExist(hErr,newpwderr) || !testFileExist(hErr,oldpwderr)) return 0;

  if(!cmdp[new1pwdidx].value || !cmdp[new1pwdidx].value[0] || !cmdp[new2pwdidx].value || !cmdp[new2pwdidx].value[0] ||
     strcmp(cmdp[new1pwdidx].value,cmdp[new2pwdidx].value) || (strlen(cmdp[new1pwdidx].value) > sizeof(tmpstring)/2))
  { if(!addFileToOutputStream(hErr,newpwderr))
    { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,newpwderr);
      sscError(hErr,szServerNameErrorPrefix,szIncorrectNewPassword);
    }
    return 0;
  }

  action_cnt = (err = 0);

  key = sscGetSharedLong(4);

  pthread_mutex_lock(&ipaddrlist_mutex); /* begin of critical section */

  if((hError = ssdbCreateErrorHandle()) != 0)
  { if((hClient = ssdbNewClient(hError,RGP_SSDBCLIENT_NAME,RGP_SSDBCLIENT_PASSWORD,LOCAL_SSDB_NEWCLIENTFLAGS)) != 0)
    { if((hConnect = ssdbOpenConnection(hError,hClient,NULL,szDatabaseName,LOCAL_SSDB_NEWCONNECTFLAGS)) != 0)
      { /* Read old password from database */
        if((hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_SELECT,"select subvalue from %s where keyname=\"userentry\"",szDbKeyTableName)) != 0)
        { if(ssdbGetNumRecords(hError,hRequest) != 0)
          { if(ssdbGetNextField(hError,hRequest,tmpstring,sizeof(tmpstring)-1))
            { if(!xsDecodeBuf(tmpstring,buffer,sizeof(buffer)-1,key) || strcmp(cmdp[oldpwdidx].value,buffer))
              { if(!addFileToOutputStream(hErr,oldpwderr))
                { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,oldpwderr);
                  sscError(hErr,szServerNameErrorPrefix,szIncorrectOldPassword);
                }
                err++;
              }
            }
            else { sscError(hErr,szServerNameErrorPrefix,szErrMsgGetNexField); err++; }
          }
          else { sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest); err++; }
          ssdbFreeRequest(hError,hRequest);

          if(!err && !strcmp(buffer,cmdp[new1pwdidx].value))
          { if(!addFileToOutputStream(hErr,conformpage))
            { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,conformpage);
              err++;
            }
          }
          else
          { if(!err) /* Scramble new password */
            { if(!xsEncodeBuf(cmdp[new1pwdidx].value,buffer,sizeof(buffer)-1,key))
              { if(!addFileToOutputStream(hErr,newpwderr))
                { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,newpwderr);
                  sscError(hErr,szServerNameErrorPrefix,szIncorrectNewPassword);
                }
                err++;
              }
            }

            /* Set new password */
            if(!err && (hRequest = ssdbSendRequest(hError,hConnect,SSDB_REQTYPE_UPDATE,"update %s set subvalue=\"%s\" where keyname=\"userentry\"",szDbKeyTableName,buffer)) != 0)
            { if(!addFileToOutputStream(hErr,conformpage))
              { sscError(hErr,szPercent3xs,szServerNameErrorPrefix0,szCantProcessFile,conformpage);
                sscError(hErr,szServerNameErrorPrefix,szPasswordChanged);
              }
              ssdbFreeRequest(hError,hRequest);
              action_cnt++;
            }
          }
        } /* end of old password request */
        else sscError(hErr,szServerNameErrorPrefix,szErrMsgExecRequest);
        ssdbCloseConnection(hError,hConnect);
      }
      else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantOpenConnection);
      ssdbDeleteClient(hError,hClient);
    }
    else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocCliHandle);
    ssdbDeleteErrorHandle(hError);    
  }
  else sscError(hErr,szServerNameErrorPrefix,szErrMsgCantAllocErrHandle);

  pthread_mutex_unlock(&ipaddrlist_mutex); /* end of critical section */

  if(action_cnt) declare_reload();

  return 0;
}
#ifdef LVA_XCODE_INCLUDE
/* ---------------------------- xcode ---------------------------------------- */
static void xcode(sscErrorHandle hError,const char *string,const char *keystr,int encodeflg)
{ char tmp[2048];
  unsigned long key;
  int len;

  if(sscanf(keystr,"%lx",&key) == 1)
  { len = encodeflg ? xsEncodeBuf(string,tmp,sizeof(tmp)-1,key) : xsDecodeBuf(string,tmp,sizeof(tmp)-1,key);
    FormatedBody("Key value: 0x%lX<br>\n",key);
    FormatedBody("Result size: %d<br>\n",len);
    FormatedBody("Result string: \"%s\"<br>\n",tmp);
  }
  else sscError(hError,szServerNameErrorPrefix,szIncorrectKeyString);
}
#endif
/* ----------------------- rgpgGenerateReport -------------------------------- */
int RGPGAPI rgpgGenerateReport(sscErrorHandle hError,rgpgSessionID session, int argc, char* argv[],char *rawCommandString,streamHandle result,char *userAccessMask)
{  CMDPAIR cmdp[MAXNUMBERS_OF_KEYS];
   int i,j,cnt;
   char tmp256[256];
   char tmp128[128];
    
   mySession *sess = (mySession *)session;

   if(!sess || sess->signature != sizeof(mySession))
   { sscError(hError,szServerNameErrorPrefix,szErrorMsgIncorrectSess);
     return RGPERR_SUCCESS;
   }

   if(((sess->argc = argc) < 2) || ((sess->argv = argv) == 0))
   { sscError(hError,szServerNameErrorPrefix,szErrMsgIncorrectArgs);
     sscError(hError,szUsageInfo,argv[0]);
     return RGPERR_SUCCESS;
   }

   /* Initialize The HTML Generator  */
   if(createMyHTMLGenerator(result))
   { sscError(hError,szServerNameErrorPrefix,szErrMsgCantOpenHtmlGen);
     return RGPERR_SUCCESS;
   }

   /* Initialize Pair array (return value eq. number of valid pairs) */
   sscInitPair(rawCommandString,cmdp,MAXNUMBERS_OF_KEYS);

   /* Check WEB browser type (Lynx is only tex based) */ 
   for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
    if(!strcasecmp(cmdp[i].value,szLynx)) sess->textonly = 1;

   if(!strcasecmp(argv[1],"websrvport"))
   { FormatedBody("%d",(int)sscGetSharedLong(0));
   }
   else if(!strcasecmp(argv[1],"websrvident"))
   { if(!sscGetSharedString(0,tmp256,sizeof(tmp256))) tmp256[0] = 0;
     Body(tmp256);
   }
   else if(!strcasecmp(argv[1],"websrvversion"))
   { if(!sscGetSharedString(1,tmp128,sizeof(tmp128))) tmp128[0] = 0;
     FormatedBody("%d.%d %s",(int)sscGetSharedLong(1),(int)sscGetSharedLong(2),tmp128);
   }
   else if(!strcasecmp(argv[1],"websrvtitle"))
   { if(!sscGetSharedString(0,tmp256,sizeof(tmp256))) tmp256[0] = 0;
     if(!sscGetSharedString(1,tmp128,sizeof(tmp128))) tmp128[0] = 0;
     FormatedBody("%s version %d.%d %s",tmp256,(int)sscGetSharedLong(1),(int)sscGetSharedLong(2),tmp128);
   }
   else if(!strcasecmp(argv[1],"getwhiteiplist"))
   { getipaddroptionlist(hError,sess,1);
   }
   else if(!strcasecmp(argv[1],"getdarkiplist"))
   { getipaddroptionlist(hError,sess,0);
   }
   else if(!strcasecmp(argv[1],"delwhiteip"))
   { if(delipaddrlist(hError,sess,1,cmdp))
      sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
   else if(!strcasecmp(argv[1],"deldarkip"))
   { if(delipaddrlist(hError,sess,0,cmdp))
      sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
   else if(!strcasecmp(argv[1],"addwhiteip"))
   { if(addipaddrlist(hError,sess,1,cmdp))
      sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
   else if(!strcasecmp(argv[1],"adddarkip"))
   { if(addipaddrlist(hError,sess,0,cmdp))
      sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
   else if(!strcasecmp(argv[1],"setuname"))
   { if(setuname(hError,sess,cmdp))
      sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
   else if(!strcasecmp(argv[1],"setpasswd"))
   { if(setpasswd(hError,sess,cmdp))
      sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
#ifdef LVA_XCODE_INCLUDE
   else if(!strcasecmp(argv[1],"xencode"))
   { if(argc > 3) xcode(hError,argv[2],argv[3],1);
     else sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
   else if(!strcasecmp(argv[1],"xdecode"))
   { if(argc > 3) xcode(hError,argv[2],argv[3],0);
     else sscError(hError,szServerNameErrorPrefix,szInvalidActionStr1);
   }
#endif
   else
   { sscError(hError,(char*)szErrMsgUnknownReqType,argv[1]);
     sscError(hError,szUsageInfo,argv[0]);
   }
   /*  Destroy The HTML Generator  */
   deleteLocalHTMLGenerator(hError);

   return RGPERR_SUCCESS;
}

