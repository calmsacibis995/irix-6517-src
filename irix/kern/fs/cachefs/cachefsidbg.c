#include <bstring.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/sema.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/pfdat.h>
#include <string.h>
#include <sys/systm.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include "cachefs_fs.h"

void    idbg_prcnode(__psint_t);

static void    idbg_fscache( __psint_t );
static void    idbg_cachefscache( __psint_t );
static void    idbg_cfsworkq( __psint_t );
static void    idbg_fid( __psint_t addr );

extern cachefscache_t *cachefs_cachelist;

/*
 * tab_vtypes from idbg module
 */
extern char *tab_vtypes[];

#ifdef DEBUG
void
cache_trace(
	int id,
	cfstrace_t *tp,
	void *addr,
	long val1,
	long val2,
	long val3,
	int line )
{
	int index;
	int ospl;

	ASSERT(VALID_ADDR(tp));
	ospl = mutex_spinlock(&tp->ct_lock);
	ASSERT( (0 <= tp->ct_index) && (tp->ct_index < CFS_TRACEBUFLEN) );
	index = tp->ct_index;
	tp->ct_buffer[index].tr_id = id;
	tp->ct_buffer[index].tr_addr = addr;
	tp->ct_buffer[index].tr_val[0] = val1;
	tp->ct_buffer[index].tr_val[1] = val2;
	tp->ct_buffer[index].tr_val[2] = val3;
	tp->ct_buffer[index].tr_line = line;
	tp->ct_index = (index + 1) % CFS_TRACEBUFLEN;
	if ( tp->ct_index == 0 ) {
		tp->ct_wrap++;
	}
	mutex_spinunlock(&tp->ct_lock, ospl);
}
#endif /* DEBUG */

/*
 * traverse the specified cnode table, printing the vnodes
 */
vnode_t *
find_cachefs_vnode( fscache_t *fscp, vnumber_t number )
{
	struct cnode *cp;
	int ci;

	for ( ci = 0; ci < CNODE_BUCKET_SIZE; ci++ ) {
		for ( cp = fscp->fs_cnode[ci]; cp; cp = cp->c_hash ) {
			if ( CTOV(cp)->v_number == number ) {
				return( CTOV(cp) );
			}
		}
	}
	return( NULL );
}

static char *
cflags_to_str( int flags )
{
	static char str[331];

	str[0] = 0;
	if ( flags & CN_NOCACHE ) {
		strcat( str, "CN_NOCACHE " );
	}
	if ( flags & CN_DESTROY ) {
		strcat( str, "CN_DESTROY " );
	}
	if ( flags & CN_UPDATE_PENDING ) {
		strcat( str, "CN_UPDATE_PENDING " );
	}
	if ( flags & CN_UPDATED ) {
		strcat( str, "CN_UPDATED " );
	}
	if ( flags & CN_ROOT ) {
		strcat( str, "CN_ROOT " );
	}
	if ( flags & CN_NEEDINVAL ) {
		strcat( str, "CN_NEEDINVAL " );
	}
	if ( flags & CN_INVAL ) {
		strcat( str, "CN_INVAL " );
	}
	if ( flags & CN_POPULATION_PENDING ) {
		strcat( str, "CN_POPULATION_PENDING " );
	}
	if ( flags & CN_MODIFIED ) {
		strcat( str, "CN_MODIFIED " );
	}
	if ( flags & CN_SYNC ) {
		strcat( str, "CN_SYNC " );
	}
	if ( flags & CN_RECLAIM ) {
		strcat( str, "CN_RECLAIM " );
	}
	if ( flags & CN_VALLOC ) {
		strcat( str, "CN_VALLOC " );
	}
	if ( flags & CN_REPLACE ) {
		strcat( str, "CN_REPLACE " );
	}
	if ( flags & CN_WRITTEN ) {
		strcat( str, "CN_WRITTEN " );
	}
	if ( flags & CN_FRLOCK ) {
		strcat( str, "CN_FRLOCK " );
	}
	return( str );
}

static char *
cioflags_to_str( u_int flags )
{
	char *str;

	if ( flags & CIO_ASYNCWRITE ) {
		str = "CIO_ASYNCWRITE";
	} else if ( flags & CIO_ASYNCREAD ) {
		str = "CIO_ASYNCREAD";
	} else {
		str = "";
	}
	return( str );
}

#if DEBUG
static char *
traceid_to_str( int id )
{
	char *str;

	switch ( id ) {
		case CNTRACE_ACT:
			str = "CNTRACE_ACT";
			break;
		case CNTRACE_WORKQ:
			str = "CNTRACE_WORKQ";
			break;
		case CNTRACE_UPDATE:
			str = "CNTRACE_UPDATE";
			break;
		case CNTRACE_DESTROY:
			str = "CNTRACE_DESTROY";
			break;
		case CNTRACE_DIRHOLD:
			str = "CNTRACE_DIRHOLD";
			break;
		case CNTRACE_HOLD:
			str = "CNTRACE_HOLD";
			break;
		case CNTRACE_WRITE:
			str = "CNTRACE_WRITE";
			break;
		case CNTRACE_READ:
			str = "CNTRACE_READ";
			break;
		case CNTRACE_SIZE:
			str = "CNTRACE_SIZE";
			break;
		case CNTRACE_ALLOCENTS:
			str = "CNTRACE_ALLOCENTS";
			break;
		case CNTRACE_ALLOCMAP:
			str = "CNTRACE_ALLOCMAP";
			break;
		case CNTRACE_DIOWRITE:
			str = "CNTRACE_DIOWRITE";
			break;
		case CNTRACE_WRITEHDR:
			str = "CNTRACE_WRITEHDR";
			break;
		case CNTRACE_ATTR:
			str = "CNTRACE_ATTR";
			break;
		case CNTRACE_FILEHEADER:
			str = "CNTRACE_FILEHEADER";
			break;
		case CNTRACE_LOOKUP:
			str = "CNTRACE_LOOKUP";
			break;
		case CNTRACE_NOCACHE:
			str = "CNTRACE_NOCACHE";
			break;
		case CNTRACE_INVAL:
			str = "CNTRACE_INVAL";
			break;
		case CNTRACE_FRONTSIZE:
			str = "CNTRACE_FRONTSIZE";
			break;
		case CNTRACE_POPDIR:
			str = "CNTRACE_POPDIR";
			break;
		case CACHEFS_FUNCTION:
			str = "CACHEFS_FUNCTION";
			break;
		default:
			str = "BAD ID";
	}

	return( str );
}

static char *
lockmode_to_str(lockmode_t lm)
{
	static char *str;

	switch (lm) {
		case READLOCKED:
			str = "READLOCKED";
			break;
		case WRITELOCKED:
			str = "WRITELOCKED";
			break;
		case UNLOCKED:
			str = "UNLOCKED";
			break;
		case NOLOCK:
			str = "NOLOCK";
			break;
		default:
			str = "BAD LOCKMODE";
	}
	return(str);
}
#endif /* DEBUG */

static char *
cfscmd_to_str( enum cachefs_cmd cmd )
{
	static char *str;

	switch ( cmd ) {
		case CFS_POPULATE_DIR:
			str = "CFS_POPULATE_DIR";
			break;
		case CFS_CACHE_SYNC:
			str = "CFS_CACHE_SYNC";
			break;
		case CFS_ASYNCREAD:
			str = "CFS_ASYNCREAD";
			break;
		case CFS_ASYNCWRITE:
			str = "CFS_ASYNCWRITE";
			break;
		case CFS_NOOP:
			str = "CFS_NOOP";
			break;
		default:
			str = "BAD_CFS_CMD";
	}
	return( str );
}

#ifdef DEBUG
static void
print_cache_trace_vals( tracerec_t *trp )
{
	switch ( trp->tr_id ) {
		case CNTRACE_ACT:
			qprintf( " %s %s count %ld\n", cflags_to_str( trp->tr_val[0] ),
				trp->tr_val[1] ? "active" : "inactive", trp->tr_val[1] );
			break;
		case CNTRACE_WORKQ:
			qprintf( " %s %s\n",
				cfscmd_to_str( (enum cachefs_cmd)trp->tr_val[0] ),
				cflags_to_str( trp->tr_val[1] ) );
			break;
		case CNTRACE_WRITEHDR:
			qprintf( " %s %ld\n", cflags_to_str( trp->tr_val[0] ),
				trp->tr_val[1] );
			break;
		case CNTRACE_UPDATE:
			qprintf( " %s %ld\n", cflags_to_str( trp->tr_val[0] ),
				trp->tr_val[1] );
			break;
		case CNTRACE_DESTROY:
			qprintf( " %s\n", cflags_to_str( trp->tr_val[0] ) );
			break;
		case CNTRACE_DIRHOLD:
		case CNTRACE_HOLD:
			qprintf( " count %ld\n", trp->tr_val[0] );
			break;
		case CNTRACE_WRITE:
		case CNTRACE_READ:
			qprintf( " %ld %ld\n", trp->tr_val[0], trp->tr_val[1] );
			break;
		case CNTRACE_DIOWRITE:
			qprintf( " %ld %ld\n", trp->tr_val[0], trp->tr_val[1] );
			break;
		case CNTRACE_SIZE:
			qprintf( " %ld\n", trp->tr_val[0] );
			break;
		case CNTRACE_ALLOCENTS:
			qprintf( " entries %ld\n", trp->tr_val[0] );
			break;
		case CNTRACE_ALLOCMAP:
			qprintf( " entry %ld size %ld\n", trp->tr_val[0], trp->tr_val[1] );
			break;
		case CNTRACE_ATTR:
			qprintf( "\n" );
			break;
		case CNTRACE_FILEHEADER:
			qprintf( " fhp 0x%x\n", trp->tr_val[0] );
			break;
		case CNTRACE_LOOKUP:
			qprintf( " count %ld\n", trp->tr_val[0] );
			break;
		case CNTRACE_NOCACHE:
			qprintf( " flags %s\n", cflags_to_str(trp->tr_val[0]) );
			break;
		case CNTRACE_FRONTSIZE:
		case CNTRACE_POPDIR:
			qprintf( " size %ld lockmode %s\n", trp->tr_val[0],
				lockmode_to_str((lockmode_t)trp->tr_val[1]) );
			break;
		case CNTRACE_INVAL:
			qprintf( " lockmode %s\n",
				lockmode_to_str((lockmode_t)trp->tr_val[0]) );
			break;
		case CACHEFS_FUNCTION:
			qprintf( " arg1 0x%x arg2 0x%x arg3 0x%x\n", trp->tr_val[0],
				trp->tr_val[1], trp->tr_val[2] );
			break;
		default:
			qprintf( " 0x%x 0x%x 0x%x\n", trp->tr_val[0], trp->tr_val[1],
				trp->tr_val[2] );
	}
}

static void
print_cache_trace( cfstrace_t *trp )
{
	int index;
	char *sym;
	int offset;
	struct trace_record *recp;

	if ( trp->ct_wrap ) {
		index = trp->ct_index;
	} else {
		index = 0;
	}
	qprintf( "Cache trace\n-----------\n" );
	qprintf( "trace structure at 0x%x\n", trp );
	qprintf( "index %d, wrap count %d\n", trp->ct_index, trp->ct_wrap );
	qprintf( "first record at index %d\n", index );
	do {
		recp = &(trp->ct_buffer[index]);
		sym = fetch_kname((void *)recp->tr_addr, (void *)&offset);
		qprintf( "%d\t%s %s+0x%x line %d", index,
			traceid_to_str( recp->tr_id ), sym, offset, recp->tr_line );
		print_cache_trace_vals( recp );
		index = (index + 1) % CFS_TRACEBUFLEN;
	} while ( index != trp->ct_index );
}
#endif /* DEBUG */

static void
prtallocmap( allocmap_t *map, u_short entries )
{
	u_short i;

	if (entries > ALLOCMAP_SIZE) {
		qprintf( "Bad allocation map.\n" );
	} else {
		qprintf( "Allocation map\n--------------\n\n" );
		qprintf( "slot   offset      size\n----   ------      ----\n" );
		for ( i = 0; i < entries; i++ ) {
			qprintf( "%4d 0x%x 0x%x\n", i, (int)map[i].am_start_off,
				(int)map[i].am_size );
		}
	}
}

static char *
mdstate_to_str( u_short mdstate )
{
	static char str[49];

	str[0] = 0;
	if (mdstate & MD_MDVALID) {
		strcat( str, "MD_MDVALID " );
	}
	if (mdstate & MD_NOALLOCMAP) {
		strcat( str, "MD_NOALLOCMAP " );
	}
	if (mdstate & MD_POPULATED) {
		strcat( str, "MD_POPULATED " );
	}
	if (mdstate & MD_KEEP) {
		strcat( str, "MD_KEEP " );
	}
	if (mdstate & MD_INIT) {
		strcat( str, "MD_INIT " );
	}
	return(str);
}

static char namebuf[MAXPATHLEN];

static void
idbg_fileheader( __psint_t addr )
{
	fileheader_t *fhp;

	if ( addr < -1 ) {
		fhp = (fileheader_t *)addr;
		qprintf( "      md_state: %s (0x%x)\n",
			mdstate_to_str(fhp->fh_metadata.md_state),
			(int)fhp->fh_metadata.md_state );
		qprintf( "   md_checksum: %d\n", fhp->fh_metadata.md_checksum );
		qprintf( "  md_allocents: %d of %d\n", fhp->fh_metadata.md_allocents,
			ALLOCMAP_SIZE );
		qprintf( "     &md_token: 0x%lx\n", &fhp->fh_metadata.md_token );
		qprintf( "&md_attributes: 0x%lx\n", &fhp->fh_metadata.md_attributes );
		qprintf( "Back fid at 0x%lx\n", &fhp->fh_metadata.md_backfid );
		idbg_fid( (__psint_t)&fhp->fh_metadata.md_backfid );
		if (fhp->fh_metadata.md_state & MD_NOALLOCMAP) {
			strncpy(namebuf, (char *)fhp->fh_allocmap,
				fhp->fh_metadata.md_allocents);
			qprintf( "   fh_allocmap: %s\n", (char *)namebuf );
		} else if (fhp->fh_metadata.md_allocents > 0) {
			prtallocmap( fhp->fh_allocmap, fhp->fh_metadata.md_allocents );
		}
	} else {
		qprintf( "Illegal fileheader address 0x%x\n", addr );
	}
}

static void
idbg_fid( __psint_t addr )
{
	fid_t *fidp;
	int i;

	if ( addr < -1 ) {
		fidp = (fid_t *)addr;
		qprintf( " fid_len: %d\n", fidp->fid_len );
		qprintf( "fid_data: " );
		for ( i = 0; (i < fidp->fid_len) && (i < MAXFIDSZ); i++ ) {
			qprintf( "0x%2x ", fidp->fid_data[i] );
		}
		qprintf( "\n" );
	} else {
		qprintf( "Illegal fid address 0x%x\n", addr );
	}
}

static void
prtcnode( cnode_t *cp )
{
	qprintf( "cnode structure at 0x%lx\n", cp );
	if ( CTOV(cp) ) {
		qprintf( "          vtype: %s\n", tab_vtypes[CTOV(cp)->v_type] );
	}
	qprintf( "       c_type: %s\n", tab_vtypes[cp->c_type] );
	qprintf( "      c_flags: %s (0x%x)\n", cflags_to_str(cp->c_flags),
		cp->c_flags );
	qprintf( "       c_hash: 0x%lx\n", cp->c_hash );
	qprintf( "c_hash_by_fid: 0x%lx\n", cp->c_hash_by_fid );
	qprintf( "    c_frontvp: 0x%lx\n", cp->c_frontvp );
	qprintf( " c_frontdirvp: 0x%lx\n", cp->c_frontdirvp );
	qprintf( "     c_backvp: 0x%lx\n", cp->c_backvp );
	qprintf( "        c_dcp: 0x%lx\n", cp->c_dcp );
	qprintf( "      c_vnode: 0x%lx\n", cp->c_vnode );
	qprintf( "   c_frontfid: 0x%lx\n", &cp->c_frontfid );
	qprintf( "c_frontdirfid: 0x%lx\n", &cp->c_frontdirfid );
	qprintf( "    c_backfid: 0x%lx\n", cp->c_backfid );
	qprintf( " c_fileheader: 0x%lx\n", cp->c_fileheader );
	qprintf( "       c_attr: 0x%lx\n", cp->c_attr );
	qprintf( "       c_size: %d\n", (int)cp->c_size );
	qprintf( "      c_token: 0x%lx\n", cp->c_token );
	qprintf( "      c_error: %d\n", cp->c_error );
	qprintf( "        c_nio: %d\n", cp->c_nio );
	qprintf( "    c_ioflags: %s\n", cioflags_to_str(cp->c_ioflags) );
	qprintf( "    c_fscache: 0x%lx\n", cp->c_fscache );
#ifdef DEBUG
	qprintf( "     c_inhash: %d\n", cp->c_inhash );
	qprintf( "  c_infidhash: %d\n", cp->c_infidhash );
#endif /* DEBUG */
	qprintf( "      &c_iosv: 0x%lx\n", &cp->c_iosv );
	qprintf( "    &c_iolock: 0x%lx\n", &cp->c_iolock );
	qprintf( "    &c_rwlock: 0x%lx\n", &cp->c_rwlock );
	qprintf( " &c_statelock: 0x%lx\n", &cp->c_statelock );
	qprintf( "&c_popwait_sv: 0x%lx\n", &cp->c_popwait_sv );
	qprintf( " &c_valloc_sv: 0x%lx\n", &cp->c_valloc_wait );
	if (cp->c_frontfid.fid_len) {
		qprintf( "Front fid at 0x%lx\n", &cp->c_frontfid );
		idbg_fid( (__psint_t)&cp->c_frontfid );
	}
	if (cp->c_frontdirfid.fid_len) {
		qprintf( "Frontdir fid at 0x%lx\n", &cp->c_frontdirfid );
		idbg_fid( (__psint_t)&cp->c_frontdirfid );
	}
	if (cp->c_fileheader) {
		idbg_fileheader((__psint_t)cp->c_fileheader);
	}
#if defined(DEBUG) && defined(_CNODE_TRACE)
	if ( cp->c_trace.ct_index || cp->c_trace.ct_wrap ) {
		print_cache_trace( &cp->c_trace );
	}
#endif /* DEBUG && _CNODE_TRACE */
}

static void
print_cachefs_cnodes( fscache_t *fscp )
{
	struct cnode *cp;
	int ci;

	for ( ci = 0; ci < CNODE_BUCKET_SIZE; ci++ ) {
		for ( cp = fscp->fs_cnode[ci]; cp; cp = cp->c_hash ) {
			prtcnode( cp );
		}
	}
}

static void
print_cnode_hash( fscache_t *fscp )
{
	int i;
	cnode_t *cp;

	qprintf("Cnode hash tables for fscache 0x%p\n", fscp);
	qprintf("Cnode hash by back file ID:\n");
	for (i = 0; i < CNODE_BUCKET_SIZE; i++) {
		if (cp = fscp->fs_cnode[i]) {
			qprintf("[%d]", i);
			for (; cp; cp = cp->c_hash) {
				qprintf(" 0x%p", cp);
			}
			qprintf("\n");
		}
	}
	qprintf("Cnode hash by front file vnode:\n");
	for (i = 0; i < CNODE_BUCKET_SIZE; i++) {
		if (cp = fscp->fs_cnode_by_fid[i]) {
			qprintf("[%d]", i);
			for (; cp; cp = cp->c_hash_by_fid) {
				qprintf(" 0x%p", cp);
			}
			qprintf("\n");
		}
	}
}

void
idbg_chash( __psint_t addr )
{
	fscache_t *fscp;
	cachefscache_t *cachep;

	if ( addr >= 0 ) {
		qprintf( "Illegal cnode address 0x%x\n", addr );
	} else if ( addr == -1 ) {
		for ( cachep = cachefs_cachelist; cachep; cachep = cachep->c_next ) {
			for ( fscp = cachep->c_fslist; fscp; fscp = fscp->fs_next ) {
				print_cnode_hash( fscp );
			}
		}
	} else {
		print_cnode_hash( (fscache_t *)addr );
	}
}

void
idbg_prcnode( __psint_t addr )
{
	cachefscache_t *cachep;
	fscache_t *fscp;

	if ( addr >= 0 ) {
		qprintf( "Illegal cnode address 0x%x\n", addr );
	} else if ( addr == -1 ) {
		for ( cachep = cachefs_cachelist; cachep; cachep = cachep->c_next ) {
			for ( fscp = cachep->c_fslist; fscp; fscp = fscp->fs_next ) {
				print_cachefs_cnodes( fscp );
			}
		}
	} else {
		prtcnode( (cnode_t *)addr );
	}
}

/*
 * traverse the specified cnode table, printing the vnodes
 */
void
print_cachefs_vnodes( fscache_t *fscp, int all )
{
	struct cnode *cp;
	int ci;

	for ( ci = 0; ci < CNODE_BUCKET_SIZE; ci++ ) {
		for ( cp = fscp->fs_cnode[ci]; cp; cp = cp->c_hash ) {
			_prvn( CTOV( cp ), all );
		}
	}
}

static char *
fscache_flags_to_str( int flags )
{
	static char str[42];

	str[0] = 0;
	if ( flags & CFS_FS_READ ) {
		strcat( str, "CFS_FS_READ " );
	}
	if ( flags & CFS_FS_WRITE ) {
		strcat( str, "CFS_FS_WRITE " );
	}
	return( str );
}

static char *
cfsmountopts_to_str( int flags )
{
	static char str[92];

	str[0] = 0;
	if ( flags & CFS_WRITE_AROUND ) {
		strcat( str, "CFS_WRITE_AROUND " );
	}
	if ( flags & CFS_DUAL_WRITE ) {
		strcat( str, "CFS_DUAL_WRITE " );
	}
	if ( flags & CFS_NOCONST_MODE ) {
		strcat( str, "CFS_NOCONST_MODE " );
	}
	if ( flags & CFS_ACCESS_BACKFS ) {
		strcat( str, "CFS_ACCESS_BACKFS " );
	}
	if ( flags & CFS_DISCONNECT ) {
		strcat( str, "CFS_DISCONNECT " );
	}
	return( str );
}

static void
prcfsreq( struct cachefs_req *req )
{
	switch ( req->cfs_cmd ) {
		case CFS_POPULATE_DIR:
			qprintf( "cnode: 0x%x\n", req->cfs_req_u.cu_popdir.cpd_cp );
			break;
		case CFS_CACHE_SYNC:
			qprintf( "cachep: 0x%x\n", req->cfs_req_u.cu_fs_sync.cf_cachep );
			break;
		case CFS_ASYNCWRITE:
		case CFS_ASYNCREAD:
			qprintf( "cnode: 0x%x\n", req->cfs_req_u.cu_io.cp_cp );
			qprintf( "  buf: 0x%x\n", req->cfs_req_u.cu_io.cp_buf );
			qprintf( "flags: 0x%x\n", req->cfs_req_u.cu_io.cp_flags );
			break;
		case CFS_NOOP:
			qprintf( "Null request\n" );
			break;
		default:
			qprintf( "Bad CFS request cmd: 0x%x\n", (int)req->cfs_cmd );
	}
}

static void
prcachefs_req( struct cachefs_req *req )
{
	qprintf( "cachefs_req at 0x%x:\n", req );
	qprintf( "     cfs_next: 0x%x\n", req->cfs_next );
	qprintf( "      cfs_cmd: %s\n", cfscmd_to_str( req->cfs_cmd ) );
	qprintf( "       cfs_cr: 0x%x\n", req->cfs_cr );
	qprintf( "Request\n-------\n" );
	prcfsreq( req );
}

static void
idbg_cfsworkq( __psint_t addr )
{
	struct cachefs_req *req;
	struct cachefs_workq *wqp;
	unsigned halt;
	unsigned keep;

	if ( addr < -1 ) {
		wqp = (struct cachefs_workq *)addr;
		halt = (unsigned)(wqp->wq_halt_request ? 1 : 0);
		keep = (unsigned)(wqp->wq_keepone ? 1 : 0);
		qprintf( "cachefs work queue at 0x%lx\n", wqp );
		qprintf( "            wq_head: 0x%lx\n", wqp->wq_head );
		qprintf( "            wq_tail: 0x%lx\n", wqp->wq_tail );
		qprintf( "          wq_length: %d\n", wqp->wq_length );
		qprintf( "    wq_thread_count: %d\n", wqp->wq_thread_count );
		qprintf( "     wq_thread_busy: %d\n", wqp->wq_thread_busy );
		qprintf( "         wq_max_len: %d\n", wqp->wq_max_len );
		qprintf( "    wq_halt_request: %d\n", halt );
		qprintf( "         wq_keepone: %d\n", keep );
		qprintf( "         &wq_req_sv: 0x%lx\n", &wqp->wq_req_sv );
		qprintf( "        &wq_halt_sv: 0x%lx\n", &wqp->wq_halt_sv );
		qprintf( "     &wq_queue_lock: 0x%lx\n", &wqp->wq_queue_lock );
		if ( wqp->wq_length ) {
			qprintf( "Request queue\n-------------\n" );
			for ( req = wqp->wq_head; req; req = req->cfs_next ) {
				prcachefs_req( req );
			}
		}
	} else {
		qprintf( "Illegal cachefs_workq address 0x%x\n", addr );
	}
}

void
prfscache( fscache_t *fscp )
{
	qprintf( "fscache structure at 0x%lx\n", fscp );
	qprintf( "        fs_cacheid: %s\n", fscp->fs_cacheid );
	qprintf( "          fs_flags: %s (0x%x)\n",
		fscache_flags_to_str(fscp->fs_flags), fscp->fs_flags );
	qprintf( "          fs_cache: 0x%p\n", fscp->fs_cache );
	qprintf( "        fs_options: %s (0x%x)\n", 
		cfsmountopts_to_str(fscp->fs_options), fscp->fs_options );
	qprintf( "           fs_vfsp: 0x%p\n", fscp->fs_bhv.bd_vobj ?
		FSC_TO_VFS(fscp) : (struct vfs *)NULL );
	qprintf( "         fs_rootvp: 0x%p\n", fscp->fs_rootvp );
	qprintf( "       fs_vnoderef: %d\n", fscp->fs_vnoderef );
	qprintf( "       fs_acregmin: %d\n", fscp->fs_acregmin );
	qprintf( "       fs_acregmax: %d\n", fscp->fs_acregmax );
	qprintf( "       fs_acdirmin: %d\n", fscp->fs_acdirmin );
	qprintf( "       fs_acdirmax: %d\n", fscp->fs_acdirmax );
	qprintf( "          fs_bsize: %lld\n", fscp->fs_bsize );
	qprintf( "       fs_bmapsize: %lld\n", fscp->fs_bmapsize );
	qprintf( "           fs_next: 0x%p\n", fscp->fs_next );
	qprintf( "         &fs_cnode: 0x%p\n", fscp->fs_cnode );
	qprintf( "  &fs_cnode_by_fid: 0x%p\n", fscp->fs_cnode_by_fid );
	qprintf( "       fs_hashvers: %d\n", fscp->fs_hashvers );
	qprintf( "     &fs_cnodelock: 0x%p\n", &fscp->fs_cnodelock );
	qprintf( "        &fs_fslock: 0x%p\n", &fscp->fs_fslock );
	qprintf( "         fs_cfsops: 0x%p\n", fscp->fs_cfsops );
}

static char *
cachefscache_flags_to_str( u_int flags )
{
	static char str[151];

	str[0] = 0;
	if ( flags & CACHE_GARBAGE_COLLECT ) {
		strcat( str, "CACHE_GARBAGE_COLLECT " );
	}
	if ( flags & CACHE_GCDAEMON_ACTIVE ) {
		strcat( str, "CACHE_GCDAEMON_ACTIVE " );
	}
	if ( flags & CACHE_GCDAEMON_HALT ) {
		strcat( str, "CACHE_GCDAEMON_HALT " );
	}
	if ( flags & CACHE_GCDAEMON_WAITING ) {
		strcat( str, "CACHE_GCDAEMON_WAITING " );
	}
	return( str );
}

static void
prcache_label( struct cache_label *lp )
{
	qprintf( "cl_cfsversion: 0x%x\n", lp->cl_cfsversion );
	qprintf( "     cl_minio: %d\n", lp->cl_minio );
	qprintf( "     cl_align: %d\n", lp->cl_align );
	qprintf( "   cl_maxblks: %d\n", lp->cl_maxblks );
	qprintf( "  cl_blkhiwat: %d\n", lp->cl_blkhiwat );
	qprintf( "  cl_blklowat: %d\n", lp->cl_blklowat );
	qprintf( "  cl_maxfiles: %d\n", lp->cl_maxfiles );
	qprintf( " cl_filehiwat: %d\n", lp->cl_filehiwat );
	qprintf( " cl_filelowat: %d\n", lp->cl_filelowat );
}

static void
prcachefscache( cachefscache_t *cachep )
{
	qprintf( "cachefscache structure at 0x%lx\n", cachep );
	qprintf( "             c_next: 0x%lx\n", cachep->c_next );
	qprintf( "            c_flags: %s (0x%x)\n",
		cachefscache_flags_to_str( cachep->c_flags ), cachep->c_flags );
	qprintf( "             c_name: %s\n", cachep->c_name );
	qprintf( "          c_labelvp: 0x%lx\n", cachep->c_labelvp );
	qprintf( "            c_dirvp: 0x%x\n", cachep->c_dirvp );
	qprintf( "           c_refcnt: %d\n", cachep->c_refcnt );
	qprintf( "           c_fslist: 0x%x\n", cachep->c_fslist );
	qprintf( "            c_bsize: %d\n", (u_int)cachep->c_bsize );
	qprintf( "         c_bmapsize: %d\n", (u_int)cachep->c_bmapsize );
	qprintf( "    &c_contentslock: 0x%lx\n", &cachep->c_contentslock );
	qprintf( "      &c_fslistlock: 0x%lx\n", &cachep->c_fslistlock );
	qprintf( "  &c_replacement_sv: 0x%lx\n", &cachep->c_replacement_sv );
	qprintf( "      &c_repwait_sv: 0x%lx\n", &cachep->c_repwait_sv );
	qprintf( "c_label\n-------\n" );
	prcache_label( &cachep->c_label );
	qprintf( "c_workq\n-------\n" );
	idbg_cfsworkq( (__psint_t)&cachep->c_workq );
#ifdef _CACHE_TRACE
#ifdef DEBUG
	if ( cachep->c_trace.ct_index || cachep->c_trace.ct_wrap ) {
		print_cache_trace( &cachep->c_trace );
	}
#endif /* DEBUG */
#endif
}

static void
idbg_cachefscache( __psint_t addr )
{
	cachefscache_t *cachep;

	if ( addr == -1 ) {
		for ( cachep = cachefs_cachelist; cachep; cachep = cachep->c_next ) {
			prcachefscache( cachep );
		}
	} else if ( addr < -1 ) {
		prcachefscache( (cachefscache_t *)addr );
	} else {
		qprintf( "Invalid cachefscache address 0x%x.\n", addr );
	}
}

/*
 * print fscache structure
 *
 * fscp == -1:  print all fscache structures
 * fscp < -1:   print the fscache structure at the given address
 * fscp >= 0:   error
 */
static void
idbg_fscache( __psint_t addr )
{
	cachefscache_t *cachep;
	fscache_t *fscp;

	if ( addr == -1 ) {
		for ( cachep = cachefs_cachelist; cachep; cachep = cachep->c_next ) {
			for ( fscp = cachep->c_fslist; fscp; fscp = fscp->fs_next ) {
				prfscache( fscp );
			}
		}
	} else if ( addr < -1 ) {
		prfscache( (fscache_t *)addr );
	} else {
		qprintf( "Invalid fscache address 0x%x.\n", addr );
	}
}

static void
idbg_dirent( __psint_t addr )
{
	dirent_t *dep;

	if ( addr < -1 ) {
		dep = (dirent_t *)addr;
		qprintf( "dirent structure at 0x%lx\n", dep );
		qprintf( "       d_ino: %d\n", dep->d_ino );
		qprintf( "       d_off: 0x%llx\n", dep->d_off );
		qprintf( "    d_reclen: %d\n", dep->d_reclen );
		qprintf( "      d_name: %s\n", dep->d_name );
	} else {
		qprintf( "Invalid dirent address 0x%x.\n", addr );
	}
}

static void
idbg_dirent5( __psint_t addr )
{
	irix5_dirent_t *dep;

	if ( addr < -1 ) {
		dep = (irix5_dirent_t *)addr;
		qprintf( "irix5_dirent structure at 0x%lx\n", dep );
		qprintf( "       d_ino: %d\n", dep->d_ino );
		qprintf( "       d_off: %d\n", dep->d_off );
		qprintf( "    d_reclen: %d\n", dep->d_reclen );
		qprintf( "      d_name: %s\n", dep->d_name );
	} else {
		qprintf( "Invalid dirent5 address 0x%x.\n", addr );
	}
}

static void
idbg_vattr( __psint_t addr )
{
	vattr_t *vap;

	if ( addr < -1 ) {
		vap = (vattr_t *)addr;
		qprintf( "vattr structure at 0x%lx\n", vap );
		qprintf( "       va_type: %s\n", tab_vtypes[vap->va_type] );
		qprintf( "       va_mode: 0x%x\n", vap->va_mode );
		qprintf( "        va_uid: %d\n", vap->va_uid );
		qprintf( "        va_gid: %d\n", vap->va_gid );
		qprintf( "       va_fsid: %d\n", vap->va_fsid );
		qprintf( "     va_nodeid: %d\n", vap->va_nodeid );
		qprintf( "      va_nlink: %d\n", vap->va_nlink );
		qprintf( "       va_size: %d\n", (int)vap->va_size );
		qprintf( "      va_atime: %d %d\n", vap->va_atime.tv_sec,
			vap->va_atime.tv_nsec );
		qprintf( "      va_mtime: %d %d\n", vap->va_mtime.tv_sec,
			vap->va_mtime.tv_nsec );
		qprintf( "      va_ctime: %d %d\n", vap->va_ctime.tv_sec,
			vap->va_ctime.tv_nsec );
		qprintf( "       va_rdev: %d\n", vap->va_rdev );
		qprintf( "    va_blksize: %d\n", vap->va_blksize );
		qprintf( "    va_nblocks: %d\n", vap->va_nblocks );
		qprintf( "      va_vcode: %d\n", vap->va_vcode );
	} else {
		qprintf( "Invalid vattr address 0x%x.\n", addr );
	}
}

static void
idbg_cattr( __psint_t addr )
{
	cattr_t *cap;

	if ( addr < -1 ) {
		cap = (cattr_t *)addr;
		qprintf( "vattr structure at 0x%lx\n", cap );
		qprintf( "       ca_type: %s\n", tab_vtypes[cap->ca_type] );
		qprintf( "       ca_mode: 0x%llx\n", cap->ca_mode );
		qprintf( "        ca_uid: %lld\n", cap->ca_uid );
		qprintf( "        ca_gid: %lld\n", cap->ca_gid );
		qprintf( "       ca_fsid: %lld\n", cap->ca_fsid );
		qprintf( "     ca_nodeid: %lld\n", cap->ca_nodeid );
		qprintf( "      ca_nlink: %lld\n", cap->ca_nlink );
		qprintf( "       ca_size: %lld\n", cap->ca_size );
		qprintf( "      ca_atime: %d %d\n", cap->ca_atime.tv_sec,
			cap->ca_atime.tv_nsec );
		qprintf( "      ca_mtime: %d %d\n", cap->ca_mtime.tv_sec,
			cap->ca_mtime.tv_nsec );
		qprintf( "      ca_ctime: %d %d\n", cap->ca_ctime.tv_sec,
			cap->ca_ctime.tv_nsec );
		qprintf( "       ca_rdev: %lld\n", cap->ca_rdev );
		qprintf( "    ca_nblocks: %lld\n", cap->ca_nblocks );
		qprintf( "      ca_vcode: %lld\n", cap->ca_vcode );
	} else {
		qprintf( "Invalid vattr address 0x%x.\n", addr );
	}
}

#ifdef DEBUG
/* ARGSUSED */
static void
idbg_ftrace( __psint_t addr )
{
	if ( Cachefs_functrace.ct_index || Cachefs_functrace.ct_wrap ) {
		print_cache_trace( &Cachefs_functrace );
	}
}

static char *
valid_wrapper(struct km_wrap *kwp)
{
	struct km_wrap *back_kwp;

	back_kwp = kwp->kw_other;
	if (back_kwp != (kwp + kwp->kw_size - WRAPSIZE)) {
		return("(bad wrapper offset)");
	} else if (back_kwp->kw_other != kwp) {
		return("(bad back wrapper other)");
	} else if (back_kwp->kw_size != kwp->kw_size) {
		return("(bad back wrapper size)");
	} else if (back_kwp->kw_caller != NULL) {
		return("(bad back wrapper caller)");
	} else if (back_kwp->kw_next != NULL) {
		return("(bad back wrapper next)");
	} else if (back_kwp->kw_prev != NULL) {
		return("(bad back wrapper prev)");
	} else {
		return("");
	}
}

/* ARGSUSED */
static void
idbg_cfsmem( __psint_t addr )
{
	struct km_wrap *kwp;
	char *sym;
	int offset;

	if (Cachefs_memlist) {
		for (kwp = Cachefs_memlist; kwp; kwp = kwp->kw_next) {
			sym = fetch_kname(kwp->kw_caller, (void *)&offset);
			qprintf("%p: caller %s+0x%x size %d req %d other %p next %p %s\n",
				kwp, sym, offset, kwp->kw_size, kwp->kw_req, kwp->kw_other,
				kwp->kw_next, valid_wrapper(kwp));
		}
	} else {
		qprintf( "CacheFS has no memory allocated\n." );
	}
}
#endif /* DEBUG */

void
cachefs_idbg_init()
{
#ifdef DEBUG
	bzero( &Cachefs_functrace, sizeof(Cachefs_functrace) );
	spinlock_init(&Cachefs_functrace.ct_lock, "cachefs func trace");
#endif /* DEBUG */

    /* print cnodes */
    idbg_addfunc( "cnode", idbg_prcnode );

    /* print cnode hash */
    idbg_addfunc( "chash", idbg_chash );

    /* print cachefs workq */
    idbg_addfunc( "cfswork", idbg_cfsworkq );

    /* print cachefs cachefscache structure */
    idbg_addfunc( "cfscach", idbg_cachefscache );

    /* print cachefs fscache structure */
    idbg_addfunc( "fscache", idbg_fscache );

    /* print fid structure */
    idbg_addfunc( "fid", idbg_fid );

	/* print dirent structure */
	idbg_addfunc( "dirent", idbg_dirent );

	/* print irix5_dirent structure */
	idbg_addfunc( "dirent5", idbg_dirent5 );

	/* print vattr structure */
	idbg_addfunc( "vattr", idbg_vattr );

	/* print cattr structure */
	idbg_addfunc( "cattr", idbg_cattr );

	/* print fileheader structure */
	idbg_addfunc( "filehdr", idbg_fileheader );

#ifdef DEBUG
	/* print function trace */
	idbg_addfunc( "cftrace", idbg_ftrace );

	/* print allocated memory list */
	idbg_addfunc( "cfsmem", idbg_cfsmem );

	idbg_addfunc( "lruchk", idbg_checklru );
	idbg_addfunc( "hdrcach", idbg_headercache );
#endif /* DEBUG */
}
