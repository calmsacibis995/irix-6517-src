/* --------------------------------------------------------------------------- */
/* -                         SSCHTMLPROCESSOR.C                              - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
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
#include <stdio.h> 
#include <string.h>
#include <sys/types.h>
#include <malloc.h>

#include "rgAPI.h"
#include "rgAPIinternal.h"
#include "sscHTMLGen.h"
#include "sscHTMLProcessor.h" 
#include "sssStatus.h"
#include "ssdbapi.h"

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

/* --------------------------------------------------------------------------- */
typedef int FPgetSSSData(char *url, sssSession *SSS);
typedef struct sss_s_SSSSWITCHER {
    const char *id;
    FPgetSSSData *getSSSData;
} SSSSwitcher;
/* --------------------------------------------------------------------------- */

static char szUrlWasProcessed[] = "\n<p>*** URL was processed: \"%s\"\n<br>======================<p>\n";
static const char szSSCOficialName[] = SSC_NAME;
static const char szAcceptRawCmdString[] = "AcceptRawCmdString"; /* RG server attribute */
SSSGLBINFO sssGlbInfo;     /* Switcher Global Info structure */
/* --------------------------- sscHTMLProcessorInit -------------------------- */
int sscHTMLProcessorInit(void)
{ memset(&sssGlbInfo,0,sizeof(sssGlbInfo));
  return srgInit(0);
}
/* --------------------------- sscHTMLProcessorDone -------------------------- */
int sscHTMLProcessorDone(void)
{ return srgDone(0);
}
/* ----------------------------- sscHTMLProcessor ---------------------------- */
int sscHTMLProcessor(sscErrorHandle hError, const char *url, streamHandle result)
{  char tmpbuffer[512];
   sssSession *SSS;
   int i, retcode;
   char *c = &tmpbuffer[0];
   char *temp = 0;

   sssGlbInfo.statTotalHTMLProcessorCall++;
   retcode = (RGPERR_NOCACHE|RGPERR_GENERICERROR);

   if(!url || (i = strlen(url)) == 0)
   { sscError(hError,"Invalid URL in sscHTMLProcessor() - NULL");
     return retcode;
   }

   if(i >= sizeof(tmpbuffer))
   { if((temp = strdup(url)) == 0)
     { sscError(hError,"No memory for URL in sscHTMLProcessor()");
       return retcode;
     }
     c = temp;
   }
   else strcpy(c,url);
   
   
   if((SSS = newSSSSession(hError)) != 0)
   { sscMarkErrorZone((SSS->err = hError));
     retcode = getSSSDataByURL(c+strlen(SSS_ROOT),result,SSS);
     if((retcode & RGPERR_RETCODE_MASK) != RGPERR_REDIRECTED)
     { if(sscWereErrorsInZone(SSS->err)) putStringFmt(sscGetErrorStream(SSS->err),szUrlWasProcessed,url);
     }
     closeSSSSession(SSS);
   }
   else sscError(hError, "No memory for SSS session data in sscHTMLProcessor()");
   
   if(temp) free(temp); /* :( */
   return retcode;
}
/* ----------------------------- getReportSession ---------------------------- */
static srgSessionID getReportSession(sssSession *SSS)
{  rgPluginHead *p; 
   char buff[512];
   srgSessionID id = 0;

   if(SSS && SSS->argv[0] && SSS->argv[0][0])
   { for(p = SSS->openRGSessions;p;p = p->link) /* Try to find open session */
     { if(!strcmp(p->pname,SSS->argv[0])) return p->session;
     }
     snprintf(buff,sizeof(buff),"/usr/lib32/internal/%s.so",SSS->argv[0]);
     if((id = srgCreateReportSession(SSS->err,buff)) != 0) addRGPlugin(SSS->argv[0],id,SSS); 
   }
   return id;
}
/* -------------------------- setRGAttributes -------------------------------- */
static void setRGAttributes(char *url, sssSession *SSS)
{ char c, *id, *extra, *value;

  if((id = strchr(url,'&')) != 0) *id = 0;

  for(;;) /* loop for all attributes to change */
  { id = url; extra = (value = 0);
    for(;;) /* ID */
    { if((c = *url) == '~')
      { *url++ = 0; extra = url;
        break;
      }  
      if(!c || c == '=') break;
      url++;
    }
    for(;;) /* extra */
    { if((c = *url) == '=')
      { *url++ = 0; value = url;
        break;
      }  
      if(!c) break;
      url++;
    }
    if(c == '=') 
    { for(;;) /* value */
      { if((c = *url) == '&')
        { *url++ = 0; 
          break;
        }  
        if(!c) break;
        url++;
      }
      srgSetAttribute(SSS->err,id,extra,value); 
    }
    if(!c) break;
  }
}
/* ------------------------ setRGServerAttribute ----------------------------- */
static void setRGServerAttribute(char *url, sssSession *SSS)
{  char c, *id, *extra, *value; 
   srgSessionID session = getReportSession(SSS);   

   if(!session) return;
   
   for(;;) /* loop for all attributes to change */
   { id = url; extra = 0;
     for(;;) /* ID */
     { c = *url;
       if(c == '~')
       { *url++ = 0; 
         extra = url;
         break;
       }  
       if (c == 0 || c == '=') break;
       url++;
     }
     for(;;) /* extra */
     { c = *url;
       if(c == '=')
       { *url++ = 0; 
         value = url;
         break;
       }  
       if(c == 0) break;
       url++;
     }
     if(c == '=') 
     { for (;;) /* value */
       { if((c = *url) == '&')
         { *url++ = 0; 
           break;
         }  
         if (c == 0) break;
         url++;
       }
       srgSetReportAttribute(SSS->err,session,id,extra,value); 
     }
     if(c == 0) break;
   }
}
/* ------------------------ putRGServerAttribute ----------------------------- */
static void putRGServerAttribute(char *url, sssSession *SSS)
{  int i;
   char c, *id, *extra = 0, *s;
   srgSessionID session;
   id = url;
   for(;;)
   { if((c = *url) == '~')
     { *url++ = 0; 
       extra = url;
       break;
     }  
     if (c == 0 || c == '?') break;
     url++;
   }
   for(;;)
   { if((c = *url) == '?')
     { *url++ = 0; 
       break;
     }  
     if(c == 0) break;
     url++;
   }
   if (c == '?') setRGServerAttribute(url, SSS);
   if((session = getReportSession(SSS)) != 0)
   { if((s = srgGetReportAttribute(SSS->err,session,id,extra,&i)) != 0)
     { putString(s, SSS->out);
       srgFreeReportAttributeString(SSS->err,session,id,extra,s,i);
     }
   }
}
/* ---------------------------- getRGData ------------------------------------ */
static int getRGData(char *url, sssSession *SSS)
{  int  argc, i,  accept_rawcmd;
   char c, *s, *rawcmdstr;
   srgSessionID id;

   rawcmdstr = 0;
   accept_rawcmd = 0;

   if(*url == '?') /* /$sss/rg? */
   { setRGAttributes(url+1, SSS);
     /* ????? Generate here the response page from RG seting attributes  */
     return (RGPERR_NOCACHE|RGPERR_GENERICERROR);
   }

   /* skip '/' in URL string */
   if(*url != '/') 
   { sscError(SSS->err,"Not complete URL for Report Generator in getRGData()");
     return (RGPERR_NOCACHE|RGPERR_GENERICERROR);
   }
   url++;
   /* now URL positioned after '.../RG/' */

   /* processing item~item~...~item into argc, argv[] */
   argc = 1;
   SSS->argv[0] = url; /* must be plugin name */
   for(;argc < MAX_RG_ARGS;)
   { c = *url;
     if(!c || c == '?' || c == '/')
     { *url = 0; if(c) url++;
       break;
     }
     if(*url == '~')
     { *url++ = 0; 
       SSS->argv[argc++] = url;
     }
     else url++;
   }
   SSS->argc = argc;

   if(c == '/') /* get RG server attribute */   /* "/$sss/rg/server~arg1~arg2 or "/$sss/rg/server/" */
   { if(argc > 1)
     { sscError(SSS->err,"Error in URL syntax in getRGData()");
       return (RGPERR_NOCACHE|RGPERR_GENERICERROR);
     }
     putRGServerAttribute(url, SSS);
     return (RGPERR_NOCACHE|RGPERR_SUCCESS);
   }

   if(c == '?')  /* "/$sss/rg/server[~argv1~argv2~...]?keyname=keyvalue&...." */
   { if((id = getReportSession(SSS)) != 0)
     { if((s = srgGetReportAttribute(SSS->err,id,szAcceptRawCmdString,0,&i)) != 0)
       { if(sscanf(s,"%d",&accept_rawcmd) != 1) accept_rawcmd = 0;
         srgFreeReportAttributeString(SSS->err,id,szAcceptRawCmdString,0,s,i);
       }
     }
     if(!accept_rawcmd) setRGServerAttribute(url,SSS);
     else rawcmdstr = url;
   }
   else /* try to get RG attribute */
   { if((s = srgGetAttribute(0,SSS->argv[0],((argc > 1) ? SSS->argv[1] : 0), &i)) != 0)
     { putString(s,SSS->out);
       srgFreeAttribute(0,SSS->argv[0],((argc > 1) ? SSS->argv[1] : 0),s,i);
       return (RGPERR_NOCACHE|RGPERR_SUCCESS);
   }
   }

   if((id = getReportSession(SSS)) != 0)
   { return srgGenerateReport(SSS->err,id,SSS->argc,SSS->argv,rawcmdstr,SSS->out);
   }
   sscError(SSS->err,"Can not getReportSession() in getRGData()");
   return (RGPERR_NOCACHE|RGPERR_GENERICERROR);
}
/* --------------------------- getSSSTitle ----------------------------------- */
static int getSSSTitle(char *url, sssSession *SSS)
{ putStringFmt(SSS->out,"<B>%s.</B> Irix Release 1999",szSSCOficialName);
  return (RGPERR_NOCACHE|RGPERR_SUCCESS);
}
/* -------------------------- getSSSVersion ---------------------------------- */
static int getSSSVersion(char *url, sssSession *SSS)
{ 
#ifdef INCLUDE_TIME_DATE_STRINGS
   putStringFmt(SSS->out,"<B>%s</B> v%d.%d %s %s",szSSCOficialName,SSC_VMAJOR,SSC_VMINOR,__TIME__,__DATE__);
#else
   putStringFmt(SSS->out,"<B>%s</B> v%d.%d",szSSCOficialName,SSC_VMAJOR,SSC_VMINOR);
#endif
  return (RGPERR_NOCACHE|RGPERR_SUCCESS);
}
/* ---------------------------- getStatus ------------------------------------ */
static int getStatus(char *url, sssSession *SSS)
{ hPlugInInfo p;

  getSSSTitle(url,SSS);

  putString("<P><font face=\"Courier\"><tt><pre>",SSS->out);

  putStringFmt(SSS->out,"stream memblk alloc:       %lu\n",streamGetInfo(STREAMLIB_GETMBLOCKALLOC));
  putStringFmt(SSS->out,"stream memblk free:        %lu\n",streamGetInfo(STREAMLIB_GETMBLOCKFREE));
  putStringFmt(SSS->out,"stream control blk alloc:  %lu\n",streamGetInfo(STREAMLIB_GETSTREAMALLOC));
  putStringFmt(SSS->out,"stream control blk free:   %lu\n\n",streamGetInfo(STREAMLIB_GETSTREAMFREE));

  putStringFmt(SSS->out,"error objects alloc:       %lu\n",sscErrorGetInfo(SSCERR_GETERRBLK_ALLOC));
  putStringFmt(SSS->out,"error objects busy:        %lu\n",sscErrorGetInfo(SSCERR_GETERRBLK_BUSY));
  putStringFmt(SSS->out,"error objects free:        %lu\n\n",sscErrorGetInfo(SSCERR_GETERRBLK_FREE));

  putStringFmt(SSS->out,"HtmlGen act_thread alloc:  %lu\n",getHTMLGeneratorInfo(GETHTMLGENINFO_ALLOC));
  putStringFmt(SSS->out,"HtmlGen act_thread busy:   %lu\n",getHTMLGeneratorInfo(GETHTMLGENINFO_BUSY));
  putStringFmt(SSS->out,"HtmlGen act_thread free:   %lu\n\n",getHTMLGeneratorInfo(GETHTMLGENINFO_FREE));

  putStringFmt(SSS->out,"sscHTMLProcessor calls:    %lu\n",sssGlbInfo.statTotalHTMLProcessorCall);
  putStringFmt(SSS->out,"sscHTMLPreProcessor calls: %lu\n\n",sssGlbInfo.statTotalHTMLPreProcessorCall);

  putStringFmt(SSS->out,"rgPluginHead alloced:      %lu\n",sssGlbInfo.statTotalrgPluginHeadAlloced);
  putStringFmt(SSS->out,"rgPluginHead in free list: %lu\n\n",sssGlbInfo.statTotalrgPluginHeadInFreeList);

  putStringFmt(SSS->out,"sssSession alloced:        %lu\n",sssGlbInfo.statTotalsssSessionAlloced);
  putStringFmt(SSS->out,"sssSession in free list:   %lu\n\n",sssGlbInfo.statTotalsssSessionInFreeList);

  putStringFmt(SSS->out,"PluginFace alloced:        %lu\n",sssGlbInfo.statTotalPluginFaceAlloced);
  putStringFmt(SSS->out,"PluginFace in free list:   %lu\n\n",sssGlbInfo.statTotalPluginFaceInFreeList);

  putStringFmt(SSS->out,"Load plugin:               %lu\n",sssGlbInfo.statTotalLoadNewPlugIn);
  putStringFmt(SSS->out,"Unload plugin:             %lu\n\n",sssGlbInfo.statTotalUnloadPlugin);

  putStringFmt(SSS->out,"ReportSession alloced:     %lu\n",sssGlbInfo.statTotalReportSessionAlloced);
  putStringFmt(SSS->out,"ReportSession in free list:%lu\n\n",sssGlbInfo.statTotalReportSessionInFreeList);
  
  rgiOpenPlugInInfo();
  for(p = rgiGetFirstPlugInInfo();p;p = rgiGetNexPlugInInfo(p))
  { putStringFmt(SSS->out,"PlugIn: \"%s\" : \"%s\" : \"%s\"\n",rgiGetPlugInPathByPlugInInfo(p),
                                                               rgiGetPlugInTitleByPlugInInfo(p),
                                                               rgiGetPlugInVersionByPlugInInfo(p));
    putStringFmt(SSS->out,"UsageCnt: %d ThreadSafe: %d Unlodable: %d UnloadTimeout: %d sec. Current timeout: %d sec.\n\n",
                 rgiGetPlugInUsageCountByPlugInInfo(p),rgiGetPlugInThreadSafeFlagByPlugInInfo(p),
		 rgiGetPlugInUnloadableFlagByPlugInInfo(p),rgiGetPlugInUnloadTimeoutByPlugInInfo(p)*RGPLUGIN_UNLOAD_TIMESTEP,
		 rgiGetPlugInCurrentUnloadTimeoutByPlugInInfo(p)*RGPLUGIN_UNLOAD_TIMESTEP);
  }
  rgiClosePlugInInfo();

  putStringFmt(SSS->out,"\nSSDB v%d.%d %s\n",
               ssdbGetParamInt(SSDBPARI_VERSIONMAJOR,0),
	       ssdbGetParamInt(SSDBPARI_VERSIONMINOR,0), 
	       ssdbGetParamString(SSDBPARS_VERDESCRIPTION,0));
  putStringFmt(SSS->out,"Client allocated:          %d\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_CLIENTALLOC));
  putStringFmt(SSS->out,"Client free:               %d\n\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_CLIENTFREE));
  putStringFmt(SSS->out,"Connection allocated:      %d\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_CONNALLOC));
  putStringFmt(SSS->out,"Connection free:           %d\n\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_CONNFREE));
  putStringFmt(SSS->out,"Request allocated:         %d\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_REQUESTALLOC));
  putStringFmt(SSS->out,"Request free:              %d\n\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_REQUESTFREE));
  putStringFmt(SSS->out,"RequestInfo allocated:     %d\n\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_REQINFOALLOC));
  putStringFmt(SSS->out,"RequestResult allocated:   %d\n\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_REQRESALLOC));
  putStringFmt(SSS->out,"ErrorHandle allocated:     %d\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_ERRHALLOC));
  putStringFmt(SSS->out,"ErrorHandle free:          %d\n\n",ssdbGetParamInt(SSDBPAPI_GLOBALSTATUS,SSDBPAPI_GS_ERRHFREE));
  putString("</pre></tt></font></P><BR><ADDRESS><A HREF=\"mailto:talnykin@sgi.com,legalov@sgi.com\">talnykin@sgi.com,legalov@sgi.com</A></ADDRESS>",SSS->out);
  return (RGPERR_NOCACHE|RGPERR_SUCCESS);
}
/* ------------------------- getNoCache -------------------------------------- */
static int getNoCache(char *url, sssSession *SSS)
{ return (RGPERR_NOCACHE|RGPERR_SUCCESS);
}
/* ------------------------- getNoParse -------------------------------------- */
static int getNoParse(char *url, sssSession *SSS)
{ return (RGPERR_NOPARSE|RGPERR_SUCCESS);
}
/* ------------------------- getRedirect ------------------------------------- */
static int getRedirect(char *url, sssSession *SSS)
{ if(url && url[0])
  { sscErrorStream(SSS->err,url);
    return RGPERR_REDIRECTED;
  }
  sscError(SSS->err,"Incorrect URL for redirection in $redirect commanad");
  return RGPERR_GENERICERROR;
}
/* --------------------------------------------------------------------------- */
static SSSSwitcher sssSwitch[] =
{   {"RGS",       getRGData},
    {"RG",        getRGData},
    {"Title",     getSSSTitle},
    {"Version",   getSSSVersion},
    {"Status",    getStatus},
    {"Info",      getStatus},
    {"$nocache",  getNoCache},
    {"$noparse",  getNoParse},
    {"$redirect", getRedirect},
    {0,          0}    /* terminator */
};
/* ------------------------ getSSSDataByURL --------------------------------- */
int getSSSDataByURL(char *url, streamHandle out, sssSession *SSS)
{ SSSSwitcher *p;
  int keylen;
   
  SSS->out = out;

  /* first level switcher, working after '/$sss/' */
  for(p = sssSwitch;p->id;p++)
  { keylen = strlen(p->id);
    if(!strncasecmp(p->id,url,keylen)) return p->getSSSData(url+keylen,SSS);
  }
  sscError(SSS->err,"Unknown SSS switch in getSSSDataByURL() - \"%s\"",url);
  return (RGPERR_NOCACHE|RGPERR_GENERICERROR);
}
