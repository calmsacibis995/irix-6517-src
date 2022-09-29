#include	<stdio.h>
#include	<sys/types.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<assert.h>
#include	<sys/syssgi.h>
#include	<sys/grio.h>
#include	<sys/capability.h>

/* 
*  locality.c:
*
*  file containing wrappers over NUMA routines so that grio library 
*  can keep track of NUMA stuff
*/ 


/* 
* grio_mldset_place():
*
* place the MLD on an appropriate set of nodes 
*/
int
grio_mldset_place(pmo_handle_t	mldset, topology_type_t topology,
		raff_info_t *rafflist, int rafflist_len)
{
	int			status;
	rqmode_t		req_mode;
	
	if(rafflist)
		req_mode = RQMODE_MANDATORY;
	else 
		req_mode = RQMODE_ADVISORY;

	status = mldset_place(mldset, topology, rafflist, 
			rafflist_len, req_mode);

	if(status < 0) {
		perror("grio_mldset_place failed");
	}

	return status;
}



/* 
* grio_memory_init():
*
*	returns	
*		0 on success
*		-1 on failure
*
* set up the MLDs, and return status to the user
*	the user can optionally state a hwgraph path name as the
*		"resource" parameter., and the number of bytes in the
*		path name as the res_len pameter; 
*/
int
grio_memory_init(long mem_sz, char *resource, 
					grio_mem_locality_t *cookie)
{
	raff_info_t	*rafflist;
	topology_type_t	topology;
	int		raflist_len;
	pmo_handle_t	grio_mld, grio_mldset;
	cap_t		ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;


	if(NULL == cookie)  {
		printf("No cookie received in grio_memory_init\n");
		return -1;
	}

	/* create an MLD of radius 0 */
	grio_mld = mld_create(0, mem_sz);
	if(grio_mld < 0)  {
		perror("grio_memory_init: mld_create returned error");
		return -1;
	}

	/* create a singleton MLDset with this MLD */
	grio_mldset = mldset_create(&(grio_mld), 1);
	if(grio_mldset < 0) {
		perror("grio_memory_init: mldset_create returns error");
		return -1;
	}


	/* place the MLDset */
	if(resource)  {
		topology = TOPOLOGY_PHYSNODES;
		rafflist = (raff_info_t *) malloc(sizeof(raff_info_t));
		assert(rafflist != NULL);
		rafflist->resource = resource;
		rafflist->reslen = (unsigned short) 
						strlen(resource);
		rafflist->restype = RAFFIDT_NAME;
		rafflist->radius = 0;
		rafflist->attr = RAFFATTR_ATTRACTION;
		raflist_len = 1;
	}
	else {
		rafflist = NULL;
		topology = TOPOLOGY_FREE;
	}

	if(grio_mldset_place(grio_mldset, topology, 
				rafflist, raflist_len) < 0)  {
		perror("grio_memory_init: internal error in mldset_place");
		return -1;
	}
	
	ocap = cap_acquire(1, &cap_device_mgt);
	if(syssgi(SGI_GRIO, GRIO_GET_MLD_CNODE, &grio_mld, 
					&(cookie->grio_cnode)))  {
		cap_surrender(ocap);
		perror("grio_memory_init: internal error; MLD wasn't placed");
		return -1;
	}
	cap_surrender(ocap);


	/* link the process */
	if(process_mldlink(getpid(), grio_mld, RQMODE_MANDATORY)  < 0)  {
		perror("grio_memory_init: process_mldlink returns error");
		return -1;
	}
	
	return 0;
}

