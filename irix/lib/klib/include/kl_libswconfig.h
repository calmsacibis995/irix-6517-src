#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_libswconfig.h,v 1.1 1999/02/23 20:38:33 tjm Exp $"

/**
 ** Function prototypes
 **/

void free_swcomponent(
	sw_component_t *	/* pointer to swcomponent to free */);

void free_swcomponents(
	sw_component_t *	/* root of swcomponent tree */);

int add_swcomponent(
	sw_component_t **   /* pointer to pointer to root */,
	sw_component_t *    /* sw_component to add */);

sw_component_t *first_swcomponent(
	sw_component_t * 	/* sw_component root */);

sw_component_t *next_swcomponent(
	sw_component_t *	/* pointer to sw_component */);

sw_component_t *prev_swcomponent(
	sw_component_t *	/* pointer to sw_component */);

sw_component_t *find_swcomponent(
	sw_component_t * 	/* sw_component root */,
	char *				/* key (name) */);

void unlink_swcomponent(
	sw_component_t ** 	/* poibter to pointer to sw_component root */,
	sw_component_t *	/* pointer to sw_component to unlink */);

swconfig_t *alloc_swconfig(
	int				/* flag values */);

int get_swconfig(
	swconfig_t *		/* pointer to swconfig_s struct */);

void free_swconfig(
	swconfig_t *		/* pointer to swconfig_s struct */);
