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

#ident "$Id: Topology.c++,v 1.14 1999/04/30 04:03:45 kenmcd Exp $"

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/actions/SoLineHighlightRenderAction.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>

#include "pmapi.h"
#include "Inv.h"
#include "ModList.h"
#include "ColorMod.h"
#include "StackMod.h"
#include "Source.h"
#include "Metric.h"
#include "Sprout.h"
#include "Scene.h"
#include "Topology.h"

// special cases and flags

// the number is significant for the forced cases
enum { reflexAxisLink = 0, orthoLink = 1, diagLink = 2, longLink = 3,
       dupLink, hcubeLink, unknownLink };
const int foundSite = -1;

// metrics magic

const int cpuMetrics = 3;
const int rlinkMetrics = 2;
const int nlinkMetrics = 1;

// constants

enum Side { POS_, NEG_ };
const SbVec3f XAxis(1, 0, 0), YAxis(0, 1, 0), ZAxis(0, 0, 1);
const SbVec3f XZAxis(1, 0, 1);
const SbVec3f nXZAxis(1, 0, -1);
#define STR_SIZ 128

static int theNumRouters;		// just for need_bend at the moment

// workaround no constructor in SoIndexedFaceSet
class IndexedFaceSet: public SoIndexedFaceSet
{
public:
    ~IndexedFaceSet()
	{}
    IndexedFaceSet()
	{}
};

/*
 * topo2scene
 *
 * expects genRPos() or something else to initialise globals
 *  RPos, RLink, NLink, RMap
 *  - fill RPos with the positions of the routers
 *  - tell it how big RPos is
 *  - fill RLink with the links between the routers
 *  - fill NLink with the links from routers to nodes
 */

int
Scene::genInventor()
{
    FILE	*fd;
    OMC_Bool	isPipe = OMC_false;
    int		sts = 0;

    // config file
    if (theConfigFile.length()) {
	if (theConfigFile == "-") {
	    fd = stdin;
	}
	else {
	    if ((fd = fopen(theConfigFile.ptr(), "r")) == NULL) {
		sts = -errno;
		pmprintf("%s: Failed to open \"%s\": %s\n",
			 pmProgname, theConfigFile.ptr(), strerror(errno));
	    }
#ifdef PCP_DEBUG
	    else if (pmDebug & DBG_TRACE_APPL0)
		cerr << "Scene::genInventor: Opening config: "
		     << theConfigFile << endl;
#endif

	}
    }

    // oview_layout
    else {
	OMC_String	cmd = Scene::theOviewLayoutCmd;

	if (theSource.defaultDefined()) {
	    if (theSource.isLive())
		cmd.append(" -h ");
	    else
		cmd.append(" -a ");

	    cmd.append(theSource.which()->source());
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "Scene::genInventor: launching \"" << cmd << '\"' << endl;
#endif

	if ((fd = popen(cmd.ptr(), "r")) == NULL) {
	    sts = -errno;
	    pmprintf("%s: Failed to launch \"%s\": %s\n",
		     pmProgname, cmd.ptr(), strerror(errno));
	}

	isPipe = OMC_true;
    }

    if (sts < 0 || fd == NULL)
	return sts;

    theStartValue = 0.0;

    // get positions and links
    theNumRouters = genRPos(fd);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Scene::genInventor: detected " << theNumRouters
	     << " routers ("
	     << (NoRouter == OMC_true ? "special" : "normal") << ')' << endl;
#endif
 
    if (theNumRouters == 0) {
	//
	// bug #533893: if oview_layout was used, don't report the error twice
	//
	if (!isPipe) {
	    if (theSource.defaultDefined()) {
		if (theSource.which()->type() == PM_CONTEXT_HOST)
		    pmprintf("%s: No routers found in config file (host \"%s\")\n",
			 pmProgname, theSource.which()->host().ptr());
		else
		    pmprintf("%s: No routers found in config file (archive \"%s\","
			 " host \"%s\")\n", pmProgname,
			 theSource.which()->source().ptr(),
			 theSource.which()->host().ptr());
	    }
	    else {
		pmprintf("%s: No routers found in config file (for localhost)\n", pmProgname);
	    }
	}
	pmflush();
	exit(1);
	/* NOTREACHED */
    }

    if (isPipe)
	pclose(fd);
    else
	fclose(fd);

    // scene
    _root = new SoSeparator;
    _root->ref();

    // rotate scene about X axis
    SoRotationXYZ* rotx = new SoRotationXYZ;
    rotx->axis.setValue(SoRotationXYZ::X);
    rotx->angle.setValue(deg2rad(_sceneXRotation));
    _root->addChild(rotx);

    // rotate scene about Y axis
    SoRotationXYZ* roty = new SoRotationXYZ;
    roty->axis.setValue(SoRotationXYZ::Y);
    roty->angle = deg2rad(_sceneYRotation);
    _root->addChild(roty);

    // all routers
    for (int i=0; i<theNumRouters; i++) {
	_root->addChild(genRouterEtc(i));
    }

    // activate // debug("activating scene");
    _root->unrefNoDelete();
    theModList->setRoot(_root);

    // Prevent user from hiding entire scene
    if (NoRouter)
	theSprout->alwaysShowNodes();

    // level of detail
    switch(theDetailLevel) {
    case 1:
	Sprout::showNodeCB((Widget)0, (XtPointer)0, (XtPointer)0);
	break;
    case 2:
	Sprout::showCpuCB((Widget)0, (XtPointer)0, (XtPointer)0);
	break;
    case 0:
	theSprout->updateMenu();
	break;
    default:
	if (theNumRouters <= 4)
	    Sprout::showCpuCB((Widget)0, (XtPointer)0, (XtPointer)0);
	else if (theNumRouters <= 8)
	    Sprout::showNodeCB((Widget)0, (XtPointer)0, (XtPointer)0);
	break;
    }

    // debug("genInventor returning");
    return 0;
}

SoSeparator *
Scene::genRouterEtc (int r_idx)
{
    int a, b;
    MetStr metric;
    int r_ord = ridx2ord(r_idx);

    router_name(r_ord, a, b);

    SoSeparator *router = new SoSeparator;
    router->ref();
    router->setName((SbName)"RouterEtc");

    // translate to position
    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(RPos[r_idx]);
    router->addChild(t1);

    // scale
    SoScale *s1 = new SoScale;
    s1->scaleFactor.setValue(_rtrScale, _rtrScale, _rtrScale);
    router->addChild(s1);

    // Sprouting stuff
    SproutRouter *spr = new SproutRouter;
    router->setName((SbName)spr->name());

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    cerr << "Scene::genRouterEtc: " << spr->name();
	    if (NoRouter)
		cerr << " (special)" << endl;
	    else
		cerr << " -> " << a << ':' << b << endl;
	}
#endif

    // nodes // debug("nodes");
    router->addChild(genNodes(r_idx, spr));

    if (NoRouter) {		// quit early
	router->unrefNoDelete();
	return router;
    }

    // set up metrics
    sprintf(metric, "hw.router.recv.total_util[router:%d.%d]", a, b);
    INV_ColorScale cscale(even_vertex(RPos[r_idx]) ? _colEvenRtr : _colOddRtr);
    if (_legendRtr1 > 0.0)
	cscale.add(new INV_ColorStep(_colRtr1, _legendRtr1));
    if (_legendRtr2 > _legendRtr1)
	cscale.add(new INV_ColorStep(_colRtr2, _legendRtr2));
    if (_legendRtr3 > _legendRtr2)
	cscale.add(new INV_ColorStep(_colRtr3, _legendRtr3));
    if (_legendRtr4 > _legendRtr3)
	cscale.add(new INV_ColorStep(_colRtr4, _legendRtr4));

    // router
    INV_ColorMod *rmod =
	new INV_ColorMod(metric, _scaleUtil, cscale, useOcto());
    if (rmod->status() < 0) {
	pmprintf("%s: Could not fetch %s (router inactive)\n", 
		 pmProgname, metric);
	router->addChild(useOcto());
    } else {
	router->addChild(rmod->root());
    }

    // router links
    for (int i=0; RLink[i][FROM_] >= 0; i++) {
	if (RLink[i][FROM_] != r_idx) continue;
	// debug("router link -> %d", RLink[i][TO_]);
	router->addChild(genRouterLink(i));
    }

    router->unrefNoDelete();
    return router;
}

SoSeparator *
Scene::genNodes(int r_idx, SproutRouter *spr)
{
    SoSeparator * nodes = new SoSeparator;
    nodes->ref();
    nodes->setName((SbName)"Nodes");

    int ortho[2][3];
    OMC_Bool reorient = findNodeCorners(r_idx, ortho);

    // populate corners
    int axis=0, i, nnodes=0;

    do for (i=0; i<2; i++) {
	if (ortho[i][axis] == foundSite) {

	    SoSeparator *sep = new SoSeparator;

	    SproutNode *spn = spr->addNode();
	    SoSwitch *sw = spn->_switch;
	    sep->addChild(sw);
	    sep->setName((SbName)spn->name());

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "Scene::genNodes: " << spn->name() << endl;
#endif

	    // rotate
	    SoRotation *r1 = new SoRotation;
	    switch(axis) {
	    case xAxis:
		r1->rotation.setValue(ZAxis, (i==0) ? -M_PI/2 : M_PI/2);
		break;
	    case zAxis:
		r1->rotation.setValue(XAxis, (i==0) ? M_PI/2 : -M_PI/2);
		break;
	    case yAxis:
		r1->rotation.setValue(ZAxis, (i==0) ? 0 : M_PI);
		break;
	    }
	    sw->addChild(r1);

	    // translate
	    SoTranslation *t1 = new SoTranslation;
	    t1->translation.setValue(0, _rtrCornerRadius, 0);
	    sw->addChild(t1);

	    // orient?
	    if (xor(reorient, axis==xAxis)) {
		SoRotation *r2 = new SoRotation;
		r2->rotation.setValue(YAxis, M_PI/2);
		sw->addChild(r2);
	    }

	    // the bits
	    sw->addChild(genNodeEtc(node_index(r_idx, nnodes), spn));
	    nnodes++;
	    
	    nodes->addChild(sep);
	}
    } while (axis = zyx(axis));

    nodes->unrefNoDelete();
    return nodes;
}  

SoSeparator *
Scene::genNodeEtc(int n_idx, SproutNode *spn)
{
    int a, b;
    MetStr metric;

    node_name(NLink[n_idx][TO_], a, b);

    SoSeparator *noddy = new SoSeparator;
    noddy->ref();
    noddy->setName((SbName)"NodeEtc");
    
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Scene::genNodeEtc: " << spn->name() << " -> " << a
	     << ':' << b << endl;
#endif

    // links
    noddy->addChild(genNodeLinks(n_idx));

    // trans to end of link 
    SoTranslation *t2 = new SoTranslation;
    t2->translation.setValue(0, 
			     _nodeTranslation - ((NoRouter) ? _nodeLLength : 0),
			     0);
    noddy->addChild(t2);

    // rotate to flatness
    SoRotation *r2 = new SoRotation;
    r2->rotation.setValue(XAxis, M_PI/2);
    noddy->addChild(r2);

    // node
    sprintf(metric, "%s[node:%d.%d]", _metricNode, a, b);
    INV_ColorScale cscale(_colNode);
    if (_legendNode1 > 0.0)
	cscale.add(new INV_ColorStep(_colNode1, _legendNode1));
    if (_legendNode2 > _legendNode1)
	cscale.add(new INV_ColorStep(_colNode2, _legendNode2));
    if (_legendNode3 > _legendNode2)
	cscale.add(new INV_ColorStep(_colNode3, _legendNode3));
    if (_legendNode4 > _legendNode3)
	cscale.add(new INV_ColorStep(_colNode4, _legendNode4));

    INV_ColorMod *nmod =
	new INV_ColorMod(metric, _scaleNode, cscale, useNode());
    if (nmod->status() < 0) {
	pmprintf("%s: Could not fetch %s (node inactive)\n", 
		 pmProgname, metric);
	noddy->addChild(useNode());
    } else
	noddy->addChild(nmod->root());

    // cpus
    noddy->addChild(genCpus(n_idx, spn));

    noddy->unrefNoDelete();
    return noddy;
}

void
Scene::addRouterLink(SoSeparator *sep, int from_idx, int to_idx,
		     OMC_Bool node, float height, float width)
{
    int rport;
    int	a, b, c, d;
    char	buf[STR_SIZ];

    router_name(ridx2ord(from_idx), a, b);
    if (node) {
	node_name(NLink[to_idx][TO_], c, d);
	rport = NLink[to_idx][PORT_];
	sprintf(buf, "CrayLink: Router %d.%d.%d -> Node %d.%d\n",
		a, b, rport, c, d);
    } else {
	router_name(ridx2ord(to_idx), c, d);
	rport = port_number(from_idx, to_idx);
	sprintf(buf, "CrayLink: Router %d.%d.%d -> "
		"Router %d.%d.%d\n",
		a, b, rport,
		c, d, port_number(to_idx, from_idx));
    }
    static const uint32_t *cols[rlinkMetrics+1] =
	{&_colRtrL1, &_colRtrL2, &_colRtrLSlack};
    float scales[rlinkMetrics] =
	{_scaleUtil, _scaleUtil};
    MetStr metrics[rlinkMetrics];
    sprintf(metrics[0], "hw.router.perport.recv.bypass_util[rport:%d.%d.%d]",
	    a, b, rport);
    sprintf(metrics[1], "hw.router.perport.recv.queued_util[rport:%d.%d.%d]",
	    a, b, rport);

    sep->addChild(genModStack(rlinkMetrics, metrics, scales, cols,
			      useLinkCyl(), height, width, buf));
}

SoSeparator *
Scene::genNodeLinks(int n_idx)
{
    SoSeparator *nlink = new SoSeparator;
    char		buf[STR_SIZ];

    nlink->ref();
    nlink->setName((SbName)"NodeLinks");

    if (NoRouter) {		// no-router visual glue
	if (n_idx) {
	    SoPackedColor *mat = new SoPackedColor;
	    mat->orderedRGBA.setValue(_colJoin);
	    nlink->addChild(mat);

	    SoScale *s1 = new SoScale;
	    s1->scaleFactor.setValue(_nodeLRtrRadius, -2.0 * _rtrCornerRadius, 
				     _nodeLRtrRadius);
	    nlink->addChild(s1);

	    nlink->addChild(useLinkCyl());
	}
	nlink->unrefNoDelete();
	return nlink;
    }

    // router meter
    addRouterLink(nlink, NLink[n_idx][FROM_], n_idx,
		  OMC_true, _nodeRtrLLength, _nodeLRtrRadius);

    // center marker
    SoSeparator *sep = new SoSeparator;
    sep->ref();
    sep->setName((SbName)"CenterMarker");
    {
	SoTranslation *t0 = new SoTranslation;
	t0->translation.setValue(0, _nodeRtrLLength, 0);
	sep->addChild(t0);

	SoPackedColor *b0 = new SoPackedColor;
	b0->orderedRGBA.setValue(_colCenterMark);
	sep->addChild(b0);

	SoScale *s0 = new SoScale;
	s0->scaleFactor.setValue(_nodeLRtrRadius, _centerMarkLength, 
				 _nodeLRtrRadius);
	sep->addChild(s0);
	sep->addChild(useLinkCyl());
    }
    sep->unrefNoDelete();
    nlink->addChild(sep);

    // node meter

    // trans to other end
    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(0, _nodeLLength, 0);
    nlink->addChild(t1);

    // metrics
    int a, b, c, d;
    node_name(NLink[n_idx][TO_], c, d);
    router_name(ridx2ord(NLink[n_idx][FROM_]), a, b);
    static const uint32_t *cols[nlinkMetrics+1] =
	{&_colNodeLinkMetric, &_colRtrLSlack};
    float scales[nlinkMetrics] =
	{_scaleUtil};
    MetStr metrics[nlinkMetrics];
    sprintf(metrics[0], "hw.router.perport.send_util[rport:%d.%d.%d]",
	    a, b, NLink[n_idx][PORT_]);
    sprintf(buf, "CrayLink: Node %d.%d -> Router %d.%d.%d\n",
	    c, d, a, b, NLink[n_idx][PORT_]);

    // -ve LEN -> backwards
    nlink->addChild(genModStack(nlinkMetrics, metrics, scales, cols,
				useLinkCyl(), -_subNodeLLength, 
				_nodeLRtrRadius, buf));

    nlink->unrefNoDelete();
    return nlink;
}

// find node corners...
// returns reorientation flag
// currently returns foundSite in ortho on found sites
// should ultimately return node indicies instead
// which will mean a move away from 0/non-0 logic...

OMC_Bool
Scene::findNodeCorners(int r_idx, int ortho[2][3])
{
    int link, i, axis;
    RealPos diff;

    // init ortho
    for (axis=0; axis<3; axis++)
	for (i=0; i<2; i++) ortho[i][axis] = 0;

    // how many nodes?
    int nnodes = 0;
    for (i=0; NLink[i][FROM_] >= 0; i++)
	if (NLink[i][FROM_] == r_idx)
	    nnodes++;
    if (!nnodes)
	return OMC_false;

    // find router's orthogonal links
    // this logic's horrible -- think of something nicer
    for (link=0; RLink[link][FROM_] >=0; link++) {
	if (RLink[link][FROM_] != r_idx)
	    continue;

	// cycle thru axes, remember any w. non-zero direction
	// bail if there's a second non-zero
	axis = -1;
	for (i=0; i<3; i++) {
	    diff[i] = RPos[RLink[link][TO_]][i] - RPos[r_idx][i];
	    if (!eqErr(diff[i], 0)) {
		if (axis == -1) {
		    axis=i;
		} else {
		    axis = -1;
		    break;
		}
	    }
	}

	if (axis >= 0) {		// it's orthogonal
	    if (diff[axis] > 0)
		ortho[POS_][axis]++;
	    else
		ortho[NEG_][axis]++;
	}
    }

    // find corners without ortholinks in order of preference: X, Z, Y
    int naxes = 0;
    OMC_Bool foundone;
    axis = 0;
    do {
	foundone = OMC_false;
	for (i=0; i<2; i++) {
	    if (!ortho[i][axis]) {
		ortho[i][axis] = foundSite;
		nnodes--;
		foundone=OMC_true;
	    }
	    if (!nnodes) break;
	}
	if (foundone) naxes++;
	if (!nnodes) break;
    } while (axis=zyx(axis));

    if (nnodes)
	pmprintf("%s: Unable to place all %d nodes for router %d\n",
		 pmProgname, nnodes, r_idx);

    return (OMC_Bool)(axis==yAxis  &&  naxes < 3); // reorient
}

SoSeparator *
Scene::useNode()
{
    static SoSeparator *node = NULL;

    if (node) return node;
    node = new SoSeparator;
    node->ref();
    node->setName((SbName)"Node");

    SoCylinder *cyl = new SoCylinder;
    cyl->height = _nodeHeight;
    cyl->radius = _nodeRadius;
    node->addChild(cyl);

    return node;
}

SoSeparator *
Scene::genCpus(int n_idx, SproutNode *spn)
{
    SoSeparator *cpus = new SoSeparator;
    cpus->ref();
    cpus->setName((SbName)"Cpus");

    if (NLink[n_idx][CPU_] & 1)
	cpus->addChild(genCpu(n_idx, 'a', spn));

    // in node-space we're above/below
    SoRotation *r1 = new SoRotation;
    r1->rotation.setValue(ZAxis, M_PI);
    cpus->addChild(r1);
    
    if (NLink[n_idx][CPU_] & 2)
	cpus->addChild(genCpu(n_idx, 'b', spn));

    cpus->unrefNoDelete();
    return cpus;
}

SoSeparator *
Scene::genCpu(int n_idx, char id, SproutNode *spn)
{
    SoSeparator	*cpu = new SoSeparator;
    char	buf[STR_SIZ];
    int		a, b;

    node_name(NLink[n_idx][TO_], a, b);

    cpu->ref();

    SproutCpu *spc = spn->addCpu();
    SoSwitch *sw = spc->_switch;
    cpu->addChild(sw);
    cpu->setName((SbName)spc->name());

    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(0, _nodeHeight/2, 0);
    sw->addChild(t1);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "Scene::genCpu: " << spc->name() << " -> " << a << ':' << b 
	     << ':' << id << endl;
#endif

    // set up metrics
    static const uint32_t *cols[cpuMetrics+1] =
	{&_colCPUMetric1, &_colCPUMetric2, &_colCPUMetric3, &_colCPUSlack};
    static float scales[cpuMetrics] =
	{_scaleCPU, _scaleCPU, _scaleCPU};
    MetStr metrics[cpuMetrics];
    sprintf(metrics[0],"kernel.percpu.cpu.wait.total[cpu:%d.%d.%c]",
	    a, b, id);
    sprintf(metrics[1], "kernel.percpu.cpu.user[cpu:%d.%d.%c]", a, b, id);
    sprintf(metrics[2], "kernel.percpu.cpu.sys[cpu:%d.%d.%c]", a, b, id);
    sprintf(buf, "CPU %c on Node %d.%d\n", id, a, b);

    // blocks
    sw->addChild(genModStack(cpuMetrics, metrics, scales, cols,
			     usePrimCube(), _cpuHeight, _cpu_size, buf));

    cpu->unrefNoDelete();
    return cpu;
}

// nb: have n+1 values in color still. based on genStack()

SoSeparator *
Scene::genModStack(int n, const MetStr metric[],
		   const float mscale[], const uint32_t *col[],
		   SoSeparator *part, float height, float width,
		   const char* str)
{
    SoSeparator *stack = new SoSeparator;

    stack->ref();
    stack->setName((SbName)"ModStack");

    SoScale *s1 = new SoScale;
    s1->scaleFactor.setValue(width, height, width);
    stack->addChild(s1);

    INV_MetricList *list = new INV_MetricList;
    for (int i=0; i<n; i++) {
	if (list->add(metric[i], mscale[i]) >= 0)
	    list->add(*col[i]);
    }

    list->resolveColors(INV_MetricList::perValue);

    INV_StackMod *mod = new INV_StackMod(list, part, INV_StackMod::fixed);
    mod->setFillColor(*col[n]);
    mod->setFillText(str);
    stack->addChild(mod->root());

    stack->unrefNoDelete();
    return stack;
}

// normalized block (sits on plane)
SoSeparator *
Scene::usePrimCube()
{
    static SoSeparator *block = NULL;
    if (block) return block;

    block = new SoSeparator;
    block->ref();
    block->setName((SbName)"PrimCube");

    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(0, 0.5, 0);
    block->addChild(t1);

    SoCube *cube = new SoCube;
    cube->height = cube->width = cube->depth = 1;
    block->addChild(cube);

    return block;
}

OMC_Bool
Scene::even_vertex(RealPos vertex)
{
    int even = 0;
    for (int i=0; i<3; i++)
	even += vertex[i];
    // if (!eqErr(vertex[i], 0)) even++;
    return (OMC_Bool)(even % 2);
}

SoSeparator *
Scene::genRouterLink (int rl_idx)
{
    int from_idx = RLink[rl_idx][FROM_];
    int to_idx = RLink[rl_idx][TO_];

    SoSeparator *rlink = new SoSeparator;
    rlink->ref();
    rlink->setName((SbName)"RouterLink");

    RealPos diff;
    int naxes=0, pathlen=0;
    for (int i=0; i<3; i++) {
	diff[i] = RPos[to_idx][i] - RPos[from_idx][i];
	if (!eqErr(diff[i], 0)) naxes++;
	pathlen += abs(diff[i]);
    }
    // debug("rlink dirn: naxes %d pathlen %d", naxes, pathlen);

    IntPos dirn = {0,0,0};
    if (RLink[rl_idx][DUP_]) {
	if (RLink[rl_idx][DUP_] > 1) // more than one duplicate
	    pmprintf("%s: Overlapping duplicate link number %d "
		     "from:%d to:%d\n", pmProgname,
		     RLink[rl_idx][DUP_],from_idx,to_idx);
	if (naxes == 1 && pathlen == 1)
	    naxes = dupLink;	// mostly equiv to 2
	pathlen = dupLink;	// cause/defeat unknown test
    }
    if (naxes == 1 && pathlen == 3) { // hcube link
	naxes = hcubeLink;		    // mostly equiv to 3
	fudgeLinkDiff(naxes, from_idx, to_idx, diff); // easier in router space
	if (from_idx < to_idx)
	    rlink->addChild(genHCubeLinkJoin(diff)); // dirn
    }
    else if (pathlen != naxes) {
	pmprintf("%s: Unrecognized%s link type! from: %d to: %d "
		 "vector: [%.1f %.1f %.1f]\n", pmProgname,
		 (RLink[rl_idx][DUP_] ? " duplicated" : ""),
		 from_idx, to_idx, diff[xAxis], diff[yAxis], diff[zAxis]);

	// a line
	SoVertexProperty *vp = new SoVertexProperty;
	vp->orderedRGBA.setValue(_colLinkWarn);
	vp->vertex.set1Value(0, 0, 0, 0);
	vp->vertex.set1Value(1, _routerDistance*diff[xAxis],
			     _routerDistance*diff[yAxis], _routerDistance*diff[zAxis]);
	rlink->addChild(vp);
	SoLineSet *lset = new SoLineSet;
	lset->numVertices = 2;
	rlink->addChild(lset);

	pathlen = unknownLink; // use this to flag behaviour later
    }
    fixDirn(dirn, diff);

    // rotate // debug("rlink rot");
    rlink->addChild(doLinkRot(naxes, dirn));

    // orient // debug("rlink orient");
    SoRotation *orient = doLinkOrient(naxes, dirn);
    if (orient)
	rlink->addChild(orient);

    // translate // debug("rlink trans");
    rlink->addChild(doLinkTrans(naxes));

    // link // debug("rlink link");
    addRouterLink(rlink, from_idx, to_idx, OMC_false, _rtrLLength, 1);

    // join // debug("rlink join");
    if (pathlen == unknownLink)
	naxes = unknownLink;
#ifndef HCUBE_MESS
    if (from_idx < to_idx  ||  naxes >= hcubeLink)
        rlink->addChild(genLinkJoin(naxes,
				    need_bend(naxes, from_idx, to_idx)));
#else /* HCUBE_MESS */
    if (from_idx < to_idx  ||  naxes >= unknownLink) // hcubeLink
        rlink->addChild(genLinkJoin(naxes, (naxes == hcubeLink) ? haxis :
				    need_bend(naxes, from_idx, to_idx)));
#endif /* HCUBE_MESS */
    
    rlink->unrefNoDelete();
    return rlink;
}

OMC_Bool
Scene::need_bend(int order, int from_idx, int to_idx)
{
    switch(order) {

    case diagLink: {		// one other to check
	// find other corners of square
	int c0 = UNASSIGNED, c1 = UNASSIGNED;
	int i, match, same;
	for (int r=0; r<theNumRouters; r++) {
	    if (r == from_idx  ||  r == to_idx) continue;
	    match = same = 0;
	    for (i=0; i<3; i++) {
		if (eqErr(RPos[r][i], RPos[to_idx][i]))
		    match++;
		if (eqErr(RPos[r][i], RPos[from_idx][i]))
		    same++;
	    }
	    if (match == 2 && same == 2) { // found a corner
		if (c0 == UNASSIGNED)
		    c0 = r;
		else if (c1 == UNASSIGNED)
		    c1 = r;
		else			// impossible!
		    pmprintf("%s: Found fifth corner of square!\n", pmProgname);
	    }
	}
	// found corners?
	if (c0 == UNASSIGNED || c1 == UNASSIGNED)
	    return OMC_false;

	// link btwn corners?
	for (i=0; RLink[i][FROM_] >= 0; i++) { // only checking one-way
	    if (RLink[i][FROM_] == c0  && RLink[i][TO_] == c1) // found diag
		return OMC_true;
	}
	return OMC_false;
    }

		case longLink: {		// three others to check
		    RealPos ctr;
		    for (int i=0; i<3; i++)
			ctr[i] = (RPos[to_idx][i]+RPos[from_idx][i])/2;
		    
		    // expensive, but *easy*
		    // if the centerpoint of the two links co-incide, we need to bend
		    float tmp;
		    for (int r=0; RLink[r][FROM_] >= 0; r++) {
			if (RLink[r][FROM_] > RLink[r][TO_] // don't check both dirns
			    || RLink[r][FROM_] == from_idx
			    || RLink[r][TO_] == to_idx)
			    continue;
			for (i=0; i<3; i++) {
			    tmp = (RPos[RLink[r][FROM_]][i] + RPos[RLink[r][TO_]][i])/2;
			    if (!eqErr(ctr[i], tmp))
				break;
			}
			if (i==3)			// the links collide
			    return OMC_true;
		    }
		    return OMC_false;
		}

		default: break;		// nothing
		}
    return OMC_false;
}

void
Scene::fixDirn(IntPos dirn, RealPos diff)
{
    for (int i=0; i<3; i++) {
	if (eqErr(diff[i], 0)) {
	    dirn[i] = 0;
	} else if (diff[i] > 0) {
	    dirn[i] = 1;
	} else
	    dirn[i] = -1;
    }
}

// nb: can return NULL

SoRotation *
Scene::doLinkOrient(int naxes, IntPos dirn)
{
    float angle = 0;

    switch(naxes) {
    case orthoLink:		// no need (no bend)
    case unknownLink:		// no need (no bend)
	break;

    case diagLink:
	angle = 0;
	if (dirn[xAxis] == 0) {	// YZ
	    angle = M_PI/2;
	    if (dirn[yAxis] != dirn[zAxis])
		angle += M_PI;
	} else if (eqErr(dirn[zAxis], 0)) { // XY
	    if (dirn[xAxis] != dirn[yAxis])
		angle += M_PI;
	} else {		// XZ
	    angle = M_PI/4;
	    if (dirn[xAxis] != dirn[zAxis])
		angle += -dirn[zAxis] * M_PI/2;
	    else if (dirn[xAxis] > 0)
		angle += M_PI;
	}
	break;

    case hcubeLink:
#ifndef HCUBE_MESS
	if (dirn[xAxis] != dirn[zAxis])
	    angle = -M_PI/2;
	else
	    angle = +M_PI/2;
	if (dirn[yAxis] < 0)
	    angle += M_PI/2;
#else /* HCUBE_MESS */
	if (dirn[xAxis] > 0) {
	    if (dirn[yAxis] > 0) {
		if (dirn[zAxis] > 0) {	// +++
		    angle = M_PI/4;
		} else {		// ++-
		    angle = -M_PI/6 + M_PI/4;
		}
	    } else {
		if (dirn[zAxis] > 0) {	// +-+
		    angle = -M_PI/6 + 3*M_PI/4;
		} else {		// +--
		    angle = -M_PI/4;
		}
	    }
	} else {
	    if (dirn[yAxis] > 0) {
		if (dirn[zAxis] > 0) {	// -++
		    angle = -M_PI/6 - 3*M_PI/4;
		} else {		// -+-
		    angle = -3*M_PI/4;
		}
	    } else {
		if (dirn[zAxis] > 0) {	// --+
		    angle = 3*M_PI/4;
		} else {		// ---
		    angle = -M_PI/6 -M_PI/4;
		}
	    }
	}
#endif /* HCUBE_MESS */
	break;

    case longLink:
	angle -= M_PI/4;
	if (dirn[xAxis] != dirn[zAxis])
	    angle += M_PI/2;
	if (dirn[xAxis] != dirn[yAxis])
	    angle += M_PI;
	break;

    case dupLink:
	if (dirn[xAxis] != 0) {
	    angle = dirn[xAxis] * M_PI/2;
	} else if (dirn[zAxis] != 0) {
	    angle = ((dirn[zAxis]<0)?3:1)* -M_PI/4;
	} else if (dirn[yAxis] > 0) {
	    angle = M_PI;
	}
	break;

    default:
	pmprintf("%s: Unexpected router join (type %d)\n", pmProgname, naxes);
    }

    if (!eqErr(angle, 0)) {
	SoRotation *orient = new SoRotation;
	orient->rotation.setValue(YAxis, angle);
	return orient;
    }

    return NULL;
}

SoSeparator *
Scene::genHCubeLinkJoin(RealPos diff)
{
    SoSeparator *join = new SoSeparator;
    join->ref();
    join->setName((SbName)"HCubeJoin");
    
    SoPickStyle *s = new SoPickStyle;
    s->style = SoPickStyle::UNPICKABLE;
    join->addChild(s);

    SoPackedColor *mat = new SoPackedColor;
    mat->orderedRGBA.setValue(_colJoin);
    join->addChild(mat);

    SoTransform *t1 = new SoTransform;
    t1->translation.setValue(_longCoord * (diff[xAxis]>0 ? 1:-1),
			     _longCoord * (diff[yAxis]>0 ? 1:-1),
			     _longCoord * (diff[zAxis]>0 ? 1:-1));
    t1->scaleFactor.setValue(1, _joinHCubeLength*2, 1);

    for (int i=0; i<3; i++)
	if (fabs(diff[i]) > 2) break;
    switch(i) {
    case xAxis:
	t1->rotation.setValue(ZAxis, -M_PI/2 * (diff[xAxis]>0 ? 1:-1));
	break;
    case yAxis:
	if (diff[yAxis]<0) t1->rotation.setValue(XAxis, M_PI);
	break;
    case zAxis:
	t1->rotation.setValue(XAxis, M_PI/2 * (diff[zAxis]>0 ? 1:-1));
	break;
    }
    join->addChild(t1);

    // cylinder
    join->addChild(useLinkCyl());
    
    join->unrefNoDelete();
    return join;
}

// modifies diff for special links

void
Scene::fudgeLinkDiff (int order, int from_idx, int to_idx, RealPos diff)
{
    if (from_idx > to_idx) {	// always defer to one end -- logic uncertain..
	int tmp = from_idx;
	from_idx = to_idx;
	to_idx = tmp;
    }
    switch(order) {
    case hcubeLink:
    case unknownLink:
	{
	    /* for each axis find another router in the local cube
	     * with a direction we need filled
	     * (don't need to fill the one we've already got) */
	    int i, j, idx;
	    float dist;

	    RealPos otherdiff;
	    otherdiff[0] = diff[0]; 
	    otherdiff[1] = diff[1]; 
	    otherdiff[2] = diff[2];

	    for (i=0; i<3; i++) { // each direction
		// each router
		for (idx=0; idx < theNumRouters && eqErr(diff[i], 0); idx++) {
		    if (idx==from_idx || idx==to_idx) continue;
		    dist=0;
		    for (j=0; j<3; j++)
			dist+= sqr(RPos[idx][j] - RPos[from_idx][j]);
		    if (leErr(dist, 3))		// local!
			diff[i] = RPos[idx][i] - RPos[from_idx][i];
		}
	    }

	    for (i=0; i<3; i++) { // each direction
		// each router
		for (idx=0; idx < theNumRouters && eqErr(otherdiff[i], 0); idx++) {
		    if (idx==from_idx || idx==to_idx) continue;
		    dist=0;
		    for (j=0; j<3; j++)
			dist+= sqr(RPos[idx][j] - RPos[to_idx][j]);
		    if (leErr(dist, 3))		// local!
			otherdiff[i] = RPos[idx][i] - RPos[to_idx][i];
		}
	    }

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		cerr << "Scene::fudgeLinkDiff: Calculating long-haul pos"
		     << endl;
#endif

	    for (i = 0; i < 3; i++) {
		if (diff[i] != otherdiff[i] && diff[i] == 0.0) {
		    diff[i] = otherdiff[i];

#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			cerr << "Scene::fudgeLinkDiff: Swapping long-haul pos for axis "
			     << i << endl;
#endif
		}
	    }

	    break;
	}
    default:			// nothing
	break;
    }
}

SoSeparator *
Scene::genLinkJoin(int order, int option)
{
    SoSeparator *join = new SoSeparator;
    SoScale	*s0 = NULL;

    join->ref();
    join->setName((SbName)"LinkJoin");
    
    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(0, _rtrLLength, 0);
    join->addChild(t1);
    
    SoPackedColor *mat = new SoPackedColor;
    mat->orderedRGBA.setValue(_colJoin);
    join->addChild(mat);

    switch (order) {
    case orthoLink:
	    mat->orderedRGBA.setValue(_colCenterMark);
	    s0 = new SoScale;
	    s0->scaleFactor.setValue(1, _centerMarkLength, 1);
	    join->addChild(s0);
	    join->addChild(useLinkCyl());
	    break;
    case diagLink:
    case longLink:
	if (option) {		// collision
	    join->addChild(useBentJoin(order));
	} else {			// straightness!
	    s0 = new SoScale;
	    s0->scaleFactor.setValue(1, 2*((order == diagLink) ?
					   _bendDiagLen : _joinLongLength), 1);
	    join->addChild(s0);
	    join->addChild(useLinkCyl());
	}

	break;

    case hcubeLink:
#ifndef HCUBE_MESS
	join->addChild(useBendCyl());
	break;
#else /* HCUBE_MESS */
	SoRotation *r0 = new SoRotation;
	switch(option) {
	case xAxis:
	    r0->rotation.setValue(YAxis, M_PI/3);
	    break;
	case yAxis:
	    // r0->rotation.setValue(XAxis, M_PI/6 - M_PI/4);
	    r0->rotation.setValue(YAxis, M_PI/3);
	    break;
	case zAxis:
	    r0->rotation.setValue(YAxis, -M_PI/3);
	    break;
	}
	join->addChild(r0);
	/* FALLTHROUGH */
#endif /* HCUBE_MESS */

    case dupLink:
	join->addChild(useAvoidJoin(order));
	break;

    case unknownLink:
	join->addChild(useWarnSphere());
	break;

    default:
	pmprintf("%s: Unexpected router join (type %d)\n", pmProgname, order);
    }

    join->unrefNoDelete();
    return join;
}

SoSeparator *
Scene::useAvoidJoin(int order)
{
    static SoSeparator *dup_join = NULL;
    static SoSeparator *hcube_join = NULL;
    SoSeparator *join;
    float LEN, ANG;

    switch(order) {
    case hcubeLink:
	if (hcube_join) return hcube_join;
	join = hcube_join = new SoSeparator;
	LEN = _joinHCubeLength;
	ANG = /* M_PI/2 - */ _longAngle;
	break;

    case dupLink:
	if (dup_join) return dup_join;
	join = dup_join = new SoSeparator;
	LEN = _joinDupLen;
	ANG = M_PI/4;
	break;

    default: // impossible! (no easy return)
	pmprintf("%s: Unexpected router join (type %d)\n", pmProgname, order);
	pmflush();
	exit(1);
	/* NOTREACHED */
    }
    join->ref();
    join->setName((SbName)"AvoidJoin");

    join->addChild(useBendCyl());

    SoRotationXYZ *r0 = new SoRotationXYZ;
    r0->axis.setValue(SoRotationXYZ::X);
    r0->angle = ANG;
    join->addChild(r0);

    SoSeparator *sep = new SoSeparator;
    sep->ref();
    {
	SoScale *s0 = new SoScale;
	s0->scaleFactor.setValue(1, LEN*2, 1);
	sep->addChild(s0);

	sep->addChild(useLinkCyl());
    }
    sep->unrefNoDelete();
    join->addChild(sep);

    SoTranslation *t0 = new SoTranslation;
    t0->translation.setValue(0, LEN*2, 0);
    join->addChild(t0);

    join->addChild(useBendCyl());
    return join;
}

SoSeparator *
Scene::useWarnSphere()
{
    static SoSeparator *warn = NULL;
    if (warn) return warn;
    warn = new SoSeparator;
    warn->ref();
    warn->setName((SbName)"WarnSphere");

    SoPackedColor *mat = new SoPackedColor;
    mat->orderedRGBA.setValue(_colLinkWarn);
    warn->addChild(mat);

    SoScale *s0 = new SoScale;
    s0->scaleFactor.setValue(0.5, 0.5, 0.5);
    warn->addChild(s0);

    SoTranslation *t0 = new SoTranslation;
    t0->translation.setValue(0, 1, 0);
    warn->addChild(t0);

    warn->addChild(new SoSphere());

    return warn;
}

// was once:
// s0->scaleFactor.setValue(1, 2*LEN, 1); // straight

SoSeparator *
Scene::useBentJoin (int order)
{
    static SoSeparator *diag_bend = NULL;
    static SoSeparator *long_bend = NULL;
    SoSeparator *bend;
    float SIZ, ROT;

    switch(order) {
    case diagLink:
	if (diag_bend) return diag_bend;
	bend = diag_bend = new SoSeparator;
	SIZ = _bendDiagSize;
	ROT = _bendDiagRotation;
	break;
    case longLink:
	if (long_bend) return long_bend;
	bend = long_bend = new SoSeparator;
	SIZ = _bendLongSize;
	ROT = _bendLongRotation;
	break;
    default: // impossible! (no easy return)
	pmprintf("%s: Unexpected router join (type %d)\n", pmProgname, order);
	pmflush();
	exit(1);
	/* NOTREACHED */
    }
    bend->ref();
    bend->setName((SbName)"BentJoin");

    SoRotation *r0;
    SoScale *s0;
    SoTranslation *t0;
    SoSeparator *sep;
    SoPickStyle *style;

    // Removing selection of joins
    style = new SoPickStyle;
    style->style = SoPickStyle::UNPICKABLE;
    bend->addChild(style);

    // * niceness 0 *
    bend->addChild(useBendCyl());

    // * link up *

    // bend 
    r0 = new SoRotation;
    r0->rotation.setValue(XAxis, ROT);
    bend->addChild(r0);

    // link
    sep = new SoSeparator;
    bend->addChild(sep);
    {
	s0 = new SoScale;
	s0->scaleFactor.setValue(1, SIZ, 1);
	sep->addChild(s0);
	// cylinder
	sep->addChild(useLinkCyl());
    }

    // trans
    t0 = new SoTranslation;
    t0->translation.setValue(0, SIZ, 0);
    bend->addChild(t0);

    // * niceness 1 *
    bend->addChild(useBendCyl());

    // * link down *

    // unbend
    r0 = new SoRotation;
    r0->rotation.setValue(XAxis, -2*ROT);
    bend->addChild(r0);

    // link
    sep = new SoSeparator;
    bend->addChild(sep);
    {
	s0 = new SoScale;
	s0->scaleFactor.setValue(1, SIZ, 1);
	sep->addChild(s0);
	// cylinder
	sep->addChild(useLinkCyl());
    }

    // trans
    t0 = new SoTranslation;
    t0->translation.setValue(0, SIZ, 0);
    bend->addChild(t0);

    // * niceness 2 *
    bend->addChild(useBendCyl());

    return bend;
}


SoSeparator *
Scene::useBendCyl()
{
    static SoSeparator *bend = NULL;
    if (bend) return bend;

    bend = new SoSeparator;
    bend->ref();
    bend->setName((SbName)"BendCyl");

    // lie on side
    SoPickStyle *s = new SoPickStyle;
    s->style = SoPickStyle::UNPICKABLE;
    bend->addChild(s);

    SoRotation *r0 = new SoRotation;
    r0->rotation.setValue(ZAxis, M_PI/2);
    bend->addChild(r0);

    SoCylinder *cyl = new SoCylinder;
    cyl->height = _rtrLRadius*2; // - BIG_EPS;
    cyl->radius = _rtrLRadius;
    bend->addChild(cyl);

    return bend;
}

SoTranslation *
Scene::doLinkTrans (int order)
{
    SoTranslation *t = new SoTranslation;
    t->ref();

    switch (order) {
    case orthoLink:
	t->translation.setValue(0, _rtrCornerRadius, 0);
	break;
    case diagLink:
    case dupLink:
	t->translation.setValue(0, _rtrDiagRadius, 0);
	break;
    case longLink:
    case hcubeLink:
    case unknownLink:
	t->translation.setValue(0, _rtrFaceRadius, 0);
	break;

    default:
	pmprintf("%s: Unexpected router join (type %d)\n", pmProgname, order);
    }

    t->unrefNoDelete();
    return t;
}


SoRotation *
Scene::doLinkRot (int order, IntPos dirn)
{
    SoRotation *rot = new SoRotation;
    rot->ref();

    switch (order) {

    case reflexAxisLink:
	pmprintf("%s: Reflexive router relationship\n", pmProgname);
	break;

    case orthoLink:
	if (dirn[xAxis] != 0) {
	    rot->rotation.setValue(ZAxis, -dirn[xAxis] * M_PI/2);
	} else if (dirn[zAxis] != 0) {
	    rot->rotation.setValue(XAxis, dirn[zAxis] * M_PI/2);
	} else if (dirn[yAxis] < 0) {
	    rot->rotation.setValue(ZAxis, M_PI);
	}
	break;

    case dupLink:		// only for ortho duplicated links at present
	if (dirn[xAxis] != 0) {
	    rot->rotation.setValue(ZAxis, -dirn[xAxis] * M_PI/4);
	} else if (dirn[zAxis] != 0) {
	    rot->rotation.setValue(((dirn[zAxis]<0) ? XZAxis : nXZAxis),
				   dirn[zAxis] * M_PI/2);
	} else if (dirn[yAxis] != 0) {
	    rot->rotation.setValue(XAxis, ((dirn[yAxis]<0)?3:1)*M_PI/4);
	}
	break;

    case diagLink:
	if (dirn[xAxis] == 0) {
	    if (dirn[yAxis] > 0)
		rot->rotation.setValue(XAxis, dirn[zAxis] * M_PI/4);
	    else
		rot->rotation.setValue(XAxis, dirn[zAxis] * 3*M_PI/4);
	} else if (dirn[zAxis] == 0) {
	    if (dirn[yAxis] > 0)
		rot->rotation.setValue(ZAxis, -dirn[xAxis] * M_PI/4);
	    else
		rot->rotation.setValue(ZAxis, -dirn[xAxis] * 3*M_PI/4);
	} else {			// Y == 0
	    if (dirn[xAxis] == dirn[zAxis])
		rot->rotation.setValue(nXZAxis, dirn[zAxis] * M_PI/2);
	    else
		rot->rotation.setValue(XZAxis, dirn[zAxis] * M_PI/2);
	}
	break;

    case longLink:		// long diagonal link
    case hcubeLink:
    case unknownLink:
	{
	    float angle = (dirn[zAxis] > 0) ? _longAngle : -_longAngle;
	    if (dirn[yAxis] < 0)
		angle = M_PI-angle;
	    
	    if (dirn[xAxis] > 0) {
		rot->rotation.setValue(((dirn[zAxis]<0)? XZAxis : nXZAxis), 
				       angle);
	    } 
	    else {
		rot->rotation.setValue(((dirn[zAxis]<0)? nXZAxis : XZAxis), 
				       angle);
	    }
	    break;
	}

    default:
	pmprintf("%s: Unexpected router join (type %d)\n", pmProgname, order);
    }

    rot->unrefNoDelete();
    return rot;
}

// PARTS

// octahedron

SoSeparator *
Scene::useOcto()
{
    static RealPos octo_verts[] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0},
			       {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
    static RealPos octo_norms[] = {{1, 1, 1}, {-1, 1, 1}, {-1, -1, 1},
			       {1, -1, 1}, {1, 1, -1}, {-1, 1, -1},
			       {-1, -1, -1}, {1, -1, -1}};
    static int32_t octo_idx[] = {0, 1, 2, -1,
				 0, 2, 3, -1,
				 0, 3, 4, -1,
				 0, 4, 1, -1,
				 5, 2, 1, -1,
				 5, 3, 2, -1,
				 5, 4, 3, -1,
				 5, 1, 4, -1};

#ifdef INVENTOR_INSANE
    static int32_t octo_nidx[] = {0, 1, 2, 3, 4, 5, 6, 7};
#endif

    static SoSeparator *octo = NULL;
    if (octo) return octo;
    
    octo = new SoSeparator;
    octo->ref();
    octo->setName((SbName)"Octahedron");

    SoShapeHints *hints = new SoShapeHints;
    hints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    hints->shapeType = SoShapeHints::SOLID;
    hints->faceType = SoShapeHints::CONVEX;
    octo->addChild(hints);

    SoCoordinate3 *coord = new SoCoordinate3;
    coord->point.setValues(0, 6, octo_verts);
    octo->addChild(coord);

    SoNormal *norm = new SoNormal;
    norm->vector.setValues(0, 8, octo_norms);
    octo->addChild(norm);

#ifndef INVENTOR_INSANE

    SoNormalBinding *nb = new SoNormalBinding;
    nb->value = SoNormalBinding::PER_FACE;
    octo->addChild(nb);

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 8*4, octo_idx);
    octo->addChild(ifs);

#else // INVENTOR_INSANE

    /* RANT:
     * inventor provides us no way to guarantee
     * that these faces will be drawn if we draw them together.
     * I quote the man page:
     * "If the current complexity value is less than 0.5, some faces will be
     *  skipped during rendering."
     * this happens even if the faces are taking up significant amounts
     * of screen space.
     * we can't guarantee that complexity is >= 0.5 here without
     * forcing it high for *everything*. (can't override an override).
     * solution: draw all faces individually and hope it won't
     * decide to skip *them*, too. (d'oh!)
     *
     * furthermore, allocating arrays of inventor objects is verboten,
     * but we're doing it anyway -- we guarantee these won't be released
     * until the end of time, by hanging on to an extra ref
     *
     * we can keep the 'solid' &c shape hints 'cos we're drawing all
     * the faces...
     */

    SoNormalBinding *nb = new SoNormalBinding;
    nb->value = SoNormalBinding::PER_FACE_INDEXED;
    octo->addChild(nb);

    IndexedFaceSet *ifs = new IndexedFaceSet[8];
    for (int i=0; i<8; i++) {
	ifs[i].coordIndex.setValues(0, 4, octo_idx+(i*4));
	ifs[i].normalIndex.setValue(octo_nidx[i]);
	octo->addChild(ifs+i);
    }

#endif // INVENTOR_INSANE

    return octo;
}

SoSeparator *
Scene::usePrimCyl()
{
    static SoSeparator *lcyl = NULL;
    if (lcyl) return lcyl;

    lcyl = new SoSeparator;
    lcyl->ref();
    lcyl->setName((SbName)"PrimCyl");

    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(0, 0.5, 0);
    lcyl->addChild(t1);
    
    SoCylinder *cyl = new SoCylinder;
    cyl->height = 1;
    cyl->radius = 0.5;
    lcyl->addChild(cyl);

    return lcyl;
}

/* the normalized cylinder has its bottom at 000 and its top at 010
 * scale in Y, then translate, rotate as desired... */

SoSeparator *
Scene::useLinkCyl()
{
    static SoSeparator *lcyl = NULL;
    if (lcyl) return lcyl;

    lcyl = new SoSeparator;
    lcyl->ref();
    lcyl->setName((SbName)"LinkCyl");

    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(0, 0.5, 0);
    lcyl->addChild(t1);

    /* it's unclear whether turning off the ends of the cylinder is cheaper
     * or more expensive -- the SOLID (backface culling) flag may be unset
     * by the object in these cases...
     * for now we turn the ends off and do our best */

    SoShapeHints *hints = new SoShapeHints;
    hints->shapeType = SoShapeHints::SOLID;
    lcyl->addChild(hints);
    
    SoCylinder *cyl = new SoCylinder;
    cyl->height = 1;
    cyl->radius = _rtrLRadius;	// used in so many places, it'd be a sin not to
    cyl->parts = SoCylinder::SIDES;
    lcyl->addChild(cyl);

    return lcyl;
}
