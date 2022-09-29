#ident "$Revision: 1.6 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <assert.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif

#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_attr_leaf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_bit.h>

#include <string.h>
#include <bstring.h>
#include <sys/acl.h>
#if VERS >= V_64
#include <sys/mac.h>
#endif
#include <sys/mac_label.h>
#include <sys/capability.h>

#include "globals.h"
#include "err_protos.h"
#include "dir.h"
#include "dinode.h"
#include "bmap.h"

#if VERS < V_64
/*
 * taken from IRIX 6.5 sys/capability.h, revision 1.15
 */
/*
 * Data types for capability sets.
 * capabilities were called privileges prior to P1003.6D14
 *
 * XFS extended attribute names
 */
#define SGI_CAP_FILE		"SGI_CAP_FILE"
#define SGI_CAP_PROCESS		"SGI_CAP_PROCESS"
#define SGI_CAP_PROCESS_FLAGS	"SGI_CAP_PROCESS_FLAGS"
#define SGI_CAP_REQUEST		"SGI_CAP_REQUEST"
#define SGI_CAP_SURRENDER	"SGI_CAP_SURRENDER"
#define SGI_CAP_DISABLED	"SGI_CAP_DISABLED"
#define SGI_CAP_SUPERUSER	"SGI_CAP_SUPERUSER"
#define SGI_CAP_NO_SUPERUSER	"SGI_CAP_NO_SUPERUSER"

#define SGI_CAP_FILE_SIZE	(sizeof (SGI_CAP_FILE) - 1)
#define SGI_CAP_PROCESS_SIZE	(sizeof (SGI_CAP_PROCESS) - 1)
#define SGI_CAP_PROCESS_FLAGS_SIZE	(sizeof (SGI_CAP_PROCESS_FLAGS) - 1)
#define SGI_CAP_REQUEST_SIZE	(sizeof (SGI_CAP_REQUEST) - 1)
#define SGI_CAP_SURRENDER_SIZE	(sizeof (SGI_CAP_SURRENDER) - 1)

/*
 * taken from IRIX 6.5 sys/mac_label.h, revision 1.40
 */
/*
 * XFS extended attribute names
 */
#define	SGI_BI_FILE	"SGI_BI_FILE"
#define	SGI_BI_IP	"SGI_BI_IP"
#define	SGI_BI_NOPLANG	"SGI_BI_NOPLANG"
#define	SGI_BI_PROCESS	"SGI_BI_PROCESS"
#define	SGI_BLS_FILE	"SGI_BLS_FILE"
#define	SGI_BLS_IP	"SGI_BLS_IP"
#define	SGI_BLS_NOPLANG	"SGI_BLS_NOPLANG"
#define	SGI_BLS_PROCESS	"SGI_BLS_PROCESS"
#define	SGI_MAC_FILE	"SGI_MAC_FILE"
#define	SGI_MAC_IP	"SGI_MAC_IP"
#define	SGI_MAC_NOPLANG	"SGI_MAC_NOPLANG"
#define	SGI_MAC_PROCESS	"SGI_MAC_PROCESS"
#define	SGI_MAC_POINTER	"SGI_MAC_POINTER"

/*
 * XFS extended attribute name lengths, not including trailing \0
 */
#define SGI_BI_FILE_SIZE	(sizeof (SGI_BI_FILE) - 1)
#define SGI_BI_IP_SIZE		(sizeof (SGI_BI_IP) - 1)
#define SGI_BI_NOPLANG_SIZE	(sizeof (SGI_BI_NOPLANG) - 1)
#define SGI_BI_PROCESS_SIZE	(sizeof (SGI_BI_PROCESS) - 1)
#define SGI_BLS_FILE_SIZE	(sizeof (SGI_BLS_FILE) - 1)
#define SGI_BLS_IP_SIZE		(sizeof (SGI_BLS_IP) - 1)
#define SGI_BLS_NOPLANG_SIZE	(sizeof (SGI_BLS_NOPLANG) - 1)
#define SGI_BLS_PROCESS_SIZE	(sizeof (SGI_BLS_PROCESS) - 1)
#define SGI_MAC_FILE_SIZE	(sizeof (SGI_MAC_FILE) - 1)
#define SGI_MAC_IP_SIZE		(sizeof (SGI_MAC_IP) - 1)
#define SGI_MAC_NOPLANG_SIZE	(sizeof (SGI_MAC_NOPLANG) - 1)
#define SGI_MAC_PROCESS_SIZE	(sizeof (SGI_MAC_PROCESS) - 1)
#define SGI_MAC_POINTER_SIZE	(sizeof (SGI_MAC_POINTER) - 1)

/*
 * XFS extended attribute name indexes.
 * Once a MAC attribute name has been looked up, the index can be used
 * thereafter.
 */
#define SGI_BI_FILE_INDEX	0
#define SGI_BI_IP_INDEX		1
#define SGI_BI_NOPLANG_INDEX	2
#define SGI_BI_PROCESS_INDEX	3
#define SGI_BLS_FILE_INDEX	4
#define SGI_BLS_IP_INDEX	5
#define SGI_BLS_NOPLANG_INDEX	6
#define SGI_BLS_PROCESS_INDEX	7
#define SGI_MAC_FILE_INDEX	8
#define SGI_MAC_IP_INDEX	9
#define SGI_MAC_NOPLANG_INDEX	10
#define SGI_MAC_PROCESS_INDEX	11
#define SGI_MAC_POINTER_INDEX	12

#define MAC_ATTR_LIST_MAX	12

/*
 * taken from IRIX 6.5 sys/acl.h, revision 1.9
 */
/*
 * Data types for Access Control Lists (ACLs)
 */
#define SGI_ACL_FILE	"SGI_ACL_FILE"
#define SGI_ACL_DEFAULT	"SGI_ACL_DEFAULT"

#define SGI_ACL_FILE_SIZE	12
#define SGI_ACL_DEFAULT_SIZE	15
#endif

/*
* For attribute repair, there are 3 formats to worry about. First, is 
* shortform attributes which reside in the inode. Second is the leaf
* form, and lastly the btree. Much of this models after the directory
* structure so code resembles the directory repair cases. 
* For shortform case, if an attribute looks corrupt, it is removed.
* If that leaves the shortform down to 0 attributes, it's okay and 
* will appear to just have a null attribute fork. Some checks are done
* for validity of the value field based on what the security needs are.
* Calls will be made out to mac_valid or acl_valid libc libraries if
* the security attributes exist. They will be cleared if invalid. No
* other values will be checked. The DMF folks do not have current
* requirements, but may in the future.
*
* For leaf block attributes, it requires more processing. One sticky
* point is that the attributes can be local (within the leaf) or 
* remote (outside the leaf in other blocks). Thinking of local only
* if you get a bad attribute, and want to delete just one, its a-okay
* if it remains large enough to still be a leaf block attribute. Otherwise,
* it may have to be converted to shortform. How to convert this and when
* is an issue. This call is happening in Phase3. Phase5 will capture empty
* blocks, but Phase6 allows you to use the simulation library which knows
* how to handle attributes in the kernel for converting formats. What we
* could do is mark an attribute to be cleared now, but in phase6 somehow
* have it cleared for real and then the format changed to shortform if
* applicable. Since this requires more work than I anticipate can be
* accomplished for the next release, we will instead just say any bad
* attribute in the leaf block will make the entire attribute fork be
* cleared. The simplest way to do that is to ignore the leaf format, and
* call clear_dinode_attr to just make a shortform attribute fork with
* zero entries. 
*
* Another issue with handling repair on leaf attributes is the remote
* blocks. To make sure that they look good and are not used multiple times
* by the attribute fork, some mechanism to keep track of all them is necessary.
* Do this in the future, time permitting. For now, note that there is no
* check for remote blocks and their allocations.
*
* For btree formatted attributes, the model can follow directories. That
* would mean go down the tree to the leftmost leaf. From there moving down
* the links and processing each. They would call back up the tree, to verify
* that the tree structure is okay. Any problems will result in the attribute
* fork being emptied and put in shortform format.
*/

/* this routine just checks what security needs are for attribute values */
/* only called when root flag is set, otherwise these names could exist in */
/* in user attribute land without a conflict. */
/* if value is non-zero, then a remote attribute is being passed in */

int valuecheck(char *namevalue, char *value, int namelen, int valuelen)
{
	/* for proper alignment issues, get the structs and bcopy the values */
	mac_label macl;
	struct acl thisacl;
	void *valuep;
	int clearit = 0;
#if VERS < V_64
	long dentries = 0;
#endif

	if ((strncmp(namevalue, SGI_ACL_FILE, SGI_ACL_FILE_SIZE) == 0) || 
			(strncmp(namevalue, SGI_ACL_DEFAULT, 
				SGI_ACL_DEFAULT_SIZE) == 0)) {
		if (value == NULL) {	
			bzero(&thisacl, sizeof(struct acl));
			bcopy(namevalue+namelen, &thisacl, valuelen);
			valuep = &thisacl;
		} else
			valuep = value;

#if VERS >= V_64
		if (acl_valid((struct acl *) valuep) != 0)
#else
		if (acl_valid(&((struct acl *)valuep)->acl_entry[0],
				((struct acl *)valuep)->acl_cnt,
			      	1, &dentries) != 0)
#endif
		{ /* 0 means valid */
			clearit = 1;
			do_warn("entry contains illegal value in attribute named SGI_ACL_FILE or SGI_ACL_DEFAULT\n");
		}
	} else if (strncmp(namevalue, SGI_MAC_FILE, SGI_MAC_FILE_SIZE) == 0) {
		if (value == NULL) {
			bzero(&macl, sizeof(mac_label));
			bcopy(namevalue+namelen, &macl, valuelen);
			valuep = &macl;
		} else 
			valuep = value;

#if VERS >= V_64
		if (mac_valid((mac_label *) valuep) != 1) /* 1 means valid */
#else
		if (mac_invalid((mac_label *) valuep) != 0) /* 0 means valid */
#endif
		{
			 /*
			 *if sysconf says MAC enabled, 
			 *	temp = mac_from_text("msenhigh/mintlow", NULL)
			 *	copy it to value, update valuelen, totsize
			 *	This causes pushing up or down of all following
			 *	attributes, forcing a attribute format change!!
			 * else clearit = 1;
			 */
			clearit = 1;
			do_warn("entry contains illegal value in attribute named SGI_MAC_LABEL\n");
		}
	} else if (strncmp(namevalue, SGI_CAP_FILE, SGI_CAP_FILE_SIZE) == 0) {
		if ( valuelen != sizeof(cap_set_t)) {
			clearit = 1;
			do_warn("entry contains illegal value in attribute named SGI_CAP_FILE\n");
		}
	}

	return(clearit);
}


/*
 * this routine validates the attributes in shortform format.
 * a non-zero return repair value means certain attributes are bogus
 * and were cleared if possible. Warnings do not generate error conditions
 * if you cannot modify the structures. repair is set to 1, if anything
 * was fixed.
 */
int
process_shortform_attr(
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	int 		*repair)	
{
	xfs_attr_shortform_t	*asf;
	xfs_attr_sf_entry_t	*currententry, *nextentry, *tempentry;
	int			i, junkit;
	int			currentsize, remainingspace;
	
	*repair = 0;

	asf = (xfs_attr_shortform_t *) XFS_DFORK_APTR(dip);

	/* Assumption: hdr.totsize is less than a leaf block and was checked
	 * by lclinode for valid sizes. Check the count though.	
	*/
	if (asf->hdr.count == 0) 
		/* then the total size should just be the header length */
		if (asf->hdr.totsize != sizeof(xfs_attr_sf_hdr_t))
			/* whoops there's a discrepancy. Clear the hdr */
			if (!no_modify) {
				do_warn("there are no attributes in the fork for inode %llu \n", ino);
				asf->hdr.totsize = sizeof(xfs_attr_sf_hdr_t);
				*repair = 1;
				return(1); 	
			} else {
				do_warn("would junk the attribute fork since the count is 0 for inode %llu\n",ino);
				return(1);
			}
		
	currentsize = sizeof(xfs_attr_sf_hdr_t); 
	remainingspace = asf->hdr.totsize - currentsize;
	nextentry = &asf->list[0];
	for (i = 0; i < asf->hdr.count; i++)  {
		currententry = nextentry;
		junkit = 0;

		/* don't go off the end if the hdr.count was off */
		if ((currentsize + (sizeof(xfs_attr_sf_entry_t) - 1)) > 
				asf->hdr.totsize)
			break; /* get out and reset count and totSize */

		/* if the namelen is 0, can't get to the rest of the entries */
		if (currententry->namelen == 0) {
			do_warn("zero length name entry in attribute fork, ");
			if (!no_modify) {
				do_warn("truncating attributes for inode %llu to %d \n", ino, i);
				*repair = 1;
				break; 	/* and then update hdr fields */
			} else {
				do_warn("would truncate attributes for inode %llu to %d \n", ino, i);
				break;
			}
		} else {
			/* It's okay to have a 0 length valuelen, but do a
			 * rough check to make sure we haven't gone outside of
			 * totsize.
			 */
			if ((remainingspace < currententry->namelen) ||
				((remainingspace - currententry->namelen)
					  < currententry->valuelen)) {
				do_warn("name or value attribute lengths are too large, \n");
				if (!no_modify) {
					do_warn(" truncating attributes for inode %llu to %d \n", ino, i);
					*repair = 1; 
					break; /* and then update hdr fields */
				} else {
					do_warn(" would truncate attributes for inode %llu to %d \n", ino, i);	
					break;
				}	
			}
		}
	
		/* namecheck checks for / and null terminated for file names. 
		 * attributes names currently follow the same rules.
		*/
		if (namecheck((char *)&currententry->nameval[0], 
				currententry->namelen))  {
			do_warn("entry contains illegal character in shortform attribute name\n");
			junkit = 1;
		}

		if (currententry->flags & XFS_ATTR_INCOMPLETE) {
			do_warn("entry has INCOMPLETE flag on in shortform attribute\n");
			junkit = 1;
		}

		/* Only check values for root security attributes */
		if (currententry->flags & XFS_ATTR_ROOT) 
		       junkit = valuecheck((char *)&currententry->nameval[0], NULL, 
				currententry->namelen, currententry->valuelen);

		remainingspace = remainingspace - 
				XFS_ATTR_SF_ENTSIZE(currententry);

		if (junkit) 
			if (!no_modify) {
				/* get rid of only this entry */
				do_warn("removing attribute entry %d for inode %llu \n", i, ino);
				tempentry = (xfs_attr_sf_entry_t *)
					((__psint_t) currententry +
					 XFS_ATTR_SF_ENTSIZE(currententry));
				memmove(currententry,tempentry,remainingspace);
				asf->hdr.count--;
				i--; /* no worries, it will wrap back to 0 */
				*repair = 1;
				continue; /* go back up now */
			} else 
				do_warn("would remove attribute entry %d for inode %llu \n", i, ino);

		/* Let's get ready for the next entry... */
		nextentry = (xfs_attr_sf_entry_t *)
			 ((__psint_t) nextentry +
			 XFS_ATTR_SF_ENTSIZE(currententry));
		currentsize = currentsize + XFS_ATTR_SF_ENTSIZE(currententry);
	
		} /* end the loop */

	
	if (asf->hdr.count != i)  {
		if (no_modify)  {
			do_warn("would have corrected attribute entry count in inode %llu from %d to %d\n",
				ino,asf->hdr.count,i);
		} else  {
			do_warn("corrected attribute entry count in inode %llu, was %d, now %d\n",
				ino,asf->hdr.count,i);
			asf->hdr.count = i;
			*repair = 1;
		}
	}
	
	/* ASSUMPTION: currentsize <= totsize */
	if (asf->hdr.totsize != currentsize)  {
		if (no_modify)  {
			do_warn("would have corrected attribute totsize in inode %llu from %d to %d\n",
				ino, asf->hdr.totsize, currentsize);
		} else  {
			do_warn("corrected attribute entry totsize in inode %llu, was %d, now %d\n",
				ino, asf->hdr.totsize, currentsize);
			asf->hdr.totsize = currentsize;
			*repair = 1;
		}
	}

	return(*repair);
}

/* This routine brings in blocks from disk one by one and assembles them
 * in the value buffer. If get_bmapi gets smarter later to return an extent
 * or list of extents, that would be great. For now, we don't expect too
 * many blocks per remote value, so one by one is sufficient.
*/
static int
rmtval_get(xfs_mount_t *mp, xfs_ino_t ino, blkmap_t *blkmap,
		xfs_dablk_t blocknum, int valuelen, char* value)
{
	xfs_dfsbno_t	bno;
	buf_t		*bp;
	int		clearit = 0, i = 0, length = 0, amountdone = 0;
	
	bp = NULL;
	/* ASSUMPTION: valuelen is a valid number, so use it for looping */
	/* Note that valuelen is not a multiple of blocksize */  
	while (amountdone < valuelen) {
		bno = blkmap_get(blkmap, blocknum + i);
		if (bno == NULLDFSBNO) {
			do_warn("remote block for attributes of inode %llu is missing\n",ino);
			clearit = 1;
			break;
		}
		bp = read_buf(mp->m_dev, (daddr_t)XFS_FSB_TO_DADDR(mp, bno),
				XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		if (bp == NULL || geterror(bp)) {
			do_warn("can't read remote block for attributes of inode %llu\n",ino);
			clearit = 1;
			break;
		}
		assert(mp->m_sb.sb_blocksize == bp->b_bcount);
		length = MIN(bp->b_bcount, valuelen - amountdone);
		bcopy(bp->b_un.b_addr, value, length); 
		amountdone += length;
		value += length;
		i++;
		if (bp != NULL) brelse(bp);  /* shouldn't be null though */
	} /* endloop */
	
	return (clearit);
}

/*
 * freespace map for directory and attribute leaf blocks (1 bit per byte)
 * 1 == used, 0 == free
 */
static da_freemap_t attr_freemap[DA_BMAP_SIZE];

/* the block is read in. The magic number and forward / backward
* links are checked by the caller process_leaf_attr.
* if any problems, occur the routine returns with non-zero. In
* this case the next step is to clear the attribute fork, by
* changing it to shortform and zeroing it out. Forkoff need not
* be changed. 
*/

int
process_leaf_attr_block(
	xfs_mount_t	*mp,
	xfs_attr_leafblock_t *leaf,
	xfs_dablk_t	da_bno,
	xfs_ino_t	ino,
	blkmap_t	*blkmap,
	xfs_dahash_t	last_hashval,
	xfs_dahash_t	*current_hashval,
	int 		*repair)	
{
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *local;
	xfs_attr_leaf_name_remote_t *remotep;
	int  i, start, stop, clearit, usedbs, firstb, thissize;

	clearit = usedbs = 0;
	*repair = 0;
	firstb = mp->m_sb.sb_blocksize; 
	stop = sizeof(xfs_attr_leaf_hdr_t);

	/* does the count look sorta valid? */
	if (leaf->hdr.count * sizeof(xfs_attr_leaf_entry_t) +
			sizeof(xfs_attr_leaf_hdr_t) > XFS_LBSIZE(mp)) {
		do_warn("bad attribute count %d in attr block %u, inode %llu\n",
			(int) leaf->hdr.count, da_bno, ino);
		return (1);
	}
 
	init_da_freemap(attr_freemap);
	(void) set_da_freemap(mp, attr_freemap, 0, stop);
	
	/* go thru each entry checking for problems */
	for (i = 0, entry = &leaf->entries[0]; 
			i < leaf->hdr.count; i++, entry++) {
	
		/* check if index is within some boundary. */
		if (entry->nameidx > XFS_LBSIZE(mp)) {
			do_warn("bad attribute nameidx %d in attr block %u, inode %llu\n",
				(int)entry->nameidx,da_bno,ino);
			clearit = 1;
			break;
			}

		if (entry->flags & XFS_ATTR_INCOMPLETE) {
			/* we are inconsistent state. get rid of us */
			do_warn("attribute entry #%d in attr block %u, inode %llu is INCOMPLETE\n",
				i, da_bno, ino);
			clearit = 1;
			break;
			}

		/* mark the entry used */
		start = (__psint_t)&leaf->entries[i] - (__psint_t)leaf;
		stop = start + sizeof(xfs_attr_leaf_entry_t);
		if (set_da_freemap(mp, attr_freemap, start, stop))  {
			do_warn("attribute entry %d in attr block %u, inode %llu claims already used space\n",
				i,da_bno,ino);
			clearit = 1;
			break;	/* got an overlap */
			}

		if (entry->flags & XFS_ATTR_LOCAL) {

			local = XFS_ATTR_LEAF_NAME_LOCAL(leaf, i);	
			if ((local->namelen == 0) || 
					(namecheck((char *)&local->nameval[0], 
						local->namelen))) {
				do_warn("attribute entry %d in attr block %u, inode %llu has bad name (namelen = %d)\n",
					i, da_bno, ino, (int) local->namelen);

				clearit = 1;
				break;
				};

			/* Check on the hash value. Checking ordering of hash values
			 * is not necessary, since one wrong one clears the whole
			 * fork. If the ordering's wrong, it's caught here or 
 			 * the kernel code has a bug with transaction logging
			 * or attributes itself. For paranoia reasons, let's check
			 * ordering anyway in case both the name value and the 
		  	 * hashvalue were wrong but matched. Unlikely, however.
			*/
			if (entry->hashval != 
				xfs_da_hashname((char *)&local->nameval[0],
					local->namelen) ||
				(entry->hashval < last_hashval)) {
				do_warn("bad hashvalue for attribute entry %d in attr block %u, inode %llu\n",
					i, da_bno, ino);
				clearit = 1;
				break;
			}

			/* Only check values for root security attributes */
			if (entry->flags & XFS_ATTR_ROOT) 
				if (valuecheck((char *)&local->nameval[0], NULL,
					    local->namelen, local->valuelen)) {
					do_warn("bad security value for attribute entry %d in attr block %u, inode %llu\n",
						i,da_bno,ino);
					clearit = 1;
					break;
				};
			thissize = XFS_ATTR_LEAF_ENTSIZE_LOCAL(
					local->namelen, local->valuelen);

		} else {
			/* do the remote case */
			remotep = XFS_ATTR_LEAF_NAME_REMOTE(leaf, i);
			thissize = XFS_ATTR_LEAF_ENTSIZE_REMOTE(
					remotep->namelen); 

			if ((remotep->namelen == 0) || 
				   (namecheck((char *)&remotep->name[0],
					remotep->namelen)) ||
				   (entry->hashval != xfs_da_hashname(
					(char *)&remotep->name[0],
					 remotep->namelen)) ||
				   (entry->hashval < last_hashval) ||
				   (remotep->valueblk == 0)) {
				do_warn("inconsistent remote attribute entry %d in attr block %u, ino %llu\n",
					i, da_bno, ino);
				clearit = 1;
				break;
			};

			if (entry->flags & XFS_ATTR_ROOT) {
				char*	value;
				if ((value = malloc(remotep->valuelen))==NULL){
					do_warn("cannot malloc enough for remotevalue attribute for inode %llu\n",ino);
					do_warn("SKIPPING this remote attribute\n");
					continue;
				}
				if (rmtval_get(mp, ino, blkmap,
						remotep->valueblk,
						remotep->valuelen, value)) {
					do_warn("remote attribute get failed for entry %d, inode %llu\n", i,ino);
					clearit = 1;
					free(value);
					break;
				}
				if (valuecheck((char *)&remotep->name[0], value,
					    remotep->namelen, remotep->valuelen)){
					do_warn("remote attribute value check  failed for entry %d, inode %llu\n", i, ino);
					clearit = 1;
					free(value);
					break;
				}
				free(value);
			}
		}

		*current_hashval = last_hashval = entry->hashval;

		if (set_da_freemap(mp, attr_freemap, entry->nameidx,
				entry->nameidx + thissize))  {
			do_warn("attribute entry %d in attr block %u, inode %llu claims used space\n",
				i, da_bno, ino);
			clearit = 1;
			break;	/* got an overlap */
		}			
		usedbs += thissize;
		if (entry->nameidx < firstb) 
			firstb = entry->nameidx;

	} /* end the loop */

	if (!clearit) {
		/* verify the header information is correct */

		/* if the holes flag is set, don't reset first_used unless it's
		 * pointing to used bytes.  we're being conservative here
		 * since the block will get compacted anyhow by the kernel. 
		 */

		if (leaf->hdr.holes == 0 && firstb != leaf->hdr.firstused ||
				leaf->hdr.firstused > firstb)  {
			if (!no_modify)  {
				do_warn("- resetting first used heap value from %d to %d in block %u of attribute fork of inode %llu\n",
					(int)leaf->hdr.firstused, firstb, da_bno, ino);
				leaf->hdr.firstused = firstb;
				*repair = 1;
			} else  {
				do_warn("- would reset first used value from %d to %d in block %u of attribute fork of inode %llu\n",
					(int)leaf->hdr.firstused, firstb, da_bno, ino);
			}
		}

		if (usedbs != leaf->hdr.usedbytes)  {
			if (!no_modify)  {
				do_warn("- resetting usedbytes cnt from %d to %d in block %u of attribute fork of inode %llu\n",
					(int)leaf->hdr.usedbytes, usedbs, da_bno, ino);
				leaf->hdr.usedbytes = usedbs;
				*repair = 1;
			} else  {
				do_warn("- would reset usedbytes cnt from %d to %d in block %u of attribute fork of %llu\n",
					(int)leaf->hdr.usedbytes, usedbs,da_bno,ino);
			}
		}

		/* there's a lot of work in process_leaf_dir_block to go thru
		* checking for holes and compacting if appropiate. I don't think
		* attributes need all that, so let's just leave the holes. If
		* we discover later that this is a good place to do compaction
		* we can add it then. 
		*/
	}
	return (clearit);  /* and repair */
}


/*
 * returns 0 if the attribute fork is ok, 1 if it has to be junked.
 */
int
process_leaf_attr_level(xfs_mount_t	*mp,
			da_bt_cursor_t	*da_cursor)
{
	int			repair;
	xfs_attr_leafblock_t	*leaf;
	buf_t			*bp;
	xfs_ino_t		ino;
	xfs_dfsbno_t		dev_bno;
	xfs_dablk_t		da_bno;
	xfs_dablk_t		prev_bno;
	xfs_dahash_t		current_hashval = 0;
	xfs_dahash_t		greatest_hashval;

	da_bno = da_cursor->level[0].bno;
	ino = da_cursor->ino;
	prev_bno = 0;

	do  {
		repair = 0;
		dev_bno = blkmap_get(da_cursor->blkmap, da_bno);
		/*
		 * 0 is the root block and no block
		 * pointer can point to the root block of the btree
		 */
		assert(da_bno != 0);

		if (dev_bno == NULLDFSBNO) {
			do_warn("can't map block %u for attribute fork for inode %llu\n",
				da_bno, ino);
			goto error_out; 
		}

		bp = read_buf(mp->m_dev, XFS_FSB_TO_DADDR(mp, dev_bno),
			XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		if (bp == NULL || geterror(bp)) {
			do_warn("can't read file block %u (fsbno %llu) for attribute fork of inode %llu\n",
				da_bno, dev_bno, ino);
			goto error_out;
		}

		leaf = (xfs_attr_leafblock_t *)(bp->b_un.b_addr);

		/* check magic number for leaf directory btree block */
		if (leaf->hdr.info.magic != XFS_ATTR_LEAF_MAGIC) {
			do_warn("bad attribute leaf magic # %#x for inode %llu\n",
				 leaf->hdr.info.magic, ino);
			brelse(bp);
			goto error_out;
		}
		
		/*
		 * for each block, process the block, verify it's path,
		 * then get next block.  update cursor values along the way
		 */
		if (process_leaf_attr_block(mp, leaf, da_bno, ino,
				da_cursor->blkmap, current_hashval,
				&greatest_hashval, &repair))  {
			brelse(bp);
			goto error_out;
		}

		/*
		 * index can be set to hdr.count so match the
		 * indexes of the interior blocks -- which at the
		 * end of the block will point to 1 after the final
		 * real entry in the block
		 */
		da_cursor->level[0].hashval = greatest_hashval;
		da_cursor->level[0].bp = bp;
		da_cursor->level[0].bno = da_bno;
		da_cursor->level[0].index = leaf->hdr.count;
		da_cursor->level[0].dirty = repair; 

		if (leaf->hdr.info.back != prev_bno)  {
			do_warn("bad sibling back pointer for block %u in attribute fork for inode %llu\n",da_bno,ino);
			brelse(bp);
			goto error_out;
		}

		prev_bno = da_bno;
		da_bno = leaf->hdr.info.forw;

		if (da_bno != 0)
			if (verify_da_path(mp, da_cursor, 0))  {
				brelse(bp);
				goto error_out;
			}

		current_hashval = greatest_hashval;

		if (repair && !no_modify)  {
			bwrite(bp);
		} else  {
			brelse(bp);
		}
	} while (da_bno != 0);

	if (verify_final_da_path(mp, da_cursor, 0))  {
		/*
		 * verify the final path up (right-hand-side) if still ok
		 */
		do_warn("bad hash path in attribute fork for inode %llu\n",
			da_cursor->ino);
		goto error_out;
	}

	/* releases all buffers holding interior btree blocks */
	release_da_cursor(mp, da_cursor, 0);
	return(0);

error_out:
	/* release all buffers holding interior btree blocks */
	err_release_da_cursor(mp, da_cursor, 0);
	return(1);
}


/*
 * a node directory is a true btree  -- where the attribute fork
 * has gotten big enough that it is represented as a non-trivial (e.g.
 * has more than just a block) btree.
 *
 * Note that if we run into any problems, we will trash the attribute fork.
 * 
 * returns 0 if things are ok, 1 if bad
 * Note this code has been based off process_node_dir. 
 */
int
process_node_attr(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap)
{
	xfs_dablk_t			bno;
	int				error = 0;
	da_bt_cursor_t			da_cursor;

	/*
	 * try again -- traverse down left-side of tree until we hit
	 * the left-most leaf block setting up the btree cursor along
	 * the way.  Then walk the leaf blocks left-to-right, calling
	 * a parent-verification routine each time we traverse a block.
	 */
	bzero(&da_cursor, sizeof(da_bt_cursor_t));
	da_cursor.active = 0;
	da_cursor.type = 0;
	da_cursor.ino = ino;
	da_cursor.dip = dip;
	da_cursor.greatest_bno = 0;
	da_cursor.blkmap = blkmap;

	/*
	 * now process interior node. don't have any buffers held in this path.
	 */
	error = traverse_int_dablock(mp, &da_cursor, &bno, XFS_ATTR_FORK);
	if (error == 0) 
		return(1);  /* 0 means unsuccessful */

	/*
	 * now pass cursor and bno into leaf-block processing routine
	 * the leaf dir level routine checks the interior paths
	 * up to the root including the final right-most path.
	 */
	
	return (process_leaf_attr_level(mp, &da_cursor));
}

/*
 * Start processing for a leaf or fuller btree.
 * A leaf directory is one where the attribute fork is too big for
 * the inode  but is small enough to fit into one btree block
 * outside the inode. This code is modelled after process_leaf_dir_block.
 *
 * returns 0 if things are ok, 1 if bad (attributes needs to be junked)
 * repair is set, if anything was changed, but attributes can live thru it
 */

int
process_longform_attr(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	int		*repair)	/* out - 1 if something was fixed */
{
	xfs_attr_leafblock_t	*leaf;
	xfs_dfsbno_t	bno;
	buf_t		*bp;
	xfs_dahash_t	next_hashval;
	int		repairlinks = 0;

	*repair = 0;

	bno = blkmap_get(blkmap, 0);

	if ( bno == NULLDFSBNO ) {
		if (dip->di_core.di_anextents == 0  &&
		    dip->di_core.di_aformat == XFS_DINODE_FMT_EXTENTS )
			/* it's okay the kernel can handle this state */
			return(0);
		else	{
			do_warn("block 0 of inode %llu attribute fork is missing\n", ino);
			return(1);
		}
	}
	/* FIX FOR bug 653709 -- EKN */
	if (mp->m_sb.sb_agcount < XFS_FSB_TO_AGNO(mp, bno)) {
		do_warn("agno of attribute fork of inode %llu out of regular partition\n",ino);
		return(1);
	}

	bp = read_buf(mp->m_dev, (daddr_t)XFS_FSB_TO_DADDR(mp, bno),
			XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
	if (bp == NULL || geterror(bp)) {
		do_warn("can't read block 0 of inode %llu attribute fork\n",ino);
		return(1);
	}

	/* verify leaf block */
	leaf = (xfs_attr_leafblock_t *)(bp->b_un.b_addr);

	/* check sibling pointers in leaf block or root block 0 before
	* we have to release the btree block
	*/
	if (leaf->hdr.info.forw != 0 || leaf->hdr.info.back != 0)  {
		if (!no_modify)  {
			do_warn("clearing forw/back pointers in block 0 for attributes in inode %llu\n", ino);
			repairlinks = 1;
			leaf->hdr.info.forw = 0;
			leaf->hdr.info.back = 0;
		} else  {
			do_warn("would clear forw/back pointers in block 0 for attributes in inode %llu\n", ino);
		}
	}

	/*
	 * use magic number to tell us what type of attribute this is.
	 * it's possible to have a node or leaf attribute in either an
	 * extent format or btree format attribute fork.
	 */
	switch (leaf->hdr.info.magic) {
	case XFS_ATTR_LEAF_MAGIC:	/* leaf-form attribute */
		if (process_leaf_attr_block(mp,leaf,0,ino,blkmap,0,&next_hashval,repair)) {
			/* the block is bad.  lose the attribute fork. */
			brelse(bp);
			return(1); 
		}
		*repair = *repair || repairlinks; 
		break;

	case XFS_DA_NODE_MAGIC:		/* btree-form attribute */
		/* got to do this now, to release block 0 before the traversal */
		if (repairlinks) {
			*repair = 1;
			bwrite(bp); 
		} else 
			brelse(bp);	
		return (process_node_attr(mp, ino, dip, blkmap)); /* and repair */
	default:
		do_warn("bad attribute leaf magic # %#x for dir ino %llu\n", 
			leaf->hdr.info.magic, ino);
		brelse(bp);
		return(1);
	}

	if (*repair && !no_modify) 
		bwrite(bp);
	else
		brelse(bp);

	return(0);  /* repair may be set */
}


/*
 * returns 1 if attributes got cleared
 * and 0 if things are ok. 
 */
int
process_attributes(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	int		*repair)  /* returned if we did repair */
{
	int err;
	xfs_dinode_core_t *dinoc;
	/* REFERENCED */
	xfs_attr_shortform_t *asf;

	dinoc = &dip->di_core;
	asf = (xfs_attr_shortform_t *) XFS_DFORK_APTR(dip);

	if (dinoc->di_aformat == XFS_DINODE_FMT_LOCAL) {
		assert(asf->hdr.totsize <= XFS_DFORK_ASIZE(dip, mp));
		err = process_shortform_attr(ino, dip, repair);
	} else if (dinoc->di_aformat == XFS_DINODE_FMT_EXTENTS ||
		   dinoc->di_aformat == XFS_DINODE_FMT_BTREE)  {
			err = process_longform_attr(mp, ino, dip, blkmap,
				repair);
			/* if err, convert this to shortform and clear it */
			/* if repair and no error, it's taken care of */
	} else  {
		do_warn("illegal attribute format %d, ino %llu\n",
			dinoc->di_aformat, ino);
		err = 1; 
	}
	return (err);  /* and repair */
}

