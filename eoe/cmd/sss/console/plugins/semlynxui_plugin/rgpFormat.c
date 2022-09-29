/* --------------------------------------------------------------------------- */
/* -                             rpgFormat.c                                 - */
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sscHTMLGen.h>
#include "rgpFormat.h"

static const char  szIndexTagName  [] = "index";
static const char  szRecBegTagName [] = "beg_record";
static const char  szRecEndTagName [] = "end_record";

#define  FORMAT_MAX_ARGS  8 
static const char *StdArgNames [] = { "arg0", "arg1", "arg2", "arg3", 
                                      "arg4", "arg5", "arg6", "arg7" };
                                      

/* -------------------------------------------------------------------------*/
/* Helpers Functions -------------------------------------------------------*/
/*------------------------------------------------------------------------- */

static void OutString( const char * pStr, int cnt );
static void OutChunk ( FormatInf * pInf, const char **V, char *szIdx );

static int GetVIdxByName ( const char **Vars  , int nVar , 
                           const char  *szName, int nName );

/* -------------------------------------------------------------------------*/
/* Helpers Functions -------------------------------------------------------*/
/*------------------------------------------------------------------------- */


 
void OutString ( const char *pStr, int nStr )
{
    int  cnt;
    char szBuf[257];
    
    if ( pStr == NULL )  
         return;

    if ( nStr == -1 ) 
         nStr = strlen ( pStr );
  
    cnt = nStr;
    while ( cnt != 0 && *pStr != 0 )
    {
       if ( cnt > 256 )
       {
         strncpy ( szBuf, pStr, 256 ); szBuf[256] = 0;
         Body (szBuf);
         cnt  -= 256;
         pStr += 256;
       } else {
         strncpy ( szBuf, pStr, cnt ); szBuf[cnt] = 0;
         Body (szBuf);
         pStr += cnt;
         break;
       }  
    }
}

void OutChunk ( FormatInf * pInf, const char **V, char *szIdx )
{
    OutString ( pInf->pStr, pInf->nStr );
    if ( pInf->VIdx == IDXVAL_EMPTY ) 
         return;
         
    if ( pInf->VIdx == IDXVAL_IDX )         
    {
         OutString ( szIdx, -1 );
         return;
    }     
    if ( pInf->VIdx >= 0 ) 
         OutString ( V[pInf->VIdx], -1 );
}

int  GetVIdxByName (  const char **Vars  , int nVar , 
                      const char  *szName, int nName )
{
  int i;
  
  if ( nName == strlen(szIndexTagName) && 
       strncasecmp ( szIndexTagName , szName, nName) == 0 )
       return IDXVAL_IDX;
  
  if ( nName == strlen(szRecBegTagName) && 
       strncasecmp ( szRecBegTagName, szName, nName) == 0 )
       return IDXVAL_RECBEG;
 
  for ( i = 0; i < nVar; ++i  )
  {
    if ( strlen  ( Vars[i] ) == nName  &&
         strncasecmp ( Vars[i], szName, nName ) == 0 )  
            return i;
  }
  
  if ( nName == strlen(szRecEndTagName) && 
       strncasecmp ( szRecEndTagName, szName, nName) == 0 )
       return IDXVAL_RECEND;
  
  return -1;
}

/* -------------------------------------------------------------------------*/
/* PreFormatRecord ---------------------------------------------------------*/
/*------------------------------------------------------------------------- */
int PreFormatRecord ( const char * pFrmt,
                      FormatInf  * pInf , int nInf, 
                      const char **Vars , int nVar )
{
   int         InfIdx;
   const  char  *pBeg;
   const  char  *pEnd;
   
   pBeg   = pFrmt;
   InfIdx = 0;
   
   pInf->pStr =  pBeg;
   pInf->nStr =  0;
   pInf->VIdx =  IDXVAL_EMPTY;
   
   while ( *pBeg != 0 )
   {
      /* search for opening tag */
      pEnd = strchr ( pBeg, '%' );
      if ( pEnd == NULL )
      { /* not found */
        pInf->nStr += strlen(pBeg);
        return InfIdx+1;
      } 
      else 
      {
        pInf->nStr += pEnd - pBeg;
        
        if ( pEnd[1] == '%' )
        {
          pEnd++;
          pInf->nStr++;
          /* We must terminate format info */
          InfIdx ++;
          pInf   ++;
          if ( InfIdx >= nInf )
               return InfIdx; /* No more room for format info */
               
          pBeg = pEnd + 1;
         
          pInf->pStr =  pBeg;
          pInf->nStr =  0;
          pInf->VIdx = IDXVAL_EMPTY;
          continue; /* for next substring */
        }
        
        pBeg = pEnd;
        
        /* tag started */
        /* looking for the closing tag */
        pBeg ++; /* skip remaining % character */
        
        pEnd = strchr ( pBeg, '%' );
        if ( pEnd == NULL  )  /* the closing tag is missing */
             return InfIdx;   /* Let's get out and return what we have */
             
        /* The Name Terminator has been found */
        /* Let's resolve this by the table provided by caller */
        
        pInf->VIdx = GetVIdxByName ( Vars, nVar, pBeg, pEnd - pBeg );
        /* Create The New FormatInf */

        InfIdx ++;
        pInf   ++;
        if ( InfIdx >= nInf )
             return InfIdx; /* No more room for format info */
               
        pBeg = pEnd + 1;
         
        pInf->pStr =  pBeg;
        pInf->nStr =  0;
        pInf->VIdx =  IDXVAL_EMPTY;
      }
   } /* End of the main while */
   
   if ( pInf->nStr != 0 && pInf->VIdx >= 0 ) /* The last Chunk is not empty */
         InfIdx++;
  
   return InfIdx;
} /* End of Preformat Record */
/*------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------*/
/* FormatRecordHead --------------------------------------------------------*/
/*------------------------------------------------------------------------- */
/*------------------------------------------------------------------------- */
void FormatRecordHead ( FormatInf * pInf, int nInf )
{
  int i;
   
  if ( pInf == NULL )
       return; 
 
  for ( i = 0; i < nInf; ++i, ++pInf )
  { 
    OutString    ( pInf->pStr, pInf->nStr );
    if ( pInf->VIdx == IDXVAL_RECBEG ) 
         break;
  }
}
/*------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------*/
/* FormatRecordBody --------------------------------------------------------*/
/*------------------------------------------------------------------------- */
/*------------------------------------------------------------------------- */
void FormatRecordBody ( FormatInf * pInf, int nInf, const char **V, char *szIdx )
{
  int         i;
   
  if ( nInf == 0 || pInf == NULL )
       return ; 

  for ( i = 0; i < nInf; ++i, ++pInf )
  {
    if ( pInf->VIdx == IDXVAL_RECBEG ) 
         break;
  }
  for ( ++i, ++pInf ; i < nInf; ++i, ++pInf )
  {
    OutChunk ( pInf, V, szIdx );
    if ( pInf->VIdx == IDXVAL_RECEND ) 
         break;
  }
}

/* -------------------------------------------------------------------------*/
/* FormatRecordTail --------------------------------------------------------*/
/*------------------------------------------------------------------------- */
void FormatRecordTail ( FormatInf * pInf, int nInf )
{
  int i;
   
  if ( pInf == NULL )
       return; 
 
  for ( i = 0; i < nInf; ++i, ++pInf )
  {
    if ( pInf->VIdx == IDXVAL_RECEND ) 
         break;
  }
  for ( ++i, ++pInf ; i < nInf; ++i, ++pInf )
  {
    OutString ( pInf->pStr, pInf->nStr );
  }
}

/* -------------------------------------------------------------------------*/
/* VarPrintf        --------------------------------------------------------*/
/*------------------------------------------------------------------------- */
void VarPrintf ( const char *pFormat, const char ** Vars, const char **Vals, int n )
{
   int       i, nInf;
   FormatInf Inf[64];
   
   if ( Vars == NULL ) 
      nInf = PreFormatRecord ( pFormat, Inf, 64, StdArgNames, FORMAT_MAX_ARGS );    
   else
      nInf = PreFormatRecord ( pFormat, Inf, 64, Vars, n ); 
      
   for ( i = 0; i < nInf; ++i )
   {
     OutChunk ( Inf + i,  Vals, NULL );
   }
}

/* -------------------------------------------------------------------------*/
/* New Version      --------------------------------------------------------*/
/*------------------------------------------------------------------------- */

void  FormatField ( const char *pFrmt, const char * val )
{
   if ( strcasecmp ( pFrmt+1, "checked" ) == 0)
   { 
     if ( strcmp ( val, "1" ) == 0)
       Body ("checked");  
     return;  
   } 
   if ( strcasecmp ( pFrmt+1, "enabled" ) == 0)
   { 
     if ( strcmp ( val, "0" ) == 0)
       Body ("disabled");  
     else
       Body ("enabled");
         
     return;  
   }
   FormatedBody ( pFrmt, val );
}

const char * GetNextTag ( const char * pFrmt, int *pLen )
{
    const  char  *pBeg;
    const  char  *pEnd;
   
   *pLen =  0;
   
    if ( pFrmt == NULL || pLen == NULL ) 
         return NULL;

    pBeg = strchr ( pFrmt, '%' );
    if ( pBeg == NULL )
    { 
         *pLen = strlen( pFrmt );
         return pFrmt + *pLen;
    } 
    
    pEnd = strchr ( pBeg+1, '%' );
    if ( pEnd == NULL )
    {
      pEnd = pBeg + strlen(pBeg);
     *pLen = pEnd - pBeg;
      return pBeg;
    }
    
    if ( pEnd == pBeg+1 )
    {
        pBeg = pEnd;
    }
    
   *pLen = pEnd - pBeg;
    return pBeg;
} /* End of Get Next Tag */



const char * FormatUntill ( const char *pFormat, CMDPAIR *Vars, int Varc, const char *szUntillTagName )
{
   int         TagLen;
   int         VarIdx;
   const char *pTag  ;
   const char *pFrmt ;
   const char *pName ;
   char  szTag[128]  ;

   if ( pFormat == NULL )
   {
        return NULL;
   }     

   while ( pFormat[0] != 0 )
   {
     pTag = GetNextTag ( pFormat, &TagLen );
     if ( pTag ) 
     {
       /* Output const string if any */
       OutString ( pFormat, pTag - pFormat );
       
       if ( pTag[0] == 0 )
            break;
             
       if ( TagLen == 0 )
       {   /* %% case */
          pFormat = pTag + 1;
          continue;
       }
       
       if ( TagLen >= sizeof(szTag))
       {
         strncpy ( szTag, pTag, sizeof(szTag));
         szTag[sizeof(szTag)-1] = 0;
       } else {
         strncpy ( szTag, pTag, TagLen);
         szTag[TagLen] = 0;
       }

       pFrmt = szTag;
       pName = strchr ( szTag, ':' );  
       if ( pName != NULL ) 
       {
         szTag[pName - szTag] = 0;
         pName++;
       } else {
         pName = szTag+1;
         pFrmt = "%s";
       }    
       
       if ( szUntillTagName )
       {
         if ( strcasecmp ( pName, szUntillTagName ) == 0 )
         { 
           pFormat = pTag + TagLen;
           if ( pFormat[0] != 0 )
                pFormat ++;
           return pFormat; 
         }     
       }  
      
       /* Resolve Tag */
       if ( Vars )
       {
         VarIdx = sscFindPairByKey ( Vars, 0, pName ); 
         if ( VarIdx >= 0 )
         {
            FormatField ( pFrmt, Vars[VarIdx].value );
         }   
       }  
       pFormat = pTag + TagLen;
       if ( pFormat[0] != 0 )
            pFormat ++;
     }
   }
   return pFormat;
}

const char * FormatHead  ( const char *pFormat, CMDPAIR *Vars, int Varc )
{
  return FormatUntill    ( pFormat, Vars, Varc, szRecBegTagName );
}

const char * FormatRecord( const char *pFormat, CMDPAIR *Vars, int Varc )
{
  return FormatUntill    ( pFormat, Vars, Varc, szRecEndTagName );
}

const char * FormatTail  ( const char *pFormat, CMDPAIR *Vars, int Varc )
{
  return FormatUntill    ( pFormat, Vars, Varc, NULL );
}

/* ------------------------------------------------------------------------- */
/* End of this file                                                          */
/* ------------------------------------------------------------------------- */

