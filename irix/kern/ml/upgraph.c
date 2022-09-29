/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * upgraph.c -
 * For the uni-processor systems that want/need the hwgraph.
 * 
 */

#ident "$Revision: 1.16 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <ksys/hwg.h>
#include <sys/giobus.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#include <sys/systm.h>
#include <sys/pda.h>

#ifdef DEBUG
void print_vertex(vertex_hdl_t, char*);
#endif

#define CHECK_RV(Y)	if (rv != GRAPH_SUCCESS) \
				cmn_err(CE_PANIC, "Hardware graph FAILURE : %s",Y);

/* ARGSUSED */
void
uphwg_init(vertex_hdl_t hwgraph_root)
{
	vertex_hdl_t	eisa;
	vertex_hdl_t	gio;
	vertex_hdl_t	hpc;
	vertex_hdl_t	mc;
	vertex_hdl_t	mem;
	vertex_hdl_t	node,scsi,scsi_ctlr,disk;
	graph_error_t rv;
	int		i;
#if IP22 || IP26 || IP28
	extern int is_fullhouse(void);
#endif

	/* node vhdl attached directly to /hw */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_NODE, &node);
	CHECK_RV(EDGE_LBL_NODE);

	/* cpu vhdl */
	add_cpu(node);	

	/* memory vhdl */
	rv = hwgraph_path_add(node, EDGE_LBL_MEMORY, &mem);
	CHECK_RV(EDGE_LBL_MEMORY);

	/* io vhdl */
	rv = hwgraph_path_add(node, EDGE_LBL_IO, &mc);
	CHECK_RV(EDGE_LBL_IO);

	/* gio vhdl */
	rv = hwgraph_path_add(mc, EDGE_LBL_GIO, &gio);
	CHECK_RV(EDGE_LBL_GIO);

	/* hpc vhdl ... generic hpc1/hpc3 */
	rv = hwgraph_path_add(gio, EDGE_LBL_HPC, &hpc);
	CHECK_RV(EDGE_LBL_HPC);

	/* scsi vhdl */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI, &scsi);
	CHECK_RV(EDGE_LBL_SCSI);

	/* scsi_ctlr vhdl */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI_CTLR, &scsi_ctlr);
	CHECK_RV(EDGE_LBL_SCSI_CTLR);

	/* disk vhdl */
	rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_DISK, &disk);
	CHECK_RV(EDGE_LBL_DISK);

	for (i = 0; i < GIOSLOTS; i++) {
		char 	slotstr[3];
		vertex_hdl_t slot;
		sprintf(slotstr, "%d", i);
#if IP22
		/* XXX some I2's may not have all slots */
		/* Indy/Challenge S have extra GIO slot? */
		if (is_fullhouse() && (i == 1)) {
			continue;
		}
#endif
		rv = hwgraph_path_add(gio, slotstr, &slot);
		CHECK_RV(EDGE_LBL_SLOT);
		if (i == GIO_SLOT_GFX) {
			/* XXX can we tell if gfx is present or not? */
			rv = hwgraph_edge_add(gio, slot, EDGE_LBL_GFX);
			CHECK_RV(EDGE_LBL_GFX);
		}
	}

#if IP22 || IP26 || IP28
	/* IP20 doesn't have eisa; neither does Indy - Indigo2 only */
	if (is_fullhouse()) {
		rv = hwgraph_path_add(gio, EDGE_LBL_EISA, &eisa);
		CHECK_RV(EDGE_LBL_EISA);
	}
#endif
}

graph_error_t
hpc_hwgraph_lookup(vertex_hdl_t *ret)
{
	char loc_str[32];

	sprintf(loc_str, "%s/%s/%s/%s", EDGE_LBL_NODE, EDGE_LBL_IO, 
		EDGE_LBL_GIO, EDGE_LBL_HPC);
	return hwgraph_traverse(GRAPH_VERTEX_NONE, loc_str, ret);
}

graph_error_t
gio_hwgraph_lookup(caddr_t addr, vertex_hdl_t *ret)
{
	char loc_str[32];
	int	slot;

	switch ((__psint_t)addr) {
	case GIO_SLOTGFX:
		slot = GIO_SLOT_GFX;
		break;
	case GIO_SLOT0:
		slot = GIO_SLOT_0;
		break;
	case GIO_SLOT1:
		slot = GIO_SLOT_1;
		break;
	default:
		/*
		 * XXX this should be an error, but probably means that
		 * this is a Challenge S, with the funk-o-matic IOPLUS board.
		 * Just lie and say it's on the HPC.
		 */
		return hpc_hwgraph_lookup(ret);
	}
	
	sprintf(loc_str, "%s/%s/%s/%d", EDGE_LBL_NODE, EDGE_LBL_IO, 
		EDGE_LBL_GIO, slot);
	return hwgraph_traverse(GRAPH_VERTEX_NONE, loc_str, ret);
}

/* since UniProcessors have only one cpu ....  cpu "0" only */
void 
add_cpu(vertex_hdl_t node)
{
	vertex_hdl_t cpu;
	vertex_hdl_t cpuv;
	graph_error_t rv;

	rv = hwgraph_path_add(node, EDGE_LBL_CPU, &cpu);
	CHECK_RV(EDGE_LBL_CPU);

	rv = hwgraph_path_add(cpu, "0", &cpuv);
	mark_cpuvertex_as_cpu(cpuv, 0);
	device_master_set(cpuv, node);
}

#define __DEVSTR1 "/hw/scsi_ctlr/"
#define __DEVSTR2 "/target/"
#define __DEVSTR3 "/lun/0/disk/partition/"

void
devnamefromarcs(char *devnm)
{
        int val;      
        char tmpnm[MAXDEVNAME];
        char *tmp1, *tmp2;

        val = strncmp(devnm, "dks", 3);
        if (val == 0) {
                tmp1 = devnm + 3;
                strcpy(tmpnm, __DEVSTR1);
                tmp2 = tmpnm + strlen(__DEVSTR1);

		while(*tmp1 != 'd') {
                        if((*tmp2++ = *tmp1++) == '\0')
                                return;
                }

                tmp1++;
                strcpy(tmp2, __DEVSTR2);
                tmp2 += strlen(__DEVSTR2);
                while (*tmp1 != 's') {
                        if((*tmp2++ = *tmp1++) == '\0')
                                return;
                }
                tmp1++;
                strcpy(tmp2, __DEVSTR3);
                tmp2 += strlen(__DEVSTR3);
                while (*tmp2++ = *tmp1++) {}
                        ;
                tmp2--;
                *tmp2++ = '/';
                strcpy(tmp2, EDGE_LBL_BLOCK);
                strcpy(devnm,tmpnm);
        }
}

#ifdef DEBUG

void
print_vertex(vertex_hdl_t start, char *comment)
{
	char devnm[MAXDEVNAME];
	vertex_to_name(start, devnm, MAXDEVNAME);
	cmn_err(CE_NOTE, "%s : %s",comment,devnm);
}

/*ARGSUSED1*/
int
print_edge (void *arg, vertex_hdl_t vtx)
{
        /* REFERENCED */
        graph_error_t           rc;
        char                    vnbuf[1024];
        char                    enbuf[1024];
        char                    tnbuf[1024];
        vertex_hdl_t            dst;
        vertex_hdl_t            back;
        graph_edge_place_t      place;

        rc = hwgraph_vertex_name_get(vtx, vnbuf, sizeof vnbuf);
        ASSERT(rc == GRAPH_SUCCESS);

        place = GRAPH_EDGE_PLACE_NONE;
        while (hwgraph_edge_get_next(vtx, enbuf, &dst, &place) == GRAPH_SUCCESS) {
                if (strcmp(enbuf, ".") &&
                    strcmp(enbuf, "..")) {
                        rc = hwgraph_traverse(dst, "..", &back);
                        if ((rc != GRAPH_SUCCESS) || (back != vtx)) {
                                rc = hwgraph_vertex_name_get(dst, tnbuf, sizeof tnbuf);
                                ASSERT(rc == GRAPH_SUCCESS);
                                printf("\t%s/%s => (%u)%s\n", vnbuf, enbuf, dst, tnbuf);
                        }
                }
        }

        return 0;
}

/*ARGSUSED1*/
int
print_leaf (void *arg, vertex_hdl_t vtx)
{
        /* REFERENCED */
        graph_error_t           rc;
        char                    vnbuf[1024];
        char                    enbuf[1024];
        vertex_hdl_t            dst;
        vertex_hdl_t            back;
        graph_edge_place_t      place;
        int                     kids = 0;

        rc = hwgraph_vertex_name_get(vtx, vnbuf, sizeof vnbuf);
        ASSERT(rc == GRAPH_SUCCESS);
        place = GRAPH_EDGE_PLACE_NONE;
        while (hwgraph_edge_get_next(vtx, enbuf, &dst, &place) == GRAPH_SUCCESS) {
                if (strcmp(enbuf, ".") &&
                    strcmp(enbuf, "..")) {
                        rc = hwgraph_traverse(dst, "..", &back);
                        if ((rc == GRAPH_SUCCESS) && (back == vtx))
                                kids ++;
                }
        }

        if (!kids)
                printf("\t%u\t%s\n", vtx, vnbuf);

        return 0;
}

/*  How to turn this stuff on ....
	extern int print_leaf(void *, vertex_hdl_t);
	extern int print_edge(void *, vertex_hdl_t);

	printf("    Current HWGRAPH leaf verticies:\n");
	hwgraph_vertex_visit(print_leaf, 0, 0, 0);
	printf("    Current HWGRAPH non-canonical edges:\n");
	hwgraph_vertex_visit(print_edge, 0, 0, 0);
	printf("\n");
*/
#endif /* DEBUG */
