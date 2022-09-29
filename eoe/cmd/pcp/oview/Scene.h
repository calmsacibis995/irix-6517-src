// -*- C++ -*-

#ifndef _SCENE_H_
#define _SCENE_H_

/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: Scene.h,v 1.3 1997/11/28 03:16:06 markgw Exp $"

#include <Vk/VkComponent.h>
#include <Vk/VkResource.h>

#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSeparator.h>

#include "oview.h"
#include "Bool.h"
#include "String.h"
#include "ModList.h"
#include "ColScaleMod.h"
#include "StackMod.h"
#include "YScaleMod.h"
#include "Sprout.h"

typedef char MetStr[80];

class Scene : public VkComponent
{
public:

    static OMC_String	theOviewLayoutCmd;

    ~Scene();

    Scene(const OMC_String &name, Widget);
    Scene(const OMC_String &name);

    void create(Widget);
    const char *className();

    int genInventor();

private:

    SoSeparator *_root;

    // Node metric

    char	*_metricNode;

    // modulation scaling variables (initialized from resources)

    float	_scaleUtil;
    float	_scaleCPU;
    float	_scaleNode;

    // geometric variables (initialized from resources)

    float	_routerDistance;	// dist btwn routers relative to size 
					// of router
    float	_rtrLRadius;		// relative to router size
    float	_axisLength;		// relative to cube side of 1
    float	_nodeLRatio;		// relative to _rtrLLength 
					// (from _routerDistance)
    float	_nodeRadius;		// relative to router size
    float	_cpuHeight;		// relative to router size
    float	_centerMarkLength;	// in router space
    float	_longBendDeviation;	// wrt _rtrLRadius
    float	_nodeLRtrRadius;	// wrt _rtrLRadius
    float	_nodeLRtrRatio;		// proportion of _nodeLLength
    float	_sceneXRotation;	// scene rotation about the X axis
    float	_sceneYRotation;	// scene rotation about the Y axis

    // derived geometric variables

    float	_cpu_size;

    float	_bendDiagDeviation;
    float	_bendDiagRotation;
    float	_bendDiagSize;
    float	_bendDiagLen;
    float	_bendLongDeviation;
    float	_bendLongRotation;
    float	_bendLongSize;

    float	_joinLongLength;
    float	_joinHCubeLength;
    float	_longAngle;
    float	_longCoord;
    float	_joinDupLen;

    float	_nodeLLength;
    float	_nodeLRadius;
    float	_subNodeLLength;
    float	_nodeHeight;
    float	_nodeTranslation;
    float	_nodeRtrLLength;

    float	_rtrCornerRadius;
    float	_rtrDiagRadius;
    float	_rtrFaceRadius;
    float	_rtrLLength;
    float	_rtrScale;

    // logical colours / resources

    uint32_t	_colCPUSlack;
    uint32_t	_colCPUMetric1;
    uint32_t	_colCPUMetric2;
    uint32_t	_colCPUMetric3;

    uint32_t	_colRtrLSlack;
    uint32_t	_colRtrL1;
    uint32_t	_colRtrL2;

    uint32_t	_colNodeLinkMetric;

    uint32_t	_colJoin;
    uint32_t	_colCenterMark;

    uint32_t	_colEvenRtr;
    uint32_t	_colOddRtr;
    uint32_t	_colRtr1;
    uint32_t	_colRtr2;
    uint32_t	_colRtr3;
    uint32_t	_colRtr4;

    uint32_t	_colNode;
    uint32_t	_colNode1;
    uint32_t	_colNode2;
    uint32_t	_colNode3;
    uint32_t	_colNode4;

    uint32_t	_colLinkWarn;

    // color legend kick-in values (set by resources)
    // _legendRtr conjugates are ROUT[12]_COL and RCOL[1-4]
    // _legendNode conjugates are _colNode and NCOL[1-4]

    float	_legendRtr1;
    float	_legendRtr2;
    float	_legendRtr3;
    float	_legendRtr4;

    float	_legendNode1;
    float	_legendNode2;
    float	_legendNode3;
    float	_legendNode4;

    // methods

    SoSeparator *genRouterEtc(int r_idx);
    SoSeparator *genNodes(int r_idx, SproutRouter *spr);
    SoSeparator *genNodeEtc(int n_idx, SproutNode *spn);
    SoSeparator *genNodeLinks(int n_idx);
    SoSeparator *genCpus(int n_idx, SproutNode *spn);
    SoSeparator *genCpu(int n_idx, char id, SproutNode *spn);
    SoSeparator *genModStack(int n, const MetStr metric[],
			     const float mscale[], const uint32_t *col[],
			     SoSeparator *part, float height, float width,
			     const char* str);
    SoSeparator *genHCubeLinkJoin(RealPos diff);
    SoSeparator *genLinkJoin(int order, int option);
    SoSeparator *genRouterLink (int rlink);

    void addRouterLink(SoSeparator *sep, int from_idx, int to_idx, OMC_Bool node,
		       float height, float width);

    SoSeparator *useNode();
    SoSeparator *usePrimCube();
    SoSeparator *useWarnSphere();
    SoSeparator *useBentJoin (int order);
    SoSeparator *useAvoidJoin(int order);
    SoSeparator *useBendCyl();
    SoSeparator *useOcto();
    SoSeparator *usePrimCyl();
    SoSeparator *useLinkCyl();

    SoRotation *doLinkOrient(int naxes, IntPos dirn);
    SoTranslation *doLinkTrans (int order);
    SoRotation *doLinkRot(int order, IntPos dirn);

    void fixDirn(IntPos dirn, RealPos diff);
    void fudgeLinkDiff(int order, int from_idx, int to_idx, RealPos diff);

    OMC_Bool need_bend(int order, int from_idx, int to_idx);
    OMC_Bool findNodeCorners(int r_idx, int ortho[2][3]);
    OMC_Bool even_vertex(RealPos vertex);

public:

    const char *metricNode() const
	{ return _metricNode; }
};

#endif /* _SCENE_H_ */
