/**************************************************************************
 *									  *
 * 	      Copyright (c) 1991 Silicon Graphics, Inc.			  *
 *			All Rights Reserved		  		  *
 *									  *
 *         THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI             *
 *									  *
 * The copyright notice above does not evidence any actual of intended    *
 * publication of such source code, and is an unpublished work by Silicon *
 * Graphics, Inc. This material contains CONFIDENTIAL INFORMATION that is *
 * the property of Silicon Graphics, Inc. Any use, duplication or         *
 * disclosure not specifically authorized by Silicon Graphics is strictly *
 * prohibited.								  *
 *									  *
 * RESTRICTED RIGHTS LEGEND:						  *
 *									  *
 * Use, duplication or disclosure by the Government is subject to         *
 * restrictions as set forth in subdivision (c)(1)(ii) of the Rights in   *
 * Technical Data and Computer Software clause at DFARS 52.227-7013,	  *
 * and/or in similar or successor clauses in the FAR, DOD or NASA FAR     *
 * Supplement. Unpublished - rights reserved under the Copyright Laws of  *
 * the United States. Contractor is SILICON GRAPHICS, INC., 2011 N.       *
 * Shoreline Blvd., Mountain View, CA 94039-7311                          *
 **************************************************************************
 *
 * File: TextView.h
 *
 * Description: Public include file for the multi-font text viewer widget.
 *
 **************************************************************************/


#ident "$Revision: 1.1 $"


#ifndef _VWR_TEXTVIEW_H_
#define _VWR_TEXTVIEW_H_


/* Resource names */

#define VwrNtextColumns		"textColumns"
#define VwrNtextRows		"textRows"
#define VwrNblankCompress	"blankCompress"
#define VwrNtextFile		"textFile"
#define VwrNverticalScrollBar	"verticalScrollBar"
#define VwrNhorizontalScrollBar	"horizontalScrollBar"

#define VwrNnormFont		"normalFont"
#define VwrNunderlineFont	"underlineFont"
#define VwrNboldFont		"boldFont"
#define VwrNboldUnderFont	"boldUnderFont"

#define VwrNnormFore		"normalForeground"
#define VwrNunderlineFore	"underlineForeground"
#define VwrNboldFore		"boldForeground"
#define VwrNboldUnderFore	"boldUnderForeground"

#define VwrNselectionFore	"selectionForeground"
#define VwrNselectionBack	"selectionBackground"

#define VwrNloadCallback	"loadCallback"

/* Resource classes */

#define VwrCTextColumns		"TextColumns"
#define VwrCTextRows		"TextRows"
#define VwrCBlankCompress	"BlankCompress"
#define VwrCTextFile		"TextFile"
#define VwrCScrollBar		"ScrollBar"
#define VwrCLoadCallback	"LoadCallback"


/* Class record info */

extern WidgetClass vwrTextViewWidgetClass;

typedef struct _VwrTextViewClassRec *VwrTextViewWidgetClass;
typedef struct _VwrTextViewRec *VwrTextViewWidget;


/* Text position structure */

typedef struct {
    int line;		/* Line number (starts at 0) */
    int col;		/* Column number (starts at 0) */
} VwrPositionStruct;


/* File load callback structure */
/*	Note that storage for the fields is reused and members
	of long term interest should be copied to the user space
*/

typedef struct {
    char *filename;	/* Name of file that was loaded */
} VwrTextViewLoadCallbackStruct;


/* Public functions */

#ifdef __cplusplus
extern "C" {
#endif
extern Widget	VwrCreateScrolledTextView(Widget, char*, Boolean, Boolean);
extern void	VwrTextViewClearSelection(Widget);
extern void	VwrTextViewSetSelection(Widget, VwrPositionStruct*,
						VwrPositionStruct*, Boolean);
extern char*	VwrTextViewReadSelection(Widget, unsigned long*);
extern int	VwrTextViewSearch(Widget, char*, Boolean, VwrPositionStruct*,
						VwrPositionStruct*);
#ifdef __cplusplus
}
#endif


#endif
