#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>   /* config stuff */
#include <libsc.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <arcs/cfgdata.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/gda.h>
#include <promgraph.h>
#include <pgdrv.h>
#include <standcfg.h>
#include <stringlist.h>

extern graph_hdl_t prom_graph_hdl ;
extern vertex_hdl_t hw_vertex_hdl ;


extern int rad1_install(COMPONENT *) ;

extern int _gfx_strat(COMPONENT *, IOBLOCK *);

static COMPONENT gfxtempl = {
	ControllerClass,		/* Class */
	DisplayController,		/* Type */
	ConsoleOut|Output,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL				/* Identifier */
};

/*
 * Only build graphics for SN0PROM product (IO6prom)
 */
#if defined(SN0PROM) || defined(SN0XXLPROM)

extern int kona_install(COMPONENT *) ;
extern int mgras_install(COMPONENT *) ;

#ifndef SN_PDI

typedef struct {
    unsigned char	gfx_type;
    int			(*install_func)(COMPONENT*);
} prom_gfx_info_t;

static prom_gfx_info_t prom_gfx_info[] = {
    { KLTYPE_GFX_KONA,	kona_install	},	/* InfiniteReality graphics */
    { KLTYPE_GFX_MGRA,	mgras_install	},	/* SI/MXI graphics */
    { NULL, NULL }
};

static klgfx_t *graphics_pipe_list = NULL;

/* 
 * find the pipenum'th logical graphics pipe (KLCLASS_GFX)
 */
lboard_t *
find_gfxpipe(int pipenum)
{
        gda_t           *gdap;
        cnodeid_t       cnode;
        nasid_t         nasid;
        lboard_t        *lb;
	klgfx_t		*kg,**pkg;
	int		i;

        gdap = (gda_t *)GDA_ADDR(get_nasid());
        if (gdap->g_magic != GDA_MAGIC)
        	return NULL;

	if (!graphics_pipe_list) {
		/* for all nodes */
        	for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
                	nasid = gdap->g_nasidtable[cnode];
                	if (nasid == INVALID_NASID)
                        	continue;
			lb = KL_CONFIG_INFO(nasid) ;
			while (lb = find_lboard_class(lb, KLCLASS_GFX)) {
				/* insert lb into list */
				if (!(kg = (klgfx_t*)find_first_component(lb,KLSTRUCT_GFX))) {
					lb = KLCF_NEXT(lb);
					continue;
				}
				/* set moduleslot now that we have brd_module set */
				kg->moduleslot = (lb->brd_module << 8) | SLOTNUM_GETSLOT(lb->brd_slot);
				/* make sure board has device flag set */
				kg->gfx_info.flags |= KLINFO_DEVICE;
				if (kg->cookie < KLGFX_COOKIE) {
				    kg->gfx_next_pipe = NULL;
				    kg->cookie = KLGFX_COOKIE;
				}
				pkg = &graphics_pipe_list;
				while (*pkg && (kg->moduleslot > (*pkg)->moduleslot))
				    pkg = &(*pkg)->gfx_next_pipe;
				kg->gfx_next_pipe = *pkg;
				*pkg = kg;
				lb = KLCF_NEXT(lb);
			}
		}
#ifdef FIND_GFXPIPE_DEBUG
		i = 0;
		kg = graphics_pipe_list;
		while (kg) {
			lboard_t *lb = find_lboard_modslot(KL_CONFIG_INFO(kg->gfx_info.nasid),
						(kg->moduleslot>>8),SLOTNUM_XTALK_CLASS|(kg->moduleslot&0xff));
			printf("find_gfxpipe(): %s pipe %d mod %d slot %d\n",lb?lb->brd_name:"!LBRD",i,
				(kg->moduleslot>>8),(kg->moduleslot&0xff));
			kg = kg->gfx_next_pipe;
			i++;
		}
#endif
        }

	i = 0;
	kg = graphics_pipe_list;
	while (kg && (i < pipenum)) {
		kg = kg->gfx_next_pipe;
		i++;
		}

	if (!kg) return NULL;

	return find_lboard_modslot(KL_CONFIG_INFO(kg->gfx_info.nasid),
				(kg->moduleslot>>8),
				SLOTNUM_XTALK_CLASS|(kg->moduleslot&0xff));
}


/*
 *  kl_graphics_install()
 *
 *  determine textport pipe
 *  "install" textport pipe
 */
void
kl_graphics_install(void)
{
        vertex_hdl_t    vhdl;
	char 		*kstr;
	lboard_t        *lbrd;
	char 		gfxpath[128];
	char		slotname[SLOTNUM_MAXLENGTH];

	COMPONENT 	*comptp=NULL;
	graph_error_t	graph_err;
	klinfo_t	*cki;
	__psunsigned_t 	hub_base;
	unsigned char 	hubwid;
	ULONG		Key;
	prom_dev_info_t	*dev_info;
	prom_gfx_info_t	*gi;
	struct standcfg	*s;
	extern struct standcfg standcfg[];

	extern struct string_list environ_str;

	if ((kstr = getenv("console")) &&
	    ((kstr[0] == 'd') || (kstr[0] == 'D')))
		return;

	printf("Installing Graphics Console... \n") ;

	/*
	 *  Select textport pipe in the following manner:
	 *
	 *  if (gConsoleOut is valid)
	 *      pipe = gConsoleOut
	 *  else if (console == g#)
	 *      pipe = #th logical pipe
	 *  else
	 *      pipe = first logical pipe
	 */
	lbrd = NULL;
	if ((kstr = getenv("gConsoleOut")) && strcmp(kstr,"default")) {
		strcpy(gfxpath,kstr);
       		graph_err = hwgraph_path_lookup(hw_vertex_hdl, gfxpath, &vhdl, NULL);
		if (graph_err == GRAPH_SUCCESS) {
			lbrd = pg_get_lbl(vhdl, INFO_LBL_BRD_INFO);
		} else {
			printf("Ignoring invalid gConsoleOut value: %s\n",gfxpath);
		}
	}

	if (!lbrd) {
		int pipenum = 0;
		kstr = getenv("console");
		if (kstr && (kstr[1] >= '0') && (kstr[1] <= '9')) {
			char *p = &kstr[1];
			while ((*p >= '0') && (*p <= '9'))
				pipenum = (pipenum*10) + (*p - '0');
		}
		if (((lbrd = find_gfxpipe(pipenum)) == NULL) && pipenum)
			lbrd = find_gfxpipe(0);
	}

	if (!lbrd) {
          	printf("graphics install: cannot find a gfx board\n");
		return;
	}

	/* put together gfx path */
	get_slotname(lbrd->brd_slot, slotname);
	sprintf(gfxpath,"/hw/module/%d/slot/%s/%s",
		lbrd->brd_module, slotname, lbrd->brd_name);

	if (hwgraph_path_lookup(hw_vertex_hdl, gfxpath, &vhdl, NULL) != GRAPH_SUCCESS) {
		printf("graphics install: cannot find path %s\n",gfxpath);
		return;
	}

	/* Get the dev_info struct of the device controller. */
        graph_err = graph_info_get_LBL(prom_graph_hdl, vhdl,
                                       INFO_LBL_DEV_INFO,NULL,
                                       (arbitrary_info_t *)&dev_info) ;
        if (graph_err != GRAPH_SUCCESS) {
          	printf("graphics install: cannot get dev_info\n");
          	return;
        }

        /* register strategy routine for controller */
        kl_reg_drv_strat(dev_info, _gfx_strat) ;

	/* Build the Key for this controller. */
	cki = dev_info->kl_comp ;
	Key = MK_SN0_KEY(cki->nasid, cki->widid, cki->physid) ;
	hub_base = (__psunsigned_t) REMOTE_HUB(cki->nasid, 0) ;
	hubwid = GET_FIELD64((hub_base + IIO_WIDGET_CTRL), WIDGET_ID_MASK, 0) ;
	ADD_HUBWID_KEY(Key, hubwid) ;
	ADD_HSTNASID_KEY(Key, get_nasid()) ;

	/* Fill up ARCS info and put it in klinfo_t of the device */
	if (!comptp && !(comptp = malloc(sizeof(COMPONENT)))) {
                printf("kl_graphics_install: malloc component failed \n") ;
                return;
        }

	bcopy(&gfxtempl, comptp, sizeof(COMPONENT)) ;
        comptp->AffinityMask = (ULONG)vhdl ;
	comptp->Key = Key ;

	for (gi = prom_gfx_info; gi->gfx_type; gi++)
		if (gi->gfx_type == lbrd->brd_type)
		    break;

	if (gi->gfx_type) {
		if (!gi->install_func(comptp)) {
			printf("kl_graphics_install: installation failed for %s\n",gfxpath);
			return;
		}
		/* NULL out other gfx_probe pointers in standcfg */
		for (s=standcfg; s->install; s++)
		    if (s->gfxprobe)
			if (s->install != (int (*)())gi->install_func)
			    s->gfxprobe = NULL;
	}

	/* associate component with klinfo structure */
        cki->arcs_compt = comptp ;

	/* need to set ConsoleOut */
	replace_str("ConsoleOut", gfxpath, &environ_str);

	return;
}
#endif /* !SN_PDI */

#endif /* SN0PROM */

static int num_rad1;
extern int rad1_install(COMPONENT *);
int
kl_rad1_install(vertex_hdl_t hw_vhdl, vertex_hdl_t ctlr_vhdl)
{
        int i;
        klinfo_t *klcomp;
        char *p;
        int rv;
        COMPONENT       *comptp;
        graph_error_t   graph_err ;
        klinfo_t        *cki ;
        __psunsigned_t  hub_base ;
        unsigned char   hubwid ;
        __psunsigned_t  greg_base;
        ULONG           Key ;
        prom_dev_info_t *dev_info ;
	struct standcfg	*s;
        char gtmp_buf[200];
	extern struct standcfg standcfg[];
        extern struct string_list environ_str;


	if ((p = getenv("console")) &&
            ((p[0] == 'd') || (p[0] == 'D')))
                return 1;

        printf("Installing RAD1 Graphics Console... \n") ;

        /* Get the dev_info struct of the device controller. */

        graph_err = graph_info_get_LBL(prom_graph_hdl, ctlr_vhdl,
                                       INFO_LBL_DEV_INFO,NULL,
                                       (arbitrary_info_t *)&dev_info) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("graphics install: cannot get dev_info\n");
                return 0;
        }

        /* register strategy routine for controller */

        kl_reg_drv_strat(dev_info, _gfx_strat) ;

        /* Update klinfo_t of the controller with its adapter number = virt id */

        cki        = dev_info->kl_comp ;
        cki->virtid = Key = num_rad1++ ; /* number of graphics ctlrs found */

        /* Build the rest of the Key for this controller. */

        Key |= MK_SN0_KEY(cki->nasid, cki->widid, cki->physid) ;

        hub_base = (__psunsigned_t) REMOTE_HUB(cki->nasid, 0) ;
        hubwid = GET_FIELD64((hub_base + IIO_WIDGET_CTRL), WIDGET_ID_MASK, 0) ;
        ADD_HUBWID_KEY(Key, hubwid) ;
        ADD_HSTNASID_KEY(Key, get_nasid()) ;

        /* ----------- NEW -------------- */

        /* Fill up ARCS info and put it in klinfo_t of the device */
        comptp = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (comptp == NULL) {
                printf("kl_graphics_install: malloc component failed \n") ;
                return 0 ;
        }

        bcopy(&gfxtempl, comptp, sizeof(COMPONENT)) ;
        comptp->AffinityMask = (ULONG)ctlr_vhdl ;
        comptp->Key = Key ;

        graph_info_get_LBL(prom_graph_hdl, ctlr_vhdl,
            INFO_LBL_KLCFG_INFO, NULL, (arbitrary_info_t *)&klcomp);

        sprintf(gtmp_buf,
            "/hw/module/%d/slot/%s%s/node/xtalk/%d/pci/%d",
            1, "", "MotherBoard", klcomp->widid, klcomp->physid);
        replace_str("ConsoleOut", gtmp_buf, &environ_str);

        klcomp->flags |= KLINFO_DEVICE;


        rv = rad1_install(comptp);
        if (!rv) {
               /*
                 * must use _setenv instead of setenv since "gfx"
                 * is read only.
                 */
                if (_setenv("gfx", "alive", 1) < 0) {
                        printf("_setenv for gfx failed!\n");
                        setenv("console", "d");
                }
        } else {
                        setenv("console", "d");
			init_consenv("d");
			return(0);
        }

        /* NULL out other gfx_probe pointers in standcfg */
        for (s=standcfg; s->install; s++)
            if (s->gfxprobe)
                if (s->install != (int (*)())rad1_install)
                     s->gfxprobe = NULL;

        /* associate component with klinfo structure */
        klcomp->arcs_compt = comptp ;

        return 1;

}
