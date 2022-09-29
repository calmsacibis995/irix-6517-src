#ident "$Header: "

/*
 * hw_component_s c_state values
 */
#define MATCH_FOUND			1

/* Flag to indicate that hwcomponent one should be inserted after 
 * hwcomponent two. 
 */
#define INSERT_AFTER        1

/* Function prototypes
 */
hw_component_t *dup_hwcomponent(
	hwconfig_t *		/* hwconfig_s pointer */, 
	hw_component_t *	/* hw_component to dup */);

hw_component_t *next_hwcmp(
	hw_component_t *	/* current hw_component_s pointer */);

hw_component_t *prev_hwcmp(
	hw_component_t *	/* current hw_component_s pointer */);

void hwcmp_insert_next(
	hw_component_t *    /* current hw_component_s pointer */,
	hw_component_t *    /* next hw_component_s pointer */);

void hwcmp_add_child(
	hw_component_t *    /* current hw_component_s pointer */,
	hw_component_t *    /* pointer to child to add */);

void hwcmp_add_peer(
	hw_component_t *    /* current hw_component_s pointer */,
	hw_component_t *    /* pointer to peer to add */);

void replace_hwcomponent(
	hw_component_t *    /* old hw_component_s pointer */,
	hw_component_t *    /* new hw_component_s pointer */);

void insert_hwcomponent(
	hw_component_t *	/* hw_component_s pointer */, 
	hw_component_t *	/* hw_component_s pointer */, 
	int 				/* flag */);

void unlink_hwcomponent(
	hw_component_t *	/* hw_component_s pointer */);

void free_next_hwcmp(
	hw_component_t *	/* hw_component_s pointer */); 

void free_hwcomponents(
	hw_component_t *    /* hw_component_s pointer */);

hw_component_t *hw_find_location(
	hw_component_t *	/* pointer to hw_component_s list */, 
	hw_component_t *	/* hwconfig_s pointer */);

hw_component_t *hw_find_insert_point(
	hw_component_t *	/* pointer to hw_component_s list */, 
	hw_component_t *	/* hwconfig_s pointer */, 
	int *				/* pointer to location flag */);

int compare_hwcomponents(
	hw_component_t *	/* first hwcomponent */, 
	hw_component_t *	/* second hwcomponent */);

hwconfig_t *kl_alloc_hwconfig(
	int                 /* flags */);

int kl_update_hwconfig(
	hwconfig_t *		/* hwconfig_s pointer */);

void kl_free_hwconfig(
	hwconfig_t *        /* hwconfig_s pointer */);

char *inv_description(
	invent_data_t *		/* inventory data struct */, 
	string_table_t *	/* pointer to strin_table_s struct (NULL if none) */, 
	int 				/* block allocation flag */);

char *inv_name(
	hw_component_t *	/* pointer to hw_component_s struct */, 
	string_table_t *	/* string table pointer */, 
	int					/* block allocation flag */);

char *cpu_name(
	unsigned int 		/* revision */);

char *hwcp_description(	
	hw_component_t *	/* pointer to hw_component_s struct */, 
	string_table_t *	/* pointer to string table */, 
	int 				/* block allocation flag */);

