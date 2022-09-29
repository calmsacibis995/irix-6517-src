#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/RCS/sysinfo.c,v 1.5 1999/05/27 23:13:43 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/systeminfo.h>
#include <klib/klib.h>
#include <sss/configmon.h>

/*
 * get_sysid()
 */
k_uint_t
get_sysid(void)
{
	int ret_val;
	module_info_t mod_info;

	ret_val = get_module_info(0, &mod_info, sizeof(module_info_t));
	if (ret_val >= 0) {
		return(mod_info.serial_num);
	}
	return(0);
}

/*
 * check_sysid()
 */
int 
check_sysid(int sys_id)
{
	int i, ret_val, num_modules;
	module_info_t mod_info;

	num_modules = get_num_modules();
	if (num_modules < 0) {
		return(0);
	}
	for (i = 0; i < num_modules; i++) {
		ret_val = get_module_info(i, &mod_info, sizeof(module_info_t));
		if (ret_val >= 0) {
			if (sys_id == mod_info.serial_num) {
				return(1);
			}
		}
	}
	return(0);
}

/*
 * print_sysid()
 */
void
print_sysid(void)
{
	int i, ret_val, num_modules;
	module_info_t mod_info;

	num_modules = get_num_modules();
	if (num_modules < 0) {
		printf("NO MODULES!\n");
	}
	for (i = 0; i < num_modules; i++) {
		ret_val = get_module_info(i, &mod_info, sizeof(module_info_t));
		if (ret_val >= 0) {
			printf("%llu ",mod_info.serial_num);
			printf("%s ", mod_info.serial_str);
			printf("Module: %d\n",mod_info.mod_num);
		}
		else {
			printf("ret_val = %d!\n", ret_val);
		}
	}
}

/* 
 * qualified_hostname()
 */
int
qualified_hostname(char *hostname, char *str)
{
	int len;

	len = strlen(hostname);
	if (strlen(str) < len) {
		return(0);
	}
	/* Make sure that the hostname is located at the start of the
	 * string and that no additional characters exist. Also make sure
	 * that the host name string contains a a dot ('.') after the
	 * hostname.
	 */
	if (!strncasecmp(str, hostname, len)) {
		if (str[len] == '.') {
			return(1);
		}
	}
	return(0);
}

/*
 * get_hostname()
 */
char *
get_hostname(char *name, int flag)
{
	int i = 0, len;
	struct hostent *hostentp;
	char hostname[256], *hnp = (char*)NULL;
	char *cp;

	if (name) {
		strcpy(hostname, name);
	}
	else {
		if (gethostname(hostname, 256)) {
			return((char *)NULL);
		}
	}
	if (!hostname[0]) {
		return((char *)NULL);
	}
	if (hostentp = gethostbyname(hostname)) {

		/* Check to see if the h_name field contains a pointer
		 * to a fully qualified hostname.
		 */
		if (qualified_hostname(hostname, hostentp->h_name)) {
			hnp = str_to_block(hostentp->h_name, flag);
			return(hnp);
		}

		/* We have to check for aliases 
		 */
		if (hostentp->h_aliases) {
			cp = hostentp->h_aliases[0];
		}
		else {
			hnp = str_to_block(hostentp->h_name, flag);
			return(hnp);
		}

		i = 1;
		while (cp) {
			if (qualified_hostname(hostname, cp)) {
				hnp = str_to_block(cp, flag);
				return(hnp);
			}
			cp = hostentp->h_aliases[i++];
		}
	}
	hnp = str_to_block(hostname, flag);
	return(hnp);
}

/*
 * get_sysinfo()
 */
system_info_t *
get_sysinfo(int bounds, int flag)
{
	int ret_val;
	int hostid;
	module_info_t mod_info;
	system_info_t *sysinfo;
	char hostname[256];
	time_t curtime;

	curtime = time(0);
	sysinfo = (system_info_t *)kl_alloc_block(sizeof(system_info_t), flag);

	if (bounds < 0) {
		ret_val = get_module_info(0, &mod_info, sizeof(module_info_t));
		if (ret_val >= 0) {
			sysinfo->sys_id = mod_info.serial_num;
			sysinfo->sys_type = K_IP;
			if (mod_info.serial_str[0]) {
				sysinfo->serial_number = 
					str_to_block(mod_info.serial_str, flag);
			}
			else {
				sysinfo->serial_number= str_to_block("Unknown", flag);
			}
			if (!gethostname(hostname, 256)) {
				sysinfo->hostname = get_hostname(hostname, flag);
			}
			else {
				sysinfo->hostname = str_to_block("Unknown", flag);
			}
			hostid = gethostid();
			sysinfo->ip_address = kl_alloc_block(16, flag);
			sprintf(sysinfo->ip_address, "%d.%d.%d.%d",
				(hostid >> 24) & 0xff, (hostid >> 16) & 0xff,
				(hostid >> 8) & 0xff, hostid & 0xff);
			sysinfo->active = 1;
			sysinfo->local = 1;
			sysinfo->time = curtime;
		}
	}
	else {
		switch (bounds) {

			/* For debugging purposes only. Hard code the system info data 
			 * for a particular vmcore image.
			 */

			default :
				return((system_info_t *)NULL);
		}
	}
	sysinfo->time = curtime;
	return(sysinfo);
}

/*
 * sysinfo_compare()
 *
 * Returns non-zero if there is any data in the two system_info_s
 * structs that do not match.
 */
int
sysinfo_compare(system_info_t *s1, system_info_t *s2)
{

	if (s1->sys_id != s2->sys_id) {
		return(1);
	}

	/* Make sure we don't do a string compare if one of the serial_number
	 * string pointers is NULL...if BOTH are NULL, it's OK since nothing
	 * changed (sort of). Actually, this is a lot of checking for NULL
	 * pointers that should never occur, but we ran into this problem 
	 * before and it doesn't hurt to play it safe.
	 */
	if (s1->serial_number && s2->serial_number) {
		if (strcmp(s1->serial_number, s2->serial_number)) {
			return(1);
		}
	}
	else {
		if (s1->serial_number || s2->serial_number) {
			return(1);
		}
		/* Both are NULL --  treat as same */
	}

	if (s1->hostname && s2->hostname) {
		if (strcmp(s1->hostname, s2->hostname)) {
			return(1);
		}
	}
	else {
		if (s1->hostname || s2->hostname) {
			return(1);
		}
		/* Both are NULL --  treat as same */
	}

	if (s1->ip_address && s2->ip_address) {
		if (strcmp(s1->ip_address, s2->ip_address)) {
			return(1);
		}
	}
	else {
		if (s1->ip_address || s2->ip_address) {
			return(1);
		}
		/* Both are NULL --  treat as same */
	}
	return(0);
}

/*
 * check_sysinfo()
 *
 * Compares the contents of sysinfo with a sysinfo struct containing
 * data from the live system. If the sys_id on the live system does
 * not match, a check is made to see if it is still in the system
 * (for Origin systems only). If it is, the current record is adjusted
 * so that the old sys_id and serial_number are used.
 */
system_info_t *
check_sysinfo(system_info_t *sysinfo, int bounds, int flag)
{
	system_info_t *sinfo;

	if (!(sinfo = get_sysinfo(bounds, flag))) {
		return((system_info_t *)NULL);
	}
	if (sysinfo->sys_id != sinfo->sys_id) {
		if (check_sysid(sysinfo->sys_id)) {
			sinfo->sys_id = sysinfo->sys_id;
			if (sinfo->serial_number) {
				kl_free_block(sinfo->serial_number);
			}
			sinfo->serial_number = str_to_block(sysinfo->serial_number, flag);
		}
		else {
			/* The current system is not the same as the one represented
			 * in the database!
			 */
			return(sinfo);
		}
	}
	if (sysinfo_compare(sysinfo, sinfo)) {
		/* One or more pieces of system info was different. We return
		 * the new system_info_s struct.
		 */
		return(sinfo);
	}

	/* The system info is the same.
	 */
	free_system_info(sinfo);
	return((system_info_t *)NULL);
}
