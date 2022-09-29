/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 * 			All Rights Reserved.				  *
 *									  *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.; *
 * the contents of this file may not be disclosed to third parties,	  *
 * copied or duplicated in any form, in whole or in part, without the	  *
 * prior written permission of Silicon Graphics, Inc.			  *
 *									  *
 * RESTRICTED RIGHTS LEGEND:						  *
 *	Use, duplication or disclosure by the Government is subject to    *
 *	restrictions as set forth in subdivision (c)(1)(ii) of the Rights *
 *	in Technical Data and Computer Software clause at DFARS 	  *
 *	252.227-7013, and/or in similar or successor clauses in the FAR,  *
 *	DOD or NASA FAR Supplement. Unpublished - rights reserved under   *
 *	the Copyright Laws of the United States.			  *
 **************************************************************************
 *
 * File: finder.c
 *
 * Description: Routines to scan for products with release notes and
 *	build the appropriate data structures.
 *
 **************************************************************************/


#ident "$Revision: 1.4 $"


#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <Xm/Xm.h>

#include "grelnotes.h"


#define MAXLINE		1024		/* Largest line */
#define NPROD_INCR	10		/* Allocation increment for products */
#define NCHAP_INCR	5		/* Allocation increment for chapters */


static PRODUCT_LIST product_list;	/* Head of the product list struct */


static void product_scan(PRODUCT_LIST*);
static int chapter_scan(PRODUCT*, char*);
static void chapter_title(CHAPTER*, char*);
static int chapter_compare(const void*, const void*);
static int product_compare(const void*, const void*);
static void add_product(PRODUCT_LIST*, char*);
static void add_chapter(PRODUCT*, char*, char*);
static void free_products(PRODUCT_LIST*);
static void free_chapters(PRODUCT*);


/**************************************************************************
 *
 * Function: find_relnotes
 *
 * Description: Supervises the search for products with release notes
 *	and the building of the product list data structure.
 *
 * Parameters: none
 *
 * Return: Pointer to a product list.
 *
 **************************************************************************/

PRODUCT_LIST *find_relnotes()
{
    PRODUCT_LIST *plist = &product_list;

    product_scan(plist);
    return plist;
}


/*
 ==========================================================================
			 LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: product_scan
 *
 * Description: Searches for products with release notes and creates the
 *	PRODUCT list filling out the product chapter entries.
 *
 * Parameters:
 *	plist (I) - Head of product list
 *
 * Return: none
 *
 **************************************************************************/

static void product_scan(PRODUCT_LIST *plist)
{
    DIR *pdirp;
    struct dirent *pdp;

    /*
     * Make sure the product list has been freed
     */
    free_products(plist);

    /*
     * Scan the release notes directory for potential products. A
     * product is not officially entered in the product list until
     * it has been verified that the products has at least one chapter
     * of release notes.
     */
    if ((pdirp = opendir(real_relnotes_path)) != NULL) {
        while ((pdp = readdir(pdirp)) != NULL) {
	    if (*pdp->d_name != '.') {
		add_product(plist, pdp->d_name);
	    }
        }
        (void)closedir(pdirp);
    }

    /*
     * Sort the products for user convenience
     */
    if (plist->nproducts)
    	qsort(plist->products, plist->nproducts,
				sizeof(PRODUCT), product_compare);
}


/**************************************************************************
 *
 * Function: chapter_scan
 *
 * Description: Fills the chapter structure of the specified product
 *
 * Parameters: 
 *	product (I) - pointer to a product structure
 *	prod_title (I) - product title
 *
 * Return: The number of chapters is returned.
 *
 **************************************************************************/

static int chapter_scan(PRODUCT *product, char *prod_title)
{
    FILE *tcptr;
    DIR *cdirp;
    struct dirent *cdp;
    char dir[MAXLINE], buffer[MAXLINE];
    int hasTC, i;

    /*
     * Make path to relntoes chapters
     */
    (void)sprintf(dir, "%s%s/", real_relnotes_path, prod_title);
    hasTC = FALSE;			/* Assume not table of contents */

    /*
     * Scan the product directory looking for chapters and a table of
     * contents.
     */
    if ((cdirp = opendir(dir)) != NULL) {
        while ((cdp = readdir(cdirp)) != NULL) {
	    if (*cdp->d_name == '.')
		continue;
	    if (strstr(cdp->d_name, "ch") || strstr(cdp->d_name, ".gz")) {
		add_chapter(product, dir, cdp->d_name);
	    } else if (strstr(cdp->d_name, "TC"))
	        hasTC = TRUE;
	}
        (void)closedir(cdirp);
    }

    if (!product->nchapters)
        return 0;

    /*
     * Sort the chapters so that they are in the same order as would
     * be found in the table of contents
     */
    qsort(product->chapters, product->nchapters, sizeof(CHAPTER),
							chapter_compare);

    /*
     * If a table of contents exists, read it and augment the chapter
     * titles with the information found in the TC
     */
    if (hasTC) {
        (void)sprintf(buffer, "%sTC", dir);
	if ((tcptr = fopen(buffer, "r")) != NULL) {
	    i = 0;
            if (fgets(buffer, MAXLINE, tcptr)) {
                while (fgets(buffer, MAXLINE, tcptr) &&
					(i < product->nchapters)) {
		    chapter_title(&product->chapters[i], buffer);
	            i++;
	        }
	    }
	    (void)fclose(tcptr);
	}
    }

    return product->nchapters;
}


/**************************************************************************
 *
 * Function: chapter_title
 *
 * Description: Replaces tabs between chapter number and description in TC
 *	with a text string. Note that we must take care to replace only
 *	the first tab character we encounter. Some Bozo TC entries
 *	apparently have tabs in the title.
 *
 * Parameters: 
 *	chapter (O) - destination chapter whose title is to be expanded
 *	source (I) - sources string from TC
 *
 * Return: none
 *
 **************************************************************************/

static void chapter_title(CHAPTER *chapter, char *source)
{
    int len, first_tab = 1;
    char *sptr, *dptr, *dest;

    dest = chapter->title;
    len = strlen(source);
    dest = (char*)realloc(dest, strlen(dest) + len + 5);
    source[len - 1] = '\0';
    *dest = '\0';
    for (sptr = source, dptr = dest; *sptr; sptr++) {
	if (*sptr == '\t' && first_tab) {
	    (void)strcat(dptr, ".  ");
	    dptr += 3;
	    first_tab = 0;
	}
	else {
	    if (*sptr == '\t')
		*dptr++ = ' ';
	    else
	        *dptr++ = *sptr;
	}
    }
    *dptr = '\0';
    chapter->title = dest;
}


/**************************************************************************
 *
 * Function: chapter_compare, product_compare
 *
 * Description: Comparison functions for the product and chapter list
 *	sorting routine. Note that numerical chapters must be compared
 *	by magnitude while appendices (letter chapters) must be compared
 *	lexically.
 *
 * Parameters: 
 *	va, vb (I) - values to compare
 *
 * Return: <, =, > 0 depending on the comparison of the two values.
 *
 **************************************************************************/


static int chapter_compare(const void *va, const void *vb)
{
    char *a_title, *b_title;
    int a_num, b_num;

    a_title = ((CHAPTER*)va)->title;
    b_title = ((CHAPTER*)vb)->title;
    a_num = atoi(a_title);
    b_num = atoi(b_title);
    return ((a_num && b_num) ? a_num - b_num: strcmp(a_title, b_title));
}

static int product_compare(const void *va, const void *vb)
{
    return strcasecmp(((PRODUCT*)va)->title, ((PRODUCT*)vb)->title);
}


/**************************************************************************
 *
 * Function: add_product
 *
 * Description: Adds a product to the product list if there are chapters
 *	available for it.
 *
 * Parameters: 
 *	plist (I) - product list
 *	title (I) - title of product
 *
 * Return: none
 *
 **************************************************************************/

static void add_product(PRODUCT_LIST *plist, char *title)
{
    register PRODUCT *prods;
    register ushort nprods;

    /*
     * Save structure vars in register vars
     */
    prods = plist->products;
    nprods = plist->nproducts;

    /*
     * If this is the first allocation then malloc, otherwise
     * realloc for NPROD_INCR more products
     */
    if (!plist->nprod_alloc) {
        prods = (PRODUCT*)malloc(NPROD_INCR * sizeof(PRODUCT));
	plist->nprod_alloc = NPROD_INCR;
    } else if (nprods / plist->nprod_alloc) {
	plist->nprod_alloc += NPROD_INCR;
        prods = (PRODUCT*)realloc(prods, plist->nprod_alloc * sizeof(PRODUCT));
    }

    /*
     * Copy the title to the product and scan for chapters, don't
     * actually incr the product count unless there are chapters
     */
    prods[nprods].nchapters = 0;
    if (chapter_scan(&prods[nprods], title)) {
        prods[nprods].title = strdup(title);
        nprods++;
    }

    /*
     * Update the structure vars
     */
    plist->products = prods;
    plist->nproducts = nprods;
}


/**************************************************************************
 *
 * Function: add_chapter
 *
 * Description: Adds a chapter to the chapter list
 *
 * Parameters: 
 *	product (I) - pointer to the product
 *	dir (I) - chapter directory
 *	fname (I) - chapter filaname
 *
 * Return: none
 *
 **************************************************************************/

static void add_chapter(PRODUCT *product, char *dir, char *fname)
{

    register unchar nchaps;
    register CHAPTER *chaps;
    register int flen;
    register char *tptr;

    /*
     * Put frequently accessed variables into registers
     */
    nchaps = product->nchapters;
    chaps = product->chapters;

    /*
     * If this is the first allocation then malloc, otherwise
     * realloc for NCHAP_INCR more chapters
     */
    if (!nchaps) {
        chaps = (CHAPTER*)malloc(NCHAP_INCR * sizeof(CHAPTER));
    } else if (!(nchaps % NCHAP_INCR)) {
        chaps = (CHAPTER*)realloc(chaps, (nchaps + NCHAP_INCR) *
							sizeof(CHAPTER));
    }

    /*
     * Copy the title and pathname to the chapters
     */
    flen = strlen(fname);
    chaps[nchaps].fname = (char*)malloc(strlen(dir) + flen + 2);
    memset(chaps[nchaps].fname, '\0', (strlen(dir) + flen + 2));
    chaps[nchaps].title = (char*)malloc(flen + 1);
    memset(chaps[nchaps].title, '\0', (flen + 1));
    (void)sprintf(chaps[nchaps].fname, "%s%s", dir, fname);

    if ((tptr = strrchr(chaps[nchaps].fname, '/'))
	&&
	strlen(tptr) > 3 && *(tptr+1) == 'c' && *(tptr+2) == 'h' ) {

    	for ((tptr += 3); *tptr == '0'; tptr++)
		;
    	(void)strcpy(chaps[nchaps].title, tptr);
    	(void)strtok(chaps[nchaps].title, ".");
    }

    /*
     * Update the structure variables
     */
    product->nchapters++;
    product->chapters = chaps;
}


/**************************************************************************
 *
 * Function: free_products
 *
 * Description: Frees strorage for all products on the product list
 *
 * Parameters: 
 *	plist (I) - head of product list
 *
 * Return: none
 *
 **************************************************************************/

static void free_products(PRODUCT_LIST *plist)
{
    register int i;
    register PRODUCT *pptr;

    if (plist->nprod_alloc) {
	for (i = 0, pptr = plist->products; i < plist->nproducts;
								i++, pptr++) {
	    free((char*)pptr->title);
	    free_chapters(pptr);
	}
	free((char*)plist->products);
	plist->nprod_alloc = 0;
	plist->nproducts = 0;
	plist->products = NULL;
    }
}


/**************************************************************************
 *
 * Function: free_chapters
 *
 * Description: Frees storage for all chapters on a products chapter list
 *
 * Parameters: 
 *	product (I) - product whose chapters are to be freed
 *
 * Return: none
 *
 **************************************************************************/

static void free_chapters(PRODUCT *product)
{
    register int i;
    register CHAPTER *cptr;

    if (product->nchapters) {
        for (i = 0, cptr = product->chapters; i < product->nchapters;
								i++, cptr++) {
	    free((char*)cptr->title);
	    free((char*)cptr->fname);
        }
        free((char*)product->chapters);
        product->nchapters = 0;
        product->chapters = NULL;
    }
}
