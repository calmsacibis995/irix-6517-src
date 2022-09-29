#include "odsy_internals.h"

#define OUTPUT(word) {				\
		if ((cnt & 0x1) == 0)		\
		    buf = (word);		\
		else {				\
		    ODSY_WRITE_HW(hw, dbe,	\
				  ODSY_DW_CFIFO_PAYLOAD(buf, (word))); \
		}				\
		cnt++; }

#define ODSY_DFIFO_WORDS	(ODSY_DFIFO_DWORDS * 2)
#define ODSY_DFIFO_SHORTS	(ODSY_DFIFO_DWORDS * 4)

#define OUTPUT32(word, scnt)			\
		switch (scnt & 0x3) {		\
		case 3:				\
			ODSY_WRITE_HW(hw, dbe,	\
				      ODSY_DW_CFIFO_PAYLOAD(buf1, buf2)); \
			scnt++;			\
		case 0:				\
			buf1 = (word);		\
			scnt += 2;		\
			break;			\
		case 1:				\
			scnt++;			\
		case 2:				\
			buf2 = (word);		\
			ODSY_WRITE_HW(hw, dbe,	\
				      ODSY_DW_CFIFO_PAYLOAD(buf1, buf2)); \
			scnt += 2;		\
			break;			\
		}

#define OUTPUT16(shrt, scnt) {					\
		if ((scnt & 0x3) == 0)				\
			buf1 = (shrt) & 0xffff;			\
		else if ((scnt & 0x3) == 1)			\
			buf1 |= ((shrt) << 16) & 0xffff0000;	\
		else if ((scnt & 0x3) == 2)			\
			buf2 = (shrt) & 0xffff;			\
		else {						\
			buf2 |= ((shrt) << 16) & 0xffff0000;	\
			ODSY_WRITE_HW(hw, dbe,			\
				      ODSY_DW_CFIFO_PAYLOAD(buf1, (buf2))); \
		}						\
		scnt++; }

#if !defined(_STANDALONE)

int gfP_OdsyBRDMGRsetColors(struct gfx_gfx *gfxp,
                            struct rrm_rnode *rnp,
                            void *arg,int *rvalp)
{
	odsy_brdmgr_set_cmap_arg set_cmap_arg;
	int array_size;
	odsy_brdmgr_cmap_colorentry *array, *pp, *qq;
	odsy_data *bdata = ODSY_BDATA_FROM_GFXP(gfxp); 
	odsy_hw *hw = bdata->hw;
	__uint32_t buf;
	int cnt = 0;

	ODSY_CLEAR_HW_TOUCH_ERROR(hw);


	GFXLOGT(ODSY_gfP_OdsyBRDMGRsetColors);
	ODSY_COPYIN_SIMPLE_ARG(arg, &set_cmap_arg, odsy_brdmgr_set_cmap_arg,
			       "gfP_OdsyBRDMGRsetColors: bad arg ptr\n");

	if (set_cmap_arg.num_entries == 0) {
		*rvalp=0;
		return 0;
	}


	array_size = set_cmap_arg.num_entries * (int) sizeof(odsy_brdmgr_cmap_colorentry);
	array = (odsy_brdmgr_cmap_colorentry *) kmem_alloc(array_size, KM_SLEEP);
	
	if ( ! array ) {
		*rvalp = ENOMEM;
		return *rvalp;
	}

#ifdef ODSY_DEBUG_CMAP
	printf("entries=%d array=%x, set_cmap_arg.colors=%x\n",
	       set_cmap_arg.num_entries, array, set_cmap_arg.colors);
#endif

	ODSY_COPYIN_ARG((void *)set_cmap_arg.colors, array, array_size,
			"gfP_OdsyBRDMGRsetColors: bad arg array\n");


	ODSY_DFIFO_PSEMA(bdata);

	/*
	 * make two passes over colorentry array
	 * first time looking for consecutive index entries
	 * then write consecutive index entries with one burst write to dbe
	 */
	for (pp = qq = array; qq < array + set_cmap_arg.num_entries; qq++) {
	    if (qq == array + set_cmap_arg.num_entries - 1 ||
		(qq->index + 1) != (qq + 1)->index) {
		/* write out pp to qq inclusive */

		if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1)))
			goto error_return;

		OUTPUT( DBE_PACKET_HDR(DBE_UPDATE_V_RETRACE,
					DBE_BUZZ_ALL,
					DBE_ADDR(cmap_clut) + pp->index,
					(__uint32_t) (qq - pp + 1)) );
		while (pp <= qq) {
		    if ((cnt % (ODSY_DFIFO_WORDS)) == 0) {
			if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
			    goto error_return;
		    }
		    OUTPUT(pp->packed_rgb);
		    pp++;
		}

	    }
	}
	if (cnt & 0x1) {
	    /* pad out the last double-word with a no-op */
	    if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1)))
		goto error_return;
	    OUTPUT(DBE_PACKET_HDR(DBE_UPDATE_V_RETRACE,
				  DBE_BUZZ_ALL,
				  DBE_ADDR(cmap_clut),
				  0));
	}

	ODSY_DFIFO_VSEMA(bdata);

	kmem_free(array, array_size);
	
#ifdef ODSY_CHECK_HW_TOUCH_ERROR
	if (ODSY_CHECK_HW_TOUCH_ERROR(hw)) {
		cmn_err(CE_WARN,"odsy: hw write error during colormap download");
		*rvalp=-1;
		return -1;
	}
#endif
	*rvalp=0;
	return 0;
error_return:
	ODSY_DFIFO_VSEMA(bdata);
	kmem_free(array, array_size);
	return(*rvalp = EIO);
}
#endif  /* _STANDALONE */

int odsyConfigFb(odsy_data *bdata,
	odsy_brdmgr_config_fb_arg *cfbarg)
{
	int bufnr;
	odsy_mm *mm;
	odsy_hw *hw;
	unsigned tdx, tdy, nrtiles, num_acbufs;
	unsigned fb_32bpp_nrtiles, ovlw_32bpp_nrtiles,
		czbfr_32bpp_nrtiles, acbfr_32bpp_nrtiles;
	/*
	 * 2 ^ (dma_fb_depth+1) = depth in bytes.
	 * E.g., dma_fb_depth = 3 implies f.b. depth = 16 bytes.
	 */
	int dma_fb_depth;
	unsigned fb_32bpp_origin_tilenr;
	unsigned ovlw_32bpp_origin_tilenr;
	unsigned czbfr_32bpp_origin_tilenr;
	unsigned acbfr_32bpp_origin_tilenr;
	unsigned ptbfr_32bpp_origin_tilenr;
	unsigned four_tile_offset;
	odsy_ddinfo *odsyinfo;

	mm = &bdata->mm;
	odsyinfo = &bdata->info.dd_info;

	/* how many tiles in each direction? */
	tdx = ODSY_MM_NR_TILES_FROM_PIXELS(cfbarg->width);
	tdx = (tdx+1)&(~0x1); /* Ensure we are double stride aligned */
	tdy = ODSY_MM_NR_TILES_FROM_PIXELS(cfbarg->height);
	nrtiles = tdx * tdy;
		
	/* 
	** the nrtiles is the same for both the frame buffer and 
	** ovly/wid buffer.  the thing is: their address space
	** footprints will be different.  recall tiles are not
	** fixed size entities.  their size depends upon the
	** depth.  the framebuffer depth is given in the args.
	** the ovly/wid buffer is always 2 bytes deep.

	** note: we know the origin address of the frame buffer already...
	** we just have to determine its sdram address space extent
	** so we know where to put the ovly/wid buffer.  
	** once we do this we then calculate the address space extent
	** for the ovly/wid buffer so we know where to start the
	** texture/pbuffer space.

	** we're going to determine how many maximum depth tiles 
	** it takes to span the frame buffer, given its depth and
	** number of tiles.  we'll do the same for the ovly/wid
	** buffer.  this code looks alot like that in odsyMMalloc...
	*/
	switch (cfbarg->depth) {
		case 32:
			fb_32bpp_nrtiles = nrtiles;
			dma_fb_depth = 4;	/* log2(depth) / 2 */
			break;
		case 16:
			fb_32bpp_nrtiles = (nrtiles>>1) + (nrtiles & 0x1 ? 1 : 0);
			dma_fb_depth = 3;
			break;
		case 8:
			dma_fb_depth = 2;
			fb_32bpp_nrtiles = (nrtiles>>2) + (nrtiles & 0x3 ? 1 : 0);
			break;
	}

	ovlw_32bpp_nrtiles = (nrtiles>>4) + (nrtiles & 0xf ? 1 : 0);




	/* this is setup way early; the frame buffer lives behind the cfifo */
	fb_32bpp_origin_tilenr =
			ODSY_MM_32BPP_TILENR_FROM_ADDR(mm->fbfr.base_addr);

	/* we need to align this to a 64 pixel (4 tile) boundary...
	** actually this alignment should happen in the native tile 
	** depth space.  doing it in 32bpp guarantees it will be in
	** all others, but can be wasteful.
	*/
	if (fb_32bpp_origin_tilenr & 0x3) {
		fb_32bpp_origin_tilenr &= ~0x3;
		fb_32bpp_origin_tilenr += 4;
	}

	mm->fbfr.base_addr = ODSY_MM_32BPP_ADDR_FROM_TILENR(fb_32bpp_origin_tilenr);
	mm->fbfr.attr.bpp  = cfbarg->depth;
	mm->fbfr.attr.dx   = cfbarg->width;
	mm->fbfr.attr.dy   = cfbarg->height;
	mm->fbfr.attr.dz   = 1;
	mm->fbfr.attr.ox   = 0;
	mm->fbfr.attr.oy   = 0;
	mm->fbfr.attr.oz   = 0;

	mm->screen_grp.objs[ODSY_MM_MAIN_LEVEL] = &mm->fbfr;


	/*
	** WID/Overlay buffer allocation --
	**
	** The ovly/wid buffer begins at the next 32bpp tile.  
	** We have to do the same 4-tile alignment (with the same note
	** about native depth vs 32bpp depth)...
	*/
	ovlw_32bpp_origin_tilenr =
				fb_32bpp_origin_tilenr + fb_32bpp_nrtiles;
	if (ovlw_32bpp_origin_tilenr & 0x3) {
		ovlw_32bpp_origin_tilenr &= ~0x3;
		ovlw_32bpp_origin_tilenr += 4;
	}

	mm->wbfr.base_addr =
		ODSY_MM_32BPP_ADDR_FROM_TILENR(ovlw_32bpp_origin_tilenr);
	mm->wbfr.attr.bpp  = 2;
	mm->wbfr.attr.dx   = cfbarg->width;
	mm->wbfr.attr.dy   = cfbarg->height;
	mm->wbfr.attr.dz   = 1;
	mm->wbfr.attr.ox   = 0;
	mm->wbfr.attr.oy   = 0;
	mm->wbfr.attr.oz   = 0;
	mm->screen_grp.objs[ODSY_MM_OVLYWID_LEVEL]=&mm->wbfr;


	/*
	** Coarse Z buffer allocation --
	**
	** The coarse z buffer begins at the next 32bpp tile.  
	** We have to do the same 4-tile alignment (with the same note
	** about native depth vs 32bpp depth)...
	*/
	czbfr_32bpp_origin_tilenr =
				ovlw_32bpp_origin_tilenr + ovlw_32bpp_nrtiles;
	if (czbfr_32bpp_origin_tilenr & 0x3) {
		czbfr_32bpp_origin_tilenr &= ~0x3;
		czbfr_32bpp_origin_tilenr += 4;
	}
					/* XXXXXXXXXXXXXXXXXXXXX */
	czbfr_32bpp_nrtiles = 0;	/* XXX unimplemented XXX */
					/* XXXXXXXXXXXXXXXXXXXXX */
	mm->screen_grp.objs[ODSY_MM_COARSEZ_LEVEL] = 0;


	/*
	** Accumulation buffer allocation --
	**
	** The accumulation buffer begins at the next 32bpp tile.  
	** Do the same evilness wrt 4-tile alignment.
	*/
	acbfr_32bpp_origin_tilenr =
				czbfr_32bpp_origin_tilenr + czbfr_32bpp_nrtiles;
	if (acbfr_32bpp_origin_tilenr & 0x3) {
		acbfr_32bpp_origin_tilenr &= ~0x3;
		acbfr_32bpp_origin_tilenr += 4;
	}

	switch (cfbarg->acbuf_type) {
		case ODSY_ACCUM_NONE:		/* Software accum buff */
			num_acbufs = 0;
			break;
		case ODSY_ACCUM_LOW_RES:	/* Low res accum buffer */
			num_acbufs = 2;
			break;
		case ODSY_ACCUM_HI_RES:		/* High res accum buffer */
			num_acbufs = 4;
			break;
	}

	for (bufnr=0; bufnr < ODSY_MM_ACCUM_MAX_LEVELS; bufnr++) {
		mm->abfr[bufnr].base_addr = 0;
		mm->screen_grp.objs[ODSY_MM_ACCUM_BASE_LEVEL+bufnr] = 0;
	}

	for (bufnr=0; bufnr < num_acbufs; bufnr++) {
		
		mm->abfr[bufnr].base_addr =
			ODSY_MM_32BPP_ADDR_FROM_TILENR(acbfr_32bpp_origin_tilenr+((nrtiles*bufnr)/(32/8)));
		mm->screen_grp.objs[ODSY_MM_ACCUM_BASE_LEVEL+bufnr] = mm->abfr+bufnr;
		mm->abfr[bufnr].attr.bpp	 = 8;
		mm->abfr[bufnr].attr.dx	 = cfbarg->width;
		mm->abfr[bufnr].attr.dy	 = cfbarg->height;
		mm->abfr[bufnr].attr.dz  = 1;
		mm->abfr[bufnr].attr.ox  = 0;
		mm->abfr[bufnr].attr.oy  = 0;
		mm->abfr[bufnr].attr.oz  = 0;
		mm->abfr[bufnr].attr.accum_type = cfbarg->acbuf_type;
	}
	acbfr_32bpp_nrtiles = (nrtiles * num_acbufs) / (32/*bpp*//8/*bpp*/);



	/*
	** Texture, imaging, and P buffers are dynamically allocated
	** from the remaining space.
	**
	** Find the base address of the first tile of texture/imaging/pbuffer
	** space...  do the same evilness wrt 4-tile alignment.
	*/
	ptbfr_32bpp_origin_tilenr =
			acbfr_32bpp_origin_tilenr + acbfr_32bpp_nrtiles;
	if (ptbfr_32bpp_origin_tilenr & 0x3) {
		ptbfr_32bpp_origin_tilenr &= ~0x3;
		ptbfr_32bpp_origin_tilenr += 4;
	}
	mm->nbfr.base_addr =
		ODSY_MM_32BPP_ADDR_FROM_TILENR(ptbfr_32bpp_origin_tilenr);


	/* check to ensure we haven't oversubscribed at this point... */
	if ((mm->nbfr.base_addr << 4) >= (odsyinfo->sdram_size << 20)) {
		cmn_err(CE_WARN,"odsy: frame buffer/fullscreen accum config causes oversubscription of sdram.\n");
		return -1;
	}

 
	
	/*
	** Return sdram base addresses back to the caller.
	*/
	cfbarg->fb_originp      = mm->fbfr.base_addr;
	cfbarg->widovly_originp = mm->wbfr.base_addr;


	/*
	** Return buffer sizes back to the caller.  We use the
	** ODSY_MM_32BPP_ADDR_FROM_TILENR macro for each item to get 
	** a buzz 32Bpp tile address delta.  Then we shift left by 4 to 
	** emulate a linear byte offset.
	*/
	cfbarg->cfifo_size = ODSY_MM_32BPP_ADDR_FROM_TILENR
		(fb_32bpp_origin_tilenr - 0) << 4;

	cfbarg->fbuf_size =  ODSY_MM_32BPP_ADDR_FROM_TILENR
		(ovlw_32bpp_origin_tilenr - fb_32bpp_origin_tilenr) << 4;

	cfbarg->wbuf_size = ODSY_MM_32BPP_ADDR_FROM_TILENR
		(czbfr_32bpp_origin_tilenr - ovlw_32bpp_origin_tilenr) << 4;  

	cfbarg->czbuf_size = ODSY_MM_32BPP_ADDR_FROM_TILENR
		(acbfr_32bpp_origin_tilenr - czbfr_32bpp_origin_tilenr) << 4;  

	cfbarg->acbuf_size = ODSY_MM_32BPP_ADDR_FROM_TILENR
		(ptbfr_32bpp_origin_tilenr - acbfr_32bpp_origin_tilenr) << 4;   


	cfbarg->gmem_size = (odsyinfo->sdram_size * 1024 * 1024);


	/* Load DBE DMA (scanout) registers with config info */
	hw = bdata->hw;

	ODSY_CLEAR_HW_TOUCH_ERROR(hw);

	ODSY_DFIFO_PSEMA(bdata);

	if (!ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(6))) {

		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			DMA_ovlyWidBaseAddr, mm->wbfr.base_addr));

		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			DMA_pixelBaseAddr, mm->fbfr.base_addr));

		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			DMA_widthPixels, cfbarg->width));
		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			DMA_heightPixels, cfbarg->height));

		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			DMA_stride, tdx));

		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			DMA_fb_depth, dma_fb_depth));
	} else {
		ODSY_DFIFO_VSEMA(bdata);
		return -1;
	}

	ODSY_DFIFO_VSEMA(bdata);

#ifdef ODSY_CHECK_HW_TOUCH_ERROR
	if (ODSY_CHECK_HW_TOUCH_ERROR(hw)) {
		cmn_err(CE_WARN,"odsy: experienced hw write errors during frame buffer config");
		return -1;
	}
#endif
	
	/* update info */
	bdata->info.gfx_info.xpmax = cfbarg->width;
	bdata->info.gfx_info.ypmax = cfbarg->height;

	return 0;

}

#if !defined(_STANDALONE)

int gfP_OdsyBRDMGRconfigFb(struct gfx_gfx   *gfxp,
			   struct rrm_rnode *rnp,
			   void             *u_cfbarg,
			   int              *rvalp)
{
	odsy_data *bdata;
	odsy_brdmgr_config_fb_arg k_cfbarg;
	int rc = 0;

	GFXLOGT(ODSY_gfP_OdsyBRDMGRconfigFb);

	ASSERT(gfxp);

	bdata = ODSY_BDATA_FROM_GFXP(gfxp);
	ASSERT(bdata);
	
	ODSY_COPYIN_SIMPLE_ARG(u_cfbarg, &k_cfbarg, odsy_brdmgr_config_fb_arg,
			       "ODSY_gfP_OdsyBRDMGRconfigFb: bad ptr to config arg\n");

	if (k_cfbarg.width > 4095 || k_cfbarg.height > 4095 ||
	   (k_cfbarg.depth != 8 && k_cfbarg.depth != 16 && k_cfbarg.depth != 32)) {
		cmn_err(CE_WARN,"odsy: invalid frame buffer config attempted, w = %d, h = %d, d = %d\n",
			k_cfbarg.width, k_cfbarg.height, k_cfbarg.depth);
		*rvalp = EINVAL;
		return -1;
	}

	rc = odsyConfigFb(bdata, &k_cfbarg);

	/*
	** Send the results (sdram base addresses) back to the board manager.
	** And save the frame buffer origin to be used by the bind drawable code.
	*/

	ODSY_COPYOUT_SIMPLE_ARG(u_cfbarg, &k_cfbarg, odsy_brdmgr_config_fb_arg,
				"odsy: bad frame buffer config arg user-land pointer\n");

#ifdef ODSY_DEBUG_SDRAM
	{
	odsy_mm *mm= &bdata->mm;	

	GFXLOGV(k_cfbarg.width);
	GFXLOGV(k_cfbarg.height);
	GFXLOGV(k_cfbarg.depth);
	GFXLOGV(k_cfbarg.acbuf_type);
	GFXLOGV(mm->fbfr.base_addr);
	GFXLOGV(mm->wbfr.base_addr);
	GFXLOGV(mm->nbfr.base_addr);

	cmn_err(CE_DEBUG,"odsy: fbconfig (dx,dy,depth:%d,%d,%d  accum:%d)\n",
		k_cfbarg.width, k_cfbarg.height, k_cfbarg.depth,
		k_cfbarg.acbuf_type);
	cmn_err(CE_DEBUG,"odsy: (f,c,w,a,n bases:0x%x,0x%x,0x%x,0x%x,0x%x)\n",
		mm->fbfr.base_addr, 
		mm->czbfr.base_addr,
		mm->wbfr.base_addr, 
		mm->abfr[0].base_addr,
		mm->nbfr.base_addr);
        }
#endif

	*rvalp = rc;
	return rc;
}

int gf_OdsyPositionCursor(struct gfx_data *_bdata, int x, int y)
{
	odsy_data *bdata = (odsy_data *)_bdata;
	odsy_hw *hw = bdata->hw;
	struct cursor_xy_reg xy;
	int rv;
	__uint64_t mouse_pos_cmd;

	GFXLOGT(ODSY_gf_OdsyPositionCursor);
	bdata->cursor_x = x;
	bdata->cursor_y = y;
	xy.x = x - bdata->cursor_xhot;
	xy.y = y - bdata->cursor_yhot;

	mouse_pos_cmd = DBE_CMD1_REG(DBE_UPDATE_IMMED,
				     cursor_xy,
				     *(__uint32_t *)&xy);

	/* Because of STREAMS interaction with blocking threads, the
	 * mouse interrupt cannot wait on a sleep lock or semaphore
	 * that the Xserver might need.  The Xserver needs the
	 * interrupt to complete after it acquires a sleep lock or
	 * semaphore, but if the interrupt needs this lock or
	 * semaphore to complete then we get deadlock.  So the mouse
	 * interrupt can only try to acquire the semaphore.  If it
	 * succeeds, we go on like normal.  If it fails, then it just
	 * stores the mouse position so that the routine with the
	 * semaphore can reposition the mouse.  So when the routine
	 * holding the semaphore does a vsema, it may need to send the
	 * mouse position itself.  */

	rv = mutex_spinlock(&bdata->mouse_pos_lock);

	if (cpsema(&bdata->dfifo_sema)) {

		/* wipe the old mouse position command */
		bdata->mouse_pos_cmd = 0L;

		mutex_spinunlock(&bdata->mouse_pos_lock, rv);

		/* send the mouse position */
		if (!ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1))) {
			ODSY_WRITE_HW(hw, dbe, mouse_pos_cmd);
		} else
			bdata->mouse_pos_cmd = mouse_pos_cmd;

		vsema(&bdata->dfifo_sema);

	} else {

		/* store the mouse position for the thread with the semaphore */
		bdata->mouse_pos_cmd = mouse_pos_cmd;

		mutex_spinunlock(&bdata->mouse_pos_lock, rv);
	}
	return 0;
}

int gfP_OdsyBRDMGRcursor(struct gfx_gfx   *gfxp,
			 struct rrm_rnode *rnp,
			 void             *u_cursorarg,
			 int              *rvalp)
{
	odsy_data                 *bdata;
	odsy_brdmgr_cursor_arg     k_cursorarg;
	odsy_hw			  *hw;
	__uint32_t buf;
	int cnt = 0;
	int ii;

	GFXLOGT(ODSY_gfP_OdsyBRDMGRcursor);

	ASSERT(gfxp);

	bdata = ODSY_BDATA_FROM_GFXP(gfxp);
	ASSERT(bdata);
	hw = bdata->hw;
	
	ODSY_COPYIN_SIMPLE_ARG(u_cursorarg, &k_cursorarg, odsy_brdmgr_cursor_arg,
			       "ODSY_gfP_OdsyBRDMGRcursor: bad ptr to cursor arg\n");

	if (k_cursorarg.flag & ODSY_CURSOR_SET_HOT) {
		struct cursor_xy_reg *p = (struct cursor_xy_reg *)&k_cursorarg.hotXY;

		bdata->cursor_xhot = p->x;
		bdata->cursor_yhot = p->y;
	}
	if (k_cursorarg.flag & ODSY_CURSOR_SET_POS) {
		struct cursor_xy_reg *p = (struct cursor_xy_reg *)&k_cursorarg.cursorXY;

		gf_OdsyPositionCursor(gfxp->gx_bdata, p->x, p->y);
	}

	/* grab the backend lock -- no subroutines can grab it after this */
	ODSY_DFIFO_PSEMA(bdata);

	if (k_cursorarg.flag & ODSY_CURSOR_SET_CONTROL) {

		if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1)))
			goto error_return;

		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
						    cursor_control,
						    k_cursorarg.control));
	}
	if (k_cursorarg.flag & ODSY_CURSOR_SET_CMAP) {
		__uint32_t tmp[ODSY_CURSOR_LUT_SIZE];

		if (copyin((void *)k_cursorarg.cmap, tmp, sizeof(tmp))) {
			ODSY_DFIFO_VSEMA(bdata);
			return EFAULT;
		}

		if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(9)))
			goto error_return;

		OUTPUT( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
					DBE_BUZZ_ALL,
					DBE_ADDR(cursor_LUT),
					ODSY_CURSOR_LUT_SIZE) );
		for (ii = 0; ii < ODSY_CURSOR_LUT_SIZE; ii++) {
			OUTPUT(tmp[ii]);
		}
	}
	if (k_cursorarg.flag & ODSY_CURSOR_SET_GLYPH) {
		__uint32_t tmp[ODSY_CURSOR_GLYPH_SIZE];

		if (copyin((void *)k_cursorarg.glyph, tmp, sizeof(tmp))) {
			ODSY_DFIFO_VSEMA(bdata);
			return EFAULT;
		}

		if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1)))
			goto error_return;

		OUTPUT( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
					DBE_BUZZ_ALL,
					DBE_ADDR(cursor_glyph),
					ODSY_CURSOR_GLYPH_SIZE) );

		for (ii = 0; ii < ODSY_CURSOR_GLYPH_SIZE; ii++) {
			if ((ii % ODSY_DFIFO_WORDS) == 0) {
				if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
					goto error_return;
			}
			OUTPUT(tmp[ii]);
		}
	}
	if (k_cursorarg.flag & ODSY_CURSOR_SET_ALPHA) {
		__uint32_t tmp[ODSY_CURSOR_ALPHA_SIZE];

		if (copyin((void *)k_cursorarg.alpha, tmp, sizeof(tmp))) {
			ODSY_DFIFO_VSEMA(bdata);
			return EFAULT;
		}

		if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1)))
			goto error_return;

		OUTPUT( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
					DBE_BUZZ_ALL,
					DBE_ADDR(cursor_alpha),
					ODSY_CURSOR_ALPHA_SIZE)  );

		for (ii = 0; ii < ODSY_CURSOR_ALPHA_SIZE; ii++) {
			if ((ii % ODSY_DFIFO_WORDS) == 0) {
				if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
					goto error_return;
			}
			OUTPUT(tmp[ii]);
		}

	}
	if (cnt & 0x1) {
		/* a no-op */
		if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(1)))
			goto error_return;
		OUTPUT( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
					DBE_BUZZ_ALL,
					DBE_ADDR(cursor_alpha),
					0) );
	}
	ODSY_DFIFO_VSEMA(bdata);

	*rvalp = 0;
	return 0;
error_return:
	ODSY_DFIFO_VSEMA(bdata);
	return(*rvalp = EIO);
}

int gfP_OdsyBRDMGRsetDpyMode(	struct gfx_gfx *gfxp,
				struct rrm_rnode *rnp,
				void *arg,
				int *rvalp)
{
	struct odsy_brdmgr_setdisplaymode_args setdisp_args;
	odsy_data *bdata = ODSY_BDATA_FROM_GFXP(gfxp);
	int rtn;

	ODSY_COPYIN_SIMPLE_ARG(arg, &setdisp_args,
			       odsy_brdmgr_setdisplaymode_args,
			       "gfP_OdsyBRDMGRsetDpyMode: bad ptr to arg\n");

	rtn = gf_OdsySetDisplayMode(gfxp, setdisp_args.wid, setdisp_args.displaymode);

	*rvalp = rtn;
	return 0;
}

int gfP_OdsyBRDMGRsetBlanking(	struct gfx_gfx *gfxp,
				struct rrm_rnode *rnp,
				void *arg,
				int *rvalp)
{
	struct odsy_brdmgr_set_blanking_arg setblank_arg;
	odsy_data *bdata = ODSY_BDATA_FROM_GFXP(gfxp);
	int rc = 0;

	ODSY_COPYIN_SIMPLE_ARG(arg, &setblank_arg,
			       odsy_brdmgr_set_blanking_arg,
			       "gfP_OdsyBRDMGRsetBlanking: bad ptr to arg\n");

	rc = odsyBlankScreen(bdata, setblank_arg.on);

	*rvalp = rc;
	return rc;
}
#endif /* !_STANDALONE */

int odsyBlankScreen(odsy_data *bdata, int blank)
{
	odsy_hw *hw = bdata->hw;

	ODSY_CLEAR_HW_TOUCH_ERROR(hw);

	ODSY_DFIFO_PSEMA(bdata);

	bdata->vtg_initialState &= ~(1 << 5);
	if (blank)
		bdata->vtg_enable &= ~HIF_DAC_CBLANK_N;/* assert dac_cblank_n */
	else
		bdata->vtg_enable |= HIF_DAC_CBLANK_N;

	if (!ODSY_POLL_DFIFO_LEVEL(bdata->hw, ODSY_DFIFO_LEVEL_REQ(2))) {
		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, VTG_initialState,
			bdata->vtg_initialState));
		ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
			VTG_enable, bdata->vtg_enable));
	} else {
		ODSY_DFIFO_VSEMA(bdata);
		return -1;
	}
	ODSY_DFIFO_VSEMA(bdata);

	/*
	 * XXX - To cause the screen to loose sync:
	 *
	 *	vtg_initialState &= ~(1 << 4);
	 *	vtg_enable       &= ~(1 << 4);
	 */

#ifdef ODSY_CHECK_HW_TOUCH_ERROR
	if (ODSY_CHECK_HW_TOUCH_ERROR(hw)) {
		cmn_err(CE_WARN,"odsy: hw write errors during blank screen");
		return -1;
	}
#endif
	return 0;

}

#if !defined(_STANDALONE)
static int odsy_copyin_frame_line_tables(odsy_brdmgr_timing_info *tinfo)
{
    unsigned short *tmp;

    if ((tmp = (unsigned short *) kern_calloc(tinfo->ftab_len,
			   sizeof(unsigned short))) == NULL) {
	return ENOMEM;
    }
    if (copyin((void *) tinfo->VTG_FrameTablePtr, tmp,
	tinfo->ftab_len * (int) sizeof(unsigned short))) {
	    kern_free(tmp);
	    return EFAULT;
    }
    tinfo->VTG_FrameTablePtr = (__uint64_t) tmp;
    if ((tmp = kern_calloc(tinfo->ltab_len, sizeof(unsigned short))) == NULL) {
	kern_free((void *) tinfo->VTG_FrameTablePtr);
	return ENOMEM;
    }
    if (copyin((void *) tinfo->VTG_LineTablePtr, tmp,
	tinfo->ltab_len * (int) sizeof(unsigned short))) {
	    kern_free((void *) tinfo->VTG_FrameTablePtr);
	    kern_free(tmp);
	    return EFAULT;
    }
    tinfo->VTG_LineTablePtr = (__uint64_t) tmp;

    return 0;
}
#endif  /* _STANDALONE */

int
odsyLoadTimingTable(odsy_data *bdata, odsy_brdmgr_timing_info *tinfo)
{
	short *ftab, *ltab;
	odsy_hw* hw = bdata->hw;
	__odsyreg_abort abort;
	buzz_HIF_config_reg buzz_hif_config;
	pbj_HIF_control_reg pbj_hif_control;
	int len, scnt, len32;
        unsigned int dcb_clock_speed = 0;
	cursor_control_reg cursor_ctrl;
	port_handle *port = &bdata->i2c_port[ODSY_I2C_OPT_PORT];
	__uint32_t buf1 = 0, buf2 = 0, mode = 0;
	vtg_control_reg vtg_ctl;
	odsy_ddinfo *odsyinfo;

	__uint32_t pll_addr0 = 0;
	__uint32_t pll_addr1 = 0;
	__uint32_t pll_addr2 = 0;
	__uint32_t pll_addr3 = 0;
	__uint32_t pll_addr4 = 0;
	__uint32_t pll_addr5 = 0;
	__uint32_t pll_addr6 = 0;
	__uint32_t pll_addr7 = 0;
	__uint32_t pll_addr0_val = 0;
	__uint32_t pll_addr1_val = 0;
	__uint32_t pll_addr2_val = 0;
	__uint32_t pll_addr3_val = 0;
	__uint32_t pll_addr4_val = 0;
	__uint32_t pll_addr5_val = 0;
	__uint32_t pll_addr6_val = 0;
	__uint32_t pll_addr7_val = 0;

	ODSY_CLEAR_HW_TOUCH_ERROR(bdata->hw);
	
 	odsyinfo = &(bdata->info.dd_info);

#if !defined(ODSY_SIM_KERN)
	/* Begin writting to the pbj HIF_Control reg */
	ODSY_DFIFO_PSEMA(bdata);
	if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, 0))
		goto error_return;
	ODSY_READ_HW(bdata->hw, dbe_diag_rd.PBJ_HIF_control, *((__uint32_t*)&pbj_hif_control));

	/* Buzz-DCB clock speed */
	dcb_clock_speed = tinfo->dcbclock_speed & 0x7;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, buzz_HIF_config, dcb_clock_speed));

	/* I2C clock select */
	pbj_hif_control.bf.i2c_main_clk_sel = tinfo->i2c_main_clock_freq;
	pbj_hif_control.bf.i2c_opt_clk_sel = tinfo->i2c_opt_clock_freq;

	pbj_hif_control.bf.pbj_soft_reset_n = 0;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, PBJ_HIF_control, pbj_hif_control.w32));

#ifdef DEBUG_TIMING
        printf("Tinfo: 0x%x\n",tinfo);
        printf("VTG_FrameTablePtr: 0x%x\n", tinfo->VTG_FrameTablePtr);
        printf("ftab_len: 0x%x\n", tinfo->ftab_len);
        printf("VTG_LineTablePtr: 0x%x\n", tinfo->VTG_LineTablePtr);
        printf("ltab_len: 0x%x\n", tinfo->ltab_len);
        printf("VTG_Control: 0x%x\n", tinfo->VTG_Control);
        printf("VTG_Init: 0x%x\n", tinfo->VTG_Init);
        printf("VTG_Chan_En: 0x%x\n", tinfo->VTG_Chan_En);
        printf("PLL_DPAControl: 0x%x\n", tinfo->DPAControl);
        printf("PLL_DPAOffset: 0x%x\n", tinfo->DPAOffset);
        printf("PLL_FdBkDiv0: 0x%x\n", tinfo->FdBkDiv0);
        printf("PLL_FdBkDiv1: 0x%x\n", tinfo->FdBkDiv1);
        printf("PLL_InputControl: 0x%x\n", tinfo->InputControl);
        printf("PLL_LoopControl: 0x%x\n", tinfo->LoopControl);
        printf("PLL_Osc_Div: 0x%x\n", tinfo->Osc_Div);
        printf("PLL_OutputEnables: 0x%x\n", tinfo->OutputEnables);
	printf("GEN_lPFD: 0x%x\n", tinfo->GEN_lPFD);
    	printf("GEN_hPFD: 0x%x\n", tinfo->GEN_hPFD);
    	printf("GEN_lPFD_HI: 0x%x\n", tinfo->GEN_lPFD_HI);
    	printf("GEN_hPFD_HI: 0x%x\n", tinfo->GEN_hPFD_HI);
    	printf("GEN_lPFD_LO: 0x%x\n", tinfo->GEN_lPFD_LO);
    	printf("GEN_hPFD_LO: 0x%x\n", tinfo->GEN_hPFD_LO);
    	printf("GEN_lPSD: 0x%x\n", tinfo->GEN_lPSD);
    	printf("GEN_hPSD: 0x%x\n", tinfo->GEN_hPSD);
    	printf("GEN_lHMASK: 0x%x\n", tinfo->GEN_lHMASK);
    	printf("GEN_hHMASK: 0x%x\n", tinfo->GEN_hHMASK);
    	printf("GEN_BP_CLAMP: 0x%x\n", tinfo->GEN_BP_CLAMP);
    	printf("GEN_GENLOCK: 0x%x\n", tinfo->GEN_GENLOCK);
    	printf("GEN_CONTROL: 0x%x\n", tinfo->GEN_CONTROL);
        printf("Dac_Control: 0x%x\n", tinfo->Dac_Control);
        printf("DP_Control: 0x%x\n", tinfo->DP_Control);
        printf("SM_Control: 0x%x\n", tinfo->SM_Control);
        printf("num_fields: 0x%x\n", tinfo->num_fields);
        printf("w: 0x%x, h: 0x%x\n", tinfo->w, tinfo->h);
        printf("refresh_rate: 0x%x\n", tinfo->refresh_rate);
        printf("cfreq: 0x%x\n", tinfo->cfreq);
        printf("flags: 0x%x\n", tinfo->flags);
        printf("dcbclock_freq: 0x%x\n", tinfo->dcbclock_freq);
        printf("dcbclock_speed: 0x%x\n", tinfo->dcbclock_speed);
        printf("i2c_opt_clock_freq: 0x%x\n", tinfo->i2c_opt_clock_freq);
        printf("i2c_main_clock_freq: 0x%x\n", tinfo->i2c_main_clock_freq);
	printf("GEN_PLL_InputControl: 0x%x\n",tinfo->GEN_PLL_InputControl);
	printf("GEN_PLL_Osc_Div: 0x%x\n",tinfo->GEN_PLL_Osc_Div);
#endif /* DEBUG_TIMING */

	if(odsyinfo->board_rev < 1)
 	{
		pll_addr0 = ODSY_PLL_DPAControl_ADDR;
		pll_addr1 = ODSY_PLL_DPAOffset_ADDR;
		pll_addr2 = ODSY_PLL_FdBkDiv0_ADDR;
		pll_addr3 = ODSY_PLL_FdBkDiv1_ADDR;
		pll_addr4 = ODSY_PLL_InputControl_ADDR;
		pll_addr5 = ODSY_PLL_LoopControl_ADDR;
		pll_addr6 = ODSY_PLL_OscDiv_ADDR;
		pll_addr7 = ODSY_PLL_OutputEnables_ADDR;
		pll_addr0_val = tinfo->DPAControl;
		pll_addr1_val = tinfo->DPAOffset;
		pll_addr2_val = tinfo->FdBkDiv0;
		pll_addr3_val = tinfo->FdBkDiv1;
		pll_addr4_val = tinfo->InputControl;
		pll_addr5_val = tinfo->LoopControl;
		pll_addr6_val = tinfo->Osc_Div;
		pll_addr7_val = tinfo->OutputEnables;
        }
	else
	{
		pll_addr0 = ODY_AMI_PLL_0x0_ADDR;
		pll_addr1 = ODY_AMI_PLL_0x1_ADDR;
		pll_addr2 = ODY_AMI_PLL_0x2_ADDR;
		pll_addr3 = ODY_AMI_PLL_0x3_ADDR;
		pll_addr4 = ODY_AMI_PLL_0x4_ADDR;
		pll_addr5 = ODY_AMI_PLL_0x5_ADDR;
		pll_addr6 = ODY_AMI_PLL_0x6_ADDR;
		pll_addr7 = ODY_AMI_PLL_0x7_ADDR;
		pll_addr0_val = tinfo->AMI_0x0;
		pll_addr1_val = tinfo->AMI_0x1;
		pll_addr2_val = tinfo->AMI_0x2;
		pll_addr3_val = tinfo->AMI_0x3;
		pll_addr4_val = tinfo->AMI_0x4;
		pll_addr5_val = tinfo->AMI_0x5;
		pll_addr6_val = tinfo->AMI_0x6;
		pll_addr7_val = tinfo->AMI_0x7;

		/*
		 * Keep a shadowed copy of the AMI PLL val's
		 * for genlock interrupt handling. Free's us
		 * from having to read back the reg over I2C.
		 */

		bdata->sysreg_shadow.ami_pll_0x1 = tinfo->AMI_0x1;
		bdata->sysreg_shadow.ami_pll_0x2 = tinfo->AMI_0x2;
		bdata->sysreg_shadow.ami_pll_0x6 = tinfo->AMI_0x6;

		bdata->sysreg_shadow.gen_ami_pll_0x1 = tinfo->GEN_AMI_REG1;
		bdata->sysreg_shadow.gen_ami_pll_0x2 = tinfo->GEN_AMI_REG2;
		bdata->sysreg_shadow.gen_ami_pll_0x6 = tinfo->GEN_AMI_REG6;

#ifdef ODSY_AMI_DEBUG
		printf("bdata->sysreg_shadow.ami_pll_0x1 %d\n", bdata->sysreg_shadow.ami_pll_0x1);
		printf("bdata->sysreg_shadow.ami_pll_0x2 %d\n", bdata->sysreg_shadow.ami_pll_0x2);
		printf("bdata->sysreg_shadow.ami_pll_0x6 %d\n", bdata->sysreg_shadow.ami_pll_0x6);

		printf("bdata->sysreg_shadow.gen_ami_pll_0x1 %d\n", bdata->sysreg_shadow.gen_ami_pll_0x1);
		printf("bdata->sysreg_shadow.gen_ami_pll_0x2 %d\n", bdata->sysreg_shadow.gen_ami_pll_0x2);
		printf("bdata->sysreg_shadow.gen_ami_pll_0x6 %d\n", bdata->sysreg_shadow.gen_ami_pll_0x6);
#endif
		
	}

	/*
	 * XXX disable genlock before writting PLL's.
	 * If not we may see race conditions with this
	 * interrupt trying to reprogram the PLL's
	 * at the same time.
	 */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, gen_control, 0x0));
	ODSY_DFIFO_VSEMA(bdata);

	/* ISC PLL */

	if (odsy_i2cPLLRegWrite(port, pll_addr0, (unsigned char)(pll_addr0_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's DPA Control\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port, pll_addr1, (unsigned char)(pll_addr1_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's DPA Offset\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port, pll_addr2, (unsigned char)(pll_addr2_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's FdBkDiv0\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port, pll_addr3, (unsigned char)(pll_addr3_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's FdBkDiv1\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port, pll_addr4, (unsigned char)(pll_addr4_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's Input Control\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port, pll_addr5, (unsigned char)(pll_addr5_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's Loop Control\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port,pll_addr6, (unsigned char)(pll_addr6_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's OscDiv\n");
		goto bail;
	}

	if (odsy_i2cPLLRegWrite(port, pll_addr7, (unsigned char)(pll_addr7_val & 0xFF)) != 0)
	{
		cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's Output Enable\n");
		goto bail;
	}

	if(odsyinfo->board_rev < 1)
	{
		if (odsy_i2cPLLRegWrite(port, ODSY_PLL_SoftReset_ADDR, 0x5A) != 0)
		{
			cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's Output Enable\n");
			goto bail;
		}
	}
bail:
#endif

	/* Need a nominal delay of 10uSec, but wait 1msec, PLL lock period */
	DELAY(1000);
	/* mimicking MGRAS code, allows MITSUBISHI monitor to recognize new timing */
	DELAY(100000);

	ODSY_DFIFO_PSEMA(bdata);
	if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, ODSY_DFIFO_LEVEL_REQ(3)))
		goto error_return;

	/* Deassert PBJ reset */
	pbj_hif_control.bf.pbj_soft_reset_n = 1;
        ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, PBJ_HIF_control, pbj_hif_control.w32));

	/* Stop the VTG prior to loading frametable & linetable
	 * remember to clear bit 17 in VTG_enable and set bit 17 in
	 * VTG_initialState so subsequent colormap loading is not stalled.
	 */
	vtg_ctl.w32 = 0;
	vtg_ctl.bf.reset = 1;
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
		VTG_control, vtg_ctl.w32));
	
	/* Load the Frame Table for the VTG */
        scnt = 0;
        len = tinfo->ftab_len;
        len32 = (len+1)/2;

#if defined(ODSY_SIM_KERN)
        /*
         * For both the frame table and the line table,
         * load one extra 32-bit word of zeros at the end so
         * the verilog simulator does not encounter X's
         * while pre-fetching the frame table and line
         * table entries.
         * This is a simulation artifact and is not needed
         * on real hardware.
         */
        len32 += 1;
#endif
        OUTPUT32( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
                                DBE_BUZZ_ALL,
                                DBE_ADDR(VTG_FrameTable),
                                len32), scnt);
	ftab = (short *) tinfo->VTG_FrameTablePtr;
#ifdef DEBUG_TIMING
        printf("LoadTimingTable, cfreq = %d, ftab_len = %d, ltab_len = %d\n",
                tinfo->cfreq, tinfo->ftab_len, tinfo->ltab_len);
#endif
        while (len > 0) {
                if ((scnt % ODSY_DFIFO_SHORTS) == 0)
                    if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, 0))
			goto error_return;
                OUTPUT16(*ftab, scnt);
                ftab++;
                len--;
        }
#ifdef DEBUG_TIMING
        printf("  load timing 2, scnt = %d\n", scnt);
#endif
        if (scnt & 0x1) {
                if ((scnt % ODSY_DFIFO_SHORTS) == 0)
			if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, 0))
				goto error_return;
                OUTPUT16(0, scnt);
	}
#if defined(ODSY_SIM_KERN)
        OUTPUT16(0, scnt);
        OUTPUT16(0, scnt);
#endif

	len = tinfo->ltab_len;
	len32 = (len+1)/2;
#if defined(ODSY_SIM_KERN)
	len32 += 1;
#endif
	if ((scnt % ODSY_DFIFO_SHORTS) == 0)
		if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, 0))
			goto error_return;
	OUTPUT32( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
				DBE_BUZZ_ALL,
				DBE_ADDR(VTG_LineTable),
				len32), scnt);
	ltab = (short *) tinfo->VTG_LineTablePtr;
	while (len > 0) {
		if ((scnt % ODSY_DFIFO_SHORTS) == 0)
		    if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
			goto error_return;
		OUTPUT16(*ltab, scnt);
		ltab++;
		len--;
	}
#ifdef DEBUG_TIMING
	printf("  load timing 3, scnt = %d\n", scnt);
#endif
	if (scnt & 0x1) {
		if ((scnt % ODSY_DFIFO_SHORTS) == 0)
			if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
				goto error_return;
		OUTPUT16(0, scnt);
	}
#if defined(ODSY_SIM_KERN)
	OUTPUT16(0, scnt);
	OUTPUT16(0, scnt);
#endif

        if ((scnt & 0x3) == 2) {
            /*
             * pad out the last double-word with a no-op
             * (if we cannot assert that the rest of the
             *  routine will write an even number words,
             *  then it would be better to move this padding
             *  code to the end of the routine).
             */
	    if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, ODSY_DFIFO_LEVEL_REQ(1)))
		goto error_return;
            OUTPUT32( DBE_PACKET_HDR(DBE_UPDATE_IMMED,
                                  DBE_BUZZ_ALL,
                                  DBE_ADDR(VTG_LineTable),
                                  0), scnt);
        }

	if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, ODSY_DFIFO_LEVEL_REQ(13)))
		goto error_return;

	/* VTG setup */
	bdata->vtg_ctl = tinfo->VTG_Control;
	bdata->vtg_ctl |= 0x1b8;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, VTG_control, bdata->vtg_ctl));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, VTG_initialState, tinfo->VTG_Init));
	/* Keep maintain blanking enable bit */
	bdata->vtg_enable = (bdata->vtg_enable & HIF_DAC_CBLANK_N) |
		(tinfo->VTG_Chan_En & ~HIF_DAC_CBLANK_N);
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, VTG_enable, bdata->vtg_enable));

	/* Buzz-DBE DMA setup */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, DMA_widthPixels, tinfo->w));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, DMA_heightPixels, tinfo->h));
	mode = tinfo->flags & ODSY_INTERLACED_MODE;
	mode |= tinfo->flags & ODSY_INTERLACED_LINE;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, DMA_interlaceMode, mode));
	mode = tinfo->flags & ODSY_DUAL_HEAD_MODE;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, DMA_dualHeadMode, mode));
	mode = tinfo->flags & ODSY_FIELD_SEQUENTIAL;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, DMA_fldSeqMode, mode));

	/*XXX Stride ??? */

	/* Buzz-DBE XMAP */
	mode = tinfo->flags & ODSY_FIELD_SEQUENTIAL;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, XMAP_config, mode));

	/* PBJ Cursor */
	cursor_ctrl.delay_enable = 0; 
	cursor_ctrl.enable = 0;
	cursor_ctrl.xhair_mode = 0;
	cursor_ctrl.fq_enable = tinfo->flags & ODSY_FIELD_SEQUENTIAL;
	cursor_ctrl.dual_head_enable = tinfo->flags & ODSY_DUAL_HEAD_MODE;
	cursor_ctrl.dual_head_right = 0; /*XXX*/
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, cursor_control, *(uint32_t*)&cursor_ctrl));

	if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, ODSY_DFIFO_LEVEL_REQ(17)))
		goto error_return;

	/* PBJ Internal DAC */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, dac_control, tinfo->Dac_Control));

	/* PBJ Data Path */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, PBJ_dpath_ctl, tinfo->DP_Control));

	/* PBJ DP State Machine */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, sm_config, tinfo->SM_Control));

	/* PBJ GenLock */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, lPFD, tinfo->GEN_lPFD));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, hPFD, tinfo->GEN_hPFD));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, lPFD_HI, tinfo->GEN_lPFD_HI));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, hPFD_HI, tinfo->GEN_hPFD_HI));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, lPFD_LO, tinfo->GEN_lPFD_LO));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, hPFD_LO, tinfo->GEN_hPFD_LO));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, lPSD, tinfo->GEN_lPSD));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, hPSD, tinfo->GEN_hPSD));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, lHMASK, tinfo->GEN_lHMASK));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, hHMASK, tinfo->GEN_hHMASK));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, BP_CLAMP, tinfo->GEN_BP_CLAMP));
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, GENLOCK, tinfo->GEN_GENLOCK));

	if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, ODSY_DFIFO_LEVEL_REQ(12)))
		goto error_return;

	ODSY_DFIFO_VSEMA(bdata);

	if(tinfo->genlocksource == ODSY_GENLOCK_SOURCE_EXTERNAL && (odsyinfo->board_rev < 1)) /*XXX using board rev 0 only */
	{
		if (odsy_i2cPLLRegWrite(port, ODSY_PLL_InputControl_ADDR, (unsigned char)(tinfo->GEN_PLL_InputControl & 0xFF)) != 0)
		{
			cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's Input Control\n");
		}
		if (odsy_i2cPLLRegWrite(port, ODSY_PLL_OscDiv_ADDR, (unsigned char)(tinfo->Osc_Div & 0x7F)) != 0)
		{
			cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the PLL's Osc Divide\n");
		}
	}
	else if(tinfo->genlocksource == ODSY_GENLOCK_SOURCE_EXTERNAL && (odsyinfo->board_rev >= 1))
	{
		if(odsy_i2cPLLRegWrite(port, ODY_AMI_PLL_0x1_ADDR, (unsigned char)(tinfo->GEN_AMI_REG1 & 0xFF)) != 0)
		{
			cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the AMI PLL's Reg 0x1\n");
		}
		if(odsy_i2cPLLRegWrite(port, ODY_AMI_PLL_0x2_ADDR, (unsigned char)(tinfo->GEN_AMI_REG2 & 0xFF)) != 0)
		{
			cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the AMI PLL's Reg 0x2\n");
		}
		if(odsy_i2cPLLRegWrite(port, ODY_AMI_PLL_0x6_ADDR, (unsigned char)(tinfo->GEN_AMI_REG6 & 0xFF)) != 0)
		{
			cmn_err(CE_ALERT,"odsyLoadTimingTable(): Failed to successfully write to the AMI PLL's Reg 0x3\n");
		}
		
	}

	/*
	 * XXX enable genlock after writting PLL's.
	 */
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, gen_control, tinfo->GEN_CONTROL));

	/* Start VTG */
	/* ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, VTG_control, tinfo->VTG_Control));*/

#ifdef ODSY_CHECK_HW_TOUCH_ERROR
        if (ODSY_CHECK_HW_TOUCH_ERROR(hw)) {
                cmn_err(CE_WARN,"odsy: hw write error during timing table download");
                return -1;
        }
#endif

	return 0;
error_return:
        ODSY_DFIFO_VSEMA(bdata);
	return -1;
}

#if !defined(_STANDALONE)

int 
gfP_OdsyBRDMGRloadTimingTable(	struct gfx_gfx *gfxp,
				struct rrm_rnode *rnp,
				void *arg,
				int *rvalp)
{
	int rv;
	struct odsy_brdmgr_timing_info_arg timing_info_arg;
	struct odsy_brdmgr_timing_info_arg *tinfo = &timing_info_arg;
	odsy_data *bdata = ODSY_BDATA_FROM_GFXP(gfxp);

	ODSY_COPYIN_SIMPLE_ARG(arg, tinfo,
			       odsy_brdmgr_timing_info_arg,
			       "gfP_OdsyBRDMGRloadTimingTable: bad ptr to arg\n");

	if ((rv = odsy_copyin_frame_line_tables(&timing_info_arg)) != 0) {
		*rvalp = rv;
		return rv;
	}

#ifndef ODSY_NO_FC
	/* own the pipe */
	if (bdata->flow_hdl == 0) {
		cmn_err(CE_WARN, "gfxinit has not set up flow control successfully");
		kern_free((void *) tinfo->VTG_FrameTablePtr);
		kern_free((void *) tinfo->VTG_LineTablePtr);

		*rvalp = EFAULT;
		return EFAULT;
	}
#endif
	if (RRM_CheckValidFault(gfxp) < 0) {
		kern_free((void *) tinfo->VTG_FrameTablePtr);
		kern_free((void *) tinfo->VTG_LineTablePtr);

		*rvalp = EFAULT;
		return EFAULT;
	}

	/* make sure we don't lose the pipe */
	INCR_GFXLOCK(gfxp);

	/* XXX use default timing table on real hardware until we get one that works */
#if 1
	*rvalp = odsyLoadTimingTable(bdata, tinfo);
#else
	*rvalp = odsyLoadTimingTable(bdata, &odsy_default_timing1);
#endif

	DECR_GFXLOCK(gfxp);

	kern_free((void *) tinfo->VTG_FrameTablePtr);
	kern_free((void *) tinfo->VTG_LineTablePtr);

	return *rvalp;
}


int
gfP_OdsyBRDMGRenableVTG(struct gfx_gfx *gfxp,
			struct rrm_rnode *rnp,
			void *arg,
			int *rvalp)
{
	odsy_data *bdata = ODSY_BDATA_FROM_GFXP(gfxp);
	odsy_hw *hw = bdata->hw;
	vtg_control_reg       vtg_ctl;

	ODSY_DFIFO_PSEMA(bdata);

	if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(2))) {
		ODSY_DFIFO_VSEMA(bdata);
		return(*rvalp = EIO);
	}

	vtg_ctl.w32 = bdata->vtg_ctl;
	vtg_ctl.bf.reset = 0;	/* start the VTG */
	bdata->vtg_ctl = vtg_ctl.w32;
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
		VTG_control, vtg_ctl.w32));

	bdata->vtg_enable |= HIF_V_UPDATE_ENABLE;
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
		VTG_enable, bdata->vtg_enable));

	ODSY_DFIFO_VSEMA(bdata);

	*rvalp = 0;
	return *rvalp;
}

int gfP_OdsyBRDMGRi2ccmd(struct gfx_gfx   *gfxp,
                         struct rrm_rnode *rnp,
                         void *arg,
                         int  *rvalp)
{
        struct odsy_brdmgr_i2c_args a, *ap = &a;
	odsy_data *bdata = ODSY_BDATA_FROM_GFXP(gfxp);
        int cmd, retval, error = 0;

        if (copyin(arg, (void *) ap, sizeof(a))) {
                error = EFAULT;
        }

        retval = odsy_i2c_cmd(bdata, ap, arg);

        if (retval < 0)
        {
                error = -retval;
        }
        else
        {
                *rvalp = retval;
        }

	return error;
}
#endif  /* _STANDALONE */
