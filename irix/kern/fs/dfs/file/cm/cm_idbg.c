/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

#ident "$Id: cm_idbg.c,v 65.1 1998/09/22 20:32:22 gwehrman Exp $"

#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>

#include <sys/cred.h>
#include <dce/id_base.h>
#include <dcedfs/common_data.h>
#include "cm.h"
#include "cm_conn.h"
#include "cm_server.h"
#include "cm_idbg.h"


extern short tkm_initialized;
extern int dfs_initialized(void);
extern long cm_cacheFiles;
static int PrintedOne;			/* Flag to say we found entries */
static  char *cm_volstates[] = {
        "VL_SGIDOK",
        "VL_DEVOK",
        "",
        "",
        "VL_RO",
        "VL_RECHECK",
        "VL_BACKUP",
        "VL_LAZYREP",
        "VL_LOCAL",
        "VL_RESTORETOKENS",
        "VL_HISTORICAL",
        "VL_TSRCRASHMOVE",
        "VL_NEEDRPC",
        NULL
};


/*
 *	Print out sockaddr information.
 */
void
pr_sockaddr_in_ent (struct sockaddr_in  *saent,    /* Actual offset */
			int verboseLevel, char *spstrin)
{
   char *spstr;
   struct  in_addr *sin_addr=&saent->sin_addr;

   spstr = spstrin;
   if (saent) qprintf ("%sSOCKADDR_IN at 0x%x:", spstr, saent);
   else  qprintf ("%sSOCKADDR_IN: ", spstr);

   qprintf ("s_addr=%d.%d.%d.%d", 
      ((u_int)sin_addr->s_addr >> 24) & 0xFF,
      ((u_int)sin_addr->s_addr >> 16) & 0xFF,
      ((u_int)sin_addr->s_addr >>  8) & 0xFF,
       (u_int)sin_addr->s_addr        & 0xFF);

}

void print_afsfid(afsFid  *id, char *prefix)
{
    qprintf("%sFID: Cell/Vol=%lx.%lx.%lx.%lx Vnode=%#lx Unique=%#lx\n",
           prefix, AFS_hgethi(id->Cell), AFS_hgetlo(id->Cell),
           AFS_hgethi(id->Volume), AFS_hgetlo(id->Volume),
           id->Vnode, id->Unique);
}


/*
 *	Print out a lock_data struct
 */
void
pr_lock_data(
struct lock_data *lockdatap,
int		verboseLevel,
char		*spstr)
{
   u_int		wait_states, excl_locked, readers_reading;

   /* Strange things were happening with the casting... so we have this */
   wait_states		= lockdatap->wait_states;
   excl_locked		= lockdatap->excl_locked;
   readers_reading	= lockdatap->readers_reading;

   qprintf ("%sLOCK DATA: wait_states=%d excl_locked=%d readers_reading=%d\n", 
			spstr, wait_states, excl_locked, readers_reading);
#if defined(DEBUG)
   qprintf ("%sLOCK DATA: current or last lock owner 0x%x\n", spstr, lockdatap->lock_holder);
#endif
}

#if 0
/*
 *	Print out an sec_id_pac structure
 */
void
pr_sec_id_pac(addr, ent, verboseLevel, spstr)
int addr;
sec_id_pac_t *ent;
int             verboseLevel;
char            *spstr;
{
   char		buff[1024];

   if (addr)
      printf ("%sSEC_ID_PAC_T AT ADDR %#o:\n", spstr, addr);
   else
      printf ("%ssec_id_pac_t addr=0\n", spstr);

   if (addr != NULL) {
      strcat (spstr, "  ");
   
      printf ("%spac_type (%#o) %s\n", spstr,
         (ent->pac_type == sec_id_pac_format_v1) ? 
            "sec_id_pac_format_v1" : "*BAD TYPE*");
   
      /* Print the realm */
      printf ("%srealm.UUID: \n", spstr);
      strcat (spstr, "  ");
      pr_uuid_ent ( &ent->realm.uuid, verboseLevel, spstr);
      spstr[strlen(spstr)-2]='\0';
      printf ("%srealm.name=%#o\n", spstr, ent->realm.name);
   
      /* Print the principal */
      printf ("%sprincipal.UUID: \n", spstr);
      strcat (spstr, "  ");
      pr_uuid_ent ( &ent->principal.uuid, verboseLevel, spstr);
      spstr[strlen(spstr)-2]='\0';
      printf ("%sprincipal.name=%#o\n", spstr, ent->principal.name);
   
      /* Print the group */
      printf ("%sgroup.UUID: \n", spstr);
      strcat (spstr, "  ");
      pr_uuid_ent ( &ent->group.uuid, verboseLevel, spstr);
      spstr[strlen(spstr)-2]='\0';
      printf ("%sgroup.name=%#o\n", spstr, ent->group.name);
   
      printf ("%sgroups=%#o foreign_groups=%#o\n", spstr,
         ent->groups, ent->foreign_groups);
      spstr[strlen(spstr)-2]='\0';
   } 
}
   
/*
 *	Print out an xcred structure
 */
pr_xcred(addr, ent, verboseLevel, spstr)
int addr;
struct xcred *ent;
int             verboseLevel;
char            *spstr;
{
   struct cred cred;

   printf ("%sXCRED AT ADDR %#o:\n", spstr, addr);
   strcat (spstr, "  ");
   printf ("%slruq.next=%#o lruq.prev=%#o refcount=%#o\n", spstr,
      ent->lruq.next, ent->lruq.prev, ent->refcount);
   printf ("%schangeCount=%#o assocPag=%#o\n", spstr,
      ent->changeCount, ent->assocPag);

   printf ("%sflags (%#o)=%s\n", spstr, ent->flags,
      (ent->flags == 0) ? "*NONE* " : "",
      (ent->flags & XCRED_FLAGS_DELETED) ? "XCRED_FLAGS_DELETED " : "");

   printf ("%suflags (%#o)=%s\n", spstr, ent->uflags,
      (ent->uflags == 0) ? "*NONE* " : "",
      (ent->uflags & XCRED_UFLAGS_RESET) ? "XCRED_UFLAGS_RESET " : "",
      (ent->uflags & XCRED_UFLAGS_HASNFS) ? "XCRED_UFLAGS_HASNFS " : "");

   printf ("%spropListP=%#o\n", spstr, ent->propListP);

   if (ent->assocUCredP != NULL) {
      if (readmem (&cred, (int)ent->assocUCredP*NBPW, sizeof(cred)) 
	 != sizeof(cred)) {
         error ("Read error on cred entry\n");
      }
      else pr_cred((int)ent->assocUCredP, &cred, verboseLevel, spstr);
   } 
   else printf ("%scred=%#o\n", spstr, ent->assocUCredP);

#if 0
      pr_lock_data(0, &ent->lock, verboseLevel, spstr);
#endif
   spstr[strlen(spstr)-2]='\0';
}

/*
 *	Print out a ucred strcuture
 */
pr_cred(addr, ent, verboseLevel, spstr)
int addr;
struct cred *ent;
int             verboseLevel;
char            *spstr;
{
   qprintf ("%sUCRED AT ADDR %#o:\n", spstr, addr);
   strcat (spstr, "  ");
   qprintf ("%scr_ref=%d cr_uid=%d cr_gid=%d\n", spstr, 
      ent->cr_ref, ent->cr_uid, ent->cr_gid);
   qprintf ("%scr_sid=%d cr_ruid=%d \n", spstr, 
      ent->cr_sid, ent->cr_ruid);

   spstr[strlen(spstr)-2]='\0';
}
#endif

/*
 * 	Check to see if DFS has been initialized in the kernel
 *
 *	Returns TRUE of FALSE and prints message if not initialized.
 */
int
dfs_initialized(void)
{
   if (cm_shashTable == 0) {
      qprintf ("DFS does not appear to be configured into the kernel\n");
      return (0);
   }
   return(1);
}

static void
print_fid( struct fid *addr , char *spstr)
{
        fid_t *fidp;
        int i;

        if ( (long)addr < -1 ) {
                fidp = (fid_t *)addr;
                qprintf( "%sfid_len: %d\n", spstr, fidp->fid_len );
                qprintf( "%sfid_data: " , spstr);
                for ( i = 0; (i < fidp->fid_len) && (i < MAXFIDSZ); i++ ) {
                        qprintf( "0x%2x ", fidp->fid_data[i] );
                }
                qprintf( "\n" );
        } else {
                qprintf( "Illegal fid address 0x%x\n", addr );
        }
}


void
idbg_dcache(long addr, int verboseLevel)
{
   int 		addrset=0;
   char		spstr[40];


   spstr[0]='\0';

    /* Set default verbose level */

#if 0
    if (dfs_initialized() && tkm_initialized ) {
        print_volumes(addr, verboseLevel);
    }
#endif


   if (addr != 0) {
      addrset++;
   }

   PrintedOne = 0;
   if (addrset) {
      strcat (spstr, "   ");
      pr_cm_dcache_entry ((struct cm_dcache *)addr, 
		verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   } else {
      strcat (spstr, "   ");
      pr_all_cm_dcache_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!PrintedOne) {
      qprintf ("No active cm_dcache entries.\n");
   }
}

/*
 *	Print all cm_dcache entries
 */
void
pr_all_cm_dcache_entries (
int	verboseLevel,
char	*spstr)
{
   int count;
   struct cm_dcache *dcp;


   qprintf("Printing All Dcache Entries\n");
   for (count=0; count<cm_cacheFiles; count++) {
	if (dcp=cm_indexTable[count]) {
	 	strcat(spstr, "   ");
         	pr_cm_dcache_entry (dcp, verboseLevel, spstr);
         	spstr[strlen(spstr)-3]='\0';
	}
   }
   qprintf("Done with Dcache Entries\n");
   return;
}

/*
 *	Print all locked cm_dcache entries
 */
void
idbg_dcache_lock (void)
{
   int count;
   int blocksUsed=0;
   struct cm_dcache *dcp;

   qprintf("cm_cacheBlocks = %d, cm_blocksUsed = %d\n",
		cm_cacheBlocks, cm_blocksUsed);
   if (cm_cacheBlocks <= cm_blocksUsed) {
	qprintf("NO BLOCKS AVAILABLE\n");
   }


   for (count=0; count<cm_cacheFiles; count++) {
	dcp = cm_indexTable[count];
	if (dcp->llock.excl_locked & WRITE_LOCK) {
		qprintf("dcp 0x%x is locked WRITE, owner 0x%x recursive=%d\n",
				dcp, dcp->llock.lock_holder,
				dcp->llock.recursive_locks);
	}
	if (dcp->llock.readers_reading) {
		qprintf("dcp 0x%x is locked READ by %d readers\n",
				dcp, dcp->llock.readers_reading);
	}
   }
}
/*
 *	Print all cm_dcache entries
 */
void
idbg_dcache_list (void)
{
   int count;
   int blocksUsed=0;
   struct cm_dcache *dcp;

   qprintf("cm_cacheBlocks = %d, cm_blocksUsed = %d\n",
		cm_cacheBlocks, cm_blocksUsed);
   if (cm_cacheBlocks <= cm_blocksUsed) {
	qprintf("NO BLOCKS AVAILABLE\n");
   }
   qprintf("cm_indexTable has %d entries at 0x%x\n",
		cm_cacheFiles, cm_indexTable);
   qprintf("cm_indexFlags at 0x%x contains states as printed below\n",
		cm_indexFlags);


   for (count=0; count<cm_cacheFiles; count++) {
	qprintf("entry %d state=0x%x  ", 
		count, cm_indexFlags[count]);
	qprintf("cm_indexTable[%d] = 0x%x\n", 
		count, cm_indexTable[count]);
        if (dcp = cm_indexTable[count]) {
		blocksUsed += dcp->f.cacheBlocks;
	}
   }
}

/*
 *	Print a single cm_dcache entry
 */
void
pr_cm_dcache_entry (struct cm_dcache *addr, 
	int verboseLevel, char *spstr)
{

   /* If there is no fid, this is an inactive entry */
#if 0
   if ( (verboseLevel & VLVLMASK) < 3 &&
	!addr->f.fid.Cell.high && !addr->f.fid.Cell.low && 
	!addr->f.fid.Volume.high && !addr->f.fid.Volume.low && 
	!addr->f.fid.Vnode && !addr->f.fid.Unique) {
	return;
   }
#endif

   /* Mark we got one */
   if (!PrintedOne) ++PrintedOne;

   qprintf ("%sCM_DCACHE at addr=0x%x \n%slruq->next=0x%x lruq->prev=0x%x\n", 
      spstr, addr, spstr, addr->lruq.next, addr->lruq.prev);

   qprintf ("%srefCount=0x%x index(cachefile #)=%d (0x%x) validPos=0x%x,0x%x\n",
	spstr, addr->refCount, addr->index, addr->index,
	AFS_hgethi(addr->validPos), AFS_hgetlo(addr->validPos));

      pr_lock_data (&addr->llock, verboseLevel, spstr); 

   qprintf ("%sstates=0x%x ", spstr, addr->states);
   if (addr->states & DC_DFNEXTSTARTED)
      qprintf ("DC_DFNEXTSTARTED ");
   if (addr->states & DC_DFFETCHING)
      qprintf ("DC_DFFETCHING ");
   if (addr->states & DC_DFWAITING)
      qprintf ("DC_DFWAITING ");
   if (addr->states & DC_DFFETCHREQ)
      qprintf ("DC_DFFETCHREQ ");
   if (addr->states & DC_DFSTORING)
      qprintf ("DC_DFSTORING ");
   qprintf ("\n");

   strcat (spstr, "   ");
   pr_cm_fcache_entry ( (long)&addr->f, verboseLevel, spstr);
   spstr[strlen(spstr)-3]='\0';
   
   return;
}

void
idbg_fcache(long addr)
{
   char *spstr;
   int verboseLevel;
   spstr = "   ";
   verboseLevel=0;
   pr_cm_fcache_entry ( addr, verboseLevel, spstr);
}


/*
 *	Print a single cm_fcache entry
 */
void
pr_cm_fcache_entry (long addr, int verboseLevel, char *spstr)
{
   struct cm_fcache *fcacheent;
   fcacheent=(struct cm_fcache *)addr;
   qprintf ("%sCM_FCACHE at addr=0x%x\n", spstr, fcacheent);
   qprintf ("%shvNextp=%d hcNextp=%d\n", spstr, fcacheent->hvNextp, fcacheent->hcNextp);
   print_afsfid(&fcacheent->fid, spstr);
   qprintf ("%sDC_VHASH=0x%x DC_CHASH=0x%x\n", spstr,
	DC_VHASH(&fcacheent->fid), DC_CHASH(&fcacheent->fid, fcacheent->chunk));
   qprintf ("%sversion=0x%x,0x%x modTime=0x%x\n", spstr, 
      AFS_hgethi(fcacheent->versionNo), AFS_hgetlo(fcacheent->versionNo),
      &fcacheent->modTime);
   qprintf ("%stokenID=0x%x,0x%x, chunk=%#d (0x%x) \n", spstr, 
      AFS_hgethi(fcacheent->tokenID),  AFS_hgetlo(fcacheent->tokenID),
	fcacheent->chunk, fcacheent->chunk);
   print_fid(&fcacheent->handle, spstr);
   qprintf ("%schunkBytes=%d startDirty=%d endDirty=%d\n", spstr, 
      fcacheent->chunkBytes, fcacheent->startDirty, fcacheent->endDirty);
   qprintf ("%scacheBlocks=0x%x states=0x%x ", spstr, 
      fcacheent->cacheBlocks, fcacheent->states);

   if (fcacheent->states & DC_DONLINE)
      qprintf ("DC_DONLINE ");
   if (fcacheent->states & DC_DWRITING)
      qprintf ("DC_DWRITING ");
   qprintf ("\n");
}




typedef struct {
   char *name;
   int  val;
} val_name;

val_name        scache_text[] = {
	{ "SC_STATD",       1 },
	{ "SC_RO",          2 },
	{ "SC_MVALID",      4 },
	{ "SC_MOUNTPOINT",  0x40 },
	{ "SC_VOLROOT",     0x80 },
	{ "SC_RETURNOPEN",  0x100 },
	{ "SC_VDIR",        0x200 },
	{ "SC_CELLROOT",    0x400 },
	{ "SC_SPECDEV",     0x800 },
	{ "SC_STOREFAILED", 0x1000 },
	{ "SC_STORING",     0x2000 },
	{ "SC_FETCHING",    0x4000 },
	{ "SC_WAITING",     0x8000 },
	{ "SC_REVOKEWAIT",  0x10000 },
	{ "SC_HASPAGES",    0x20000 },
	{ "SC_MARKINGBAD",  0x80000 },
	{ "SC_BADSPECDEV",  0x100000 },
	{ "SC_STOREINVAL",  0x200000 },
	{ "SC_RETURNREF",   0x400000 }
};

static  char *volerrs[] = {
        "LOWEST",
        "DELETED",
        "BADDUMPOPCODE",
        "BADDUMP",
        "BADFTSOPSVECTOR",
        "REPDESTROY",
        "DAMAGED",
        "BADVOLOPSVECTOR",
        "OUTOFSERVICE",
        "CLONECLEAN",
        "DETACH",
        "UNLICENSED",
        "AGEOLD",
        "LCLMOUNT"
};


#define scache_states_count        sizeof(scache_text)/sizeof(val_name)

void
idbg_scache(long addr, int verboseLevel)
{
   int 		addrset=0;
   struct cm_scache  *scp;
   char		spstr[40];

   spstr[0]='\0';




   if (addr != 0) {
      addrset++;
   }

   PrintedOne = 0;

   if (addrset) {
      scp = (struct cm_scache *)addr;
      strcat (spstr, "   ");
      pr_cm_scache_entry (addr, scp, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   else {
      strcat (spstr, "   ");
      pr_all_cm_scache_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!PrintedOne) {
      qprintf("No active cm_scache entries.\n");
   }

   return;
}

/*
 *	Print all cm_scache entries
 */
void
pr_all_cm_scache_entries (int verboseLevel, char *spstr)
{
   struct cm_scache *hashp;
   int index;

   for (index=0; index < SC_HASHSIZE; index++) {
	for (hashp = cm_shashTable[index]; hashp; hashp = hashp->next) {
	    	pr_cm_scache_entry((long)hashp, hashp, verboseLevel, spstr);
       }
   }
   return;
}
/*
 *	Print all cm_scache addresses/vnodes that are locked
 */
void
idbg_scache_lock (long addrin)
{
   struct cm_scache *hashp;
   int index;

   for (index=0; index < SC_HASHSIZE; index++) {
	for (hashp = cm_shashTable[index]; hashp; hashp = hashp->next) {
		if (hashp->llock.excl_locked || hashp->llock.readers_reading
		|| hashp->rwl_owner || hashp->rl_count) {
			qprintf("scache=0x%x  vnode=0x%x v_count=%d\n", hashp, 
			hashp->vp, hashp->vp->v_count);
			if (hashp->llock.excl_locked) {
				qprintf(
				"   Write locked by 0x%x, recursive=%d, %d waiter\n",
					hashp->llock.lock_holder,
					hashp->llock.recursive_locks,
					hashp->llock.wait_states);
			}
			if (hashp->llock.readers_reading) {
				qprintf(
				"   Read locked by %d readers, %d waiter\n",
				hashp->llock.readers_reading,
				hashp->llock.wait_states); 
			}
			if (hashp->rwl_owner) {
				qprintf("   rwlock held write by 0x%x recursive=%d rl_count=%d address=0x%x\n",
				hashp->rwl_owner, hashp->rwl_count,
				hashp->rl_count, &hashp->rwlock);
			}
		}
       }
   }
}
/*
 *	Print all cm_scache addresses/vnodes
 */
void
idbg_scache_list (long addrin)
{
   struct cm_scache *hashp;
   int index;

   for (index=0; index < SC_HASHSIZE; index++) {
	for (hashp = cm_shashTable[index]; hashp; hashp = hashp->next) {
		qprintf("scache=0x%x  vnode=0x%x v_count=%d\n", hashp, 
		hashp->vp, hashp->vp->v_count);
       }
   }
}

/*
 *	Print a single cm_scache entry
 */
int
pr_cm_scache_entry (long addr, struct cm_scache *scp, 
		int verboseLevel, char *spstr)
{
   int i;

   /* Mark we got one */
   if (!PrintedOne) ++PrintedOne;

   qprintf("%sSCACHE addr=0x%x, Next: 0x%x\n", spstr, addr, scp->next);
   strcat (spstr, "   ");
   qprintf("%svnode=0x%x\n %slruq: next 0x%x prev 0x%x\n", spstr, scp->vp,
	spstr, scp->lruq.next, scp->lruq.prev);

   pr_lock_data(&scp->llock, verboseLevel, spstr);
   qprintf("%sllock *=0x%x, this is a dfs lock_data\n", spstr, &scp->llock);
   qprintf("%srwlock *=0x%x, this is an mrlock\n", spstr, &scp->rwlock);
   qprintf("%srwl_owner=0x%x  ", spstr, scp->rwl_owner);
   qprintf("rwl_count=%d  ", scp->rwl_count);
   qprintf("rl_count=%d  ", scp->rl_count);
   qprintf("rw_demote=%d\n", scp->rw_demote);
   print_afsfid(&scp->fid, spstr);
   qprintf("Check it against this\n");
   qprintf("%sFID: Cell/Vol=%u.%u.%u.%u\n%sVnode=%u  Unique=%u\n",
      spstr, AFS_hgethi(scp->fid.Cell), AFS_hgetlo(scp->fid.Cell), 
	AFS_hgethi(scp->fid.Volume), AFS_hgetlo(scp->fid.Volume), 
	spstr, scp->fid.Vnode, scp->fid.Unique);

   prcmattr(&scp->m, &scp->m, verboseLevel, spstr);

#if 0
   if (verboseLevel < 1) 
      goto done;
#endif

   if (scp->vdirList) {
      qprintf("%svdirList:\n", spstr);
      strcat (spstr, "   ");
      prvdirent(scp->vdirList, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   if (scp->parentVDIR) {
      qprintf("%sparentVDIR:\n", spstr);
      strcat (spstr, "   ");
      prvdirent(scp->parentVDIR, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   qprintf("%sopens=%d, writers=%d\n", spstr, scp->opens, scp->writers);
   qprintf("%sreaders=%u, shareds=%u\n", spstr, scp->readers, scp->shareds);
   qprintf("%sexclusives=%u, storeCount=%u, fetchCount=%u\n", spstr, scp->exclusives, 
      scp->storeCount, scp->fetchCount);
   qprintf("%skaExpiration=%d", spstr, scp->kaExpiration);
   qprintf("%sstoreFailTime=%d", spstr, scp->storeFailTime);
   qprintf("%sdirRevokes=%ld, modChunks=%ld, truncPos=%ld, devno=%ld\n",
      spstr, scp->dirRevokes, scp->modChunks, scp->truncPos,  scp->devno);
   if (scp->states) {
     qprintf("%sstates=", spstr);
     for (i = 0; i < scache_states_count; i++) {
        if (scp->states & scache_text[i].val) {
            qprintf(" %s", scache_text[i].name);
        }
     }
     qprintf("\n");
   } else {
      qprintf("%sstates=none\n", spstr);
   }
   qprintf("%sds: scanOffset=%ld, scanCookie=%ld, scanChunk=%ld\n",
      spstr, scp->ds.scanOffset, scp->ds.scanCookie, scp->ds.scanChunk);
   qprintf("%sanyAccess=%ld, asyncStatus=%d\n", spstr, scp->anyAccess, 
      scp->asyncStatus);

   pr_cm_tokenlist (&scp->tokenList, verboseLevel, spstr);

   qprintf ("%spfid.parentFidp=0x%x linkDatap=0x%x\n", spstr, 
      scp->pfid.parentFidp, scp->linkDatap);
   qprintf ("%svolp=0x%x\n", spstr, scp->volp);


   qprintf ("%swriterCredp=0x%x cookieList=0x%x\n", 
      spstr, scp->writerCredp, scp->cookieList);

   qprintf ("%scookieDV=%lu,%lu\n", spstr, AFS_hgethi(scp->cookieDV), 
		AFS_hgetlo(scp->cookieDV));
   if (scp->cookieList) {
      qprintf ("%sCookie List for Dirs:\n");
      strcat (spstr, "   ");
      prcookielist(scp->cookieList, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   qprintf ("%srandomACL=0x%x\n", spstr, scp->randomACL);
   qprintf ("%slockList.next=0x%x lockList.prev=0x%x\n", spstr, 
      scp->lockList.next, scp->lockList.prev);
#if 0
   /*  From Cray code...added this for MAC support */
   qprintf ("%swriterCredp=%#o writerUid=%#o writerPag=%#o cookieList=%#o\n", 
      spstr, scp->writerCredp, scp->writerUid, scp->writerPag, scp->cookieList);
#endif

done:
   spstr[strlen(spstr)-3]='\0';
   return(0);
}

/*
 *	Print out one cmattr structure
 */
void
prcmattr(struct cm_attrs *addr, struct cm_attrs *cmattrs, int verboseLevel, 
							char *spstr)
{
   unsigned long sec;
   
   if (addr) qprintf ("%sCM_ATTR at addr=0x%x:\n", spstr, addr);
   else      qprintf ("%sCM_ATTR at addr:\n", spstr);

   strcat (spstr, "   ");
   qprintf("%sModFlags=%ld\n", spstr, cmattrs->ModFlags);
   qprintf("%sDataVersion=(%u,%u)\n", spstr, AFS_hgethi(cmattrs->DataVersion),
      AFS_hgetlo(cmattrs->DataVersion));

   qprintf("%sLength=%lld, serverAccessTime=%lu\n", spstr, cmattrs->Length,
      cmattrs->serverAccessTime);
   sec = cmattrs->ModTime.sec;
   qprintf("%sModTime          = (0x%x,0x%x) %lu\n", spstr, sec, 
      cmattrs->ModTime.usec, sec);
   sec = cmattrs->ChangeTime.sec;
   qprintf("%sChangeTime       = (0x%x,0x%x) %lu\n", spstr, sec,
      cmattrs->ChangeTime.usec, sec);
   sec = cmattrs->AccessTime.sec;
   qprintf("%sAccessTime       = (0x%x,0x%x) %lu\n", spstr, sec,
      cmattrs->AccessTime.usec, sec);
   sec = cmattrs->ServerChangeTime.sec;
   qprintf("%sServerChangeTime = (0x%x,0x%x) %lu\n", spstr, sec,
      cmattrs->ServerChangeTime.usec, sec);

   qprintf("%sOwner=%ld, Group=%ld\n", spstr, cmattrs->Owner, cmattrs->Group);
   qprintf("%sBlocksUsed=%lu KBytes\n", spstr, cmattrs->BlocksUsed);
   qprintf("%sMode=0x%x, LinkCount=%u\n", spstr, cmattrs->Mode, cmattrs->LinkCount);
   spstr[strlen(spstr)-3]='\0';
}

/*
 *	Print out a cm_vdirent
 */
void
prvdirent(struct cm_vdirent *vdirpin, int verboseLevel, char *spstr)
{
   struct cm_vdirent *entry;

   entry=vdirpin;
   while (entry) {
      qprintf("%svdirp (0x%x) : inode=0x%x\n", spstr, entry, entry->inode);
      qprintf("%svdirp (0x%x) : inode=0x%x, states=%s%s%s%s\n",
         spstr, entry, entry->inode,
         (!entry->states) ? "none" : "",
         (entry->states & CM_VDIR_GOODNAME) ? "GOODNAME" : "",
         (entry->states & CM_VDIR_BADNAME) ? "BADNAME" : "",
         (entry->states & CM_VDIR_FIRSTINBLK) ? ",FIRSTINBLK" : "");
      qprintf("%sname=%c\n", spstr, entry->name);
      entry = entry->next;
   }
}

#if 0
/*
 *	Print cray lockinfo
 */
prlockinfo(addr, lk, verboseLevel, spstr)
int addr;
lockinfo_t *lk;
int verboseLevel;
char *spstr;
{
   char    syscallname[MAXSCNAME+1];
   struct  sysent sbuf;
   register int scall;
   scall = lk->hi_syscall;
   if (scall < -1 || scall >= v.v_sysent)
      strcpy (syscallname, "illegal");
   else if (scall == -1)
      strcpy (syscallname, "dumping-core");
   else if (scall == 0)
      strcpy (syscallname, "");
   else if (readmem((char *)&sbuf,
      SYM_VALUE(Sysent)+scall * sizeof(struct sysent),
      sizeof (struct sysent)) != sizeof (struct sysent))
      strcpy(syscallname, "unknown");
   else {
      strncpy(syscallname, sbuf.sy_name, MAXSCNAME);
      syscallname[MAXSCNAME] = '\0'; /* Force null termination */
   }
   qprintf("%slockinf[hi_proc]: %d\tlockinf[syscall]: %d<%-8.8s>\n",
      spstr, ((lk->hi_proc << 3) - SYM_VALUE(Proc))
      / sizeof(struct proc), scall, syscallname);
   qprintf("%slockinf[pid]: %d\tlockinf[hi_time]: %d", spstr,
      lk->hi_pid, lk->hi_time);
}
#endif

/*
 *	Print the token list from an scache entry
 */
void
prcookielist(
struct cm_cookieList *cookieList,	/* Kernel Address */
int verboseLevel,
char *spstr)
{
   struct cm_cookieList *ent;

   ent = cookieList;
   while (ent) {
      qprintf ("%sCOOKIE AT 0x%x: next=%#w chunk=0x%x cookie=0x%x\n", spstr, 
	 ent, ent->next, ent->chunk, ent->cookie);
      ent = ent->next;
   }
}

/*
 *	Print the token list from an scache entry
 */
void
pr_cm_tokenlist (struct squeue *tokenList, int verboseLevel, char *spstr)
{
#if 1
   return;
#else
   int	tloff;   			/* Offset of tokenlist entry */
   int  nchars;
   extern char *token_names[];
   int mask, cnt, i;

   tloff = (int)tokenList->next * NBPW;
   while (tloff && tloff != addr) {
      struct cm_tokenList tlent;
      if (readmem (&tlent, tloff, sizeof(tlent)) != sizeof(tlent)) {
	 error ("Read error on cm_tokenList entry at %#o\n", tloff/NBPW);
	 return(-1);
      }
      qprintf ("%sTOKEN LIST AT ADDR = %#o\n", spstr, tloff/NBPW);
      strcat (spstr, "   ");
      qprintf ("%sq.next=%#o q.prev=%#o refCount=%#d\n", spstr,
	 tlent.q.next, tlent.q.prev, tlent.refCount);
      qprintf ("%sflags=%#o ", spstr, tlent.flags);
      if (tlent.flags & CM_TOKENLIST_RETURNL) qprintf ("CM_TOKENLIST_RETURNL "); 
      if (tlent.flags & CM_TOKENLIST_RETURNO) qprintf ("CM_TOKENLIST_RETURNO "); 
      if (tlent.flags & CM_TOKENLIST_DELETED) qprintf ("CM_TOKENLIST_DELETED "); 
      qprintf ("\n");

      qprintf("%sLOCALFID: Vol=%lx.%lx  Vnode=%#lx  Unique=%#lx\n",
         spstr, tlent.tokenFid.Volume.high, tlent.tokenFid.Volume.low, 
	 tlent.tokenFid.Vnode, tlent.tokenFid.Unique);
      qprintf ("%stokenID=0x%x,,0x%x expires=%d", spstr,
         tlent.token.tokenID.high, tlent.token.tokenID.low,
	tlent.token.expirationTime == 01777777777777777777777 ? "never\n" :
	tlent.token.expirationTime);

	qprintf("%sByte Range Low=%#o,,%#o Byte Range Hi=%#o,,%#o\n", spstr,
		tlent.token.beginRange, tlent.token.beginRangeExt, 
		tlent.token.endRange, tlent.token.endRangeExt);
      qprintf ("%stoken types=%#04x", spstr, tlent.token.type.low);
      mask = tlent.token.type.low;

      cnt = strlen(spstr) + 20;
      for (i = 0; i < TKM_ALL_TOKENS; i++) {
	  if (mask & (1 << i)) {
	      cnt += strlen(token_names[i]) + 1;
		if (cnt < 80) {
		    qprintf(" %s", token_names[i]);
		} else {
		    cnt = strlen(spstr) + 20 + strlen(token_names[i]);
		    qprintf("\n%s%19.19s%s", spstr, " ", token_names[i]);
		}
	   }
     }
      qprintf ("\n");

      spstr[strlen(spstr)-3]='\0';

      tloff = (int)tlent.q.next * NBPW;
   }
#endif

}

#if 0

/* ********************************************************************
 *
 *   THE FOLLOWING ARE USED OUTSIDE OF DFS SPECIFIC CRASH COMMANDS
 *
 * ********************************************************************/

scache_dnlcpr(dnode)
int dnode;
{
        qprintf("  <dfs>   <%5d>   ",
                ((dnode<<3) - cm_shashTable) /
                sizeof(struct cm_scache *));

}

scache_dnlcpr2(dnode, name)
int dnode, name;
{
        qprintf("  <dfs>   <%5d>   ",
                ((dnode<<3) - cm_shashTable) /
                sizeof(struct cm_scache *), name );
}

scache_prmnt(c,root,maj,min)
int c, root, maj,min;
{
        qprintf("%4u dfs%6u%4d%4d", c,
                ( c ? ( root -
                cm_shashTable)/sizeof(struct cm_scache *) : 0 ), maj, min);
        return;
}

scache_slot1(addr)
int addr;
{
        return(cm_shashTable + addr * sizeof(struct cm_scache *));
}

scache_slot2(addr)
int addr;
{
        if ( addr >= cm_shashTable ) {
                return(( addr - cm_shashTable ) / sizeof( struct cm_scache *));
        }
}

#endif



/*
 *	Print all cm_server entries
 */
pr_all_cm_server_entries ( int verboseLevel, char *spstr)
{
   int		i;
   struct cm_server	*serverent;

   for (i=0 ; i < SR_NSERVERS ; i++) {
	serverent = cm_servers[i];
	while (serverent) {	/* Don't try empty entries */
	    pr_cm_server_entry (serverent, verboseLevel, spstr);
	    serverent = serverent->next;
	}
   }
   return(0);
}

/*
 *	Print a single cm_server entry
 */
void
pr_cm_server_entry (
struct cm_server  *serverent,
int	verboseLevel,
char	*spstr)
{

   struct cm_conn *connp;

   /* We may have come in through cm_conn */
   if (verboseLevel == 1) {
      goto connonly;
   }

   /* Mark we got on if we havn't already (for entry fm "tkm")*/
   if (!PrintedOne) ++PrintedOne;

   qprintf ("%sCM_SERVER=0x%x principal=%s\n", spstr,
      serverent, serverent->principal);

   strcat (spstr, "  ");
   qprintf ("%snext=0x%x nextPPL=0x%x\n", spstr,
      serverent->next, serverent->nextPPL);
   
   qprintf ("%scellp=0x%x\n", spstr, serverent->cellp);
   qprintf ("%sasyncGrantsp=0x%x\n", spstr, serverent->asyncGrantsp);
#if 0
   pr_sockaddr_in_ent (NULL, &serverent->serverAddr, verboseLevel, spstr);
#endif
   qprintf ("%stokenCount=%d returnList: 0x%x\n",
      spstr, serverent->tokenCount, serverent->returnList);

      strcat (spstr, "   ");
      pr_lock_data (&serverent->lock, verboseLevel, spstr); 
      spstr[strlen(spstr)-3]='\0';

   qprintf ("%slastCall: 0x%x   lastAuthenticatedCall: 0x%x\n", spstr,
      serverent->lastCall, serverent->lastAuthenticatedCall);

   qprintf ("%savgRtt=%d   downTime: 0x%x   serverEpoch: 0x%x\n", spstr,
      serverent->avgRtt, serverent->downTime, serverent->serverEpoch);

   qprintf ("%shostLifeTime=%d origHostLifeTime=%d deadServerTime=%d refCount=%d\n",
	spstr, serverent->hostLifeTime, serverent->origHostLifeTime,
	serverent->deadServerTime, serverent->refCount);

   qprintf ("%sminAuthn=%d ", spstr, serverent->minAuthn);
   switch (serverent->minAuthn) {
	case rpc_c_protect_level_default:     qprintf ("DEFAULT   "); break;
	case rpc_c_protect_level_none:        qprintf ("NONE      "); break;
	case rpc_c_protect_level_connect:     qprintf ("CONNECT   "); break;
	case rpc_c_protect_level_call:        qprintf ("CALL      "); break;
	case rpc_c_protect_level_pkt:         qprintf ("PACKET    "); break;
	case rpc_c_protect_level_pkt_integ:   qprintf ("INTEGRITY "); break;
	case rpc_c_protect_level_pkt_privacy: qprintf ("PRIVACY   "); break;
	default:                              qprintf ("          ");
   }
   qprintf ("maxAuthn=%d ", serverent->maxAuthn);
   switch (serverent->maxAuthn) {
	case rpc_c_protect_level_default:     qprintf ("DEFAULT   "); break;
	case rpc_c_protect_level_none:        qprintf ("NONE      "); break;
	case rpc_c_protect_level_connect:     qprintf ("CONNECT   "); break;
	case rpc_c_protect_level_call:        qprintf ("CALL      "); break;
	case rpc_c_protect_level_pkt:         qprintf ("PACKET    "); break;
	case rpc_c_protect_level_pkt_integ:   qprintf ("INTEGRITY "); break;
	case rpc_c_protect_level_pkt_privacy: qprintf ("PRIVACY   "); break;
	default:                              qprintf ("          ");
   }
   qprintf ("maxSuppAuthn=%d ", serverent->maxSuppAuthn);
   switch (serverent->maxSuppAuthn) {
	case rpc_c_protect_level_default:     qprintf ("DEFAULT  \n"); break;
	case rpc_c_protect_level_none:        qprintf ("NONE     \n"); break;
	case rpc_c_protect_level_connect:     qprintf ("CONNECT  \n"); break;
	case rpc_c_protect_level_call:        qprintf ("CALL     \n"); break;
	case rpc_c_protect_level_pkt:         qprintf ("PACKET   \n"); break;
	case rpc_c_protect_level_pkt_integ:   qprintf ("INTEGRITY\n"); break;
	case rpc_c_protect_level_pkt_privacy: qprintf ("PRIVACY  \n"); break;
	default:                              qprintf ("         \n");
   }

   qprintf ("%sstates=0x%x ", spstr, serverent->states);
   if (serverent->states & SR_DOWN) qprintf (" SERVER_DWN");
   if (serverent->states & SR_TSR) qprintf  (" TSR_MODE");
   if (serverent->states & SR_HASTOKENS) qprintf  (" HAS_TKNS");
   if (serverent->states & SR_NEWSTYLESERVER) qprintf  (" NEWSTYLE");
   if (serverent->states & SR_DOINGSETCONTEXT) qprintf  (" DOING_SETCTXT");
   if (serverent->states & SR_RPC_BULKSTAT) qprintf  (" RPC_BULKSTAT");
   qprintf ("\n");

   qprintf ("%ssType=0x%x ", spstr, serverent->sType);
   switch (serverent->sType) {
   case SRT_FL:
      qprintf ("FILESERVER\n");
      break;
   case SRT_FX:
      qprintf ("FILE EXPORTER\n");
      break;
   case SRT_REP:
      qprintf ("REP SERVER\n");
      break;
   default:
      qprintf ("BAD OR UNKNOWN TYPE\n");
      break;
   }

   qprintf ("%smaxFileSize: 0x%x  maxFileParm: 0x%x  ",
	spstr, serverent->maxFileSize, serverent->maxFileParm);
   if (serverent->supports64bit)
	qprintf("SUPPORTS64BIT\n");
   else
	qprintf("\n");

   qprintf ("%sserver ", spstr);
   pr_uuid_ent(&serverent->serverUUID, verboseLevel, "");
   qprintf ("%sifspec ", spstr);
   pr_uuid_ent(&serverent->objId, verboseLevel, "");

#if 0
   qprintf ("%sMLS level: %d, min/max: %d/%d\n", spstr,
        serverent->act_label.lt_level,
        serverent->min_label.lt_level,
        serverent->max_label.lt_level);
   qprintf ("%scompartment: %#o, valcmp: %#o\n", spstr,
        serverent->act_label.lt_compart,
        serverent->max_label.lt_compart);
#endif
   spstr[strlen(spstr)-2]='\0';

connonly:
   if (serverent->connsp) {
      for (connp = serverent->connsp; connp; connp = connp->next) {
            strcat (spstr, "   ");
            pr_cm_conn_entry (connp, verboseLevel, spstr);
            spstr[strlen(spstr)-3]='\0';
      }
   }

   return;
}

/*
 *	Print a single cm_conn entry
 */
void
pr_cm_conn_entry ( struct cm_conn  *connent, int verboseLevel, char *spstr)
{


   /* Mark we got one */
   if (!PrintedOne) ++PrintedOne;

   qprintf ("%sCM_CONN at addr=0x%x next=0x%x\n", spstr, 
				connent, connent->next);
   strcat (spstr, "  ");

   qprintf ("%sauth[pag, uid]=0x%x,0x%x\n", spstr, 
	connent->authIdentity[0], connent->authIdentity[1]);
   qprintf ("%sRPC Handle=0x%x cm_server=0x%x\n%ssvcID=0x%x\n", spstr, 
	connent->connp, connent->serverp, spstr, connent->service);

      strcat (spstr, "   ");
      pr_lock_data (&connent->lock, verboseLevel, spstr); 
      spstr[strlen(spstr)-3]='\0';

   qprintf ("%slifeTime=0x%x refCount=0x%x callCount=0x%x\n", spstr, 
	connent->lifeTime, connent->refCount, connent->callCount);
   qprintf ("%sstates=0x%x ", spstr, connent->states);
   if (connent->states & CN_FORCECONN)
      qprintf ("FORCECONN ");
   if (connent->states & CN_INPROGRESS)
      qprintf ("INPROGRESS ");
   if (connent->states & CN_EXPIRED)
      qprintf ("EXPIRED ");
   if (connent->states & CN_SETCONTEXT)
      qprintf ("SETCONTEXT ");
   if (connent->states & CN_CALLCOUNTWAIT)
      qprintf ("CALLCOUNTWAIT ");
   qprintf ("\n");

   spstr[strlen(spstr)-2]='\0';
   return;
}

void
idbg_cm_conn(__psint_t addr)
{
   int 		addrset=0;
   int		verboseLevel=1;
   char		spstr[40];

   spstr[0]='\0';


   if (addr != 0) {
      addrset++;
   }

   PrintedOne = 0;
   if (addrset) {
      strcat (spstr, "   ");
      pr_cm_conn_entry ((struct cm_conn *)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   } else {
      strcat (spstr, "   ");
      pr_all_cm_server_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!PrintedOne) {
      qprintf ("No active cm_conn entries.\n");
   }
}

void
idbg_cm_servers(__psint_t addr)
{
   int 		addrset=0;
   int		verboseLevel=0;
   char		spstr[40];

   spstr[0]='\0';


   if (addr != 0) {
      addrset++;
   }

   PrintedOne = 0;
   if (addrset) {
      strcat (spstr, "   ");
      pr_cm_server_entry ((struct cm_server *)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   else {
      strcat (spstr, "   ");
      pr_all_cm_server_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!PrintedOne) {
      qprintf ("No active cm_server entries.\n");
   }
}

/*
 *	Print a single cm_volume entry
 */
void
pr_cm_volume_entry (
struct cm_volume  *volumeent,
int	verboseLevel,
char	*spstr)
{

   int i, mask;

   /* Mark we got on if we havn't already */
   if (!PrintedOne) ++PrintedOne;

   qprintf ("%sCM_VOLUME=0x%x next=0x%x\n", spstr,
      volumeent , volumeent->next);
   
   strcat (spstr, "  ");
   qprintf ("%scellp=0x%x\n", spstr, volumeent->cellp);

      strcat (spstr, "   ");
      pr_lock_data (&volumeent->lock, verboseLevel, spstr); 
      spstr[strlen(spstr)-3]='\0';

   qprintf ("%svolume ID=0x%x,0x%x\n", spstr, AFS_hgethi(volumeent->volume),
	AFS_hgetlo(volumeent->volume));
   qprintf("%svolume name =", spstr);
   if (volumeent->volnamep) {
	qprintf("%s\n", volumeent->volnamep);
   } else {
	qprintf("NULL\n");
   }
   for (i=0; i < CM_VL_MAX_HOSTS; i++) {
	if (volumeent->serverHost[i]){
		qprintf("%sserverHost[%d]=0x%x", spstr, 
			i, volumeent->serverHost[i]);
		if (volumeent->perSrvReason[i]) {
			qprintf(" perSrvReason=0x%x timeBad=0x%x", 
				volumeent->perSrvReason[i],
				volumeent->timeBad[i]);
		} 
		if (volumeent->repHosts[i]){
			qprintf(" repHosts=0x%x\n", volumeent->repHosts[i]);
		} else {
			qprintf("\n");
		}
	}
   }
   qprintf("%s", spstr);
   print_afsfid(&volumeent->mtpoint, "mtpoint  ");
   qprintf("%s", spstr);
   print_afsfid(&volumeent->rootFid, "rootFid  ");
   qprintf("%s", spstr);
   print_afsfid(&volumeent->dotdot, "dotdot  ");

   qprintf("%saccessTime:    %d\n", spstr, volumeent->accessTime);
   qprintf("%slastProbeTime: %d\n", spstr, volumeent->lastProbeTime);
   qprintf("%slastRepTry:    %d\n", spstr, volumeent->lastRepTry);
   qprintf("%soutstandingRPC=%d refCount=%d states=0x%x ", spstr,
           volumeent->outstandingRPC, volumeent->refCount,
		volumeent->states);

    mask = volumeent->states;
    for (i = 0; cm_volstates[i]; i++) {
        if (mask & (1 << i)) {
            qprintf(" %s", cm_volstates[i]);
        }
    }
    qprintf("\n");



   qprintf("%shostGen=%d, lastIdx=%d\n", spstr, volumeent->hostGen,
	volumeent->lastIdx);
   qprintf("%slastIdxhostGen=%d, lastIdxAddrGen=%d, lastIdxTimeout=%d\n", 
	spstr, volumeent->lastIdxHostGen, volumeent->lastIdxAddrGen,
	volumeent->lastIdxTimeout);

   qprintf ("%sRO volume ID=0x%x,0x%x\n", spstr, AFS_hgethi(volumeent->roVol),
	AFS_hgetlo(volumeent->roVol));
   qprintf ("%sbackup volume ID=0x%x,0x%x\n", spstr, AFS_hgethi(volumeent->backVol),
	AFS_hgetlo(volumeent->backVol));
   qprintf ("%sRW volume ID=0x%x,0x%x\n", spstr, AFS_hgethi(volumeent->rwVol),
	AFS_hgetlo(volumeent->rwVol));


   spstr[strlen(spstr)-2]='\0';

   return;
}

/*
 *	Print all cm_server entries
 */
pr_all_cm_volume_entries ( int verboseLevel, char *spstr)
{
   int		i;

   for (i=0 ; i < VL_NVOLS ; i++) {
	if (!cm_volumes[i]) continue;	/* Don't try empty entries */
        strcat (spstr, "   ");
        pr_cm_volume_entry (cm_volumes[i], verboseLevel, spstr);
        spstr[strlen(spstr)-3]='\0';
   }
   return(0);
}

void
idbg_cm_volumes(__psint_t addr)
{
   int 		addrset=0;
   int		verboseLevel=0;
   char		spstr[40];

   spstr[0]='\0';


   if (addr != 0) {
      addrset++;
   }

   PrintedOne = 0;
   if (addrset) {
      strcat (spstr, "   ");
      pr_cm_volume_entry ((struct cm_volume *)addr, verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }
   else {
      strcat (spstr, "   ");
      pr_all_cm_volume_entries(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   if (!PrintedOne) {
      qprintf ("No active cm_volume entries.\n");
   }
}
