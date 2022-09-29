#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/include/RCS/hwconfig.h,v 1.1 1999/05/08 03:19:00 tjm Exp $"

int select_hwcomponents(
	configmon_t *		/* pointer to configmon_s struct */);

hw_component_t *get_next_hwcomponent(
	configmon_t *		/* pointer to configmon_s struct */);

int get_hwcomponents(
	configmon_t *		/* pointer to configmon_s struct */);

hw_component_t *find_hwcmp(
    hw_component_t *    /* hw_component_s pointer (root) */,
    int                 /* key */);

hw_component_t *find_sub_hwcmp(
    hw_component_t *    /* hw_component_s pointer (root) */,
    int                 /* parent_key */,
    char *              /* location */);

int remove_hwcomponent(
    hw_component_t *    /* hw_component_s pointer (root) */,
    int                 /* key */);

