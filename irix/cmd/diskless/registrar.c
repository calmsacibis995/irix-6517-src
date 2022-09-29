/*
 *	registrar.c	7/27/88		initial release
 */

#include <stdio.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <sys/file.h>
#include <sys/time.h>
#include <syslog.h>
#include "hostreg.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define REG_STRLEN	256
#define DONOTWAIT	1

char	hostbyname[] = "hosts.byname";
char	*domain, hostbyaddr[] = "hosts.byaddr";
char	t_ipadr[REG_STRLEN];	/* new allocated ip address */
struct	in_addr	ipadr_ptr; 	/* spot for insertion */
int	found = 0; 		/* 0: not found, 1: preinsert, 2: postinsert */
int	line_no, use_comment = 0;
int	debug = 0;

void
cexit(int stat)
{
	printf("%d", stat);
	exit(stat);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	char	name[MAXNETNAMELEN + 1], *data_p;
	char	key[120], *data, host[33];
	int	keylen, datalen, op, err, i, arg_index;
	char	*fname, *tmpname;
	reg_info	*reg_data;

	openlog("registrar", LOG_PID, LOG_USER);

	arg_index = 1;
	if (argc != arg_index + 2) {
		syslog(LOG_ERR, "bad command line");
		cexit(YPERR_YPERR);
	}

	fname = argv[arg_index];
	tmpname = malloc(strlen(fname) + 4);
	if (tmpname == NULL) {
		syslog(LOG_ERR, "out of memory");
		cexit(YPERR_YPERR);
	}
	sprintf(tmpname, "%s.tmp", fname);
	
	/*
	 * Get input
	 */
	if (!scanf("%s\n", name))
		cexit(YPERR_YPERR);	
	if (!scanf("%u\n", &op) || op == YPOP_STORE)
		cexit(YPERR_YPERR);
	if (!scanf("%u\n", &keylen))
		cexit(YPERR_YPERR);
	if (!fread(key, keylen, 1, stdin))
		cexit(YPERR_YPERR);
	for (i = 0; i < keylen; i++)
		if (key[i] == ' ' || key[i] == '\t')
			cexit(YPERR_BADARGS);
	key[keylen] = 0;

	if (!scanf("%u\n", &datalen) || datalen != sizeof(reg_info))
		cexit(YPERR_YPERR);

	/* for long aligned and 0 at end */
	data = malloc(sizeof(reg_info) + 4);
	data_p = (char *)RNDUP((int)data);
	if (!fread(data_p, datalen, 1, stdin)) {
		cexit(YPERR_YPERR);
	}
	reg_data = (reg_info *)data_p;

	if (yp_get_default_domain(&domain))
		return (YPERR_YPERR);

	/*
	 * Check permission
	 */
	gethostname(host, sizeof(host));
	if ((err = check_permission(host, reg_data))) {
		syslog(LOG_INFO, "permission denied");
		cexit(err);
	}

	/* t_ipadr is set by check_dup if op == CHAGNE or DELETE */
	if (err = check_dup(key, op, reg_data)) {
		syslog(LOG_INFO, "key or alias is used already");
		cexit(err);
	}

	/*
	 * Allocate IP address
	 */
	/* t_ipadr is set in allocate_ipaddr if op == RENAME */
	if (reg_data->action == REG_NEWNAME) {
		if (err = allocate_ipaddr(reg_data, fname, tmpname))
			cexit(err);
	}

	/*
	 * Modify host file
	 */
	if (err = mod_hostf(op, reg_data, fname, tmpname, key))
		cexit(YPERR_YPERR);

	err = fork();
	if (err < 0) {
		syslog(LOG_INFO, "forking ypmake failed");
		cexit(YPERR_YPERR);
	}
	if (err == 0) {
		close(0); close(1); close(2);
		open("/dev/null", O_RDWR, 0);
		dup(0); dup(0);
		err = execl("/bin/sh", "sh", "-c", argv[arg_index+1], NULL);
		syslog(LOG_ERR, "execl \"%s\" returned(%d)",
			argv[arg_index + 1], err);
		exit(err);
	}

	if (debug)
		syslog(LOG_INFO, "RETURN ipaddr=%s", t_ipadr);
	/* return ipaddr */
	err = (int)inet_addr(t_ipadr);
	cexit(err);
}

int
mod_hostf(op, data, fname, tmpname, key)
int		op;
reg_info	*data;
char		*fname, *tmpname, *key;
{
	FILE	*rf, *wf;
	char	line[REG_STRLEN], ipadr[50], new_line[REG_STRLEN];
	u_long	cur_net, new_net, ip_adr, insert_addr;
	int	err, done, netwk;

	rf = fopen(fname, "r");
	if (rf == NULL) {
		syslog(LOG_INFO, "mod_hostf: open %s failed", fname);
		return(YPERR_YPERR);
	}

	wf = fopen(tmpname, "w");
	if (wf == NULL) {
		syslog(LOG_INFO, "mod_hostf: open %s failed", tmpname);
		fclose(rf);
		return(YPERR_YPERR);
	}

	if ((insert_addr = inet_addr(t_ipadr)) == -1) {
		syslog(LOG_INFO, "mod_hostf: bad ipaddr=%s", t_ipadr);
		fclose(rf);
		fclose(wf);
		return(YPERR_YPERR);
	}

	done = 0;
	while (fgets(line, sizeof(line), rf)) {
		if (use_comment) {
			fputs(line, wf);
			if (--line_no == 0) {
				fprintf(wf, "%s	%s %s\n", t_ipadr, key,
					data->aliases);
				done = 1;
			}
			continue;
		}

		/* skip blanks line and comment lines*/
		if ((sscanf(line, "%s", ipadr) != 1) ||
		   (ipadr[0] == '#') || done) {
			fputs(line, wf);
			continue;
		}

		if (op != YPOP_INSERT) {
			if (inet_addr(ipadr) == insert_addr) {
				done = 1;
				switch (op) {
				case YPOP_CHANGE:
					fprintf(wf, "%s	%s %s\n", t_ipadr,
						data->info_rename,
						data->aliases);	
					break;
				case YPOP_DELETE:
				default:
					/* do nothing */
					break;
				}
			} else
				fputs(line, wf);
		} else if (!found) {
			/*
			 * attach to the end, otherwise, it may go behind
			 * other groups comment.
			 */
			fputs(line, wf);
		} else {
			if (inet_addr(ipadr) == ipadr_ptr.s_addr) {
				if (debug)
				 syslog(LOG_INFO,
				  "MOD_HOSTF: found=%d, ipadr=%s, ipadr_ptr=%s",
					found, ipadr, inet_ntoa(ipadr_ptr));
				if (found == 1) {
					fprintf(wf, "%s	%s %s\n",
						t_ipadr, key, data->aliases);
				}
				fputs(line, wf);
				if (found == 2) {
					fprintf(wf, "%s	%s %s\n",
						t_ipadr, key, data->aliases);
				}
				done = 1;
			} else
				fputs(line, wf);
		}
	}

	/* use_comment case append */
	if (!done) {
		fprintf(wf, "%s	%s %s\n", t_ipadr, key, data->aliases);
		syslog(LOG_INFO, "mod_hostf: (%s %s %s) appended to %s",
			t_ipadr, key, data->aliases, fname);
	}

	fclose(wf);
	fclose(rf);

	if (chmod(tmpname, 0644) < 0) {
		syslog(LOG_INFO, "mod_hostf: can't chmod %s to 0644",
				tmpname);
		return(YPERR_YPERR);
	}

	if (rename(tmpname, fname) < 0) {
		syslog(LOG_INFO, "mod_hostf: can't rename %s to %s",
				tmpname, fname);
		return(YPERR_YPERR);
	}

	return (0);
}


check_dup(name, op, data)
char		*name;
int		op;
reg_info	*data;
{
	char	nme[32], *result, *c_ptr;
	int	i, err = 0, result_len;
	u_long	ipadr;

	if (yp_match(domain, hostbyname, name, strlen(name), &result,
		&result_len))
		err = YPERR_YPERR;

	switch (op) {
	case YPOP_DELETE:
		if (err) {
			syslog(LOG_INFO,
				"check_dup: DELETE: %s not exists", name);
			return(err);
		}
		for (i = 0; result[i] != 0 && result[i] != ' ' &&
			result[i] != '\t'; i++)
			t_ipadr[i] = result[i];
		t_ipadr[i] = 0;
		if (debug)
			syslog(LOG_INFO,
				"check_dup: DELETE: %s(%s)", name, t_ipadr);
		break;

	case YPOP_CHANGE:
		if (err) { 
			syslog(LOG_INFO,
				"check_dup: CHANGE: %s not exists", name);
			return (err);
		}
		for (i = 0; result[i] != 0 && result[i] != ' ' &&
			result[i] != '\t'; i++)
			t_ipadr[i] = result[i];
		t_ipadr[i] = 0;
		for (c_ptr = data->aliases; get_nextname(&c_ptr, nme);) {
			if (yp_match(domain, hostbyname, nme, strlen(nme),
				&result, &result_len))
				continue;
			if (strncmp(t_ipadr, result, strlen(t_ipadr))) {
				syslog(LOG_INFO,
					"check_dup: CHANGE: %s already exists",
					name);
				return(YPERR_ACCESS);
			}
		}
		if (debug)
			syslog(LOG_INFO,
			    "check_dup: CHANGE: %s(%s)=>%s\n",
				name, t_ipadr, data->aliases);
		break;

	case YPOP_INSERT:
		if (!err) {
			syslog(LOG_INFO,
				"check_dup: INSERT: %s already exists", name);
			return (YPERR_ACCESS);
		}
		for (c_ptr = data->aliases; get_nextname(&c_ptr, nme);) {
		    if (!yp_match(domain, hostbyname, nme, strlen(nme),
			&result, &result_len)) {
			syslog(LOG_INFO,
			"check_dup: INSERT: alias %s exists already for %s\n",
			nme, name);
			return (YPERR_ACCESS);
		    }
		}
		if (debug)
			syslog(LOG_INFO,
				"check_dup: INSERT: %s\n", name);
		break;

	default:
		syslog(LOG_INFO, "check_dup: unknown op(%d)", op);
		return (YPERR_YPERR);
	}

	return (0);
}


int
check_permission(host, data)
char		*host;
reg_info	*data;
{
	FILE	*fid;
	char	line[REG_STRLEN], *end, *pwd;

	if (data->action == REG_NEWNAME)
		return(0);

	if ((fid = fopen("/etc/passwd", "r")) == NULL)
		return(YPERR_ACCESS);
	while (fgets(line, sizeof(line), fid))
	    if (strncmp(line, "root:", 5) == 0) {
		pwd = strchr(line, ':');
		pwd++;
		if (*pwd != ':') {
			end = strchr(pwd, ':');
			*end = 0;
			if (strcmp((char *)crypt(data->info_passwd,pwd),pwd)) {
				fclose(fid);
				syslog(LOG_INFO, "chk_perm: incorrect login\n");
				return (YPERR_ACCESS);
			}
		} else if (strlen(data->info_passwd)) {
			syslog(LOG_INFO, "chk_perm: no passwd required\n");
			return (YPERR_ACCESS);
		}
		break;
	    }

	fclose(fid);
	if (debug)
		syslog(LOG_INFO, "chk_perm: good login\n");
	return(0);
}


/*
 *	set use_comment=1 and t_ipadr if found
 */
int
parse_comment(rf, buf, net, needmask, inc)
FILE		*rf;
char		*buf;
unsigned	net;
int		needmask, inc;
{
	char	*a_net, *result;
	char	reg[REG_STRLEN], startstr[REG_STRLEN];
	char	endstr[REG_STRLEN], tmask[REG_STRLEN];
	int	result_len;
	u_long	mk, tmp;
	struct	in_addr etmp_addr, tmp_addr, st_addr, end_addr;

	buf++;
	if ((sscanf(buf, "%s", reg) != 1) ||
	    (strncmp(reg, "registrar", 9)))
		return(0);

	if (needmask) {
		if (sscanf(buf,"%s %s %s %s",reg,startstr,endstr,tmask) != 4) {
			syslog(LOG_INFO, "parse: bad registrar cmd(%s)", buf);
			return(-1);
		}
		if (debug)
			syslog(LOG_INFO, "RANGE: %s<=>%s, mask=%s",
				startstr, endstr,tmask);
	} else {
		if (sscanf(buf, "%s %s %s", reg, startstr, endstr) != 3) {
			syslog(LOG_INFO, "parse: bad registrar cmd(%s)", buf);
			return(-1);
		}
		if (debug)
			syslog(LOG_INFO, "RANGE: %s<=>%s, mask=%s",
				startstr, endstr,tmask);
	}

	if ((strncmp(startstr, "start=", 6)) ||
	   (strncmp(endstr, "end=", 4)) ||
	   (needmask && strncmp(tmask, "mask=0x", 7))) {
		syslog(LOG_INFO, "parse: bad registrar cmd(%s)", buf);
		return(-1);
	}

	if (((st_addr.s_addr = inet_addr(&startstr[6])) == -1) ||
	   (((tmp = st_addr.s_addr & 0xff) == 0) || (tmp == 0xff))) {
		syslog(LOG_INFO, "parse: bad start addr(%s)", &startstr[6]);
		return (-1);
	}

	if (((end_addr.s_addr = inet_addr(&endstr[4])) == -1) ||
	   (((tmp = end_addr.s_addr & 0xff) == 0) || (tmp == 0xff))) {
		syslog(LOG_INFO, "parse: bad end addr(%s)", &endstr[4]);
		return (-1);
	}

	if (needmask && sscanf(&tmask[7], "%x", &mk) != 1) {
		syslog(LOG_INFO, "parse: bad netmask(%s)", &tmask[7]);
		return(-1);
	}

	if (needmask) {
		if ((st_addr.s_addr&mk) != net || (end_addr.s_addr&mk) != net) {
			if (debug)
			    syslog(LOG_INFO,
				"parse: netmask(%x) missmatch %x", net, mk);
			return(0);
		}
	} else if ((st_addr.s_addr&net)!=net || (end_addr.s_addr&net)!=net) {
		if (debug)
		    syslog(LOG_INFO, "parse: net(%x) missmatch %x",
				net, inet_netof(st_addr));
		return(0);
	}

	tmp_addr = st_addr;
	for (; st_addr.s_addr <= end_addr.s_addr; st_addr.s_addr += inc) {
		a_net = inet_ntoa(st_addr);
		if (!yp_match(domain, hostbyaddr, a_net, strlen(a_net),
			&result, &result_len)) {
			if (debug >= 2)
				syslog(LOG_INFO, "parse: %s skipped", a_net);
			continue;
		}
		if (((st_addr.s_addr&net)!=net)||((st_addr.s_addr&~net)>~net)) {
			syslog(LOG_INFO, "parse: range(%s<=>%s) full",
				startstr, endstr);
			return(-1);
		}
		use_comment = 1;
		strcpy(t_ipadr, a_net);
		while (fgets(startstr, sizeof(startstr), rf)) {
			/* ignore blank line */
			if (sscanf(startstr, "%s", &endstr[0]) != 1)
				continue;
			/*XXX*/
			if (*endstr == '#')
				continue;

			if ((etmp_addr.s_addr = inet_addr(endstr)) == -1) {
				syslog(LOG_INFO,
				    "parse: bad host file entry(%s)", endstr);
				return(-1);
			}

			if (((etmp_addr.s_addr&net) != net) ||
			   (st_addr.s_addr < etmp_addr.s_addr) ||
			   (etmp_addr.s_addr < tmp_addr.s_addr))
				break;
			line_no++;
		}
		if (debug)
			syslog(LOG_INFO, "parse: returning %s", t_ipadr);
		return(1);
	}
	syslog(LOG_INFO,
		"parse: reserved range(%s<=>%s) full", startstr, endstr);
	return(-1);
}


int
allocate_ipaddr(reg_data, fname, tmpname)
	reg_info	*reg_data;
	char		*fname;
	char		*tmpname;
{
	FILE		*ptr, *tmp;
	struct in_addr	tmp_netaddr, netaddr, t_net, t_lna, lna, f_net;
	char		*addr_p;
	char		buf[REG_STRLEN], ipadr[REG_STRLEN], cmd[REG_STRLEN];
	int		inc = 0;
	int 		tstat = 0;

	syslog(LOG_INFO, "allocate new ip addr");
	if (reg_data->info_netmask) {
		t_net.s_addr = reg_data->info_network & reg_data->info_netmask;
		inc = (reg_data->info_netmask + 1)&(~reg_data->info_netmask);
		/* make sure new ipaddr > 0 */
		if (inc == 0) {
			syslog(LOG_INFO, "allocip: bad netmask(%x)",
				reg_data->info_netmask);
			return(YPERR_YPERR);
		}
	} else {
		tmp_netaddr=inet_makeaddr((int)reg_data->info_network, (int)0);
		t_net = tmp_netaddr;
		inc++;
	}

	if (debug) {
		syslog(LOG_INFO, "allocip: ***** netreq=%x, maskreq=%x *****",
			reg_data->info_network, reg_data->info_netmask);
		syslog(LOG_INFO, "allocip: t_net=%x, inc=%x",t_net.s_addr,inc);
	}

	if ((tmp = fopen(tmpname, "w")) == NULL) {
		syslog(LOG_INFO, "allocip: can't open tmp_file(%s)", tmpname);
		return(YPERR_YPERR);
	}
	if ((ptr = fopen(fname, "r")) == NULL) {
		syslog(LOG_INFO, "allocip: can't open host_file(%s)", fname);
		fclose(tmp);
		return (YPERR_YPERR);
	}

	line_no = 0;
	while (fgets(buf, sizeof(buf), ptr)) {
		line_no++;

		/* ignore blank line */
		if (sscanf(buf, "%s", ipadr) != 1)
			continue;

		/* handle comment line */
		if (ipadr[0] == '#') {
			tstat = parse_comment(ptr, buf, t_net.s_addr,
					reg_data->info_netmask, inc);
			if (tstat == 0)
				continue;

			if (tstat < 0)
				tstat = YPERR_YPERR;
			else {
				syslog(LOG_INFO,
			    		"new ip-addr(%s) from reserved range",
					t_ipadr);
				tstat = 0;
			}

			fclose(ptr);
			fclose(tmp);
			return(tstat);
		}

		if ((netaddr.s_addr = inet_addr(ipadr)) == -1) {
			syslog(LOG_INFO, "bad host file entry(%s)", buf);
			fclose(ptr);
			fclose(tmp);
			cexit(YPERR_YPERR);
		}

		if (reg_data->info_netmask) {
			if (t_net.s_addr
			    == (netaddr.s_addr & reg_data->info_netmask)) {
				if (debug >= 2)
			    	    syslog(LOG_INFO, "allocip: masktmp %x(%s)",
				    	netaddr.s_addr, ipadr);
				fprintf(tmp, "%x\n", netaddr.s_addr);
				continue;
			}
		} else {
			if (reg_data->info_network == inet_netof(netaddr)) {
				if (debug >= 2)
			    	    syslog(LOG_INFO, "allocip: addtmp %x(%s)",
					netaddr.s_addr, ipadr);
				fprintf(tmp, "%x\n", netaddr.s_addr);
				continue;
			}
		}
		if (debug >=2)
			syslog(LOG_INFO, "allocip: skiptmp(%x<>%x)",
				t_net.s_addr, (netaddr.s_addr&t_net.s_addr));
	}
	fclose(ptr);

	if (reg_data->info_netmask) {
		lna.s_addr = inet_lnaof(t_net);
		if (lna.s_addr == -1) {
			syslog(LOG_INFO, "allocip: bad local net(%s)",
				t_net.s_addr);
			tstat = YPERR_YPERR;
			goto alloc_ret1;
		}
	} else
		lna.s_addr = 0;
	lna.s_addr += inc;

	if (debug)
		syslog(LOG_INFO, "allocip: lna=%x", lna.s_addr);

	found = 0;
	rewind(tmp);
	sprintf(cmd, "sort %s", tmpname);
	if ((ptr = popen(cmd, "r")) == NULL) {
		syslog(LOG_INFO, "allocip: can't open pipe");
		tstat = YPERR_YPERR;
		goto alloc_ret1;
	}

	while ((tstat = fscanf(ptr, "%x", &netaddr.s_addr)) != EOF) {
		t_lna.s_addr = inet_lnaof(netaddr);
		if (t_lna.s_addr > lna.s_addr) {
			if (found == 0) {
				ipadr_ptr = netaddr;
				found = 1;
			} else
				found = 2;
			break;
		}
		ipadr_ptr = netaddr;
		found = 2;
		if (t_lna.s_addr == lna.s_addr)
			lna.s_addr += inc;
	}

	if (debug)
		syslog(LOG_INFO, "allocip: found=%d, lna=%d, tlna=%d, ipadr=%s",
			found, lna.s_addr, t_lna.s_addr, inet_ntoa(ipadr_ptr));

	f_net.s_addr = inet_netof(t_net);
	tmp_netaddr = inet_makeaddr((int)f_net.s_addr, (int)lna.s_addr);
	netaddr = tmp_netaddr;

	if ((reg_data->info_netmask &&
	   (netaddr.s_addr&reg_data->info_netmask) != t_net.s_addr) ||
	   (reg_data->info_netmask == 0 &&
	   (reg_data->info_network != inet_netof(netaddr)))) {
		syslog(LOG_INFO, "allocip: subnet(%x) is full", t_net.s_addr);
		tstat = YPERR_YPERR;
		goto alloc_ret;
	}

	addr_p = inet_ntoa(netaddr);
	(void)strcpy(t_ipadr, addr_p);
	syslog(LOG_INFO, "new ip addr = %s", t_ipadr);
	tstat = 0;

alloc_ret:
	pclose(ptr);
alloc_ret1:
	fclose(tmp);
	return(tstat);
}


int
get_nextname(names, a_name)
char	**names, *a_name;
{
	char	c, *ptr;

	ptr = *names;
	for ( c = *ptr;  c != 0 && (c == ' ' || c == '\t'); c = *++ptr );
	if ( c == 0 )
		return (FALSE);

	for ( c = *ptr;  c != 0 && c != ' ' && c != '\t'; c = *++ptr )
		*a_name++ = c;
	*a_name = 0;
	*names = ptr;
	return (TRUE);
}
