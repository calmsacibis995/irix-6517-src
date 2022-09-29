#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/include/RCS/swconfig.h,v 1.1 1999/05/08 03:19:00 tjm Exp $"

/**
 ** Function prototypes
 **/

int get_swcomponents(
	configmon_t *       /* pointer to configmon_s struct */);

int select_swcomponents(
	configmon_t *		/* pointer to configmon_s struct */);

sw_component_t *get_next_swcomponent(
	configmon_t *		/* pointer to configmon_s struct */);

sw_component_t *find_swcmp(
	sw_component_t *	/* pointer to root of swcomponent tree */, 
	int 				/* record key */);

int remove_swcmp(
	sw_component_t **	/* pointer to pointer to root of swcomponent tree */, 
	int 				/* record key */);
