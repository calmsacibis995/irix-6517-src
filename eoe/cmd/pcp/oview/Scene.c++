/*
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident "$Id: Scene.c++,v 1.4 1999/04/30 04:03:45 kenmcd Exp $"

#include <Vk/VkApp.h>
#include <X11/Xcms.h>
#include <stdlib.h>
#include "Scene.h"

const float	theBigEPS = 1e-2;

OMC_String	Scene::theOviewLayoutCmd = "/usr/pcp/bin/oview_layout -p oview";

Scene		*theScene;

static void
x2packcol(XcmsColor &xcol, uint32_t &col)
{
    col = 0xff;			// alpha
    col |= ((int)(xcol.spec.RGBi.blue * 255.0)) << 8;
    col |= ((int)(xcol.spec.RGBi.green * 255.0)) << 16;
    col |= ((int)(xcol.spec.RGBi.red * 255.0)) << 24;
}

static void
resCol(uint32_t &col, char *rsrc, char *def)
{
    XcmsColor	xcol_exact;
    XcmsColor	xcol;
    char	*val = VkGetResource(rsrc, XmRString);
    Display	*display = theApplication->display();
    Status	s;

    if (val == NULL)
	val = def; // default;

    s = XcmsLookupColor(display,
			 DefaultColormap(display, DefaultScreen(display)),
			 val, &xcol_exact, &xcol, XcmsRGBiFormat);

    if (s == XcmsFailure) {
	if (val != def) {
	    pmprintf("%s: Unable to resolve \"%s\" as an X color, using default.\n",
		     pmProgname, val);

	    s = XcmsLookupColor(display,
				 DefaultColormap(display, 
				 DefaultScreen(display)),
				 def, &xcol_exact, &xcol, XcmsRGBiFormat);

	    if (s == XcmsFailure) {
		pmprintf("%s: Unable to resolve default \"%s\" as an X color!\n",
			 pmProgname, val);
	    }
	}
    }
    else {
	x2packcol(xcol, col);	// nb: using 'closest' rather than exact col...
    }
}

static void
resFloat(float &val, char *rsrc, float def)
{
    char	*sval = VkGetResource(rsrc, XmRString);

    if (sval == NULL)
	val = def; // default
    else
	val = (float)atof(sval);
}

static void
resString(char *&val, char *rsrc, const char *def)
{
    char	*sval = VkGetResource(rsrc, XmRString);

    if (sval == NULL)
	val = strdup(def); // default
    else
	val = strdup(sval);
}

Scene::~Scene()
{
}

Scene::Scene(const OMC_String &name, Widget parent)
: VkComponent(name.ptr())
{
    create(parent);
}

void
Scene::create(Widget parent)
{
    _baseWidget = parent;
    installDestroyHandler();

    // assign color variables with resource values or defaults
    {
	resCol(_colCPUSlack, "cpuSlack.color", "grey80");
	resCol(_colCPUMetric1, "cpuMetric1.color", "cyan2");
	resCol(_colCPUMetric2, "cpuMetric2.color", "blue2");
	resCol(_colCPUMetric3, "cpuMetric3.color", "red2");
	resCol(_colRtrLSlack, "routerLinkSlack.color", "grey80");
	resCol(_colRtrL1, "routerLinkMetric1.color", "green2");
	resCol(_colRtrL2, "routerLinkMetric2.color", "red2");
	resCol(_colNodeLinkMetric, "nodeLinkMetric.color", "green2");
	resCol(_colEvenRtr, "evenRouter.color", "cyan3");
	resCol(_colOddRtr, "oddRouter.color", "cyan3");
	resCol(_colRtr1, "routerLevel1.color", "cyan1");
	resCol(_colRtr2, "routerLevel2.color", "yellow");
	resCol(_colRtr3, "routerLevel3.color", "orange");
	resCol(_colRtr4, "routerLevel4.color", "red");
	resCol(_colNode, "node.color", "light yellow");
	resCol(_colNode1, "nodeLevel1.color", "blue");
	resCol(_colNode2, "nodeLevel2.color", "magenta");
	resCol(_colNode3, "nodeLevel3.color", "orange");
	resCol(_colNode4, "nodeLevel4.color", "red");
	resCol(_colJoin, "join.color", "grey70");
	resCol(_colCenterMark, "centerMark.color", "grey50");
	resCol(_colLinkWarn, "linkWarn.color", "red");
    }

    // assign color legend kick-in values from resources or defaults
    {
	// router color legend
	resFloat(_legendRtr1, "routerLevel1.legend", 0.0);
	resFloat(_legendRtr2, "routerLevel2.legend", 0.0);
	resFloat(_legendRtr3, "routerLevel3.legend", 0.0);
	resFloat(_legendRtr4, "routerLevel4.legend", 0.0);

	// node color legend
	resFloat(_legendNode1, "nodeLevel1.legend", 0.0);
	resFloat(_legendNode2, "nodeLevel2.legend", 0.0);
	resFloat(_legendNode3, "nodeLevel3.legend", 0.0);
	resFloat(_legendNode4, "nodeLevel4.legend", 0.0);
    }

    // assign modulation scaling variables with resource values or defaults
    {
	resFloat(_scaleUtil, "routerUtilModulationScale", 100);
	resFloat(_scaleCPU, "cpuUtilModulationScale", 1000);
	resFloat(_scaleNode, "nodeUtilModulationScale", 10);
	resString(_metricNode, "nodeMetric", "origin.numa.migr.intr.total");
    }

    // assign geometry variables with resource values or defaults
    {
	resFloat(_routerDistance, "routerDistance", 10.0);
	resFloat(_rtrLRadius, "routerLinkRadius", 0.25);
	resFloat(_axisLength, "axisLength", 0.25);
	resFloat(_nodeLRatio, "nodeLinkLength", 2.0 / 3.0);
	resFloat(_nodeLRtrRatio, "nodeLinkRouterProportion", 0.5);
	resFloat(_nodeRadius, "nodeRadius", 0.4);
	resFloat(_cpuHeight, "cpuHeight", 0.8);
	resFloat(_centerMarkLength, "centerMarkLength", 2 * theBigEPS);
	resFloat(_longBendDeviation, "longBendDeviation", 3);
	resFloat(_nodeLRtrRadius, "nodeLinkRadius", 1.0);
	resFloat(_sceneXRotation, "sceneXRotation", -3);
	resFloat(_sceneYRotation, "sceneYRotation", 30);
    }

    // simple geometry sanity
    {
	lbound(2, _routerDistance);
	fence(theBigEPS, _rtrLRadius, fsqrt(2.0) / 2.0); // _rtrCornerRadius
	lbound(theBigEPS, _axisLength);
	lbound(theBigEPS, _nodeLRatio);
	lbound(theBigEPS, _nodeRadius);
	lbound(theBigEPS, _cpuHeight);
	lbound(0, _centerMarkLength);
	lbound(theBigEPS, _longBendDeviation);
	lbound(theBigEPS, _nodeLRtrRadius);
	fence(0, _nodeLRtrRatio, 1);
    }

    // derive other geometric variables
    {
	// how much to scale the router by 
	_rtrScale = 1.0/_routerDistance;

	/* how far out from the center to translate a cylinder aligned with
	   corner, face, or diagonal */
	_rtrCornerRadius = 1.0 - (fsqrt(2.0) * _rtrLRadius);
	_rtrFaceRadius = fsqrt(2.0 / 6.0);
	_rtrDiagRadius = (1.0 - _rtrLRadius) * fsqrt(2.0) / 2.0;

	// the normal length of a router link
	_rtrLLength = ((_routerDistance - _centerMarkLength) / 2.0) -
	    _rtrCornerRadius;

	// the angle for long diagonals
	_longAngle = fasin(fsqrt(2.0 / 3.0));

	// the position of the end of a long diagonal
	_longCoord = (_rtrFaceRadius + _rtrLLength) / fsqrt(3.0);

	// How long are the null sections?
	_joinLongLength = _routerDistance * fsqrt(3.0) / 2 - 
	    (_rtrLLength + _rtrFaceRadius);
	_bendDiagLen = _routerDistance * fsqrt(2.0) / 2 - 
	    (_rtrLLength + _rtrDiagRadius);
	_joinHCubeLength = (3 * _routerDistance) / 2 - _longCoord;
	_joinDupLen = _bendDiagLen / fsqrt(2.0);

	// node link length and radius
	_nodeLLength = _rtrLLength*_nodeLRatio;
	_nodeLRadius = _nodeLRtrRadius * _rtrLRadius;

	// how far out is a node?
	if (geErr(_nodeLRadius, _nodeRadius)) { // sqrt sanity
	    _nodeLRadius = _nodeRadius - theBigEPS;
	    _nodeLRtrRadius = _nodeLRadius / _rtrLRadius;
	}
	_nodeTranslation = _nodeLLength + 
	    fsqrt(sqr(_nodeRadius) - sqr(_nodeLRadius));

	// how high is a node?
	_nodeHeight = 2 * _nodeLRadius + theBigEPS;

	// how wide is a cpu?
	_cpu_size = fsqrt(2) * _nodeRadius;

	// bent join deviations and lengths
	_bendDiagDeviation = _rtrLRadius + theBigEPS;
	_bendDiagRotation = fatan(_bendDiagDeviation / _bendDiagLen);
	_bendDiagSize = fsqrt(sqr(_bendDiagDeviation) + sqr(_bendDiagLen));

	/* _longBendDeviation *should* be calculated but my trig's not up to it
	 * _joinLongLength should probably be involved somewhere...
	 * fsqrt(3.0 / 2.0)	// 'diag' calc : 1.2
	 * 2 * fsqrt(2.0)	// 45deg calc : 2.8
	 * 2  /asin(M_PI / 6)	// 30deg cal : 3.? */

	_bendLongDeviation = _longBendDeviation  *_rtrLRadius + theBigEPS;
	_bendLongRotation = fatan(_bendLongDeviation / _joinLongLength);
	_bendLongSize = fsqrt(sqr(_bendLongDeviation) + sqr(_joinLongLength));

	// how long are the sublinks on node links?
	_nodeRtrLLength = (_nodeLLength - _centerMarkLength) * _nodeLRtrRatio;
	_subNodeLLength = (_nodeLLength - _centerMarkLength) * 
	    (1 - _nodeLRtrRatio);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	cerr << "Scene::create:" << endl;
	cerr << "  _rtrScale " << _rtrScale << endl;
	cerr << "  _rtrScale " << _rtrScale << endl;
	cerr << "  _rtrCornerRadius " << _rtrCornerRadius << endl;
	cerr << "  _rtrFaceRadius " << _rtrFaceRadius << endl;
	cerr << "  _rtrDiagRadius " << _rtrDiagRadius << endl;
	cerr << "  _rtrLLength " << _rtrLLength << endl;
	cerr << "  _longAngle " << _longAngle << endl;
	cerr << "  _longCoord " << _longCoord << endl;
	cerr << "  _joinLongLength " << _joinLongLength << endl;
	cerr << "  _bendDiagLen " << _bendDiagLen << endl;
	cerr << "  _joinHCubeLength " << _joinHCubeLength << endl;
	cerr << "  _nodeLLength " << _nodeLLength << endl;
	cerr << "  _nodeLRadius " << _nodeLRadius << endl;
	cerr << "  _nodeTranslation " << _nodeTranslation << endl;
	cerr << "  _nodeHeight " << _nodeHeight << endl;
	cerr << "  _cpu_size " << _cpu_size << endl;
	cerr << "  _bendDiagDeviation " << _bendDiagDeviation << endl;
	cerr << "  _bendDiagRotation " << _bendDiagRotation << endl;
	cerr << "  _bendDiagSize " << _bendDiagSize << endl;
	cerr << "  _bendLongRotation " << _bendLongRotation << endl;
	cerr << "  _bendLongSize " << _bendLongSize << endl;
	cerr << "  _nodeRtrLLength " << _nodeRtrLLength << endl;
	cerr << "  _subNodeLLength " << _subNodeLLength << endl;
    }
#endif

}

const char *
Scene::className()
{
    return ("Scene");
}
