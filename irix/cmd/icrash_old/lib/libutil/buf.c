#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/buf.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include <sys/pfdat.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/stream.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"

extern int active;
extern int debug;			/* Global debug flag */
extern struct var *varp;                /* pointer to var structure */
extern buf_t *_hbuf;		        /* pointer to buffer hash table */

#define BHASH(d,b) \
	((struct hbuf *)((unsigned)_hbuf + \
	(((int)(d)+(int)(b))&varp->v_hmask) * sizeof (struct hbuf)))

#define GET_BUF(addr, buffp) GET_STRUCT(buf_t, addr, buffp, "buf")
#define GET_HBUF(addr, buffp) GET_STRUCT(struct hbuf, addr, buffp, "hbuf")

#ifndef IRIX5_3
extern cbucket_t *_cbucket;		/* pointer to chunk hash table */

#define GET_CBUKET(addr, buffp) GET_STRUCT(cbucket_t, addr, buffp, "cbucket")
#define BKTSHFT 7
#define lbucket(BB)     ((BB) >> BKTSHFT)

/*
 * bkthash() -- Set bucket hash.
 */
cbucket_t *
bkthash(struct vnode *vp, unsigned start)
{
	cbucket_t *bktp;

	if (debug > 3) {
		fprintf(stderr, "bkthash: vp=0x%x, start=%d\n", vp, start);
	}

	bktp = (cbucket_t*)((unsigned)_cbucket + ((vp->v_number + 
		lbucket(start)) & varp->v_hmask) * sizeof (cbucket_t));
	
	if (debug > 4) {
		fprintf(stderr, "bkthash: bktp=0x%x", bktp);
	}

	return (bktp);
}
#endif

/*
 * find_chunk() -- Returns the kernel address of the buf struct for
 *                 'vnode' that starts at 'offset.' The contents of
 *                 the found buf are placed in the buf struct pointed
 *                 to by bufp.
 */
buf_t *
find_chunk(struct vnode *vn, unsigned offset, buf_t *bufp)
{
	buf_t *bp, *bpp;
	struct vnode *vp, vbuf, *get_vnode();
#ifndef IRIX5_3
	cbucket_t *bktp, *cp, cbuf;
#endif

	if (debug > 3) {
		fprintf(stderr, "find_chunk: vn=0x%x, offset=%d, bufp=0x%x\n", 
			vn, offset, bufp);
	}

	if ((vp = get_vnode(vn, &vbuf)) == 0) {
		if (debug) {
			fprintf(stderr, "find_chunk: couldn't get vnode at 0x%x\n", vn);
		}
		return((buf_t *)0);
	}

#ifdef IRIX5_3
	bpp = vp->v_buf;
	while (bpp) {
		bp = GET_BUF(bpp, bufp);
	/*
	 * See if offset is within buf range.
	 */
	if (bp->b_offset < offset) {
		bp = bp->b_forw;
		continue;
	}
	if (bp->b_offset > offset) {
		bp = bp->b_back;
		continue;
	}
#else
	if ((bktp = bkthash(vp, offset)) == 0) {
		if (debug) {
			fprintf(stderr,
				"find_chunk: could not locate cbucket for 0x%x/%d\n",
				vp, offset);
		}
		return((buf_t *)0);
	}

	if ((cp = GET_CBUKET(bktp, &cbuf)) == 0) {
		if (debug) {
			fprintf(stderr, "find_chunk: couldn't get cbucket at 0x%x\n", 
				bktp);
		}
		return((buf_t *)0);
	}

	bpp = cp->b_forw;
	while (bpp != (buf_t *)bktp) {
		bp = GET_BUF(bpp, bufp);
		/*
		 * See if it's valid and matches exactly.
		 */
		if ((bp->b_flags & B_STALE) || (bp->b_vp != vn) ||
			(bp->b_offset != offset)) {
				bpp = bp->b_forw;
				continue;
		}
#endif
		return (bpp);
	}
	return ((buf_t*)0);
}

/*
 * find_buf() -- Find a buffer for device number (MAJOR/MINOR) and
 *               block number on device.
 */
buf_t *
find_buf(unsigned device, unsigned blkno, buf_t *bufp)
{
	buf_t *bp, *bpp;
	struct hbuf *bhashp, *bhp, bhbuf;

	if (debug > 3) {
		fprintf(stderr, "find_buf: device=0x%x, blkno=%d, bufp=0x%x\n", 
			device, blkno, bufp);
	}

	bhashp = BHASH(device, blkno >> BIO_BBSPERBUCKETSHIFT);

	if ((bhp = GET_HBUF(bhashp, &bhbuf)) == 0) {
		if (debug) {
			fprintf(stderr, "find_buf: couldn't get hbuf at 0x%x\n", 
				bhashp);
		}
		return((buf_t *)0);
	}

	bpp = bhp->b_forw;
	while (bpp != (buf_t *)bhashp) {
		bp = GET_BUF(bpp, bufp);
		/*
		 * See if it's valid and matches exactly.
		 */
		if ((bp->b_flags & B_STALE) || (bp->b_edev != device) ||
			(bp->b_blkno != blkno)) {
				bpp = bp->b_forw;
				continue;
		}
		return (bpp);
	}
	return ((buf_t*)0);
}

/*
 * dobuf() -- Print out the information in a buffer.  This will also
 *            try to dump out the struct proc * / struct vnode *, if
 *            possible.
 */
int
dobuf(command_t cmd)
{
	int i, buf_cnt = 0, firsttime = 1;
	buf_t *bpp, *bp, *nbp, bbuf;
	struct vnode *vn, *vp, vbuf;

	fprintf(cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
	fprintf(cmd.ofp, "====================================================================\n");
	for (i = 0; i < cmd.nargs; i++) {
		bpp = (buf_t*)GET_VALUE(cmd.args[i]);
		if (bp = GET_BUF(bpp, &bbuf)) {
			if (debug > 1 || (cmd.flags & (C_FULL|C_NEXT))) {
				if (!firsttime) {
					fprintf (cmd.ofp, "\n");
					fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
					fprintf (cmd.ofp, "====================================================================\n");
				} else {
					firsttime = 0;
				}
			}
			print_buf(bpp, bp, cmd.flags, cmd.ofp);
			buf_cnt++;
			if (cmd.flags & C_NEXT) {
				buf_cnt += print_bufchain(bpp, bp, 
					((cmd.flags & C_AVAIL) ? 1 : 0), cmd.flags, cmd.ofp);
			}
		} else {
			if (debug) {
				fprintf(stderr, "Could not read buf at 0x%x\n", bpp);
			}
			continue;
		}
	}
	fprintf (cmd.ofp, "====================================================================\n");
	fprintf(cmd.ofp, "%d buf struct%s found\n", 
		buf_cnt, (buf_cnt != 1) ? "s" : "");
}

/* XXX -- Not finished */
int
dobfree(command_t cmd)
{
	int i, buf_cnt = 0, firsttime = 1;
	buf_t *bpp, *bp, *nbp, bbuf;
	struct vnode *vn, *vp, vbuf;

	fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
	fprintf (cmd.ofp, "====================================================================\n");
	buf_cnt += print_bufchain(bpp, bp, 1, cmd.flags, cmd.ofp);
	fprintf (cmd.ofp, "====================================================================\n");
	fprintf(cmd.ofp, "%d buf struct%s found\n", 
		buf_cnt, (buf_cnt != 1) ? "s" : "");
}


#ifndef IRIX5_3
/*
 * dochunkhash() -- Dump out the chunk hash data.  Note that this isn't
 *                  in 5.3, so don't expect the command to be there.
 */
int
dochunkhash(command_t cmd)
{
	int i, mode, bkt = 0, firsttime = 1, buf_cnt = 0;
	unsigned b;
	char *loc;
	cbucket_t *bktp, *cp, cbuf;

	if (cmd.nargs == 0) {
		for (i = 0; i < varp->v_hbuf; i++) {
			bktp = (cbucket_t*)((unsigned)_cbucket + 
				(i * sizeof (cbucket_t)));
			if (debug > 2) {
				fprintf(stderr, "dochunkhash: i=%d, bktp=0x%x\n", i, bktp);
			}
			cp = GET_CBUKET(bktp, &cbuf);
			fprintf(cmd.ofp,
				"\nCBUCKET[%d]: 0x%x, FORW: 0x%x, BACK: 0x%x\n\n",
				i, bktp, cp->b_forw, cp->b_back);

			if (cp->b_forw != (buf_t*)bktp) {
				fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
				fprintf (cmd.ofp, "====================================================================\n");
				buf_cnt += print_bufchain(bktp, cp, 0, cmd.flags, cmd.ofp);
			}
		}
	} else {
		for (i = 0; i < cmd.nargs; i++) {
			b = get_value(cmd.args[i], &mode, varp->v_hbuf);
			if (mode == 2) { /* hex value */
				bktp = (cbucket_t*)b;
			} else {
				bktp = (cbucket_t*)((unsigned)_cbucket +
					(b * sizeof (cbucket_t)));
			}

			if ((cp = GET_CBUKET(bktp, &cbuf)) == 0) {
				fprintf(stderr, "dochunkhash: couldn't read cbucket at 0x%x\n",
					bktp);
				continue;
			}
			bkt = ((unsigned)bktp - (unsigned)_cbucket) / sizeof (cbucket_t);
			if ((bkt < 0) || (bkt >= varp->v_hbuf)) {
				fprintf(stderr, "0x%x is not a valid cbucket pointer\n", bktp);
				if (debug > 2) {
					fprintf(stderr, "dochunkhash: bkt=%d\n", bkt);
				}
				continue;
			}
			fprintf(cmd.ofp,
				"\nCBUCKET[%d]: 0x%x, FORW: 0x%x, BACK: 0x%x\n\n",
				bkt, bktp, cp->b_forw, cp->b_back);
			if (cp->b_forw != (buf_t*)bktp) {
				fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
				fprintf (cmd.ofp, "====================================================================\n");
				buf_cnt += print_bufchain(bktp, cp, 0, cmd.flags, cmd.ofp);
			}
		}
	}
	fprintf (cmd.ofp, "\n====================================================================\n");
	fprintf(cmd.ofp, "%d buf struct%s found\n", 
		buf_cnt, (buf_cnt != 1) ? "s" : "");
}
#endif

/*
 * dobufhash() -- Print out the hash buffer information.
 */
int
dobufhash(command_t cmd)
{
	int i, mode, bhi = 0, firsttime = 1, buf_cnt = 0;
	unsigned b;
	char *loc;
	struct hbuf *bhashp, *bhp, bhbuf;

	if (cmd.nargs == 0) {
		for (i = 0; i < varp->v_hbuf; i++) {
			bhashp = (struct hbuf*)((unsigned)_hbuf + 
				(i * sizeof (struct hbuf)));
			if (debug) {
				fprintf(stderr, "dobufhash: sizeof (hbuf) == %d, ", 
					sizeof (struct hbuf));
				fprintf(stderr, "bhashp == 0x%x (0x%x)\n", bhashp,
					((unsigned)_hbuf + (i * sizeof (struct hbuf))));
			}
			if (debug > 2) {
				fprintf(stderr, "dobufhash: i=%d, bhashp=0x%x\n", i, bhashp);
			}
			bhp = GET_HBUF(bhashp, &bhbuf);
			fprintf(cmd.ofp, "\nBHASH[%d]: 0x%x, FORW: 0x%x, BACK: 0x%x\n\n",
				i, bhashp, bhp->b_forw, bhp->b_back);

			if (bhp->b_forw != (buf_t*)bhashp) {
				fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
				fprintf (cmd.ofp, "====================================================================\n");
				buf_cnt += print_bufchain(bhashp, bhp, 0, cmd.flags, cmd.ofp);
			}
		}
	} else {
		for (i = 0; i < cmd.nargs; i++) {
			b = get_value(cmd.args[i], &mode, varp->v_hbuf);
			if (mode == 2) { /* hex addr */
				bhashp = (struct hbuf*)b;
			} else {
				bhashp = (struct hbuf*)((unsigned)_hbuf +
					(b * sizeof (struct hbuf)));
			}

			if ((bhp = GET_HBUF(bhashp, &bhbuf)) == 0) {
				fprintf(stderr, "bufhash: couldn't read buf at 0x%x\n",
					bhashp);
				continue;
			}

			bhi = ((unsigned)bhashp - (unsigned)_hbuf) / sizeof (struct hbuf);
			if ((bhi < 0) || (bhi >= varp->v_hbuf)) {
				fprintf(stderr, "0x%x is not a valid buf pointer\n", bhashp);
				if (debug > 2) {
					fprintf(stderr, "dobufhash: bhi=%d\n", bhi);
				}
				continue;
			}
			fprintf(cmd.ofp, "\nBHASH[%d]: 0x%x, FORW: 0x%x, BACK: 0x%x\n\n",
				bhi, bhashp, bhp->b_forw, bhp->b_back);
			if (bhp->b_forw != (buf_t*)bhashp) {
				fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
				fprintf (cmd.ofp, "====================================================================\n");
				buf_cnt += print_bufchain(bhashp, bhp, 0, cmd.flags, cmd.ofp);
			}
		}
	}
	fprintf (cmd.ofp, "\n====================================================================\n");
	fprintf(cmd.ofp, "%d buf struct%s found\n", 
		buf_cnt, (buf_cnt != 1) ? "s" : "");
}

#ifndef IRIX5_3
/*
 * docfind() -- Look for cbucket information.
 */
int
docfind(cmd)
command_t cmd;
{
	int i, firsttime = 1, value = 0, buf_cnt = 0;
	unsigned offset;
	struct vnode *vn, *vp, vbuf;
	buf_t *bp, *bpp, bbuf, *hbufp;
	cbucket_t *bktp, *cp, cbuf;

	vn = (struct vnode*)GET_VALUE(cmd.args[0]);
	if (cmd.nargs == 2) {
		offset = GET_VALUE(cmd.args[1]);
		if ((bp = find_chunk(vn, offset, &bbuf)) == 0) {
			fprintf(stderr, "Could not locate buf for 0x%x/%d\n",
				vn, offset);
			return 1;
		}
		fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
		fprintf (cmd.ofp, "====================================================================\n");
		print_buf(bp, &bbuf, cmd.flags, cmd.ofp);
	} else {
		if ((vp = GET_VNODE(vn, &vbuf)) == 0) {
			fprintf(stderr, "couldn't get vnode at 0x%x\n", vn);
			return 1;
		}
		fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
		fprintf (cmd.ofp, "====================================================================\n");
		for (i = 0; i < varp->v_hbuf; i++) {
			bktp = (cbucket_t*)((unsigned)_cbucket +
				(i * sizeof (cbucket_t)));
			if (debug > 2) {
				fprintf(stderr, "docfind: i=%d, bktp=0x%x\n", i, bktp);
			}
			cp = GET_CBUKET(bktp, &cbuf);
			bpp = cp->b_forw;
			while (bpp != (buf_t *)bktp) {
				if (debug > 2) {
					fprintf(stderr, "cfind: bpp=0x%x\n", bpp);
				}
				/* this should never happen (but it did once!) */
				if ((bp = GET_BUF(bpp, &bbuf)) == 0) {
					fprintf(stderr,
						"WARNING: 0x%x is not a valid buf pointer\n", bpp);
					break;
				}
				/*
				 * See if it's valid and vnode matches.
				 */
				if ((bp->b_flags & B_STALE) || (bp->b_vp != vn)) {
					bpp = bp->b_forw;
					continue;
				}
				print_buf(bpp, bp, cmd.flags, cmd.ofp);
				bpp = bp->b_forw;
			}
		}
	}
}
#endif

/*
 * dobfind() -- Find a buffer, plain and simple.
 */
int
dobfind(command_t cmd)
{
	int i, firsttime = 1, value = 0, buf_cnt = 0;
	unsigned device, blkno;
	buf_t *bp, *bpp, bbuf;
	struct hbuf *hbufp, *bhp, bhbuf;

	device = GET_VALUE(cmd.args[0]);
	if (cmd.nargs == 2) {
		blkno = GET_VALUE(cmd.args[1]);

		if ((bp = find_buf(device, blkno, &bbuf)) == 0) {
			fprintf(stderr, "Could not locate buf for 0x%x/%d\n",
				device, blkno);
			return 1;
		}

		fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
		fprintf (cmd.ofp, "====================================================================\n");
		print_buf(bp, &bbuf, cmd.flags, cmd.ofp);
	} else {
		fprintf (cmd.ofp, "     BUF      EDEV   PROC/VP      SIZE    OFFSET    BCOUNT     FLAGS\n");
		fprintf (cmd.ofp, "====================================================================\n");
		for (i = 0; i < varp->v_hbuf; i++) {
			hbufp = (struct hbuf*)((unsigned)_hbuf + 
				(i * sizeof (struct hbuf)));
			if (debug > 2) {
				fprintf(stderr, "dobfind: i=%d, hbufp=0x%x\n", i, hbufp);
			}
			bhp = GET_HBUF(hbufp, &bhbuf);
			bpp = bhp->b_forw;
			while (bpp != (buf_t*)hbufp) {
				if (debug > 2) {
					fprintf(stderr, "bfind: bpp=0x%x\n", bpp);
				}

				/* this should never happen (but it did once!) */
				if ((bp = GET_BUF(bpp, &bbuf)) == 0) {
					fprintf(stderr,
						"WARNING: 0x%x is not a valid buf pointer\n", bpp);
					break;
				}
				/*
				 * See if it's valid and device matches.
				 */
				if ((bp->b_flags & B_STALE) || (bp->b_edev != device)) {
					bpp = bp->b_forw;
					continue;
				}
				print_buf(bpp, bp, cmd.flags, cmd.ofp);
				bpp = bp->b_forw;
			}
		}
	}
}


/*
 * print_buf() -- Print out the data inside a buffer, normally called from
 *                one of the buffer routines.
 */
int
print_buf(unsigned bp, buf_t *bufp, int flags, FILE *ofp)
{
	fprintf(ofp, "%8x  %8x  %8x  %8d  %8d  %8d  %8x\n",
		bp, bufp->b_edev, bufp->b_vp, bufp->b_bufsize, 
		bufp->b_offset, bufp->b_bcount, bufp->b_flags);

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		fprintf(ofp, "  B_FORW=0x%x, B_BACK=0x%x\n", 
			bufp->b_forw, bufp->b_back);
		fprintf(ofp, "  AV_FORW=0x%x, AV_BACK=0x%x\n", 
			bufp->av_forw, bufp->av_back);
#ifdef IRIX5_3
		fprintf(ofp, "  B_DFORW=0x%x, B_DBACK=0x%x\n", 
			bufp->b_dforw, bufp->b_dback);
#else
		fprintf(ofp, "  B_VFORW=0x%x, B_VBACK=0x%x\n", 
			bufp->b_vforw, bufp->b_vback);
#endif
		fprintf(ofp, "  B_UN=0x%x, B_PAGES=0x%x\n",
			bufp->b_un.b_addr, bufp->b_pages);
		fprintf(ofp, "  B_BLKNO=%d\n", bufp->b_blkno);
		fprintf(ofp, "\n");
	}
}


/*
 * print_bufchain() -- Print out the chain of buffers, handle if
 *                     it is a live system (don't recurse!)
 */
int
print_bufchain(buf_t *buf, buf_t *bp, int avlist, int flags, FILE *ofp)
{
	int buf_cnt = 0, loop_trap = 0;
	buf_t *nbp, *b, nbbuf;

	if (debug > 3) {
		fprintf(stderr, "buf=0x%x, bp=0x%x, avlist=%d, flags=0x%x\n",
			buf, bp, avlist, flags);
	}

	if (avlist) {
		nbp = bp->av_forw;
	} else {
		nbp = bp->b_forw;
	}

	while (nbp != buf) {
		if (debug > 2) {
			fprintf(stderr, "print_bufchain: nbp=0x%x\n", nbp);
		}

		/* Make sure we can at least read a buf (valid or not) */
		if ((b = GET_BUF(nbp, &nbbuf)) == 0) {
			fprintf(stderr, "WARNING: 0x%x is not a valid buf pointer\n", 
				nbp);
			break;
		}
		if (b->b_flags) {
			print_buf((unsigned)nbp, b, flags, ofp);
			buf_cnt++;
		} else {
			if (loop_trap) {
				fprintf(stderr, "possible endless loop (live system)!\n"); 
			}
			fprintf(ofp, "BUF: 0x%x, FORW: 0x%x, BACK: 0x%x\n",
				nbp, b->b_forw, b->b_back);

			if (active)
				loop_trap++;
		}

		/*
		 * Make sure buf doesn't point to a NULL buf and
		 * that it doen't point to itself.
		 */
		if (avlist) {
			if (!b->av_forw || (b->av_forw == nbp))
				break;
			nbp = b->av_forw;
		} else {
			if (!b->b_forw || (b->b_forw == nbp))
				break;
			nbp = b->b_forw;
		}
	}
	return(buf_cnt);
}
