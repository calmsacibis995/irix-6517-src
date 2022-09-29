#ifndef _CRMDEFS_H_
#define _CRMDEFS_H_

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#define crmNSSet(_hwpage, _reg, _val ) \
     (_hwpage)->_reg = (_val);

#define crmNSSet64(_hwpage, _reg, _val ) \
     WRITE_REG64(_val, (&((_hwpage)->_reg)), unsigned long long);

#define crmNSSetForeground(_hwpage, _val) \
       (_hwpage)->Shade.fgColor = (_val);

#define crmNSSetBackground(_hwpage, _val) \
       (_hwpage)->Shade.bgColor = _val;

#define crmNSSetMode(_hwpage, _mode) \
       (_hwpage)->dstBufMode = _val;

#define crmNSSetAndGo(_hwpage, _reg, _val ) \
      *((&((_hwpage)->_reg)) + 0x0200) = _val;

#define crmNSSet64AndGo(_hwpage, _reg, _val ) \
     WRITE_REG64(_val, ((&((_hwpage)->_reg)) + 0x0200), unsigned long long);

#define crmGet(_hwpage, _reg, _val ) \
     (_val) = (_hwpage)->_reg;

#define crmGet64(_hwpage, _reg, _val ) \
     (_val) = (unsigned long long) READ_REG64((&((_hwpage)->_reg)), unsigned long long);

#if 0
#define crmSet64(_hwpage, _shadow, _reg, _val ) \
     (*((unsigned long long *) (&((_hwpage)->_reg)))) = (_val);
#define crmNSSet64(_hwpage, _reg, _val ) \
     (*((unsigned long long *) (&((_hwpage)->_reg)))) = (_val);

#define crmSet64AndGo(_hwpage, _shadow, _reg, _val ) \
      (*((unsigned long long *) ((&((_hwpage)->_reg)) + 0x200))) = (_val);
#define crmNSSet64AndGo(_hwpage, _reg, _val ) \
      (*((unsigned long long *) ((&((_hwpage)->_reg)) + 0x200))) = (_val);
#endif


#define crmFlush(_hwpage) \
       (_hwpage)->PixPipeFlush = 0;

#define MTEFlush(_hwpage) \
       (_hwpage)->Mte.flush = 0;

#ifndef REAL_HARDWARE
#define _128KB_PAGE_MASK ((1 << 18)-1)
#endif

#define CRM_COPY_FORWARD_SETUP(_xs,_xs2, _xd, _xd2, _x1, _x2, _xoff, _vertex_shift, _byte_offset) \
{							\
    _xd2 = (( (_x2) - 1) << (_vertex_shift)) + (_byte_offset);	\
    _xs2 = ((( (_x2) - 1) + (_xoff)) << (_vertex_shift)) + (_byte_offset); \
    _xd = (_x1) << (_vertex_shift);			\
    _xs = ((_x1) + (_xoff)) << (_vertex_shift);		\
}  

#define CRM_DMA_SETUP(_mem_addr, height)	\
{							\
    overhead = dmastart & (~CRM_4KB_PAGE_MASK);		\
    validbytes = CRM_128KB_PAGE_SIZE - overhead;	\
    if ((totalbytes + overhead) > CRM_128KB_PAGE_SIZE){ \
        tmp_ylen = validbytes/widthSrc;			\
    } else {						\
        tmp_ylen = tileh;				\
    }							\
    count = widthSrc * tmp_ylen;			\
    dmaBuf.buf = dmastart;				\
    dmaBuf.ylen = tmp_ylen;				\
}

#define SET_COLORBYTEMASK(_planemask, _bytemask)	\
{							\
int _tmp;						\
    _bytemask = 0;					\
    _tmp = (_planemask & 0xff000000) >> 24;		\
    if ((_tmp == 0) || (_tmp == 0xff)){			\
	_bytemask = 0x40;				\
	_tmp = (_planemask & 0xff0000) >> 16;		\
	if ((_tmp == 0) || (_tmp == 0xff)){		\
	    _bytemask |= 0x20;				\
	    _tmp = (_planemask & 0xff00) >> 8;		\
	    if ((_tmp == 0) || (_tmp == 0xff)){		\
		_bytemask |= 0x10;			\
		_tmp = _planemask & 0xff;		\
		if ((_tmp == 0) || (_tmp == 0xff)){	\
		    _bytemask |= 0x8;			\
		} else {				\
		    _bytemask = 0;			\
		} 					\
	    } else {					\
		_bytemask = 0; 				\
	    }						\
	} else {					\
	    _bytemask = 0;				\
	}						\
    } else {						\
	 _bytemask = 0;					\
    }							\
}
#define SET_COPY_BYTEMASK(_planemask, _use_bytemask)   \
{                                                       \
int _tmp;                                               \
int _bytemask = 0;                                      \
    _tmp = _planemask & 0x000000ff;                     \
    if ( (_tmp == 0) || (_tmp == 0xff)){                \
        _bytemask |= _tmp &  0x11;                	\
        _tmp = _planemask & 0x0000ff00;                 \
        if ( (_tmp == 0) || (_tmp == 0x0ff00)){         \
            _bytemask |=  (_tmp >> 8) & 0x22;     	\
            _tmp = _planemask & 0x00ff0000;             \
            if ( (_tmp == 0) || (_tmp == 0x0ff0000)){   \
                _bytemask |= (_tmp >> 16) & 0x44; 	\
                _tmp = _planemask & 0xff000000;         \
                if ((_tmp==0) || (_tmp==0xff000000)){   \
                    _bytemask |= (_tmp >> 24) & 0x88;   \
		    _use_bytemask = TRUE;		\
		    _planemask = _bytemask | (_bytemask<<8);	\
		    _planemask |= (_planemask << 16);	\
                } else {                                \
		    _use_bytemask = FALSE;		\
		}					\
             } else {                                   \
		    _use_bytemask = FALSE;		\
	    }						\
        } else {                                        \
		    _use_bytemask = FALSE;		\
	}						\
    } else{                                             \
		    _use_bytemask = FALSE;		\
    }                                                   \
}
  
/*
 * pixel DMA threshold
 */
#define CRM_DMA_W_THRESHOLD    0
#define CRM_DMA_R_THRESHOLD    0
#define CRM_PIO_W_THRESHOLD    0
#define CRM_PIO_R_THRESHOLD    0

/*
 * some defines for memory operation
 */
#define CRM_4KB_PAGE_SIZE	0x1000
#define CRM_4KB_PAGE_SHIFT	12
#define CRM_4KB_PAGE_MASK	0xfffff000
#define CRM_128KB_PAGE_SIZE	0x20000
#define CRM_124KB_SIZE		0x1f000

/*
 * maximum screen size, this is the largest size Crime can handle
 */
#define CRM_MAX_SCREEN_WIDTH   2048
#define CRM_MAX_SCREEN_HEIGHT  2048

#define CRM_BASE_DMODE	( DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK)

extern struct _nfbGCOps crmSolidPrivOps;
extern struct _nfbGCOps crmStippledPrivOps;
extern struct _rrmScreenPriv crmRRMPriv;
extern struct _nfbGCOps crmTiled8_W4PrivOps;
extern struct _nfbGCOps crmTiledPrivOps;
extern struct _GCOps crmSolidOps;

/*
 *  Display ID assignments
 */

#define CRM_8BIT_PC_DID		0	/* 3 maps??? */
#define CRM_8BIT_TC_DID		3
#define CRM_12BIT_PC_DID	4
#define CRM_16BIT_TC_DID	5
#define CRM_24BIT_DC_DID	6
#define CRM_24BIT_TC_DID	7
#define CRM_24BITDB_TC_DID	8
#define CRM_8BIT_OLAY_DID	32

/*
 * the definition let us distinguish different color mode,
 * because we fake it out sometimes, for example: 8-bit True Color
 * pixel is drawing with 8-bit Color Index mode, the bit should be
 * don't care for hardware
 */
#define CRM_RGB8_FLAG		0x80000000
#define CRM_RGB16_FLAG		0x40000000

/*
 * hwmode, each one must be unique
 */
#define CRM_OLAY_MODE_8		( BM_FB_B | BM_BUF_DEPTH_8 | \
					BM_COLOR_INDEX | BM_PIX_DEPTH_8 )

#define CRM32_CI_MODE_8		( BM_FB_A | BM_BUF_DEPTH_32 | \
					BM_COLOR_INDEX | BM_PIX_DEPTH_8 )

#define CRM32_CI_MODE_12	( BM_FB_A | BM_BUF_DEPTH_32 | \
					BM_COLOR_INDEX | BM_PIX_DEPTH_16 )

#define CRM32_RGB_MODE_8	( CRM_RGB8_FLAG | BM_FB_A | BM_BUF_DEPTH_32 \
					| BM_COLOR_INDEX | BM_PIX_DEPTH_8 )

#define CRM32_RGB_MODE_16	( CRM_RGB16_FLAG | BM_FB_A | BM_BUF_DEPTH_32 \
					| BM_COLOR_INDEX | BM_PIX_DEPTH_16 )

#define CRM32_RGB_MODE_24_BASE	( BM_BUF_DEPTH_32 | \
					BM_RGBA | BM_PIX_DEPTH_32 )

#define CRM32_RGB_MODE_24	( BM_FB_A | CRM32_RGB_MODE_24_BASE )

#define CRM32_RGB_MODE_24_ALT	( BM_FB_C | CRM32_RGB_MODE_24_BASE )

#define CRM16_CI_MODE_8		( BM_FB_A | BM_BUF_DEPTH_16 | \
					BM_COLOR_INDEX | BM_PIX_DEPTH_8 )

#define CRM16_CI_MODE_12	( BM_FB_A | BM_BUF_DEPTH_16 | \
					BM_COLOR_INDEX | BM_PIX_DEPTH_16 )

#define CRM16_RGB_MODE_8	( CRM_RGB8_FLAG | BM_FB_A | BM_BUF_DEPTH_16 \
					| BM_COLOR_INDEX | BM_PIX_DEPTH_8 )

#define CRM16_RGB_MODE_16	( CRM_RGB16_FLAG | BM_FB_A | BM_BUF_DEPTH_16 \
					| BM_COLOR_INDEX | BM_PIX_DEPTH_16 )

/*
 * select buffer 1
 */
#define CRM_MODE_BUF_1		( BM_DOUBLE_PIX | BM_DOUBLE_PIX_SEL)

#define CRM_MUNGE_PIXEL_8TC(c) ( \
		(c & 0xff) )

#define CRM_MUNGE_PIXEL_16TC(c) ( \
		(c & 0x7fff) )



#ifdef CRM_DEBUG
extern int crmDebugLevel;

#define DBGMSG1(m) if (crmDebugLevel >= 1) ErrorF(m)
#define DBGMSG2(m) if (crmDebugLevel >= 2) ErrorF(m)
#define DBGMSG3(m) if (crmDebugLevel >= 3) ErrorF(m)
#define DBGMSG4(m) if (crmDebugLevel >= 4) ErrorF(m)
#define DBGMSG5(m) if (crmDebugLevel >= 5) ErrorF(m)

#else  /* CRM_DEBUG */
#define DBGMSG1(m)
#define DBGMSG2(m)
#define DBGMSG3(m)
#define DBGMSG4(m)
#define DBGMSG5(m)
#endif /* CRM_DEBUG */


#endif /* _CRMDEFS_H_ */

