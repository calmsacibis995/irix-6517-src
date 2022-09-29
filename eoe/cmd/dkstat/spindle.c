static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/dkstat/RCS/spindle.c,v 1.12 1997/12/12 03:34:24 markgw Exp $";
/*
 * --- Disk spindle statistics --
 */

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <pcp/pmapi.h>

#include "spindle.h"

/*
 * spindle_stats() alternates returning buffer 'a' and then 'b'
 * This makes it easy and efficient for the caller to compute deltas.
 */
static int ndevs = 0;
static spindle *stats_a = NULL;
static spindle *stats_b = NULL;
static int which_stats = 0; /* 0 means stats_a, 1 means stats_b */
static int initialized=0;

static int ncontrollers = 0;
static controller *controllers = NULL;

/* metric names with ordinal pmid */
static char *metrics[] = {
#define PMID_READ	0
	"irix.disk.dev.read",
#define PMID_WRITE	1
	"irix.disk.dev.write",
#define PMID_BREAD	2
	"irix.disk.dev.blkread",
#define PMID_BWRITE	3
	"irix.disk.dev.blkwrite",
#define PMID_ACTIVE	4
	"irix.disk.dev.active",
#define PMID_RESPONSE	5
	"irix.disk.dev.response",
};

static char *ctl_metric[] = {
	"irix.disk.ctl.total"
};

#define NMETRICS (sizeof(metrics)/sizeof(metrics[0]))

static pmID pmid_list[NMETRICS];
static pmID pmid_ctl_list[1];
static pmInDom spindle_indom;
static pmInDom controller_indom;

int
spindle_stats_init(spindle **spindle_list)
{
    int err;
    pmDesc desc;
    int i, j;
    int *inst_list;
    char *p;
    char **name_list;

    if (initialized)
	fprintf(stderr, "spindle_stats_init: should only be called once");
    initialized++;

    ndevs = 0;

    /* scan the disk instance domain and allocate each spindle */
    if ((err = pmLookupName(NMETRICS, metrics, pmid_list)) < 0) {
	fprintf(stderr, "pmLookupName failed: ");
	return err;
    }

    if ((err = pmLookupName(1, ctl_metric, pmid_ctl_list)) < 0) {
	fprintf(stderr, "pmLookupName failed: ");
	return err;
    }
    
    if ((err = pmLookupDesc(pmid_list[0], &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc failed: ");
	return err;
    }
    spindle_indom = desc.indom;

    if ((err = pmGetInDom(spindle_indom, &inst_list, &name_list)) < 0) {
	fprintf(stderr, "pmGetInDom failed to lookup disk spindle instance domain: ");
	return err;
    }

    ndevs = err;
    if (ndevs == 0) {
	fprintf(stderr, "diskless system?");
	return 0;
    }

    if ((stats_a = (spindle *)malloc(ndevs * sizeof(spindle))) == NULL) {
	fprintf(stderr, "out of memory allocating spindles");
	return -errno;
    }
    if ((stats_b = (spindle *)malloc(ndevs * sizeof(spindle))) == NULL) {
	fprintf(stderr, "out of memory allocating spindles");
	return -errno;
    }
    memset(stats_a, 0, ndevs * sizeof(spindle));

    for (i=0; i < ndevs; i++) {
	stats_a[i].id = inst_list[i];
	stats_a[i].dname = strdup(name_list[i]);
	stats_a[i].wname = strlen(stats_a[i].dname);
	p = stats_a[i].dname + 3;
	while (*p != NULL && isdigit(*p))
	    p++;
	if (*p == 'd')
	    p++; /* dks[0-9]*d */
	stats_a[i].unit = p;
    }

    free(inst_list); inst_list = NULL;
    free(name_list); name_list = NULL;
    memcpy(stats_b, stats_a, ndevs * sizeof(spindle));

    if ((err = pmLookupDesc(pmid_ctl_list[0], &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc failed to lookup descriptor for controllers");
	return err;
    }
    controller_indom = desc.indom;

    if ((err = pmGetInDom(controller_indom, &inst_list, &name_list)) < 0) {
	fprintf(stderr, "pmGetInDom failed to lookup disk controller instance domain");
	return err;
    }
    ncontrollers = err;

    if ((controllers = (controller *)malloc(ncontrollers * sizeof(controller))) == NULL)
	fprintf(stderr, "out of memory allocating disk controllers");
    memset(controllers, 0, ncontrollers * sizeof(controller));

    for (i=0; i < ncontrollers; i++) {
	controller *c = &controllers[i];
	c->id = inst_list[i];
	c->dname = strdup(name_list[i]);
	c->wname = strlen(c->dname);
	/* printf("controller<%s>", c->dname); */
	/* find all the drives on this controller */
	for (c->ndrives=0, c->drives=NULL, j=0; j < ndevs; j++) {
	    if (strncmp(c->dname, stats_a[j].dname, c->wname) == 0 && stats_a[j].dname[c->wname] == 'd') {
		c->ndrives++;
		if ((c->drives = (int *)realloc(c->drives, c->ndrives * sizeof(int))) == NULL)
		    return -errno;
		c->drives[c->ndrives-1] = j;
		/* printf(" disk<%s>", stats_a[j].dname); */
	    }
	}
	/* printf("\n"); */
    }

    /* success */
    *spindle_list = stats_a;
    return ndevs;
}

static void
extract_values(pmResult *r, spindle *stats)
{
    int i, j, k;
    pmValueSet *vs;
    pmValue *v;

    for (i=0; i < r->numpmid; i++) {
	vs = r->vset[i];
	if (vs->numval < 0) {
	    fprintf(stderr, "fetch failed for metric %d\n", i);
	    continue;
	}
	for (j=0; j < vs->numval; j++) {
	    v = &vs->vlist[j];
	    for (k=0; k < ndevs; k++) {
		if (v->inst == stats[k].id) {
		    switch (i) { /* ordinal pmid in pmid_list */
		    case PMID_READ:
			stats[k].reads = v->value.lval;
			break;
		    case PMID_WRITE:
			stats[k].writes = v->value.lval;
			break;
		    case PMID_BREAD:
			stats[k].breads = v->value.lval;
			break;
		    case PMID_BWRITE:
			stats[k].bwrites = v->value.lval;
			break;
		    case PMID_ACTIVE:
			stats[k].active = v->value.lval;
			break;
		    case PMID_RESPONSE:
			stats[k].response = v->value.lval;
			break;
		    default:
			assert(0); /* coding botch */
		    }
		    break;
		}
	    }
	    if (k == ndevs) {
		fprintf(stderr, "Error: instance %d \"%s\" not found in fetch result\n",
		stats[k].id, stats[k].dname);
	    }
	}
    }
}

int
spindle_stats(spindle **spindle_list, struct timeval *fetchtime)
{
    int err;
    spindle *ret;
    pmResult *res;

    if (!initialized) {
	fprintf(stderr, "spindle_stats: spindle_stats_init() has not been called");
	exit(1);
    }

    ret = which_stats ? stats_a : stats_b;
    which_stats = which_stats ? 0 : 1;

    /* do a fetch, extract results */
    if ((err = pmFetch(NMETRICS, pmid_list, &res)) < 0)
	return err;
    
    extract_values(res, ret);

    /* success */
    *fetchtime = res->timestamp;
    *spindle_list = ret;
    pmFreeResult(res);
    return 0;
}

int
spindle_stats_profile(int *inclusion_map)
{
    int err;
    int i, nlist=0, *list;

    if ((err = pmDelProfile(spindle_indom, 0, NULL)) < 0)
	return err;
    if ((list = (int *)malloc(ndevs * sizeof(int))) == NULL)
	return -errno;

    for (i=0; i < ndevs; i++) {
	if (inclusion_map[i])
	    list[nlist++] = stats_a[i].id;
    }

    err = pmAddProfile(spindle_indom, nlist, list);
    free(list);
    
    return err;
}

controller *
controller_info(int *n)
{
    *n = ncontrollers;
    return controllers;
}
