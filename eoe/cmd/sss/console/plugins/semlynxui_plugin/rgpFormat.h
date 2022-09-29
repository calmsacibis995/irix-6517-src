/* --------------------------------------------------------------------------- */
/* -                             rpgFormat.h                                 - */
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

#ifndef RGPFORMAT_INCLUDED
#define RGPFORMAT_INCLUDED

#include <sscPair.h>

 typedef struct 
 {
   const char *pStr ;
   int         nStr ;
   int         VIdx ;
 } FormatInf;

#define IDXVAL_EMPTY   -1
#define IDXVAL_IDX     -2
#define IDXVAL_RECBEG  -3
#define IDXVAL_RECEND  -4  

/* ******************************** ***************** */

int  PreFormatRecord  ( const char * pFormat,
                        FormatInf  * pInf, int nInf, 
                        const char **Vars, int nVar );

void FormatRecordHead ( FormatInf  * pInf, int nInf );
void FormatRecordTail ( FormatInf  * pInf, int nInf );
void FormatRecordBody ( FormatInf  * pInf, int nInf, 
                        const char **Vars, char *szIdx);
                        

void VarPrintf ( const char *pFormat, 
                 const char **Vars, 
                 const char **Vals, int n );

/* ******************************** ***************** */

const char * FormatHead   ( const char *pFormat, CMDPAIR *Vars, int Varc );
const char * FormatTail   ( const char *pFormat, CMDPAIR *Vars, int Varc );
const char * FormatRecord ( const char *pFormat, CMDPAIR *Vars, int Varc );

/* ******************************** ***************** */
                        
extern const char szIndexTagName  []; /* TagName of the index Tag Variable  */
extern const char szRecBegTagName []; /* TagName of the Start of the record */
extern const char szRecEndTagName []; /* TagNAme of the end of the record   */

#endif  /* RGPFORMAT_INCLUDED */

/* -------------------------------------------------------------------------- */
/* End of this file                                                           */
/* -------------------------------------------------------------------------- */
