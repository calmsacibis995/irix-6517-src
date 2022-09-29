/*
 * RegionInfo.h
 *
 *	declaration of RegionInfo class for regview
 *
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.4 $"

#include <sys/procfs.h>

#include <X11/Intrinsic.h>

#include <Vk/VkCallbackObject.h>
#include <limits.h>

class RegionInfo : public VkCallbackObject {
public:
    static const char updateCB[];
    static const char newRegionCB[];

    enum EPageState { kResident, kZapped, kPagedOut, kInvalid };

    RegionInfo();
    ~RegionInfo();
    void detach();
    void zap();
    prmap_sgi_t *getMap() { return &map; }
    prpgd_sgi_t *getPageData() { return pageData; }
    prpsinfo_t *getPSInfo() { return &psinfo; }
    char *getObjName() { return objName; }
    int getPageSize() { return pageSize; }
    long getOffset() { return offset; }
    static EPageState getPageState(pgd_t *pgd);
private:
    pid_t pid;
    caddr_t vaddr;
    char objName[PATH_MAX];
    XtInputId inputId;
    Boolean detached;

    int pageSize;
    prmap_sgi map;
    prpgd_sgi *pageData;
    prpsinfo psinfo;
    long offset;

    unsigned int interval;
    XtIntervalId timer;

    void readInput();
    int update();
    void setOffset();
    void setObjName();
    bool findObjInPath(const char *path);
    
    static void readInputCB(XtPointer client, int *fd, XtInputId *id);
    static void updateTimerProc(XtPointer client, XtIntervalId *id);
};
    
