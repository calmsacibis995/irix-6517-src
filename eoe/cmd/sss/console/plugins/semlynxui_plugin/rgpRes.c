/* --------------------------------------------------------------------------- */
/* -                             rpgRes.C                                    - */
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <pthread.h>

#include <sscShared.h>

#include "rgpRes.h"

#define   MAX_RGP_RES_NAME_LENGTH   256  /* number of characters in Resource Name     */
#define   MAX_RGP_RES_SUFFIX_LENGTH  16  /* max number of characters in rgpRes suffix */
#define   MAX_RGP_RES_STRINGS_NUM   256  /* number of Strings per resource file       */
 
typedef struct
{
  int            cData ; /* Template length      */
  const char   * pData ; /* Template string      */
  const char   * szName; /* Template Name        */
  unsigned int   uFlags; /* We are not going to use it for now */
} rgpString;
 
typedef struct rgpResource_tag
{
  struct rgpResource_tag *pNext;
  char                   *pRawData;
  int                     nRawDataLen;
  int                     nTemplUsed ;
  unsigned int            uFlags     ;
  unsigned int            NameLen    ;
  char                    szName  [MAX_RGP_RES_NAME_LENGTH];
  rgpString               TemplArr[MAX_RGP_RES_STRINGS_NUM];
} rgpResource;
 
/*------------------------------------------------------------------------- */
 
#define  RESPATH_SHARED_IDX  2
 
/*------------------------------------------------------------------------- */
/* Static Variables-------------------------------------------------------- */
/*------------------------------------------------------------------------- */
 
static const char       szResSuffix[MAX_RGP_RES_SUFFIX_LENGTH] = ".RGPRes";
static const char       szResTagOpen[] = "<!-- #";
static const char       szResTagTerm[] = "#";
static rgpResource     *g_pResList     = NULL;     /* Resource List         */
static int volatile     g_bInitialized = 0;        /* Initialized Flag      */
static pthread_mutex_t  g_ResourceMutex;           /* Resource List Mutex   */

/* -------------------------------------------------------------------------*/
/* Static Function declaration -------------------------------------------- */
/*------------------------------------------------------------------------- */
 
static void ParseResFile ( sscErrorHandle hErr, rgpResource* pRes );
 
/*------------------------------------------------------------------------- */
/* Initialize Resource Manager                                              */
/*------------------------------------------------------------------------- */
int RGPResInit (sscErrorHandle  hError)
{
  if ( g_bInitialized == 0 )
  {
    if ( pthread_mutex_init ( &g_ResourceMutex, NULL) )
         return 0; /* An error */
    g_bInitialized = 1;
    g_pResList     = NULL;
  }
  return g_bInitialized;
}
 
/* ------------------------------------------------------------------------- */
/* Deinitialize Resource Manager                                             */
/* ------------------------------------------------------------------------- */
void RGPResDone (sscErrorHandle  hError)
{
  if ( g_bInitialized )
  {
    rgpResource *pRes;
 
    pthread_mutex_destroy(&g_ResourceMutex);
    g_bInitialized = 0;
 
    while( g_pResList != NULL )
    {
      pRes        = g_pResList;
      g_pResList  = g_pResList->pNext;
      free ( pRes );
    }
  }
}
 

/* ------------------------------------------------------------------------- */
/* GetRGPResHandle                                                           */
/* ------------------------------------------------------------------------- */

int IsPathValid (const char *pPath, int nPath )
{
  int  i, nPoints = 0, level=0;
  char ch;
  
  for ( i = 0; i < nPath; ++i )
  {
    ch = pPath[i];
    if ( ch == '\\' )
    {   
      if( i+1 < nPath )
      {
        ch = pPath[i+1];
        i++; 
      }
    }
    
    if ( ch == '/' )
    {
      switch ( nPoints )
      {
        case  1:    /*it is ./ case  same level */ 
        break;    
        
        case -1:    /* Level down case  */
           level++;
        break;
        
        case  2:    /* Level up case ../  */
           level--;   
        break;
        
        default:
          return 0; /* the path is wrong anyway */
      }  
      if ( level < 0 )
           return  0;
      nPoints = 0;
    } 
    else if ( ch == '.' )
    { 
       if ( nPoints >= 0 )
       {
         if ( ++nPoints > 2 )
              return 0;
       }       
    } 
    else 
    {
       nPoints = -1;    
    }
  }
  
  if ( level < 0 ) 
       return 0;
  
  return 1;
}
 

RGPResHandle GetRGPResHandle ( sscErrorHandle  hError, const char * pResName, int ResNameLen )
{
  rgpResource * pRes;
 
  if ( pResName == NULL )
       return NULL;
       
  if ( ResNameLen == -1 ) 
       ResNameLen = strlen ( pResName );  
       
  if ( ResNameLen >= MAX_RGP_RES_NAME_LENGTH )
  {
     sscError ( hError, "Resource file name is too long"); 
     return NULL;
  }
       
  if ( !IsPathValid ( pResName, ResNameLen ))
  {
     char nm[128];
     memset   ( nm, 0, sizeof(nm));
     strncpy  ( nm, pResName,  (ResNameLen > sizeof(nm)-1) ? (sizeof(nm)-1) : ResNameLen);   
     sscError ( hError, "Unable to access resource file : %s", nm ); 
     return NULL;
  }      
 
  pthread_mutex_lock   (&g_ResourceMutex);
  
  pRes = g_pResList;
  while ( pRes != NULL )
  {
    if ( pRes->NameLen == ResNameLen )
    {
      if ( strncmp  ( pRes->szName, pResName, ResNameLen ) == 0 )
      { /* Resource Found */
        break;
      }
    }  
    pRes = pRes->pNext;
  }
 
  if ( pRes == NULL )
  {
    int   PathLen;
    int   NameLen;
    int   FileLen;
    char  Path[MAX_RGP_RES_NAME_LENGTH + MAX_RGP_RES_SUFFIX_LENGTH];
 
    /* Let's construct full path to the resource */
    NameLen = ResNameLen + 1;
    PathLen = sscGetSharedString ( RESPATH_SHARED_IDX, Path, sizeof(Path));
    if ( PathLen + NameLen + 1 < sizeof(Path) )
    { /* Path constructed*/
     FILE *pFile  = NULL;
     struct stat statbuf;
 
     if( PathLen ) 
       if( Path[PathLen-1] != '/' && Path[PathLen-1] != '\\')
           strcat( Path,"/" );
 
     strcat (Path, pResName    );
     strcat (Path, szResSuffix ); 
 
     if (  stat ( Path, &statbuf) == 0 /*&& S_ISREG(statbuf.st_mode)*/ )
     { /* File Exist */
       pFile  = fopen ( Path, "rb");
       if ( pFile  )
       {
         FileLen = (int)statbuf.st_size;
         if ( FileLen != 0 )
         {  /* file size is not zero */
            pRes = (rgpResource *) malloc ( FileLen + sizeof(rgpResource) + 2);
            if ( pRes != NULL )
            { /* Allocated */
              memset ( pRes, 0, sizeof(rgpResource));
              pRes->pRawData    = (char*) pRes + sizeof(rgpResource);
              
              strncpy ( pRes->szName, pResName, ResNameLen );
              pRes->szName[ResNameLen] = 0;
              pRes->NameLen   = ResNameLen;
              
              /* Read Raw Data */
              if ( fread ( pRes->pRawData, FileLen, 1, pFile ) == 1)
              {
                pRes->nRawDataLen         = FileLen;
                pRes->pRawData[FileLen ]  = 0; /* Terminate String */
                pRes->pRawData[FileLen+1] = 0; /* Terminate String */
                
                ParseResFile ( hError, pRes );
                
                /* Add to the List */
                pRes->pNext = g_pResList;
                g_pResList  = pRes;
              }
              else
              {
                free ( pRes );
                pRes = NULL;
              } /* End of Read Raw Data */
            } /* End of Allocated */
          } /* End of File size is not zero */
          fclose (pFile);
        } /* End if file opened */
      } /*End of file Exist */
    } /* End of Path Constructed*/
  } /* End of Resource Found */
  
  pthread_mutex_unlock (&g_ResourceMutex);
 
  return pRes;
}
 
/* ------------------------------------------------------------------------- */
/* GetRGPResString                                                           */
/* ------------------------------------------------------------------------- */
 
const char * GetRGPResString  ( sscErrorHandle  hError  , RGPResHandle hRes, 
                                const char *    pStrName, int StrLen )
{
  int           i;  
  int           nTmpl;
  rgpString   * pTmpl; 
  rgpResource * pRes = (rgpResource *) hRes;
 
  if ( pRes == NULL || pStrName == NULL || StrLen == 0 )
       return NULL;
  
  if ( StrLen == -1 ) 
       StrLen = strlen ( pStrName );

  nTmpl = pRes->nTemplUsed;
  pTmpl = pRes->TemplArr  ;
  for ( i = 0; i < nTmpl; i++, pTmpl++ )
  {
      if ( strncasecmp ( pTmpl->szName, pStrName, StrLen ) == 0 )
           return (const char *) pTmpl->pData; /* Found */
  }

  if ( hError ) 
  {
       sscError ( hError, "Resource File : %s", pRes->szName ); 
       sscError ( hError, "Unable to find String: %s", pStrName );
  }     
  
  return NULL; /* Not found */
}

/* ------------------------------------------------------------------------- */
/* RGPResExtractString                                                       */
/* ------------------------------------------------------------------------- */

const char * RGPResExtractString ( sscErrorHandle hError, RGPResHandle hDefRes, char *pArgStr  )
{
   int   FileNameLen =  0;
   int   TmplNameLen =  0;
   char *pFileName = NULL;
   char *pTmplName = NULL;
   char *pFrmtStr  = NULL;
   char *pOrgStr;

   if ( pArgStr == NULL ) 
        return NULL;

   /* Extract Resource file name */
   
   pOrgStr    = pArgStr; 
   
   pFileName  = pArgStr;
   
   pFileName += strspn ( pArgStr, " \t"     ); /* Skip leading spaces */
   pArgStr    = strpbrk ( pFileName, " :\t" ); /* find the end of resource name */
   if ( pArgStr == NULL )
   {
     FileNameLen = strlen ( pFileName );
     pArgStr     = pFileName + FileNameLen;
   } else {
     FileNameLen = pArgStr - pFileName;
   }     
   
   /* Extract template name  */
   pTmplName  = strchr ( pArgStr, ':' );  /* find ':' separator */
   if ( pTmplName != NULL )
   {
     pTmplName += 1;                             /* skip separator */   
     pTmplName += strspn ( pTmplName, " \t" );   /* skip leading spaces */
     pArgStr = strpbrk ( pTmplName,   " :\t");   /* find the end of template name */
     if ( pArgStr ) 
     {
       TmplNameLen = pArgStr - pTmplName; /* Calc length */
     } else {
       TmplNameLen = strlen(pTmplName);
       pArgStr = pTmplName + TmplNameLen;
     }
   } else {
     TmplNameLen = strlen ( pArgStr );
     pArgStr    += TmplNameLen;           /* move pointer to the end of the string  */
     pTmplName   = pArgStr;
   }
   
   /* Extact string */
   pFrmtStr = strchr ( pArgStr, ':' );  /* find ':' separator */
   if ( pFrmtStr )
   {  
      pFrmtStr += 1;  /* skip separator */ 
   }
   
   if ( pFrmtStr ) 
        return pFrmtStr; /* just format string */

   if ( TmplNameLen != 0 )
   { /* we have template name */
     RGPResHandle hRes = NULL; 
   
     if ( FileNameLen != 0 ) 
     { /* we have resource to load */
       hRes = GetRGPResHandle ( hError, pFileName, FileNameLen ); 
       if ( hRes == NULL )
       {
         if ( hError ) 
         {
              sscError ("Unable to load resource file : %s", pFileName ); 
         }     
         return  NULL;
       }     
            
     } else {
       /* Or Let's try default resorce */
       hRes = hDefRes;
     }
     
     if ( hRes )
          return GetRGPResString ( hError, hRes, pTmplName, TmplNameLen );
   }
   
   /* Let's return */
   
   if ( hError  ) 
   {
      sscError ( hError, "Unable to unpack string \"%s\"", pOrgStr  ); 
   }     
   return NULL;
}
 
/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */
void ParseResFile ( sscErrorHandle hError, rgpResource* pRes )
{
   int          TIdx ;
   int          TLen ;
   char        *pChar;
   char        *pSEnd;
   char        *pTmp ;
   rgpString   *pTmpl;
   int          OpenTagLen = strlen ( szResTagOpen );
   int          TermTagLen = strlen ( szResTagTerm );
 
   TIdx  = 0;
   pTmpl = NULL;
   pChar = pRes->pRawData;
   if ( pChar )
   {
     while ( pChar[0] != 0 )
     {
       /* Find end of the string  */
       pSEnd = strchr( pChar, '\n');
       if ( pSEnd != NULL ) /* End of string has been found */
         TLen = pSEnd - pChar;
       else
         TLen = strlen ( pChar );
 
       TLen += 1;
 
       /* Process String here */
       /* Check if new Template has been started */
       
       if ( strncmp ( pChar, szResTagOpen, OpenTagLen ) == 0 )
       { /* Template Separator has been found */
         if ( pTmpl )
         {  /* Finish Prev Template if exist */
            pTmp = (char*) pTmpl->pData + pTmpl->cData;
            pTmp[0] = 0; pTmp--;
            if ( pTmpl->cData && pTmp[0] == '\n' )  
            { 
               pTmp[0] = 0; pTmp--;
               if ( pTmpl->cData > 1 && pTmp[0] == '\r' )
               {
                    pTmp[0] = 0;
               } 
            }
            pTmpl = NULL;
         }
         /* Extract Template Name */
         /* terminate the string  */
         pChar[TLen-1] = 0;
         
         pTmp = strchr ( pChar + OpenTagLen, szResTagTerm[0] );
         if ( pTmp )
         { /* Name found */
         
           pTmp[0] = 0; /* Zero Terminate Name */
 
           /*Start New Template */
           if ( pRes->nTemplUsed >= MAX_RGP_RES_STRINGS_NUM )
                return; /* No more room for templates */
 
           pTmpl = pRes->TemplArr + pRes->nTemplUsed;
           pRes->nTemplUsed++;
 
           pTmpl->szName = pChar+OpenTagLen;    /* Set Name pointer  */
           pTmpl->pData  = pChar+TLen;          /* Start of template */
           pTmpl->cData  = 0;                   /* No data yet       */
           pTmpl->uFlags = 0;                   /* Not used for now  */
         }
         
       }
       else
       { /* Template separator was not found */
         /* Update current Template if Any   */
         if ( pTmpl )
         { /* Add this string to the template */
           pTmpl->cData += TLen;
         } else {
           /* just skip this string */
         }
       }
       pChar = pChar + TLen; /* skip '\n'  or '\0' character */
     } /* End of While*/
 
     if ( pTmpl )
     {  /* Finish Prev Template if exist */
        pTmp = (char*) pTmpl->pData + pTmpl->cData;
        pTmp[0] = 0; pTmp--;
        if ( pTmpl->cData && pTmp[0] == '\n' )  
        { 
          pTmp[0] = 0; pTmp--;
          if ( pTmpl->cData > 1 && pTmp[0] == '\r' )
          {
            pTmp[0] = 0;
          } 
        }
        pTmpl = NULL;
     }
  }
} /* End of Function ParseResFile */

/* ------------------------------------------------------------------------- */
/* End of this file                                                          */
/* ------------------------------------------------------------------------- */
