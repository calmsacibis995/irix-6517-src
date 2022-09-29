#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/libswconfig/src/RCS/kl_libswconfig.c,v 1.2 1999/05/08 11:01:44 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <klib/klib.h>

/*
 * free_swcomponent()
 */
void
free_swcomponent(sw_component_t *swcp)
{
	if (swcp->sw_name) {
		kl_free_block(swcp->sw_name);
	}
	if (swcp->sw_description) {
		kl_free_block(swcp->sw_description);
	}
	kl_free_block(swcp);
}

/*
 * free_swcomponents()
 */
void
free_swcomponents(sw_component_t *swcp)
{
	if (swcp->sw_left) {
		free_swcomponents((sw_component_t *)swcp->sw_left);
	}
	if (swcp->sw_right) {
		free_swcomponents((sw_component_t *)swcp->sw_right);
	}
	free_swcomponent(swcp);
}

/*
 * add_swcomponent()
 */
int
add_swcomponent(sw_component_t **root, sw_component_t *swcp)
{
	int ret;

	/* If there isn't any root node, make swcp the root node
	 */
	if (!(*root)) {
		*root = swcp;
		return(0);
	}
	ret = insert_btnode((btnode_t **)root, (btnode_t *)swcp, 0);
	if (ret != -1) {
		return(0);
	}
	return(1);
}

/*
 * first_swcomponent()
 */
sw_component_t *
first_swcomponent(sw_component_t *swcp_root)
{
	return((sw_component_t *)first_btnode((btnode_t *)swcp_root));
}

/*
 * next_swcomponent()
 */
sw_component_t *
next_swcomponent(sw_component_t *swcp)
{
	return((sw_component_t *)next_btnode((btnode_t *)swcp));
}

/*
 * prev_swcomponent()
 */
sw_component_t *
prev_swcomponent(sw_component_t *swcp)
{
	return((sw_component_t *)prev_btnode((btnode_t *)swcp));
}

/*
 * find_swcomponent()
 */
sw_component_t *
find_swcomponent(sw_component_t *root, char *name)
{
	sw_component_t *swcp;

	swcp = (sw_component_t *)find_btnode((btnode_t *)root, name, 0);
	return(swcp);
}

/*
 * unlink_swcomponent()
 */
void
unlink_swcomponent(sw_component_t **root, sw_component_t *swcp)
{
	delete_btnode((btnode_t **)root, (btnode_t *)swcp, NULL, 0);
}

/*
 * alloc_swconfig()
 */
swconfig_t *
alloc_swconfig(int flags)
{
	swconfig_t *scp;

	scp = (swconfig_t *)kl_alloc_block(sizeof(swconfig_t), ALLOCFLG(flags));
	scp->s_flags = flags;
	return(scp);
}

/* 
 * get_swconfig()
 */
int
get_swconfig(swconfig_t *scp)
{
	int i, flag = ALLOCFLG(scp->s_flags);
	FILE *ip;
	char s[256];
	char *state, *name, *version, *date, *desc, *p;
	sw_component_t *swcp, *root = (sw_component_t *)NULL;
	time_t curtime, test_curtime;

	system("/usr/sbin/showprods -E -D 1 > /usr/tmp/prods.tmp");
	if (!(ip = fopen("/usr/tmp/prods.tmp", "r"))) {
		return(1);
	}
	i = 0;
	while (fgets(s, 256, ip)) {
		if (i++ > 2) {
			state = s;
			p = strchr(s, ' ');
			while (*p == ' ') {
				*p = 0;
				p++;
			}
			name = p;

			p = strchr(name, ' ');
			while (*p == ' ') {
				*p = 0;
				p++;
			}
			date = p;

			p = strchr(date, ' ');
			while (*p == ' ') {
				*p = 0;
				p++;
			}
			desc = p;
			p = strchr(desc, '\n');
			*p = 0;
			
			swcp = kl_alloc_block(sizeof(sw_component_t), flag);
			swcp->sw_name = str_to_block(name, flag);
			swcp->sw_description = str_to_block(desc, flag);
			swcp->sw_install_time = str_to_ctime(date);
			add_swcomponent((sw_component_t **)&root, swcp);
		}
	}
	fclose(ip);

	system("/usr/sbin/showprods -n -E -D 1 > /usr/tmp/prods.tmp");
	if (!(ip = fopen("/usr/tmp/prods.tmp", "r"))) {
		free_swcomponents(root);
		return(1);
	}
	i = 0;
	while (fgets(s, 256, ip)) {
		if (i++ > 2) {
			state = s;
			p = strchr(s, ' ');
			while (*p == ' ') {
				*p = 0;
				p++;
			}
			name = p;

			p = strchr(name, ' ');
			while (*p == ' ') {
				*p = 0;
				p++;
			}
			version = p;

			p = strchr(version, ' ');
			while (*p == ' ') {
				*p = 0;
				p++;
			}
			desc = p;
			if (swcp = (sw_component_t*)
						find_btnode((btnode_t *)root, name, (int *)NULL)) {
				swcp->sw_version = strtoul(version, (char **)NULL, 0);
			}
			else {
				fprintf(stderr, "ERROR: could not find %s\n", name);
			}
		}
	}
	scp->s_swcmp_root = root;
	return(0);
}

/*
 * free_swconfig()
 */
void
free_swconfig(swconfig_t *scp)
{
	if (scp) {
		if (scp->s_swcmp_root) {
			free_swcomponents(scp->s_swcmp_root);
		}
		kl_free_block(scp);
	}
}
