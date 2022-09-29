//
// Copyright 1997, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//

#ifndef FLAGLIST_H
#define FLAGLIST_H

#define PMRUN_UNKNOWN	0
#define PMRUN_TEXTFIELD	1
#define PMRUN_FINDER	2
#define PMRUN_RADIO	3
#define PMRUN_CHECK	4

typedef struct {
    int		specType;
    Widget	specWidget;
    char	*specFlag;
} FlagListEntry;

class FlagList : public VkComponent
{ 
  public:

    FlagList();
    virtual ~FlagList();

    int  queryType(const char *, int *);

    Widget createTextField(
	Widget parent,
	const char *labelname,
	const char *textname,
	const char *flagstr,
	const char *labelstr,
	const char *fieldstr,
	Boolean editable);

    char   *getFlag(int index);
    int    getType(int index);
    Widget getWidget(int index);


  private: 

    // the list of valid entries in _specs
    FlagListEntry	*_flaglist;
    int			_flagcount;
    int			_flagbound;

    void newFlagListEntry(int, Widget, const char *);

};

#endif

