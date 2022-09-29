/*
 * FILE: eoe/cmd/miser/cmd/miser/debug.h
 *
 * DESCRIPTION:
 *      Miser debug macro definition and declaration.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef __MISER_DEBUG_H
#define __MISER_DEBUG_H


#include "private.h"
#include "iostream.h"
id_type_t read_id(istream& is);

istream& operator>> (istream& is, id_type_t& id);
ostream& operator<< (ostream& os, space& queue);
istream& operator>> (istream& is, miser_seg_t& segment);
ostream& operator<< (ostream& os, miser_seg_t& segment);
istream& operator>> (istream& os, miser_qseg_t& resource);
ostream& operator<< (ostream& os, miser_qseg_t& resource);


#ifdef DEBUG

/* Print a debug string and file name and line number */
#define STRING_PRINT(string) \
        cerr << "DBG: " << string << " (" << __FILE__ << ":" << __LINE__ \
        << ")" << endl;


/* Print a debug symbol and value pair, and the file name and line number */
#define SYMBOL_PRINT(symbol) \
        cerr << "DBG: " << #symbol << " = " << symbol << " (" << __FILE__ \
        << ":" << __LINE__ << ")" << endl;


/* Print a debug function trace message, and the file name and line number */
#define TRACE(func) \
        cerr << "Call: " << #func <<  " (" << __FILE__ << ":" << __LINE__ \
        << ")" << endl; func;

#else

#define STRING_PRINT(string) ;
#define SYMBOL_PRINT(symbol) ;
#define TRACE(func) ;

#endif /* DEBUG */


#endif /* __MISER_DEBUG_H */
