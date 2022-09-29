/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Header file for locale utility functions.
 */

#define LU_SORT_SET  5000      /* size of wcs buffer that triggers sorting */

/* public function prototypes */

void __lu_sort(wchar_t *wcset, size_t sizeofset);

int __lu_cclass(cset *cs, char *class_name );
int __lu_eclass(cset *cs, char *name, int len);
int __lu_collorder(cset *cs, wchar_t wcmin, wchar_t wcmax);

int __lu_doessubexist(char *value, char *substring, size_t len);
int __lu_getstrfromsubcode(char value, char *substring, size_t *len);
int __lu_collsymbol(char *value, char *symbol, int length);

/* NOT YET USED
int __lu_hascase(char *class_name);
wchar_t __lu_charmap(char *symbol);
*/

