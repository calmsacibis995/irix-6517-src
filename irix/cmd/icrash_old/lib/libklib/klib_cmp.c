#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_cmp.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/stream.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/dump.h>

#include "klib.h"
#include "klib_extern.h"
#include "klib_cmp.h"

/**
 ** Compression Code -- Read through a vmcore file to get compressed memory
 **/

/* Global variables
 */
ptableentry **page_table;     /* global page table                */
ptableindex **page_index;     /* global page index                */
unsigned int mem_start = 0;   /* start of physical memory in dump */

/* Internal variables that define the cache
 */
int DB;
int DBD;
int sizeof_dir_entry_header;
int cache_count = 0;
int cache_water_mark = CMP_HIGH_WATER_MARK;
int cache_flags = CMP_VM_CACHED;
ptableentry *cache_head;
ptableentry *cache_tail;

int dir_ent_flags;		/* directory entry flags, for directory memory */

/*
 * cmpphashbase() -- Compute the hash algorithm based on the 64 vs. 32 bit
 *                   field.  The hash flag tells us whether we are using
 *                   the return value for hashing, or for page offset data.
 */
kaddr_t
cmpphashbase(klib_t *klp, kaddr_t in_addr, int hash)
{
    	if (IS_HUBSPACE(0, in_addr)) {
		/* keep full address for hashing */
		return(in_addr & 0xfffffffffffff000);
	}
	if ((!hash) && (PTRSZ64(klp))) {
		/* don't worry about NASID bit on hash indexing -- no need */
		return(in_addr & 0xffffc000);
	} else if (PTRSZ64(klp)) {
		/* make sure NASID bit is considered to get the right page value */
		return(in_addr & 0xffffffc000);
	}
	/* 32-bit hashing vs. non-hashing doesn't matter -- small value */
	return(in_addr & 0xfffff000);
}

/*
 * cmpphash() -- 
 * 
 *   This function sets up the upper 5 nibbles as the hash value for
 *   our hash table.
 */
int
cmpphash(klib_t *klp, kaddr_t in_addr)
{
	return (cmpphashbase(klp, in_addr, 0) % NUM_BUCKETS);
}

/*
 * cmpcleanindex() -- Clean out the table indices.
 */
void
cmpcleanindex()
{
	int i;
	ptableindex *tmpptr, *tmpptr2;

	for (i = 0; i < NUM_BUCKETS; i++) {
		if (page_index[i]) {
			tmpptr = page_index[i];
			while (tmpptr) {
				tmpptr2 = tmpptr;
				tmpptr = tmpptr->next;
				free(tmpptr2);
			}
		}
	}
}

/*
 * uncompress_page() -- Uncompress a buffer of data
 * 
 *   Return the flags, size, and the number of bytes read. It also
 *   verifies the compression type.
 */
int
uncompress_page(klib_t *klp, unsigned char *comp_buf, unsigned char *uncomp_buf,
	int flags, kaddr_t blk_size, int *new_size)
{
	int i;
	unsigned char value, count, cur_byte;
	kaddr_t read_index, write_index;

	/* Initialize the read / write indices.
	 */
	read_index = write_index = 0;

	/* If this is not a run length encoded dump (RLE), return.
	 */
	if ((flags & DUMP_COMPMASK) != DUMP_RLE) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, "Errror!  Bogus compression type!\n");
		}
		return 0;
	}

	/* Otherwise, decompress using run length encoding.
	 */
	while(read_index < blk_size) {
		cur_byte = comp_buf[read_index++];
		if (cur_byte == 0) {
			count = comp_buf[read_index++];
			if (count == 0) {
				uncomp_buf[write_index++] = 0;
			} else {
				value = comp_buf[read_index++];
				for (i = 0; i <= count; i++) {
					uncomp_buf[write_index++] = value;
				}
			}
		} else {
			uncomp_buf[write_index++] = cur_byte;
		}

		/* If our write index is beyond the page size of our dump, 
		 * exit out.
		 */
		if (write_index > DB) {
			if (DEBUG(KLDC_CMP, 2)) {
				fprintf(KL_ERRORFP,
					"ERROR: Attempted to decompress beyond page boundaries "
					"- file corrupted!\n");
			}
			return (0);
		}
	}

	/* Set the return size to be equal to our uncompressed size 
	 * (in bytes)
	 */
	*new_size = write_index;
	return 1;
}

/*
 * cmpconvertaddr() -- Convert an address to a particular offset.
 */
kaddr_t
cmpconvertaddr(dir_entry_header *dir)
{
	kaddr_t paddr;

#define BTOP(x) (((x) + (DB - 1)) / DB)
	paddr = (dir->addr_lo & (~0xfff)) - ((BTOP(mem_start)) << 12);
#undef BTOP
	paddr |= ((kaddr_t)(dir->addr_hi) << 32);
	return (paddr);
}

/*
 * cmpgetdirentry() -- Get a compressed core dump directory entry  
 * 
 *   Since we need to read each header separately, return it as an
 *   allocated pointer, if possible.
 */
int
cmpgetdirentry(klib_t *klp, int fd, dir_entry_header *dir)
{
	int num_bytes;

	num_bytes = read(fd, (void *)dir, sizeof_dir_entry_header);
	if (DEBUG(KLDC_CMP, 3)) {
		fprintf(KL_ERRORFP,
			"cmpgetdirentry(): 0x%0.8lx 0x%0.8lx 0x%0.4lx %6lx\n",
			dir->addr_hi, dir->addr_lo, dir->flags, dir->length);
	}

	if (num_bytes < 0) {
		return -1;
	} else if (num_bytes != sizeof_dir_entry_header) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP,
				"cmpgetdirentry(): read short, %d bytes != %d bytes\n",
				num_bytes, sizeof(dir_entry_header));
		}
		return 0;
	} else {
	    	if (K_DUMP_HDR(klp)->dmp_version < 4 ) {
	    		/*
			 * Checking for the "end of the road". If this is
			 * the end of the dump, stop here instead of later,
			 * otherwise we write out an inadvertant 0x0 page!
			 */
	    		if ((dir->addr_lo & 0x0fff) & DUMP_END) {
				if (DEBUG(KLDC_CMP, 2)) {
					fprintf(KL_ERRORFP,
						"cmpgetdirentry(): returning 0 (DUMP_END)\n");
				}
				return 0;
			}
		
		} else {
	    		if (dir->flags & DUMP_END) {
				if (DEBUG(KLDC_CMP, 2)) {
					fprintf(KL_ERRORFP,
						"cmpgetdirentry(): returning 0 (DUMP_END)\n");
				}
		    		return 0;
			}
		}
	}
	return 1;
}

/*
 * cmppindexprint()
 */
void
cmppindexprint(klib_t *klp)
{
	int first_time = 1;
	ptableindex *tmpptr;
	int i;

	fprintf(KL_ERRORFP, "PRINTING CHAIN:\n");
	for (i = 0; i < NUM_BUCKETS; i++) {
		tmpptr = page_index[i];
		if (tmpptr) {
			fprintf(KL_ERRORFP, "%6d: ", i);
			while (tmpptr != (ptableindex *)NULL) {
				fprintf(KL_ERRORFP, "0x%llx->", tmpptr->addr);
				tmpptr = tmpptr->next;
			}
			fprintf(KL_ERRORFP, "NULL\n%6d: ", i);
		}
	}
}

/*
 * cmppinsert() -- Insert a directory entry / buffer into a table
 *
 *   This is so it can be hashed on later. Note that this will also
 *   contain caching flags in the future.
 */
void
cmppinsert(klib_t *klp, kaddr_t in_addr, char *buffer, int nbytes, int flags)
{
	int hash, thash;
	ptableentry *tmpptr, *tmpptr2;

	tmpptr = (ptableentry *)NULL;

	/* Since we know where the page is, and it is inserted, let's
	 * go ahead and insert it into the cache (if we should...)
	 */
	if (cache_flags == CMP_VM_CACHED) {
		if (cache_count > cache_water_mark) {

			if (DEBUG(KLDC_CMP, 2)) {
				fprintf(KL_ERRORFP,
					"cmppinsert(): Cleaning excess page out of cache! "
					"(0x%llx) [%d]...\n", in_addr, cache_count);
			}

			tmpptr = cache_head;
			if (tmpptr) {

				/* First, take the first block off the list.  It's easier
				 * to take off the head (and a tad bit faster...)  Keep
				 * a copy of it, because we have to remove it from the
				 * regular set of blocks.  We also want to use its data
				 * space for later (why malloc() if we don't need to?)
				 */
				cache_head = cache_head->nextcache;
				if (cache_head) {
					cache_head->prevcache = (ptableentry *)NULL;
				}
				cache_count--;

				tmpptr->nextcache = (ptableentry *)NULL;
				tmpptr->prevcache = (ptableentry *)NULL;

				/* Next, take the block out of the pointer
				 * list, because it's not needed.
				 */
				if ((tmpptr->next == (ptableentry *)NULL) &&
					(tmpptr->prev != (ptableentry *)NULL)) {

					/* We are removing the last block on a list.
					 * Assuming it isn't the only block, just back up one.
					 */ 
					tmpptr->prev->next = (ptableentry *)NULL;
				} else if (tmpptr->prev == (ptableentry *)NULL) {

					/* We are at the head of a list.  In this case,
					 * we have to find the page_table hash entry,
					 * move it forward, reset pointers, etc.
					 */
					thash = cmpphash(klp, tmpptr->addr);
					if (page_table[thash] == tmpptr) {
						page_table[thash] = page_table[thash]->next;
						if (page_table[thash] != (ptableentry *)NULL) {
							page_table[thash]->prev = (ptableentry *)NULL;
						}
					} else {
						if (DEBUG(KLDC_CMP, 2)) {
							fprintf(KL_ERRORFP,
								"cmppinsert(): hash table at head!\n");
						}
					}
				} else if ((tmpptr->next != (ptableentry *)NULL) &&
					(tmpptr->prev != (ptableentry *)NULL)) {

						/* We're in the middle of a list.  Just set our
						 * neighbor's pointers around us.
						 */
						tmpptr2 = tmpptr->next;
						tmpptr->prev->next = tmpptr->next;
						tmpptr2->prev = tmpptr->prev;
				}
				tmpptr->cached = FALSE;
			}
		}
	}

	/* At this point, we *might* have a valid buffer.  If we
	 * do, use it!  Then, stick it back into the cache and the
	 * hash table.
	 */
	if (tmpptr == (ptableentry *)NULL) {
		if (DEBUG(KLDC_CMP, 1)) {
			fprintf(KL_ERRORFP, "cmppinsert(): Malloc occurred! [%d]\n",
				cache_count);
		}
		tmpptr = (ptableentry *)malloc(sizeof(ptableentry));
		tmpptr->data = (char *)malloc(nbytes);
	}
	memcpy((void *)tmpptr->data, (const void *)buffer, nbytes);

	tmpptr->addr = cmpphashbase(klp, in_addr, 1);
	tmpptr->flags = flags;
	tmpptr->length = nbytes;
	tmpptr->next = (ptableentry *)NULL;
	tmpptr->prev = (ptableentry *)NULL;
	tmpptr->nextcache = (ptableentry *)NULL;
	tmpptr->prevcache = (ptableentry *)NULL;
	tmpptr->cached = FALSE;

	hash = cmpphash(klp, in_addr);

	/* Insert the page into the page table, as normal.
	 * We do this whether we are caching or not.
	 */
	if (page_table[hash] != (ptableentry *)NULL) {
		tmpptr->next = page_table[hash];
		page_table[hash]->prev = tmpptr;
		page_table[hash] = tmpptr;
	} else {
		page_table[hash] = tmpptr;
	}

	/* Now, if we are using a caching scheme, store the page
	 * at the end of the chain.
	 */
	if (cache_flags == CMP_VM_CACHED) {
		page_table[hash]->cached = TRUE;
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, "cmppinsert(): Inserting page into cache! "
				"(0x%llx) [%d]...\n", in_addr, cache_count);
		}
		if (cache_tail != (ptableentry *)NULL) {
			cache_tail->nextcache = tmpptr;
			tmpptr->prevcache = cache_tail;
			cache_tail = cache_tail->nextcache;
		} else {
			cache_head = tmpptr;
			cache_tail = tmpptr;
			cache_head->nextcache = (ptableentry *)NULL;
			cache_head->prevcache = (ptableentry *)NULL;
		}
		cache_count++;
	}
}

/*
 * cmppget() -- Try to get the page of data hashed out.  
 * 
 *   This will search through the hash table looking for our page. If
 *   we find it, and it is all we need, copy it into the buffer and
 *   return. Otherwise, read the rest from cmppread() again and return.
 */
int
cmppget(klib_t *klp, int fd, kaddr_t in_addr, char *buffer,
	unsigned int nbytes, unsigned int flags)
{
	ptableentry *tmpptr, *tmpptr2;
	int hash;
	unsigned int tmpbytes;
	unsigned int offset = 0;
	unsigned int bytes_left = 0;

	/* First things first, snag the hash index for this item.
	 */
	hash = cmpphash(klp, in_addr);

	/* Now, see if the item exists in the list.  If the first
	 * pointer doesn't even exist, just return.
	 */
	if ((hash < 0) || (hash >= NUM_BUCKETS) || (!page_table[hash])) {
		return -1;
	}

	/* Otherwise, we've got a valid pointer.  Look for the hashed
	 * page in the table.
	 */
	tmpptr = page_table[hash];
	while (tmpptr) {
		if ((in_addr >= tmpptr->addr) &&
			(in_addr <= tmpptr->addr + DB)) {

				/* Check out if this page is on the cache list
				 * or not.  If it is, then go ahead and move it
				 * to the end of the list.  We only need to play
				 * with the cache list, we don't have to touch
				 * its position in the hash table.
				 */
				if ((cache_flags == CMP_VM_CACHED) && (tmpptr->cached)) {

					/* Move the page in the cache.  First, take it
					 * from where it is currently located.  Note that
					 * there is no reason to re-insert if we are
					 * already at the end of the list!
					 */
					if (tmpptr->nextcache != (ptableentry *)NULL) {

						/* See if we are at the head of the list.
						 */
						if (tmpptr->prevcache == (ptableentry *)NULL) {
							if (cache_head) {
								cache_head = cache_head->nextcache;
								if (cache_head) {
									cache_head->prevcache =
										(ptableentry *)NULL;
								}
							}
						} else {
							tmpptr->prevcache->nextcache = tmpptr->nextcache;
							tmpptr->nextcache->prevcache = tmpptr->prevcache;
						}
						tmpptr->nextcache = (ptableentry *)NULL;
						tmpptr->prevcache = (ptableentry *)NULL;

						/* Now, re-insert it at the end of the list.
						 */
						if (cache_tail != (ptableentry *)NULL) {
							cache_tail->nextcache = tmpptr;
							tmpptr->prevcache = cache_tail;
							cache_tail = cache_tail->nextcache;
						} else {
							cache_head = tmpptr;
							cache_tail = tmpptr;
							cache_head->nextcache = (ptableentry *)NULL;
							cache_head->prevcache = (ptableentry *)NULL;
						}
					}
				}

				/* If we have the buffer where the address is located,
				 * determine how much of it needs to be copied, and
				 * from which offset.
				 */
				offset = (int)(in_addr - tmpptr->addr);
				bytes_left = (int)((tmpptr->addr + DB) - in_addr);
				if (in_addr + nbytes > tmpptr->addr + DB) {
					if (DEBUG(KLDC_CMP, 2)) {
						fprintf(KL_ERRORFP, "cmppget(): reading more data "
							"(bytes_left = %d, nbytes = %d)\n",
							bytes_left, nbytes);
					}
					memcpy((void *)buffer,
						(const void *)(tmpptr->data + offset),
						bytes_left);
					return(cmppread(klp, fd, tmpptr->addr + DB,
						buffer + bytes_left, nbytes - bytes_left,
						flags));
				}
				if (DEBUG(KLDC_CMP, 2)) {
					fprintf(KL_ERRORFP, "cmppget(): copying page of data "
						"(nbytes = %d, offset = %d, in_addr = 0x%llx)\n",
						nbytes, offset, in_addr);
				}
				memcpy((void *)buffer,
					(const void *)(tmpptr->data + offset),
					nbytes);
						/* needed for 'dirmem' command */
				dir_ent_flags = tmpptr->flags;
				return 1;
		}
		tmpptr = tmpptr->next;
	}
	return -1;
}

/*
 * cmppindex() -- 
 * 
 *   This function is responsible for grabbing a page out of our index
 *   list, and returning the directory entry header at that location.
 */
ptableindex *
cmppindex(klib_t *klp, kaddr_t in_addr)
{
	ptableindex *tmpptr;
	int hash;

	/* Grab the hash value based on the address passed in,
	 * after the address is processed based on a 0x1000 page.
	 */
	hash = cmpphash(klp, in_addr);

	/* Now, see if the page exists in the page table index.
	 * We already know it isn't in the page table itself,
	 * but if the page doesn't exist in the core dump, we
	 * want to return -1.
	 */
	if (DEBUG(KLDC_CMP, 3)) {
		fprintf(KL_ERRORFP, "cmppindex(): hash = %6d, addr = 0x%llx\n",
			hash, in_addr);
	}
	tmpptr = page_index[hash];
	while (tmpptr != (ptableindex *)NULL) {
		if (DEBUG(KLDC_CMP, 4)) {
			fprintf(KL_ERRORFP, "cmppindex(): addr = 0x%llx, "
				"tmpptr->addr = 0x%llx\n", in_addr, tmpptr->addr);
		}
		if ((in_addr >= tmpptr->addr) &&
			(in_addr <= tmpptr->addr + DB)) {

			/* We found the address we want.  Return the
			 * directory entry pointer.
			 */
			return (tmpptr);
		}
		tmpptr = tmpptr->next;
	}
	return ((ptableindex *)NULL);
}

/*
 * cmppread() -- Read a page of a compressed core dump.
 */
int
cmppread(klib_t *klp, int fd, kaddr_t in_addr, char *buffer,
	unsigned int nbytes, unsigned int flags)
{
	char *ptr;
	static char *compr_page, *uncompr_page;
	int tmpflags, new_size = 0, len;
	static int first_time = FALSE;
	off_t addr;
	ptableindex *pindexitem;

	/* Print out a message that we are searching.
	 */
	if (DEBUG(KLDC_CMP, 2)) {
		fprintf(KL_ERRORFP, 
			"cmppread(): initiating search for 0x%llx\n", in_addr);
	}

	kl_hold_signals();

	/* If we haven't created these pages yet, do so now.  This
	 * changes because of DB.
	 */
	if (first_time == FALSE) {
		if ((compr_page = (char *)malloc(DB)) == (char *)NULL) {
			fprintf(KL_ERRORFP, "uncompress_page: out of memory!\n");
			return (-1);
		}

		if ((uncompr_page = (char *)malloc(DB)) == (char *)NULL) {
			fprintf(KL_ERRORFP, "uncompress_page: out of memory!\n");
			return (-1);
		}
		first_time = TRUE;
	}

	/* First things first: try to go through the page table
	 * and get a page of data.  It might not contain the entire
	 * set of data that we are looking for, but it will grab
	 * any additional pages we might need.
	 */
	if (cmppget(klp, fd, in_addr, buffer, nbytes, flags) >= 0) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, 
				"cmppread(): found the item in the hash table!\n");
		}
		kl_release_signals();
		return 1;
	}

	/* If we get to here, we didn't find the page in the page table.
	 * So, search through the compressed core dump index to see if
	 * the address exists in the core dump's list.
	 */
	if ((pindexitem = cmppindex(klp, in_addr)) == (ptableindex *)NULL) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, 
				"cmppread(): page not found! (0x%llx)\n", in_addr);
		}
		bzero(buffer, nbytes);
		kl_release_signals();
		return 0;
	} else {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, 
				"cmppread(): found the page in the page index!\n");
		}
	}

	/* If we get to here, we have found the page we want.
	 * So, let's seek to the location and read it out.
	 */
	if (LSEEK(klp, fd, pindexitem->coreaddr, SEEK_SET) < 0) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, "cmppread(): couldn't seek to page 0x%llx\n", 
				in_addr);
		}
		kl_release_signals();
		return -1;
	}

	/* Flags are in different places for version <4 and >= 4
	 */
	if (K_DUMP_HDR(klp)->dmp_version < 4 )
		tmpflags = pindexitem->dir.addr_lo & 0x0fff;
	else
	        tmpflags = pindexitem->dir.flags;

					/* needed by 'dirmem' command */
	dir_ent_flags = tmpflags;

	/* read the length of the compressed page */
	len = read(fd, (void *)compr_page, pindexitem->dir.length);
	if (len != pindexitem->dir.length) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, "cmppread(): warning: "
				"uncompressed file may be incomplete.\n");
		}
		kl_release_signals();
		return 1;
	}

	/* If this is a raw page, go ahead and read in the whole thing.
	 * Otherwise, uncompress it and read it in.
	 */
	if ((tmpflags & DUMP_COMPMASK) == DUMP_RAW) {
		if (len != DB) {
			if (DEBUG(KLDC_CMP, 2)) {
				fprintf(KL_ERRORFP, "cmppread(): warning: RAW page isn't "
					"the right size (%d)!\n", len);
			}
			kl_release_signals();
			return -1;
		}

		/* Set the pointer to the appropriate char * data buffer, and
		 * set the size appropriately.
		 */
		ptr = compr_page;
		new_size = DB;
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, "0x%llx: %d -> %d RAW, writing %d bytes\n",
				pindexitem->addr, len, new_size, DB);
		}
	} else {

		/* Uncompress the page.
		 */
		uncompress_page(klp, (unsigned char*)compr_page, 
				(unsigned char*)uncompr_page, tmpflags, len, &new_size);

		/* Check to make sure that the length is right.
		 * If:    for the kernel pages the length is not the same as DB
		 *     or for the directory memory pages the length is not DBD
		 * something's wrong.
		 */

		if (( tmpflags & DUMP_DIRECTORY) && new_size != DBD ||
		    (!tmpflags & DUMP_DIRECTORY) && new_size != DB) {
			if (DEBUG(KLDC_CMP, 2)) {
				fprintf(KL_ERRORFP, "cmppread(): warning: COMPRESSED page "
					"isn't the right size (%d)!\n", new_size);
			}
			kl_release_signals();
			return -1;
		}

		/* Set the pointer to the appropriate char * data buffer.
		 */
		ptr = uncompr_page;
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, 
				"0x%llx: %d -> %d COMPRESSED, writing %d bytes\n",
				pindexitem->addr, len, new_size, DB);
		}
	}

	/* Now, insert the page into the hash table.
	 */
	cmppinsert(klp, in_addr, ptr, new_size, tmpflags);

	/* Officially grab the page now that we have
	 * placed it into the table.
	 */
	if (cmppget(klp, fd, in_addr, buffer, nbytes, flags) >= 0) {
		if (DEBUG(KLDC_CMP, 2)) {
			fprintf(KL_ERRORFP, "cmppread(): found the item in the hash "
				"table second time!\n");
		}
		kl_release_signals();
		return 1;
	}

	/* Return that everything succeeded.
	 */
	kl_release_signals();
	return 1;
}

/*
 * cmpreadmem() -- Read the compressed core dump 
 * 
 *   The core dump is read through to the size of the buffer. We will
 *   call cmppread() as appropriate to get the page of data as we need
 *   it.
 */
int
cmpreadmem(klib_t *klp, int fd, kaddr_t in_addr, char *buffer,
	unsigned int nbytes, unsigned int flags)
{
	int offset = 0;

	/* First, see if we want a size greater than DB.
	 */
	if (nbytes <= DB) {
		if (DEBUG(KLDC_CMP, 3)) {
			fprintf(KL_ERRORFP, "cmpreadmem(): %d bytes, 0x%llx (just a "
				"page)\n", nbytes, in_addr);
		}
		return (cmppread(klp, fd, in_addr, buffer, nbytes, flags)); 
	} else if (nbytes > DB) {

		/* Otherwise, just read in the data DB at a time.
		 */
		while (offset < nbytes) {
			if ((nbytes - offset) <= DB) {

				/* Read the leftovers.
				 */
				if (DEBUG(KLDC_CMP, 3)) {
					fprintf(KL_ERRORFP, "cmpreadmem(): reading %d bytes, "
						"0x%llx (leftovers)\n",
						nbytes - offset, in_addr + offset);
				}
				return (cmppread(klp, fd, in_addr + offset,
					buffer + offset, nbytes - offset, flags));
			} else {

				/* Read a page in at a time.
				 */
				if (DEBUG(KLDC_CMP, 3)) {
					fprintf(KL_ERRORFP, "cmpreadmem(): reading %d bytes, "
						"0x%llx (a new page)\n", DB, in_addr + offset);
				}
				if (cmppread(klp, fd, in_addr + offset,
					buffer + offset, DB, flags) < 0) {
					return -1;
				}
			}

			/* Increment the offset by DB.
			 */
			offset += DB;
		}
		/* NOT REACHED */
		return 1;
	}
	/* NOT REACHED */
	return 1;
}

/*
 * cmppindexcreate() -- Create the page table index from the
 *                      compressed vmcore file.
 */
int
cmppindexcreate(klib_t *klp, int fd, int flags)
{
	int i, cmpset = FALSE, counter = 0;
	kaddr_t cur_addr;
	ptableindex *tmpptr, *tmpptr2;

	/* Now that we have the base index, run through the core dump
	 * and fill up the table.
	 */
	if (DEBUG(KLDC_CMP, 2)) {
		fprintf(KL_ERRORFP, "cmppindexcreate(): Number of pages in "
			"dump: %d\n", K_DUMP_HDR(klp)->dmp_pages);
		fprintf(KL_ERRORFP, "cmppindexcreate(): Page size in dump: %d\n",
			K_DUMP_HDR(klp)->dmp_pg_sz);
	}

	/* Here we handle the case between DUMP_VERSION 2 and 3.  The
	 * real problem is that the putbuf information and location has
	 * changed a bit in the dump header.  It modifies the way we
	 * index past to the first compressed page.  Once this is taken
	 * care of, we can read in all the compressed pages.  Luckily,
	 * the dmp_hdr_sz includes all the size of the header including
	 * the offset of the putbuf and errbuf.
	 */
	cur_addr = K_DUMP_HDR(klp)->dmp_hdr_sz;
	if (LSEEK(klp, fd, cur_addr, SEEK_SET) < 0) {
		fprintf(KL_ERRORFP, "%s: LSEEK() in cmppindexcreate failed [%d]!\n", 
			K_PROGRAM(klp), errno);
	}

	/* Grab the directory entry for this page.
	 */
	while (!cmpset) {

		tmpptr = (ptableindex *)malloc(sizeof(ptableindex));

		/* Grab the directory entry itself.  If we fail, free up
		 * the excess space and return.  Just keep going if this
		 * might fail.  We assume we had a short write for some reason.
		 */
		i = cmpgetdirentry(klp, fd, &(tmpptr->dir));
		switch (i) {
			case 1:
				break;

			case 0:
				free(tmpptr);
				if (DEBUG(KLDC_CMP, 2)) {
					fprintf(KL_ERRORFP, 
						"cmppindexcreate(): returning okay\n");
				}
				return 1;

			case -1:
				if (DEBUG(KLDC_CMP, 2)) {
					fprintf(KL_ERRORFP, "cmppindexcreate(): cmpgetdirentry() "
						"failed!\n");
				}
				return -1;
		}

		/* Set the current address location.
		 */
		cur_addr += sizeof_dir_entry_header;

		/*
		 * Now grab the hash from the base address and insert
		 * it into the table appropriately.
		 */
		tmpptr->addr = cmpconvertaddr(&(tmpptr->dir));
		tmpptr->coreaddr = cur_addr;
		tmpptr->hash = cmpphash(klp, tmpptr->addr);
		tmpptr->next = (ptableindex *)NULL;

		/* Go ahead and insert the pointer into the table.
		 */
		tmpptr2 = page_index[tmpptr->hash];
		page_index[tmpptr->hash] = tmpptr;
		page_index[tmpptr->hash]->next = tmpptr2;

		if (DEBUG(KLDC_CMP, 3)) {
			fprintf(KL_ERRORFP, "cmppindexcreate(): addr = 0x%llx, "
				"hash = %-6d, counter = %-6d\n",
				tmpptr->addr, tmpptr->hash, counter);
		}

		/* Before we quit out, go ahead and lseek() over the length.
		 * Just because we can't seek doesn't mean we have an error.
		 * We could be at EOF, with a short core dump.
		 */
		if ((!DEBUG(KLDC_GLOBAL, 1)) && 
			(!(counter & 0xfff)) && (!(flags & REPORT_FLAG))) {
			fprintf(KL_ERRORFP, ".");
		}

		if (LSEEK(klp, fd, tmpptr->dir.length, SEEK_CUR) < 0) {
			fprintf(KL_ERRORFP, "LSEEK() failed!\n");
			return 1;
		}

		cur_addr += tmpptr->dir.length;
		counter++;

		if ((tmpptr->dir.addr_lo & 0x0fff) & DUMP_END) {
			cmpset = TRUE;
		}
	}
	return 1;
}

/*
 * cmpcheckheader() -- Read off the core dump header information.
 */
int
cmpcheckheader(klib_t *klp, int fd)
{
	int res;

	/*
	 * Set DB to start!
	 * Remember: DB  = page size of the kernel that created the dump
	 *           DBD = page size of the directory memory page in the dump
	 */
	DB  = K_DUMP_HDR(klp)->dmp_pg_sz;
	DBD = K_DUMP_HDR(klp)->dmp_dir_mem_sz;

	/* Allocate the pointer list.  The variable 'pages' is really
	 * defined by the dmp_pages variable from the compressed core
	 * dump header.
	 */
	for (res = 0; res < NUM_BUCKETS; res++) {
		page_index[res] = (ptableindex *)NULL;
		page_table[res] = (ptableentry *)NULL;
	}

	return 1;
}

/*
 * cmpsaveindex() -- Save the index for the compressed core dump.
 */
void
cmpsaveindex(klib_t *klp, char *filename, int flags)
{
	int i;
	long offset;
	FILE *ifp;
	ptableindex *tmpptr;

	if (flags & REPORT_FLAG) {

		/* If we are only performing a FRU analysis, don't bother
		 * to save the compressed index file, as we don't really
		 * need to dump it anywhere specific.
		 */
		return;
	}

	if (DEBUG(KLDC_CMP, 2)) {
		fprintf(KL_ERRORFP, "\nAttempting to save \"%s\" ... \n", filename);
	}

	if ((ifp = fopen(filename, "w")) == (FILE *)NULL) {
		fprintf(KL_ERRORFP, "%s: save of index \"%s\" failed: %s",
			K_PROGRAM(klp), filename, strerror(errno));
		return;
	}
	if (fprintf(ifp, "%d\n", CMP_VERSION) < 0) {
		fprintf(KL_ERRORFP, "%s: save of index \"%s\" failed: %s",
			K_PROGRAM(klp), filename, strerror(errno));
		fclose(ifp);
		unlink(filename);
		return;
	}
	if (fprintf(ifp, "%d\n", K_DUMP_HDR(klp)->dmp_crash_time) < 0) {
		fprintf(KL_ERRORFP, "%s: save of index \"%s\" failed: %s",
			K_PROGRAM(klp), filename, strerror(errno));
		fclose(ifp);
		unlink(filename);
		return;
	}

	/* we add this offset to throw out incomplete index files */
	offset = ftell(ifp);
	if (fprintf(ifp, "0\n") < 0) {
		fprintf(KL_ERRORFP, "%s: save of index \"%s\" failed: %s",
			K_PROGRAM(klp), filename, strerror(errno));
		fclose(ifp);
		unlink(filename);
		return;
	}

	for (i = 0; i < NUM_BUCKETS; i++) {
		tmpptr = page_index[i];
		while (tmpptr) {
			if (fwrite((const void *)tmpptr,sizeof(ptableindex),1,ifp) < 1) {
				fprintf(KL_ERRORFP,
					"%s: save of index \"%s\" failed: %s",
					K_PROGRAM(klp), filename, strerror(errno));
				fclose(ifp);
				unlink(filename);
				return;
			}
			tmpptr = tmpptr->next;
		}
	}

	/* now go back to the right offset and mark the dump as complete */
	if (fseek(ifp, offset, SEEK_SET) < 0) {
		fprintf(KL_ERRORFP, "%s: save of index \"%s\" failed: %s",
			K_PROGRAM(klp), filename, strerror(errno));
		fclose(ifp);
		unlink(filename);
		return;
	}

	/* write a 1 instead of a 0 so the dump of the index is done */
	if (fprintf(ifp, "1\n") < 0) {
		fprintf(KL_ERRORFP, "%s: save of index \"%s\" failed: %s",
			K_PROGRAM(klp), filename, strerror(errno));
		fclose(ifp);
		unlink(filename);
		return;
	}

	if (DEBUG(KLDC_CMP, 2)) {
		fprintf(KL_ERRORFP, "complete.\n");
	}
}

/*
 * cmploadindex() -- Load the index for the compressed core dump.
 */
int
cmploadindex(klib_t *klp, char *filename, int debug_flag)
{
	FILE *ifp;
	ptableindex *tmpptr, *tmpptr2;
	struct stat statbuf;
	long version;
	char buf[80];
	time_t cmp_date = 0;

	if ((ifp = fopen(filename, "r")) == (FILE *)NULL) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: load of index \"%s\" failed: %s\n Loading from "
				"core file", K_PROGRAM(klp), filename, strerror(errno));
		}
		return (-1);
	}

	/* Try to run stat() on the file to get some information.
	 */
	if (stat(filename, &statbuf) < 0) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: load of index \"%s\" failed: %s\n Loading from "
				"core file", K_PROGRAM(klp), filename, strerror(errno));
		}
		return (-1);
	}

	/* Make sure this isn't a zero byte file.
	 */
	if (statbuf.st_size == (off_t)0) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: load of index \"%s\" failed: zero byte file "
				"specified\nLoading from core file", K_PROGRAM(klp), filename);
		}
		return (-1);
	}

	/* See if a version number exists on the dump.
	 */
	bzero(buf, 80);
	if (fgets(buf, 80, ifp) == (char *)NULL) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: old index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	if (sscanf(buf, "%d", &version) != 1) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: old index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	if (version != CMP_VERSION) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: old index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* Make sure that the date on the core dump and the index match up!
	 */
	bzero(buf, 80);
	if (fgets(buf, 80, ifp) == (char *)NULL) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: old index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* read the date field in the crash dump (dmp_crash_time)
	 */
	if (sscanf(buf, "%d", &cmp_date) != 1) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: old index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* compare the time from the dump header with the index file time
	 */
	if (cmp_date != K_DUMP_HDR(klp)->dmp_crash_time) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: old index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* Make sure that the index file write was complete!
	 **/
	bzero(buf, 80);
	if (fgets(buf, 80, ifp) == (char *)NULL) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: short index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* read the index complete flag (1 or 0) from the index file
	 */
	if (sscanf(buf, "%d", &version) != 1) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: short index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* if the version is not 1, we are screwed up
	 */
	if (version != 1) {
		if (debug_flag) {
			fprintf(KL_ERRORFP,
				"%s: short index file, creating new index.\n", K_PROGRAM(klp));
		}
		return (-1);
	}

	/* Try to read in the data file, if possible.
	 */
	while ((!feof(ifp)) && (!ferror(ifp))) {
		tmpptr = (ptableindex *)malloc(sizeof(ptableindex));
		if (fread((void *)tmpptr, sizeof(ptableindex), 1, ifp) < 1) {
			if (feof(ifp)) {
				return 1;
			}
			if (debug_flag) {
				fprintf(KL_ERRORFP,
					"%s: load of index \"%s\" failed: %s\n"
					"Loading from core file", K_PROGRAM(klp), filename,
					strerror(errno));
			}
			return -1;
		}

		tmpptr->next = (ptableindex *)NULL;
		tmpptr2 = page_index[tmpptr->hash];
		page_index[tmpptr->hash] = tmpptr;
		page_index[tmpptr->hash]->next = tmpptr2;
	}
	return -1;
}

/*
 * cmpinit() -- Initialize the compression code (if need be).
 */
int
cmpinit(klib_t *klp, int fd, char *indexname, int flags)
{
	int size, result;
	char iname[BUFSIZ];

	/*
	 * Read in the core dump header.
	 */
	kl_hold_signals();
	page_table = (ptableentry **)malloc((NUM_BUCKETS+1) *
		sizeof(ptableentry *));
	if (page_table == (ptableentry **)NULL) {
		fprintf(KL_ERRORFP, "Page table allocation failure!  Exiting!\n");
		exit(1);
	}
	page_index = (ptableindex **)malloc((NUM_BUCKETS+1) *
		sizeof(ptableindex *));
	if (page_index == (ptableindex **)NULL) {
		fprintf(KL_ERRORFP, "Page index allocation failure!  Exiting!\n");
		exit(1);
	}

	/* If we have a newer dump header, we include a flags field
	 * in the dir_entry_header.  This means that we need to subtract
	 * sizeof(int) when going through the offsets for this field.
	 * This applies when we create the index in cmppcreateindex()
	 * as well.
	 */
	sizeof_dir_entry_header = sizeof(dir_entry_header);
	if (DEBUG(KLDC_CMP, 2)) {
		fprintf(KL_ERRORFP, "cmpinit(): dmp_version = %d\n", K_DUMP_HDR(klp)->dmp_version);
	}
	if (K_DUMP_HDR(klp)->dmp_version < 4) {
		sizeof_dir_entry_header -= sizeof(int);
	}

	if (cmpcheckheader(klp, fd) < 0) {
		kl_release_signals();
		return (-1);
	}

	/* Initialize the cache pointers.
	 */
	cache_head = (ptableentry *)NULL;
	cache_tail = (ptableentry *)NULL;

	/* If the user has specified an index name, try to read off of it.
	 */
	if (indexname && indexname[0] != '\0') {
		if (cmploadindex(klp, indexname, TRUE) < 0) {
			cmpcleanindex();
			if (cmppindexcreate(klp, fd, flags) < 0) {
				kl_release_signals();
				return (-1);
			}
			cmpsaveindex(klp, indexname, flags);
			kl_release_signals();
			return 1;
		}
		kl_release_signals();
		return 1;
	}

	/* If we get here, we need to look to see if an index already exists.
	 */
	size = strlen(K_COREFILE(klp));
	size -= 5;
	strncpy(iname, K_COREFILE(klp), size);
	iname[size] = '\0';			/* make sure it's null-terminated */
	strcat(iname, ".index\0");
	if (DEBUG(KLDC_CMP, 2)) {
		result = cmploadindex(klp, iname, TRUE);
	} else {
		result = cmploadindex(klp, iname, FALSE);
	}
	if (result < 0) {
		cmpcleanindex();
		if (cmppindexcreate(klp, fd, flags) < 0) {
			kl_release_signals();
			return (-1);
		}
		cmpsaveindex(klp, iname, flags);
		kl_release_signals();
		return 1;
	}

	/* Return that everything is okay.
	 */
	kl_release_signals();
	return 1;
}

/*
 * klib_page_in_dump()
 *
 *   External function that returns 1 if page is in dump and 0 if page 
 *   is not in dump.
 */
int
klib_page_in_dump(klib_t *klp, kaddr_t addr)
{
	if (!COMPRESSED(klp)) {
		return(0);
	}

	if (cmppindex(klp, addr)) {
		return(1);
	}
	return(0);
}

/*
 * klib_page_istype()
 *
 *   External function that returns 1 if a page is in the dump and is
 *   of the appropriate type. Otherwise, it returns 0.
 */
int
klib_page_istype(klib_t *klp, kaddr_t addr, int type) {
	ptableindex *tmpptr;

	if (!COMPRESSED(klp)) {
		return(0);
	}

	if (tmpptr = cmppindex(klp, addr)) {
		if (tmpptr->dir.addr_lo & type) {
			return(1);
		}
		if (DEBUG(KLDC_CMP, 1)) {
			fprintf(KL_ERRORFP, "klib_page_istype: Dir entry for 0x%llx "
				"does not match with type %d\n", addr, type);
		}
	}
	return(0);
}
