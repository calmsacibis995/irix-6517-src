/*
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/* Fetchop user library */

#include <fcntl.h>       /* open()                */
#include <sys/stat.h>    /* open()                */
#include <malloc.h>      /* malloc()              */
#include <stdlib.h>      /* getenv()              */
#include <mutex.h>       /* atomic operations     */
#include <sgidefs.h>     /* __uint32_t, uint64_t  */
#include <strings.h>     /* bzero()               */
#include <unistd.h>      /* getpagesize()         */
#include <sys/types.h>   /* open(), mmap()        */
#include <sys/mman.h>    /* mmap()                */
#include <sys/pmo.h>     /* pmo_handle_t          */
#include <errno.h>
#include <fetchop.h>     /* fetchop               */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syssgi.h>

#if defined(OLD_FETCHOP_COMPATIBILITY)
	#pragma weak fetchop_free	= atomic_free_variable

	#pragma weak fetchop_load 	= atomic_load
	#pragma weak fetchop_increment 	= atomic_fetch_and_increment
	#pragma weak fetchop_decrement 	= atomic_fetch_and_decrement
	#pragma weak fetchop_clear	= atomic_clear

	#pragma weak storeop_store 	= atomic_store
	#pragma weak storeop_and	= atomic_store_and_and
	#pragma weak storeop_or		= atomic_store_and_or

#endif /* OLD_FETCHOP_COMPATIBILITY */

/* There are two shared memory structures the maintenance region and vars */
struct atomic_maint_s {
	__uint32_t bitmap_lock;       
	size_t bitmap_size;
	int vars_ident;
	int maint_shmid;
	int vars_shmid;
};

/*
 * The two below structure are per process - we need them because we 
 * could have a different virtual address for each process that
 * represents the same set of variables.
 */

struct atomic_var_reg_s {
	atomic_var_t *vars;  
	uchar_t *bitmap; /* 1 means users alloced it, 2 means we did */
	int maint_ident;
};

struct atomic_reservoir_s {
	atomic_maint_t maint_reg;
	atomic_var_reg_t var_reg;
};



/* Lock state */
#define LOCK_FREE         (0)
#define LOCK_HELD         (1)


static void atomic_lock(atomic_maint_t);
static void atomic_unlock(atomic_maint_t);

#ifdef OLD_FETCHOP_COMPATIBILITY
fetchop_reservoir_t 
fetchop_init(pmo_handle_t pm, size_t numvars)
{
	return(atomic_alloc_reservoir(pm, numvars, NULL));
}

fetchop_var_t  *
fetchop_alloc(fetchop_reservoir_t the_reservoir)
{
	return(atomic_alloc_variable(the_reservoir, NULL));
}

#endif /* OLD_FETCHOP_COMPATIBILITY */

/* ARGSUSED */
static atomic_res_ident_t
do_atomic_alloc_res_ident(size_t numvars)
{
#define MAX_TRIES 1000
	int shmid1, shmid2;
	uint shmflag = 0777;
	size_t len;
	int count, ident;
	atomic_maint_t the_maint;
	atomic_var_reg_t loc_res_ident;

	ident = getpid() * MAX_TRIES;
	count = 0;

	loc_res_ident = (atomic_var_reg_t) 
		malloc(sizeof (struct atomic_var_reg_s));

	if (loc_res_ident == NULL) {
		fprintf(stderr, "atomic_alloc_res_ident: out of memory\n");
		return(NULL);
	}

	len = sizeof(struct atomic_maint_s) + (numvars * sizeof(uchar_t));

	/* first get shared memory for the maintenance region */
      try_again:
	shmid1 = shmget(ident+count, len, shmflag|IPC_CREAT|IPC_EXCL);
	
	if (shmid1 == -1) {
		if (errno == EEXIST) {
			count++;
			if (count > MAX_TRIES) {
				fprintf(stderr,"error:atomic_alloc_res_ident can't find unique shared key\n");
			}
			goto try_again;
		}
		
		if (errno == ENOSPC)
			fprintf(stderr, "Maximum number of ipc sgements exceeded see ipcrm(1)\n");
		perror("atomic_alloc_res_ident");
		free(loc_res_ident);
		return(NULL);
	}

	the_maint = (atomic_maint_t) shmat(shmid1, 0, 0);

	if ((__int64_t)(the_maint) == -1) {
		perror("Could not attach to maintenance region");
		shmctl(shmid1, IPC_RMID);
		free(loc_res_ident);
		return(NULL);
	}

	the_maint->maint_shmid = shmid1;
	loc_res_ident->maint_ident = ident+count;

	/* now get the region for the fetchop variables */
	len = numvars * FETCHOP_VAR_SIZE;
	
	/*
	 * Start with an updated count/key to look for next shmem
	 */
	count++;

      try_again1:
	shmid2 = shmget(ident+count, len, shmflag|IPC_CREAT|IPC_EXCL);
	
	if (shmid2 == -1) {
		if (errno == EEXIST) {
			count++;
			if (count > MAX_TRIES) {
				fprintf(stderr,"error:atomic_alloc_res_ident can't find unique shared key\n");
			}
			goto try_again1;
		}
		
		if (errno == ENOSPC)
			fprintf(stderr, "Maximum number of ipc sgements exceeded see ipcrm(1)\n");
		perror("atomic_alloc_res_ident");
		shmdt(the_maint);
		shmctl(shmid1, IPC_RMID);
		free(loc_res_ident);
		return(NULL);
	}

	loc_res_ident->vars = (atomic_var_t *) shmat(shmid2, 0, 0);

	if ((__int64_t)(loc_res_ident->vars) == -1) {
		perror("Could not attach maint variables");
		shmdt(the_maint);
		shmctl(shmid2, IPC_RMID);
		shmctl(shmid1, IPC_RMID);
		free(loc_res_ident);
		return(NULL);
	}
	the_maint->vars_shmid = shmid2;


	if (syssgi(SGI_FETCHOP_SETUP, loc_res_ident->vars, len) < 0) {
		/*
		 * EALREADY is the case where a peer of us has already
		 * made the call - bad return value in syssgi is there
		 * for easier potential field debugging - ignore
		 */
		if (errno != EALREADY) {
			fprintf(stderr, "failed to create fetchopable area error\n");
			perror("atomic_alloc_res_ident");
			shmdt(loc_res_ident->vars);
			shmdt(the_maint);
			shmctl(shmid2, IPC_RMID);
			shmctl(shmid1, IPC_RMID);
			free(loc_res_ident);
			return(NULL);
		}
	}

	if (mpin(loc_res_ident->vars, len) < 0) {
		perror("failed to pin pages");
		shmdt(loc_res_ident->vars);
		shmdt(the_maint);
		shmctl(shmid2, IPC_RMID);
		shmctl(shmid1, IPC_RMID);
		free(loc_res_ident);
		return(NULL);
	}


	the_maint->vars_ident = ident + count;
	
	loc_res_ident->bitmap = (uchar_t *) the_maint + 
		                   sizeof(struct atomic_maint_s);
	bzero(loc_res_ident->bitmap, numvars);

	the_maint->bitmap_lock = LOCK_FREE;
	the_maint->bitmap_size = numvars;

	/* now mark this region fetchopable */

	shmdt(loc_res_ident->vars);
	shmdt(the_maint);

	ident = loc_res_ident->maint_ident;
	free(loc_res_ident);
	return(ident);
	
}


atomic_res_ident_t
atomic_alloc_res_ident(size_t numvars)
{
	return(do_atomic_alloc_res_ident(numvars));
}


atomic_res_ident_t
atomic_alloc_res_ident_addr(size_t numvars)
{
	return(do_atomic_alloc_res_ident(numvars));
}


/* ARGSUSED */
static atomic_reservoir_t 
atomic_alloc_null_reservoir(pmo_handle_t pm, size_t numvars,
			    atomic_var_t *var_addr, uint flags)
{
	size_t len1, len2;
	atomic_maint_t the_maint;
	atomic_var_reg_t the_var_reg;
	atomic_reservoir_t the_reservoir;
	int fd;

	the_reservoir = (atomic_reservoir_t) 
		malloc(sizeof (struct atomic_reservoir_s));

	if (the_reservoir == NULL) {
		fprintf(stderr, "atomic_alloc_reservoir: out of memory\n");
		return(NULL);
	}


	len1 = sizeof(struct atomic_maint_s) + (numvars * sizeof(uchar_t));

	fd = open("/dev/zero", O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "atomic_alloc_reservoir: can't open /dev/zero\n");
		free(the_reservoir);
		return(NULL);
	}

	the_maint = mmap(0, len1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (the_maint == (atomic_maint_t) MAP_FAILED) {
		printf("atomic_alloc_reservoir: mmap() failed\n");
		free(the_reservoir);
		return(NULL); 
	}

/*
	the_maint = (atomic_maint_t) malloc(sizeof(struct atomic_maint_s));
	if (the_maint == NULL) {
		fprintf(stderr, "atomic_alloc_reservoir: out of memory\n");
		free(the_reservoir);
		return(NULL);
	}
*/

	the_maint->bitmap_lock = LOCK_FREE;
	the_maint->bitmap_size = numvars;
	the_maint->vars_ident = NULL;


	the_var_reg = (atomic_var_reg_t) 
		malloc(sizeof(struct atomic_var_reg_s));
	if (the_var_reg == NULL) {
		fprintf(stderr, "atomic_alloc_reservoir: out of memory\n");
		munmap(the_maint, len1);
		free(the_reservoir);
		return(NULL);
	}
	the_var_reg->maint_ident = NULL;


	the_var_reg->bitmap = (uchar_t *) the_maint + 
		                   sizeof(struct atomic_maint_s);

	bzero(the_var_reg->bitmap, numvars);


	if (pm != USE_DEFAULT_PM) {
		if (pm_attach(pm, the_var_reg->bitmap, len1) == -1) {
			fprintf(stderr, "atomic_alloc_reservoir: pm_attach() failed\n");
			free(the_reservoir);
			free(the_var_reg);
			munmap(the_maint, len1);
			return(NULL);
		}
	}


	len2 = numvars * FETCHOP_VAR_SIZE;
	len2 = (getpagesize()) * (((len2/getpagesize()) + 1));

	fd = open("/dev/zero", O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "atomic_alloc_reservoir: can't open /dev/zero\n");
		free(the_reservoir);
		free(the_var_reg);
		munmap(the_maint, len1);
		return(NULL);
	}

	if (flags == ATOMIC_SET_VADDR) {
		the_var_reg->vars = mmap((void *)*var_addr, len2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	}
	else if (flags == ATOMIC_GET_VADDR) {
		the_var_reg->vars = mmap(0, len2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		*var_addr = (atomic_var_t)the_var_reg->vars;
	}
	else {
		the_var_reg->vars = mmap(0, len2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	}

	close(fd);
	if (the_var_reg->vars == (atomic_var_t *) MAP_FAILED) {
		printf("atomic_alloc_reservoir: mmap() failed\n");
		free(the_reservoir);
		free(the_var_reg);
		munmap(the_maint, len1);
		return(NULL); 
	}

	if (syssgi(SGI_FETCHOP_SETUP, the_var_reg->vars, len2) < 0) {
		/*
		 * EALREADY is the case where a peer of us has already
		 * made the call - bad return value in syssgi is there
		 * for easier potential field debugging - ignore
		 */
		if (errno != EALREADY) {
			fprintf(stderr, "failed to create fetchopable area error\n");
			perror("atomic_alloc_reservoir");
			free(the_reservoir);
			munmap(the_var_reg->vars, len2);
			free(the_var_reg);
			munmap(the_maint, len1);
			return(NULL);
		}
	}


	if (mpin(the_var_reg->vars, len2) < 0) {
		perror("failed to pin pages");
		free(the_reservoir);
		munmap(the_var_reg->vars, len2);
		free(the_var_reg);
		munmap(the_maint, len1);
		return(NULL);
	}


	if (pm != USE_DEFAULT_PM) {
		if (pm_attach(pm, the_var_reg->vars, len2) < 0) {
			fprintf(stderr, "atomic_alloc_reservoir: pm_attach() failed\n");
			free(the_reservoir);
			munpin(the_var_reg->vars, len2);
			munmap(the_var_reg->vars, len2);
			free(the_var_reg);
			munmap(the_maint, len1);
			return(NULL);
		}
	}

	the_reservoir->maint_reg = the_maint;
	the_reservoir->var_reg = the_var_reg;
	return(the_reservoir);
}


/*
 * Initialize the atomic variable reservoir by the given policy module 
 * and from the reservoir in ident.  If ident is NULL then get a new ident
 * If no particular policy modules are desired, USE_DEFAULT_PM is passed. 
 * Return: a pointer to the reservoir if successful
 *         NULL if failing to initialize
 */

/* ARGSUSED */
atomic_reservoir_t 
do_atomic_alloc_reservoir(pmo_handle_t pm, size_t numvars,
			  atomic_res_ident_t res_ident, 
                          atomic_var_t *var_addr, uint flags)
{
	int shmid;
	uint shmflag = 0777;
	size_t len;
	atomic_maint_t the_maint;
	atomic_reservoir_t the_reservoir;
	atomic_var_reg_t the_var_reg;

	if (res_ident == NULL) {
		return (atomic_alloc_null_reservoir(pm, numvars, var_addr, flags));
	}
	
	the_reservoir = (atomic_reservoir_t) 
		malloc(sizeof (struct atomic_reservoir_s));

	if (the_reservoir == NULL) {
		fprintf(stderr, "atomic_alloc_reservoir: out of memory\n");
		return(NULL);
	}

	len = sizeof(struct atomic_maint_s) + (numvars * sizeof(uchar_t));

	shmid = shmget(res_ident, len, shmflag);
	
	if (shmid == -1) {
		perror("atomic_alloc_reservoir");
		fprintf(stderr, "Has atomic_alloc_res_ident been called successfully\n");
		free(the_reservoir);
		return(NULL);
	}

	the_maint = (atomic_maint_t) shmat(shmid, 0, 0);

	if ((__int64_t)(the_maint) == -1) {
		perror("Could not attach to maintenance region");
		free(the_reservoir);
		return(NULL);
	}

	/* 
	 * Attach the policy module to the address range.
	 * Use the default policy module if told so.  In this case,
	 * nothing need to be done since the default policy module is
	 * attached at the pregion creation time.  If a different 
	 * policy module is needed, the new policy module is attached.
	 */
	if (pm != USE_DEFAULT_PM) {
		if (pm_attach(pm, the_maint, len) == -1) {
			fprintf(stderr, "atomic_alloc_reservoir: pm_attach() failed\n");
			free(the_reservoir);
			shmdt(the_maint);
			return(NULL);
		}
	}
	/* now get the region for the fetchop variables */
	len = numvars * FETCHOP_VAR_SIZE;

	shmid = shmget(the_maint->vars_ident, len, shmflag);

	if (shmid == -1) {
		perror("atomic_alloc_reservoir");
		free(the_reservoir);
		shmdt(the_maint);
		return(NULL);
	}

	the_var_reg = (atomic_var_reg_t) malloc(sizeof (struct atomic_var_reg_s));
	
	if (the_var_reg == NULL) {
		fprintf(stderr, "atomic_alloc_reservoir: out of memory\n");
		free(the_reservoir);
		shmdt(the_maint);
		return(NULL);
	}

	if (flags == ATOMIC_SET_VADDR) {
		the_var_reg->vars = (atomic_var_t *) shmat(shmid, (void *)*var_addr, 0);
	}
	else if (flags == ATOMIC_GET_VADDR) {
		the_var_reg->vars = (atomic_var_t *) shmat(shmid, 0, 0);
		*var_addr = (atomic_var_t)the_var_reg->vars;
	}
	else {
		the_var_reg->vars = (atomic_var_t *) shmat(shmid, 0, 0);
	}

	if ((__int64_t)(the_var_reg->vars) == -1) {
		perror("Could not attach reservoir variables");
		free(the_reservoir);
		shmdt(the_maint);
		free(the_var_reg);
		return(NULL);
	}

	the_var_reg->bitmap = (uchar_t *) the_maint + 
		                   sizeof(struct atomic_maint_s);

	if (syssgi(SGI_FETCHOP_SETUP, the_var_reg->vars, len) < 0) {
		/*
		 * EALREADY is the case where a peer of us has already
		 * made the call - bad return value in syssgi is there
		 * for easier potential field debugging - ignore
		 */
		if (errno != EALREADY) {
			fprintf(stderr, "failed to create fetchopable area error\n");
			perror("atomic_alloc_res_ident");
			free(the_reservoir);
			shmdt(the_maint);
			shmdt(the_var_reg->vars);
			free(the_var_reg);
			return(NULL);
		}
	}

	if (mpin(the_var_reg->vars, len) < 0) {
		perror("failed to pin pages");
		free(the_reservoir);
		shmdt(the_maint);
		shmdt(the_var_reg->vars);
		free(the_var_reg);
		return(NULL);
	}

	if (pm != USE_DEFAULT_PM) {
		if (pm_attach(pm, the_var_reg->vars, len) == -1) {
			fprintf(stderr, "atomic_alloc_reservoir: pm_attach() failed\n");
			free(the_reservoir);
			shmdt(the_maint);
			munpin(the_var_reg->vars, len);
			shmdt(the_var_reg->vars);
			free(the_var_reg);
			return(NULL);
		}
	}

	the_reservoir->maint_reg = the_maint;
	the_reservoir->var_reg = the_var_reg;
	return(the_reservoir);
}


/* ARGSUSED */
atomic_reservoir_t 
atomic_alloc_reservoir(pmo_handle_t pm, size_t numvars, atomic_res_ident_t res_ident)
{
	return (do_atomic_alloc_reservoir(pm, numvars, res_ident, NULL, NULL));
}


/* ARGSUSED */
atomic_reservoir_t 
atomic_alloc_reservoir_addr(pmo_handle_t pm, size_t numvars, 
		       atomic_res_ident_t res_ident, 
		       atomic_var_t *var_addr, uint flags)
{
	return (do_atomic_alloc_reservoir(pm, numvars, res_ident, var_addr, flags));
}


/*
 * Returns a reserved place in the bitmap array and sets the bit
 * indicating that it has been reserved.  Returns 0 on failure.
 */
static atomic_var_ident_t
atomic_do_alloc_var_ident(atomic_reservoir_t the_reservoir, int val)
{
	int i;
	
	atomic_lock(the_reservoir->maint_reg);
	
	/* we're going to number them 1 to n so 0 can indicate failure */
	for (i=1; i<=the_reservoir->maint_reg->bitmap_size; i++) {
		if (the_reservoir->var_reg->bitmap[i-1] == 0) {
			the_reservoir->var_reg->bitmap[i-1] = val;
			atomic_unlock(the_reservoir->maint_reg);
			return(i);
		}
	}

	atomic_unlock(the_reservoir->maint_reg);
	return(NULL); /* No more fetchop variables initialized */

}

atomic_var_ident_t
atomic_alloc_var_ident(atomic_reservoir_t the_reservoir)
{
	return(atomic_do_alloc_var_ident(the_reservoir, 1));
}

/*
 * return the virtual address for a given variable identifier
 * If ident is 0 then first get a valid place in the 
 * array 
 * return NULL on failure
 */
atomic_var_t *
atomic_alloc_variable(atomic_reservoir_t the_reservoir, atomic_var_ident_t ident)
{
	if (ident == NULL) {
		ident = atomic_do_alloc_var_ident(the_reservoir, 2);

		if (ident == NULL) {
			fprintf(stderr, "error: failed to obtain unique variable ident\n");
			return(NULL);
		}
	}

	/* user asked for a greater variable then we have to give out */
	if (ident > the_reservoir->maint_reg->bitmap_size)
		return(NULL);

	/* vars numbered 1 to n */
	return((atomic_var_t *)
	       ((caddr_t) the_reservoir->var_reg->vars + 
		(ident-1) * FETCHOP_VAR_SIZE));
}

int
atomic_set_perms(atomic_reservoir_t the_reservoir, atomic_perm_t perm)
{
	struct shmid_ds buf;
	int maint_ident, vars_ident;

	vars_ident = the_reservoir->maint_reg->vars_shmid;
	maint_ident = the_reservoir->maint_reg->maint_shmid;

	if (vars_ident == NULL) {
		fprintf(stderr, "atomic_set_perms valid only for fetchop regions allocated after atomic_alloc_res_ident()\n");
		return -1;
	}

	buf.shm_perm.uid = perm.uid;
	buf.shm_perm.gid = perm.gid;
	buf.shm_perm.mode = perm.mode;

	if (shmctl(maint_ident, IPC_SET, &buf) < 0) {
		perror("atomic_set_perms");
		return -1;
	}
	if (shmctl(vars_ident, IPC_SET, &buf) < 0) {
		perror("atomic_set_perms");
		return -1;
	}
	return 0;
	
}

/* Return the atomic variable back to the reservoir */
void
atomic_free_variable(atomic_reservoir_t the_reservoir, atomic_var_t *the_var)
{
	size_t index;

	/* Get the index in the reservoir */
	index = (the_var - the_reservoir->var_reg->vars) / FETCHOP_VAR_SIZE;

	if (index > the_reservoir->maint_reg->bitmap_size) {
		fprintf(stderr, "atomic_free_variable: identifier out of range\n");
		return;
	}
	/* I don't think we need to lock here 64 bits archs, are they all? */
	atomic_lock(the_reservoir->maint_reg);
	if (the_reservoir->var_reg->bitmap[index] == 1)
		;  /* do nothing user alloced it will next call free_var_ident */
	else {
		the_reservoir->var_reg->bitmap[index] = 0;
	}
	atomic_unlock(the_reservoir->maint_reg);
}

void
atomic_free_var_ident(atomic_reservoir_t the_reservoir, atomic_var_ident_t the_ident)
{
	if (the_ident > the_reservoir->maint_reg->bitmap_size) {
		fprintf(stderr, "atomic_free_var_ident: identifier out of range\n");
		return;
	}
	/* I don't think we need to lock here 64 bits archs, are they all? */
	atomic_lock(the_reservoir->maint_reg);
	the_reservoir->var_reg->bitmap[the_ident] = 0;
	atomic_unlock(the_reservoir->maint_reg);
}

/* remove the shared memory segments associated with this reservoir */
void
atomic_free_reservoir(atomic_reservoir_t the_reservoir)
{
	int maint_ident, vars_ident;

	vars_ident = the_reservoir->maint_reg->vars_shmid;
	maint_ident = the_reservoir->maint_reg->maint_shmid;

	shmctl(vars_ident, IPC_RMID);
	shmctl(maint_ident, IPC_RMID);
}

/* Store a value in the atomic variable */
void
atomic_store(atomic_var_t *the_variable, atomic_var_t mask)
{
#if SN0 
	FETCHOP_STORE_OP(the_variable, FETCHOP_STORE, mask);
#else /* !SN0 */
	*the_variable = mask; /* long assignment is atomic */
#endif /* SN0 */ 
}

/* Logical OR operation */
void
atomic_store_and_or(atomic_var_t *the_variable, atomic_var_t mask)
{
#if SN0 
	FETCHOP_STORE_OP(the_variable, FETCHOP_OR, mask);
#else /* ! SN0 */
	test_then_or((ulong_t *)the_variable, (ulong_t)mask);
#endif /* SN0 */
}

/* Logical AND operation */
void
atomic_store_and_and(atomic_var_t *the_variable, atomic_var_t mask)
{
#if SN0 
	FETCHOP_STORE_OP(the_variable, FETCHOP_AND, mask);
#else /* ! SN0 */
	test_then_and((ulong_t *)the_variable, (ulong_t)mask);
#endif /* SN0 */
}

/* Load the value of the atomic variable */
atomic_var_t
atomic_load(atomic_var_t *the_variable)
{
#if SN0 
	return(FETCHOP_LOAD_OP(the_variable, FETCHOP_LOAD));
#else /* ! SN0 */
	return((ulong_t)*the_variable); 
#endif /* SN0 */
}


/*
 * Increment the variable
 * Return: the old value stored in the variable
 */
atomic_var_t 
atomic_fetch_and_increment(atomic_var_t *the_variable)
{
#if SN0 
	return(FETCHOP_LOAD_OP(the_variable, FETCHOP_INCREMENT));
#else /* ! SN0 */
	return(test_then_add((ulong_t *)the_variable, 1));
#endif /* SN0 */
}

/*
 * Decrement the variable
 * Return: the old value stored in the variable
 */
atomic_var_t
atomic_fetch_and_decrement(atomic_var_t *the_variable)
{
#if SN0 
	return(FETCHOP_LOAD_OP(the_variable, FETCHOP_DECREMENT));
#else /* ! SN0 */
	return(test_then_add((ulong_t *)the_variable, -1));
#endif /* SN0 */
}

/*
 * Clear the variable
 * Return: the old value stored in the variable
 */
atomic_var_t
atomic_clear(atomic_var_t *the_variable)
{
#if SN0 
	return(FETCHOP_LOAD_OP(the_variable, FETCHOP_CLEAR));
#else /* ! SN0 */
	return(test_then_and((ulong_t *)the_variable, 0));
#endif /* SN0 */
}


static void
atomic_lock(atomic_maint_t the_maint)
{
	while (test_and_set32(&(the_maint->bitmap_lock), LOCK_HELD) != 
	       LOCK_FREE) {
		;
	}
}

static void
atomic_unlock(atomic_maint_t the_maint)
{
	test_and_set32(&(the_maint->bitmap_lock), LOCK_FREE);
}
