/* --------------------------------------------------------------------------- */
/* -                     SSCHTMLPREPROCESSOR.C                               - */
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
/* $Revision: 1.6 $ */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <malloc.h>

#include "SSC.h" 
#include "sssSession.h" 
#include "sssStatus.h"
#include "sscHTMLProcessor.h"

extern SSSGLBINFO sssGlbInfo;     /* Switcher Global Info structure */

/* ---------------------------- RLP ----------------------------------------- */
static int RLP(sssSession *SSS, streamHandle in, streamHandle out, const char *parpnt)
{  streamHandle delayed, lurl, sssResult;
   char *q, *urlptr;
   char cmdname[17], urlbuf[1024];
   int i, justcopy, c, retcode, wasParInURL;
   
   sssResult = newMemoryStream();
   delayed   = newMemoryStream();
   lurl      = newMemoryStream();

   if(!delayed || !lurl || !sssResult)
   { destroyStream(sssResult);
     destroyStream(delayed);
     destroyStream(lurl);
     if(SSS) putString("\n<p>RLP - Can't alloc memory streams'",sscGetErrorStream(SSS->err));
     return (RGPERR_NOCACHE|RGPERR_GENERICERROR);
   }

   justcopy = 0;
   retcode = RGPERR_SUCCESS;

   for(urlptr = 0;;)
   { if((c = getChar(in)) < 0) break;
     if(!justcopy && c == '<')
     { c = getChar(in);
       if (c == 'a' || c == 'A') /* process <a...> HTML command */
       { rewriteStream(delayed);
         putChar('<',delayed); putChar(c,delayed); 
         c = getChar(in); putChar(c,delayed);
         if (!isspace(c)) goto ignoreAnchor;
          while(isspace(c)) { c = getChar(in); putChar(c,delayed); }
	  
          if (!isalpha(c)) goto ignoreAnchor;
          for (i = 0; i < (sizeof(cmdname)-1) && isalnum(c); i++)
          { cmdname[i] = tolower(c);
            c = getChar(in); putChar(c,delayed);
          }
          cmdname[i] = 0;
          if (strcasecmp(cmdname,"href")) goto ignoreAnchor;
          while (isspace(c)) { c = getChar(in); putChar(c,delayed); }
          if (c != '=') goto ignoreAnchor;
          c = getChar(in); putChar(c,delayed);
          while (isspace(c))
          { c = getChar(in); putChar(c,delayed);
          }
          if (c != '"') goto ignoreAnchor;
          rewriteStream(lurl);
          wasParInURL = 0;
          c = getChar(in); putChar(c,delayed);
          while (c != '"')
          { if (c == '?') wasParInURL = 1;
            putChar(c,lurl); 
            c = getChar(in); if (c == -1) break; putChar(c,delayed);
          }
          c = getChar(in); putChar(c,delayed);
         
          if (parpnt && parpnt[0])  /* Add Par string to URL */
          {  if (wasParInURL) putChar('&',lurl); else putChar('?',lurl);
             putString(parpnt,lurl); 
          }   
          i = sizeofStream(lurl);
          
	  urlptr = urlbuf;
	  if(i >= sizeof(urlbuf))
	  { if((urlptr = (char*)malloc(i+1)) == 0) goto ignoreAnchor; /* !!!! */
	  }
	  
          rewindStream(lurl);
          q = urlptr;
          for (;i > 0;i--) *q++ = getChar(lurl);
          *q = 0;
          i = strlen(SSS_ROOT);
          if(strncasecmp(urlptr,SSS_ROOT,i) != 0)  
          { if(urlptr != urlbuf && urlptr) free(urlptr);
	    urlptr = 0;
            goto ignoreAnchor; 
          }
          while(c != '>')
          { if((c = getChar(in)) == (-1)) break;
	    putChar(c,delayed);
          }
          for(;;)
          { if(c == '<')
            { c = getChar(in); putChar(c,delayed);
              if(c != '/') continue;
              c = getChar(in); putChar(c,delayed);
              if(c != 'a' && c != 'A') continue;
              c = getChar(in); putChar(c,delayed);
              if(c == '>') break;
            }
            else if(c == (-1)) break;
            else
            { c = getChar(in); putChar(c,delayed);
            }
          }

	  sscMarkErrorZone(SSS->err);
          if((i = getSSSDataByURL(urlptr+i,out,SSS)) & RGPERR_NOPARSE) justcopy++;
	  if((i & RGPERR_RETCODE_MASK) == RGPERR_REDIRECTED) { retcode = i; break; } /* Need redirection immediately */
          if((i & RGPERR_RETCODE_MASK) == RGPERR_SUCCESS) retcode |= (i & RGPERR_RETFLAG_MASK); /* Save flags */
	  else                                            retcode = i; /* Some Error */

	  if(sscWereErrorsInZone(SSS->err)) 
          { streamHandle str = sscGetErrorStream(SSS->err);
            putString("\n<p>*** URL was processed: '", str);
            putStream(lurl, str);
            putString("'\n<br>======================<p>\n", str);
          }
	  
          if(urlptr != urlbuf && urlptr) free(urlptr);
          urlptr = 0;
          continue;

ignoreAnchor:  
          putStream(delayed, out);
          continue;
        }
        else /* read '<' and not recognized 'a' or 'A' */
        { putChar('<', out);
        }
     }
     putChar(c, out); /* copy out regular symbol */
   }
   destroyStream(sssResult);
   destroyStream(delayed);
   destroyStream(lurl);
   return retcode;
}
/* ------------------------ sscHTMLPreProcessor ----------------------------- */
int sscHTMLPreProcessor(sscErrorHandle hError, streamHandle source, streamHandle result, const char *parpnt)
{  sssSession *SSS;
   int retcode = (RGPERR_NOCACHE|RGPERR_GENERICERROR);
   sssGlbInfo.statTotalHTMLPreProcessorCall++;
   if((SSS = newSSSSession(hError)) != 0)
   { retcode = RLP(SSS, source, result, parpnt);
     closeSSSSession(SSS);
   }
   else sscError(hError,"No memory for SSS session data in sscHTMLPreProcessor()");
   return retcode;
}
