/*
 *	@(#)nfs_rmount.c	1.1	9/7/88	initial release
 */


#include <sys/vnode_private.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/sysinfo.h>
#include <sys/uio.h>
#include <string.h>
#include <sys/ckpt.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include "types.h"
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <ksys/vfile.h>
#include <sys/kopt.h>
#include <sys/mount.h>		/* MS_RDONLY */
#include <sys/socket.h>
#include <sys/socketvar.h>	/* for lint and, someday, socreate prototype */
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <net/if.h>
#include <net/soioctl.h>
#include <netinet/in.h>
#include "string.h"
#include "auth.h"
#include "clnt.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "bootparam.h"
#include "mount.h"		/* must come after nfs.h */
#include "net/route.h"
#include "sys/kopt.h"
typedef	__uint32_t	iaddr_t;
#include <protocols/bootp.h>
#include "sys/mbuf.h"

extern int diskless;
extern char *bootswapfile;

static int 	local_traverse(vnode_t **);
static int 	local_lookupname(char *, enum uio_seg, enum symfollow,
				 vnode_t **, vnode_t **, ckpt_handle_t *);
static int 	local_lookuppn(register struct pathname *, enum symfollow,
				 vnode_t **, vnode_t **, ckpt_handle_t *);
static int	nfs_mount_one(struct vfs *, bp_getfile_res *, struct vnode **,
			 struct cred *);
static int	prim_ifconfig(struct in_addr);
static int	prim_route(struct in_addr, struct in_addr);
static CLIENT	*get_bootparam(struct in_addr *,struct in_addr *,struct cred *);
static void	bootparam_cleanup(CLIENT **);
static int	nfs_mount_more(char *, struct vfs *, bp_getfile_res *,
			struct vnode **, struct cred *);
static int	bootp_rfc951(struct in_addr *, char *,
				struct in_addr *, struct in_addr *);

#define	BOOTPARAM_RETRIES	3	/* XXXsgi */
#define	MOUNT_RETRIES		3	/* XXXsgi */

bp_getfile_res	dl_root;
bp_getfile_res	dl_swap;
bp_getfile_res	dl_sbin;
bp_getfile_res	dl_usr;
bp_getfile_res	dl_var_share;

extern enum	clnt_stat dl_getfh(bp_getfile_res *, struct fhstatus *,
			struct cred *, struct mac_label *);

static int
prim_ifconfig(struct in_addr myipaddr)
{
	struct ifnet *ifp;
	struct socket *so;
	struct sockaddr_in sin;
	int error, len;
	struct ifreq ifr;
 	char *dlif;
  
 	dlif = kopt_find("dlif");
	for (ifp = ifnet;; ifp = ifp->if_next) {
		if (!ifp) {
			return (EINVAL);
		}
 		if (ifp->if_name && !(ifp->if_flags & IFF_LOOPBACK) &&
 		    (dlif == NULL || *dlif == '\0' || 
		     !strcmp(dlif, ifp->if_name)) && ifp->if_unit == 0)
  			break;
	}
	error = socreate(AF_INET, &so, SOCK_DGRAM, IPPROTO_UDP);
	if (error) {
		return (error);
	}
	len = strlen(ifp->if_name);
	bcopy(ifp->if_name, &ifr.ifr_name[0], len);
	/*
	 * Append the unit number. The unit number could be more
	 * than one digit (> 9).
	 */
	sprintf(&ifr.ifr_name[len], "%d", ifp->if_unit);
	for( ; ifr.ifr_name[len] != '\0'; len++)
		;
	bzero(&ifr.ifr_name[len], sizeof ifr.ifr_name - len);
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr = myipaddr;
	ifr.ifr_addr = *(struct sockaddr *)&sin;
	SOCKET_LOCK(so);
	error = ifioctl(so, SIOCSIFADDR, (caddr_t)&ifr);
	SOCKET_UNLOCK(so);
	soclose(so);
	return (error);
}

/*
 * add the given route
 */
static int
prim_route(struct in_addr sipaddr, struct in_addr gipaddr) {
        struct sockaddr_in dst, gateway;

        bzero(&dst, sizeof(dst));
#ifdef _HAVE_SIN_LEN
	dst.sin_len = 8;
#endif
	dst.sin_family = AF_INET;
	dst.sin_port = 0;
	dst.sin_addr = sipaddr;
        bzero(&gateway, sizeof(gateway));
#ifdef _HAVE_SIN_LEN
	gateway.sin_len = 8;
#endif
	gateway.sin_family = AF_INET;
	gateway.sin_port = 0;
	gateway.sin_addr = gipaddr;
	return rtrequest(RTM_ADD,
			 (struct sockaddr *)&dst,
			 (struct sockaddr *)&gateway,
			 0, RTF_HOST, 0);
}

/*
 * In the interest of keeping sashARCS machine independent this function
 * implements a BOOTP client here in the kernel to get the gateway and
 * server IP addr if they weren't passed as kernel args.  
 *
 * This is to cover for a "feature" of earlier ARCS proms (in IP20, IP22, IP19)
 * whose bootp code doesn't pass the gateaddr.
 */
static int
bootp_rfc951(struct in_addr *myipaddr, char *servername,
		struct in_addr *sipaddr, struct in_addr *gipaddr)
{
	struct socket *sockreply, *socksend;
	struct mbuf *m;
	struct sockaddr_in *inaddr, outaddr;
	struct bootp *bp;
	int error = 0;
	int bootp_xid;
#define	BOOTP_RETRIES	4	/* just like stand bootp */
#define	BOOTP_TIMEOUT	5
	int retries = BOOTP_RETRIES;

	if (error = socreate(AF_INET, &socksend, SOCK_DGRAM, IPPROTO_UDP))
		return error;
	socksend->so_options |= SO_BROADCAST;
	if (error = socreate(AF_INET, &sockreply, SOCK_DGRAM, IPPROTO_UDP)) {
		soclose(socksend);
		return error;
	}

	/*
	 * bind the recv socket to the bootp client (reply) port
	 */
	if (!(m = m_vget(M_DONTWAIT, sizeof(struct sockaddr_in), MT_DATA))) {
		printf("bootp_rfc951 m_vget in_addr failed");
		error = ENOBUFS;
		goto done;
	}
	inaddr = mtod(m, struct sockaddr_in *);
	bzero(inaddr, sizeof(*inaddr));
	inaddr->sin_family = AF_INET;
	inaddr->sin_port = IPPORT_BOOTPC;
	inaddr->sin_addr.s_addr = INADDR_ANY;
	if (error = sobind(&(sockreply->so_bhv), m))
		goto done;
	m_freem(m);

	/*
	 * send out the bootp request (broadcast)
	 */
	if (!(m = m_vget(M_DONTWAIT, sizeof(struct bootp), MT_DATA))) {
		printf("bootp_rfc951 m_vget bootp failed");
		error = ENOBUFS;
		goto done;
	}
	bp = mtod(m, struct bootp *);
	bzero(bp, sizeof(*bp));
	bp->bp_op = BOOTREQUEST;
	bp->bp_htype = ARPHRD_ETHER;
	bp->bp_hlen = 6; /* ether addr length XXX needed? */
	bootp_xid = getrand();
	bp->bp_xid = htonl(bootp_xid);
	bp->bp_ciaddr = myipaddr->s_addr;
	bp->bp_secs = 5;
	strcpy((char *)bp->bp_sname, servername);
	outaddr.sin_family = AF_INET;
	outaddr.sin_addr.s_addr = INADDR_BROADCAST;
	outaddr.sin_port = IPPORT_BOOTPS;
	while (retries--) {
		time_t starttime;
		extern time_t time;
		struct mbuf *mrecv;

		if (error = ku_sendto_mbuf(socksend, m, &outaddr))
			goto done;
	
		/*
		 * watch for a bootp reply
		 */
		starttime = time;
		while(!(mrecv = ku_recvfrom(sockreply, 0, NULL)) &&
		      time < starttime + BOOTP_TIMEOUT)
				;
		if (!mrecv)
			continue;	/* resend request */
		/*
		 * is it a reply and is it for us?
		 */
		bp = mtod(mrecv, struct bootp *);
		if (bp->bp_op == BOOTREPLY &&
		    ntohl(bp->bp_xid) == bootp_xid &&
		    strcmp(servername, (char *)bp->bp_sname) == 0) {
			/*
			 * it's from our server... 
			 * dig out the server and gateway ip addr
			 */
			gipaddr->s_addr = bp->bp_giaddr;
			sipaddr->s_addr = bp->bp_siaddr;
			m_freem(mrecv);
			error = 0;
			goto done;
		}
	}

done:
	/*
	 * free stuff
	 */
	m_freem(m);
	soclose(sockreply);
	soclose(socksend);
	return error;
}


/*
 * dl_getfh:
 *
 *	get the remote file handle.
 *
 */
enum clnt_stat
dl_getfh(pp, fhp, cred, lp)
	bp_getfile_res	*pp;
	struct fhstatus	*fhp;
	struct cred	*cred;
	mac_label	*lp;
{
	register CLIENT 	*clnt;
	struct timeval		wait;
	enum clnt_stat 		clnt_stat;
	struct sockaddr_in	sin;

	sin.sin_addr.s_addr = *(int *)&(pp->server_address.bp_address.ip_addr);
	sin.sin_family = AF_INET;
	if (getport_loop(&sin, MOUNTPROG, MOUNTVERS, IPPROTO_UDP, 1)) {
		return(RPC_FAILED);
	}

	clnt = clntkudp_create(&sin, MOUNTPROG, MOUNTVERS, MOUNT_RETRIES, 
				KUDP_NOINTR, KUDP_XID_CREATE, cred);
	if (clnt == 0) {
		return(RPC_FAILED);
	}
	clntkudp_soattr(clnt, lp);
	wait.tv_sec = 20;
	wait.tv_usec = 0;
	clnt_stat = clnt_call(clnt, MOUNTPROC_MNT, xdr_path, &pp->server_path,
			      xdr_fhstatus, fhp, wait);
	clnt_destroy(clnt);
	return(clnt_stat);
}


/*
 * get class, src points to path name ended with /unix
 *
 */
char *
get_class(dest, src)
	char	*dest, *src;
{
	char	*pt, *save_pt = 0, ch;
	register int	sz=0, len;

	len = strlen(src);
	pt = src + len - 1;

	/* skip /unix if it is there */
	if ( len > 5 && strncmp( pt - 4, "/unix", 5) == 0 ) {
		save_pt = pt - 4;
		ch = *save_pt;
		*save_pt = 0;
		pt -= 5;
		len -= 5;
	}

	for ( ; len; pt--, sz++, len--)
		if ( *pt == '/' || *pt == ':' || *pt == ')' )
				break;
	pt++;
	if ( !sz )
		return ( 0 );
	bcopy(pt, dest, sz);
	*(dest + sz) = 0;
	if ( save_pt )
		*save_pt = ch;
	return (dest);
}

/*
 * get boot params by rpc call to bootparamd at given address.
 *
 *	Followings are set by this procedure:
 *
 *		dl_root
 *		dl_swap
 *		dl_sbin
 *		dl_usr
 *		dl_var_share
 *
 */
static CLIENT *
get_bootparam(srvipaddr, myipaddr, cred)
	struct in_addr	*srvipaddr;	/* server ip addr. */
	struct in_addr	*myipaddr;	/* clnt ip addr. */
	struct cred	*cred;
{
	CLIENT		*clnt;
	struct timeval	wait;
	enum clnt_stat	stat;
	bp_whoami_arg	wmi_arg;
	bp_getfile_arg	bp_arg;
	struct in_addr	*tin;
	bp_whoami_res	dl_whoami;
	struct	sockaddr_in addr;
	char		cl_name[50];

	addr.sin_addr.s_addr = srvipaddr->s_addr;
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	if (getport_loop(&addr, BOOTPARAMPROG, BOOTPARAMVERS, IPPROTO_UDP, 1)) {
		return (NULL);
	}
	clnt = clntkudp_create(&addr, BOOTPARAMPROG, BOOTPARAMVERS,
			       BOOTPARAM_RETRIES, KUDP_NOINTR,
			       KUDP_XID_PERCALL, cred);
	if (clnt == 0) {
		printf("can't call server %s\n", inet_ntoa(*srvipaddr));
		return(NULL);
	}
	wait.tv_sec = 20;
	wait.tv_usec = 0;
	if ( !is_specified(arg_tapedevice) ||
			 !get_class(cl_name, arg_tapedevice) ) {
		int len;

		dl_whoami.client_name = dl_whoami.domain_name = 0;
		wmi_arg.client_address.address_type = IP_ADDR_TYPE;
		tin =
		 (struct in_addr *)&(wmi_arg.client_address.bp_address.ip_addr);
		*tin = *myipaddr;
		stat = clnt_call(clnt, BOOTPARAMPROC_WHOAMI,
				 xdr_bp_whoami_arg, &wmi_arg,
				 xdr_bp_whoami_res, &dl_whoami,
				 wait);
		if (stat != RPC_SUCCESS) {
			printf("GET_BOOTP: WHOAMI fail, myaddr=%s/0x%x ", 
					inet_ntoa(*myipaddr),*myipaddr);
			printf("srvaddr=%s/0x%x srvport=%d\n",
					inet_ntoa(*srvipaddr),*srvipaddr, addr.sin_port);
			goto bad;
		}
		len = strlen(dl_whoami.client_name);
		bcopy(dl_whoami.client_name, cl_name, len);
		cl_name[len] = 0;
		clnt_freeres(clnt, xdr_bp_whoami_res, &dl_whoami);
	}
	bp_arg.client_name = &cl_name[0];
	bp_arg.file_id = "root";
	stat = clnt_call(clnt, BOOTPARAMPROC_GETFILE, xdr_bp_getfile_arg,
			 &bp_arg, xdr_bp_getfile_res, &dl_root, wait);
	if (stat != RPC_SUCCESS) {
		printf("GET_BOOTP: root not found - %s\n", bp_arg.client_name);
		goto bad;
	}
	bp_arg.file_id = "swap";
	stat = clnt_call(clnt, BOOTPARAMPROC_GETFILE,
			 xdr_bp_getfile_arg, &bp_arg,
			 xdr_bp_getfile_res, &dl_swap,
			 wait);
	if (stat == RPC_SUCCESS) {
		if ( strlen(dl_swap.server_path) == 0 )
			*(int *)&dl_swap.server_address.bp_address = 0;
	}

	bp_arg.file_id = "sbin";
	stat = clnt_call(clnt, BOOTPARAMPROC_GETFILE,
			 xdr_bp_getfile_arg, &bp_arg,
			 xdr_bp_getfile_res, &dl_sbin,
			 wait);
	if (stat == RPC_SUCCESS) {
		if ( strlen(dl_sbin.server_path) == 0 )
			*(int *)&dl_sbin.server_address.bp_address = 0;
	}

	bp_arg.file_id = "usr";
	stat = clnt_call(clnt, BOOTPARAMPROC_GETFILE,
			 xdr_bp_getfile_arg, &bp_arg,
			 xdr_bp_getfile_res, &dl_usr,
			 wait);
	if (stat == RPC_SUCCESS) {
		if ( strlen(dl_usr.server_path) == 0 )
			*(int *)&dl_usr.server_address.bp_address = 0;
	}

	bp_arg.file_id = "var_share";
	stat = clnt_call(clnt, BOOTPARAMPROC_GETFILE,
			 xdr_bp_getfile_arg, &bp_arg,
			 xdr_bp_getfile_res, &dl_var_share,
			 wait);
	if (stat == RPC_SUCCESS) {
			if ( strlen(dl_usr.server_path) == 0 )
				*(int *)&dl_usr.server_address.bp_address = 0;
	}

	return clnt;

bad:
	clnt_destroy(clnt);
	return NULL;
}


static void
bootparam_cleanup(CLIENT **clnt)
{
	if (! *clnt)
		return;
	clnt_freeres(*clnt, xdr_bp_getfile_res, &dl_root);
	clnt_freeres(*clnt, xdr_bp_getfile_res, &dl_sbin);
	clnt_freeres(*clnt, xdr_bp_getfile_res, &dl_swap);
	clnt_freeres(*clnt, xdr_bp_getfile_res, &dl_usr);
	clnt_freeres(*clnt, xdr_bp_getfile_res, &dl_var_share);
	clnt_destroy(*clnt);
	*clnt = 0;
}

/*
 * Lookup the user file name,
 * Handle allocation and freeing of pathname buffer, return error.
 */
static int
local_lookupname(char *fnamep,                /* user pathname */
        enum uio_seg seg,               /* addr space that name is in */
        enum symfollow followlink,      /* follow sym links */
        vnode_t **dirvpp,               /* ret for ptr to parent dir vnode */
        vnode_t **compvpp,              /* ret for ptr to component vnode */
        ckpt_handle_t *ckpt)            /* ret for ckpt lookup info */
{
        struct pathname lookpn;
        register int error;

        if (error = pn_get(fnamep, seg, &lookpn))
                return error;
        error =local_lookuppn(&lookpn, followlink, dirvpp, compvpp, ckpt);
        pn_free(&lookpn);
        return error;
}

/*
 * Starting at current directory, translate pathname pnp to end.
 * Leave pathname of final component in pnp, return the vnode
 * for the final component in *compvpp, and return the vnode
 * for the parent of the final component in dirvpp.
 */

static int
local_lookuppn(register struct pathname *pnp,	/* pathname to lookup */
	enum symfollow followlink,	/* (don't) follow sym links */
	vnode_t **dirvpp,		/* ptr for parent vnode */
	vnode_t **compvpp,		/* ptr for entry vnode */
	ckpt_handle_t *ckpt)		/* ptr for ckpt */
{
	vnode_t *vp;			/* current directory vp */
	vnode_t *cvp = NULLVP;		/* current component vp */
	vnode_t *tvp;			/* addressable temp ptr */
	char component[MAXNAMELEN];	/* buffer for component (incl null) */
	int error;
	int nlink = 0;
	int lookup_flags = dirvpp ? LOOKUP_DIR : 0;
	vnode_t *rdir;
	cred_t *cr = get_current_cred();

	/* use local root dir */
	rdir = rootdir;
	SYSINFO.namei++;

	/*
	 * Start at current (in this case is root ) directory.
	 */
	vp = rdir;
	VN_HOLD(vp);

begin:
	/*
	 * Disallow the empty path name.
	 */
	if (pnp->pn_pathlen == 0) {
		error = ENOENT;
		goto bad;
	}

	/*
	 * Each time we begin a new name interpretation (e.g.
	 * when first called and after each symbolic link is
	 * substituted), we allow the search to start at the
	 * root directory if the name starts with a '/', otherwise
	 * continuing from the current directory.
	 */
	if (*pnp->pn_path == '/') {
		VN_RELE(vp);
		pn_skipslash(pnp);
		vp = rdir;
		VN_HOLD(vp);
	}

	/*
	 * Eliminate any trailing slashes in the pathname.
	 */
	pn_fixslash(pnp);
next:
	/*
	 * Make sure we have a directory.
	 */
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto bad;
	}

	/*
	 * Process the next component of the pathname.
	 */
	if (error = pn_stripcomponent(pnp, component))
		goto bad;

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. "/." or ".".
	 */
	if (component[0] == 0) {
		/*
		 * If the caller was interested in the parent then
		 * return an error since we don't have the real parent.
		 */
		if (dirvpp != NULL) {
			VN_RELE(vp);
		}
		(void) pn_set(pnp, ".");
		if (compvpp != NULL) 
			*compvpp = vp;
		else
			VN_RELE(vp);
		return 0;
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If we're at the root directory (e.g. after chroot)
	 *    then ignore ".." so we can't get out of this subtree.
	 * 2. If this vnode is the root of a mounted file system,
	 *    then replace it with the vnode that was mounted on
	      so that we take the ".." in the other file system.
	 */
	if (component[0] == '.' && component[1] == '.' && component[2] == 0) {
checkforroot:
		if (VN_CMP(vp, rootdir)) {
			cvp = vp;
			VN_HOLD(cvp);
			goto skip;
		}
		if (vp->v_flag & VROOT) {
			cvp = vp;
			vp = vp->v_vfsp->vfs_vnodecovered;
			VN_HOLD(vp);
			VN_RELE(cvp);
			goto checkforroot;
		}
	}
	VOP_LOOKUP(vp, component, &tvp, pnp, lookup_flags, rdir, cr, error);
	if (error == 0)
		cvp = tvp;
	else {
		cvp = NULL;
		/*
		 * On error, return hard error if
		 * (a) we're not at the end of the pathname yet, or
		 * (b) the caller didn't want the parent directory, or
		 * (c) we failed for some reason other than a missing entry.
		 */
		if (pn_pathleft(pnp) || dirvpp == NULL || error != ENOENT)
			goto bad;
		pn_setlast(pnp);

#if CKPT
		if (ckpt)
			*ckpt = ckpt_lookup(vp);
#endif
		*dirvpp = vp;
		if (compvpp != NULL)
			*compvpp = NULL;
		return 0;
	}
	/*
	 * Traverse mount points.
	 *
	 * We do not call VN_LOCK in order to test cvp->v_vfsmountedhere
	 * because either it's about to be set after this test by a racing
	 * mount, but that mount will fail because cvp has more than one
	 * reference; or it's about to be cleared, in which case traverse
	 * will do nothing (possibly synchronizing via vfs_mlock).
	 */
	if (cvp->v_vfsmountedhere != NULL) {
		tvp = cvp;
		if ((error = local_traverse(&tvp)) != 0)
			goto bad;
		cvp = tvp;
	}

	/*
	 * Identify and process a moldy directory in the MAC configuration.
	 * Works much like the symlink processing below, prepending
	 * an additional component to the path if appropriate.
	 * The "-1" error return is a bit hackish.
	 */
	if ((error = _MAC_MOLDY_PATH(cvp, component, pnp, cr)) == -1) {
		VN_RELE(vp);
		vp = cvp;
		cvp = NULL;
		goto begin;
	}
	else if (error)
		goto bad;

	/*
	 * If we hit a symbolic link and there is more path to be
	 * translated or this operation does not wish to apply
	 * to a link, then place the contents of the link at the
	 * front of the remaining pathname.
	 */
	if (cvp->v_type == VLNK
	    && (followlink == FOLLOW || pn_pathleft(pnp))) {
		struct pathname linkpath;
		extern int maxsymlinks;

		if (++nlink > maxsymlinks) {
			error = ELOOP;
			goto bad;
		}
		pn_alloc(&linkpath);
		if (error = pn_getsymlink(cvp, &linkpath, cr)) {
			pn_free(&linkpath);
			goto bad;
		}
		if (pn_pathleft(&linkpath) == 0)
			(void) pn_set(&linkpath, ".");
		error = pn_insert(pnp, &linkpath);	/* linkpath before pn */
		pn_free(&linkpath);
		if (error)
			goto bad;
		VN_RELE(cvp);
		cvp = NULL;
		goto begin;
	}

skip:
	/*
	 * If no more components, return last directory (if wanted) and
	 * last component (if wanted).
	 */
	if (pn_pathleft(pnp) == 0) {
		pn_setlast(pnp);

#if CKPT
		if (ckpt)
			*ckpt = ckpt_lookup(vp);
#endif

		if (dirvpp != NULL) {
			/*
			 * Check that we have the real parent and not
			 * an alias of the last component.
			 */
			if (VN_CMP(vp, cvp)) {
				VN_RELE(vp);
				VN_RELE(cvp);

#if CKPT
				if (ckpt && *ckpt)
					ckpt_lookup_free(*ckpt);
#endif
				return EINVAL;
			}
			*dirvpp = vp;
		} else
			VN_RELE(vp);
		if (compvpp != NULL)
			*compvpp = cvp;
		else
			VN_RELE(cvp);
		return 0;
	}

	/*
	 * Skip over slashes from end of last component.
	 */ 
	pn_skipslash(pnp);

	/*
	 * Searched through another level of directory:
	 * release previous directory handle and save new (result
	 * of lookup) as current directory.
	 */
	VN_RELE(vp);
	vp = cvp;
	cvp = NULL;
	goto next;

bad:
	/*
	 * Error.  Release vnodes and return.
	 */
	if (cvp)
		VN_RELE(cvp);
	VN_RELE(vp);
	return error;
}

/*
 * Traverse a mount point.  Routine accepts a vnode pointer as a reference
 * parameter and performs the indirection, releasing the original vnode.
 */
static int
local_traverse(vnode_t **cvpp)
{
	register struct vfs *vfsp;
	register int error;
	register vnode_t *cvp;
	int s;
	vnode_t *tvp;

	cvp = *cvpp;
	/*
	 * If this vnode is mounted on, then we transparently indirect
	 * to the vnode that is the root of the mounted file system.
	 * Before we do this we must check that an unmount is not in
	 * progress on this vnode.
	 */
	s = VN_LOCK(cvp);
	while ((vfsp = cvp->v_vfsmountedhere) != NULL) {
		nested_spinlock(&vfslock);
		NESTED_VN_UNLOCK(cvp);
		if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
			ASSERT(vfsp->vfs_flag & VFS_MWANT ||
			       vfsp->vfs_busycnt == 0);
			vfsp->vfs_flag |= VFS_MWAIT;
			sv_wait(&vfsp->vfs_wait, PZERO, &vfslock, s);
			s = VN_LOCK(cvp);
			continue;
		}

		/*
		 * Emulate vfs_busycnt here so the filesystem is not unmounted
		 * while VFS_ROOT executes (uniprocessor vnodes assumes that
		 * VFS_ROOT does not sleep).
		 */
		ASSERT(vfsp->vfs_busycnt >= 0);
		vfsp->vfs_busycnt++;
		mutex_spinunlock(&vfslock, s);
		VFS_ROOT(vfsp, &tvp, error);
		vfs_unbusy(vfsp);
		if (error)
			return error;
		VN_RELE(cvp);
		cvp = tvp;
		s = VN_LOCK(cvp);
	}
	VN_UNLOCK(cvp, s);
	*cvpp = cvp;
	return 0;
}


/*  mount the rest of system directories
 * vfs is linked to vnode
 *
 */
static int
nfs_mount_more(str, vfsp, pp, rtvp, cred)
	char		*str;
	struct vfs	*vfsp;
	bp_getfile_res	*pp;
	struct vnode	**rtvp;		/* the server's root */
	struct cred	*cred;
{
	int		error;
	struct vnode	*vp;
	
	ASSERT(cred);
	if (error = local_lookupname(str, UIO_SYSSPACE, FOLLOW,
					 NULLVPP, &vp, NULL)) 
		return (error);

	if (vp->v_type != VDIR) {
		return (ENOTDIR);
	}
	if (error = nfs_mount_one(vfsp, pp, rtvp, cred)) {
		return (error);
	}
	vp->v_vfsmountedhere = vfsp;
	vfsp->vfs_vnodecovered = vp;
	return (0);
}


/*
 * get file handle and makenfsroot.
 */
static int
nfs_mount_one(vfsp, pp, rtvp, cred)
	struct vfs	*vfsp;
	bp_getfile_res	*pp;
	struct vnode	**rtvp;		/* the server's root */
	struct cred	*cred;
{
	enum clnt_stat	clnt_stat;
	int		error = 0;
	struct fhstatus	fhp;
	struct sockaddr_in	addr;
	struct rnode	*rp = 0;

	clnt_stat = dl_getfh(pp, &fhp, cred,
	    vfsp->vfs_mac ? vfsp->vfs_mac->mv_ipmac : NULL);
	if (clnt_stat != RPC_SUCCESS || fhp.fhs_status ) {
		error = EBUSY;
		goto errout;	/* EBUSY is good one ?? */
	}

	/*
	 * Get root vnode.
	 */
	addr.sin_addr.s_addr = *(int*)(&pp->server_address.bp_address.ip_addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NFS_PORT);
	error = makenfsroot(vfsp, &addr, &fhp.fhs_fh, pp->server_name, &rp,
			    NFSMNT_PRIVATE|NFSMNT_NOINT);
	if (rp)
		*rtvp = rtov(rp);	
	if (error) {
		goto errout;	/* error might not be EBUSY */
	}

errout:
	if (error && *rtvp) {
		VN_RELE(*rtvp);
	}
	return (error);
}

/*
 * Try to mount a remote root.  Return an error code in the
 * thread structure if the root is not a remote root, or if some
 * other error occurs. 
 */
/*ARGSUSED*/
int
nfs_rmount(root_vfsp, why)
	struct vfs *root_vfsp;
	enum whymountroot why;
{	
	struct cred	*mycred;		
	struct vnode	*rtvp = NULL;		/* root vnode ptr */
	struct vnode	*sbin_rtvp;	/* sbin root vnode ptr */
	struct vnode	*swap_rtvp;	/* swap root vnode ptr */
	struct vnode	*usr_rtvp;	/* usr root vnode ptr */
	struct vnode	*var_share_rtvp;	/* var/share root vnode ptr */
	struct mntinfo	*mi;		/* root mount structure */
	struct in_addr	dl_myipaddr;	/* clnt ip addr. */
	struct in_addr	dl_srvipaddr;	/* server ip addr. */
	struct in_addr	dl_gipaddr;	/* gateway ip addr. */
	CLIENT		*client;
	int		error;
	int		minor;
	struct vfs	*sbin_vfsp,	/* vfsp for /sbin */
			*swap_vfsp,	/* vfsp for /swap */
			*usr_vfsp,	/* vfsp for /usr */
			*var_share_vfsp;	/* vfsp for /var/share */
	char		*cp, servername[128];

	/*
	 * check if it is diskless node
	 * if not just return quietly.
	 */
	if (diskless == 0) {
		return EINVAL;
	}
	
	/* initialize network */
	mycred=get_current_cred(); /*Will only fail once */
	ASSERT(mycred);
	bsd_init();
	cp = kopt_find("netaddr");
	if (!cp || ((dl_myipaddr.s_addr = inet_addr(cp)) == -1)) {
		printf("No netaddr found!\n");
		return EINVAL;
	}
	if (cp = kopt_find("dlserver")) {
		dl_srvipaddr.s_addr = inet_addr(cp);
		if (dl_srvipaddr.s_addr == -1) {
			printf("Unknown dlserver, %s\n", cp);
			return EINVAL;
		}
	} 
	else if (diskless == 1) {
		 /* okay if no dlserver arg for == 2 */
		printf("No dlserver argument?!\n");
		return EINVAL;
	} 
	else { 
		/* bootp for it after the interface is up */
		dl_srvipaddr.s_addr = 0;
	}
	if (diskless == 2) {
		/*
		 * Crack tapedevice for server and path (mount point).
		 * and, if we can, dlserver and dlgate for IP addrs
		 */
		if (cp = kopt_find("dlgate")) {
			/* if there is a dlgate assume it's good */
			if ((dl_gipaddr.s_addr = inet_addr(cp)) == -1) {
				printf("Bad dlgate, %s\n", cp);
				return EINVAL;
			}
		} 
		else {
			 /* bootp for it after the interface is up */
			dl_gipaddr.s_addr = 0;
		}

		/* 
		 * Start building dl_root...
		 * Expect tapedevice=bootp()host:nfs-mountable-path
		 */
		cp = kopt_find("tapedevice");
		if (!cp) {
			printf("No tapedevice!?\n");
			return EINVAL;
		}
		/* if ARCS then tapedevice="network(1)bootp()...." */
		if (*cp == 'n' && (cp = strchr(cp, 'b')) == 0) {
			printf("Bad tapedevice: %s\n", cp);
			return EINVAL;
		}
		dl_root.server_path = strchr(cp, ':') + 1;
		if (!dl_root.server_path) {
			printf("Bad tapedevice: %s\n", cp);
			return EINVAL;
		}
		cp = strchr(cp, ')') + 1;
		if (!cp) {
			printf("Bad tapedevice: %s\n", cp);
			return EINVAL;
		}
		strncpy(servername, cp, dl_root.server_path - cp - 1);
		servername[dl_root.server_path - cp - 1] = '\0';
		dl_root.server_name = servername;
	}

	for (;;) {
		if (prim_ifconfig(dl_myipaddr)) {
			printf("Warning: prim_ifconfig failed.");
			error = EINVAL;
			goto mount_loop;
		}
		switch (diskless) {
		case 1:
			/*
			 * Normal diskless: get boot params.
			 */
			client = get_bootparam(&dl_srvipaddr,
					       &dl_myipaddr,
					       mycred);
			if (!client) {
				printf("Get_bootparam failed.");
				error = EINVAL;
				goto mount_loop;
			}
			break;
		case 2:
			/*
			 * Bootable miniroot over NFS: craft dl_root from
			 * tapedevice and dlserver
			 */
		
			/*
			 * if we don't know our server IP addr or if it's on
			 * a different net and we don't know the gateway
			 * then figure out server/gateway IP addr using bootp
			 * XXX subnets?
			 */
			if (!dl_srvipaddr.s_addr ||
			    ((in_netof(dl_srvipaddr) !=
					 in_netof(dl_myipaddr)) &&
			    !dl_gipaddr.s_addr)) {
				error = bootp_rfc951(&dl_myipaddr, servername,
						&dl_srvipaddr, &dl_gipaddr);
				if (error) {
					printf("bootp_rfc951 failed\n");
					goto mount_loop;
				}
			}

			/* fill in the last need part of dl_root */
			*(__uint32_t *)&dl_root.server_address.bp_address.ip_addr = 
							dl_srvipaddr.s_addr;

			/* add a route to the server if it's on another net */
			if (in_netof(dl_srvipaddr) != in_netof(dl_myipaddr)) {
				error = prim_route(dl_srvipaddr, dl_gipaddr);
				if (error) {
					printf("prim_route failed error=%d\n",
									error);
					return error;
				}
			}
			break;
		}

		/*
	 	 * mount root
	 	 */
		if (*(int *)&dl_root.server_address.bp_address == 0
		    || nfs_mount_one(root_vfsp, &dl_root, &rtvp,
			 mycred)) {
			printf("Diskless root mount failed\n");
			error = EINVAL;
			goto mount_loop;
		}
        	vfs_add(NULL, root_vfsp,
			(root_vfsp->vfs_flag & VFS_RDONLY) ? MS_RDONLY : 0);
		/* VN_HOLD ? */
		rootdir = rtvp;
		VN_FLAGSET(rootdir, VROOT);
		if ((minor = vfs_getnum(&nfs_minormap)) == -1) {
			error = ENODEV;
			goto mount_loop;
		}
		rootdev = makedev(nfs_major, minor);
		rtvp->v_rdev = rootdev;
		mi = vfs_bhvtomi(bhv_lookup_unlocked(VFS_BHVHEAD(root_vfsp), &nfs_vfsops));
		clkset((time_t)(mi->mi_root->r_nfsattr.na_atime.tv_sec));

		/*
		 * mount sbin
		 */
		if (*(int *)&dl_swap.server_address.bp_address == 0)
			goto nfs_nosbin;

		sbin_vfsp =
			(struct vfs *)kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
		if (sbin_vfsp == NULL) {
			error = EBUSY;
			goto mount_loop;
		}
		VFS_INIT(sbin_vfsp);
		if (*(int *)&dl_sbin.server_address.bp_address != 0) {
			if (error =
			    nfs_mount_more("/sbin", sbin_vfsp,
					   &dl_sbin, &sbin_rtvp,
					   mycred)) {
				printf("/sbin MOUNT FAILED: errno %d.\n",
					error);
				VFS_FREE(sbin_vfsp);
			} 
			else {
				vfs_add(sbin_vfsp->vfs_vnodecovered,
					sbin_vfsp, 0);
			}
		}
		if ((minor = vfs_getnum(&nfs_minormap)) == -1) {
			error = ENODEV;
			goto mount_loop;
		}
		sbin_rtvp->v_rdev= makedev(nfs_major, minor);
nfs_nosbin:
		/*
		 * mount usr
		 */
		if (*(int *)&dl_swap.server_address.bp_address == 0)
			goto nfs_nousr;

		usr_vfsp =
			(struct vfs *)kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
		if (usr_vfsp == NULL) {
			error = EBUSY;
			goto mount_loop;
		}
		VFS_INIT(usr_vfsp);
		if (*(int *)&dl_usr.server_address.bp_address != 0) {
			if (error =
			    nfs_mount_more("/usr", usr_vfsp,
					   &dl_usr, &usr_rtvp,
						 mycred)) {
				printf("/usr MOUNT FAILED: errno %d.\n",
					error);
				VFS_FREE(usr_vfsp);
			} 
			else {
				vfs_add(usr_vfsp->vfs_vnodecovered,
					usr_vfsp, 0);
			}
		}
		if ((minor = vfs_getnum(&nfs_minormap)) == -1) {
			error = ENODEV;
			goto mount_loop;
		}
		usr_rtvp->v_rdev= makedev(nfs_major, minor);
nfs_nousr:
                /*
		 * mount var/share
		 */
		if (*(int *)&dl_swap.server_address.bp_address == 0)
	 		goto nfs_novar_share;
		var_share_vfsp = (struct vfs *)kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
		if (var_share_vfsp == NULL) {
			error = EBUSY;
		 	goto mount_loop;
	  	}
		VFS_INIT(var_share_vfsp);
		if (*(int *)&dl_var_share.server_address.bp_address != 0) {
			if (error = nfs_mount_more("/var/share", var_share_vfsp,
						  &dl_var_share, &var_share_rtvp,
						   mycred)) {
				printf("/var/share MOUNT FAILED: errno %d.\n", error);
				VFS_FREE(var_share_vfsp);
			}
			else {
				vfs_add(var_share_vfsp->vfs_vnodecovered,
					var_share_vfsp, 0);
			}
		}
		if ((minor = vfs_getnum(&nfs_minormap)) == -1) {
			error = ENODEV;
			goto mount_loop;
		}
		var_share_rtvp->v_rdev= makedev(nfs_major, minor);
										nfs_novar_share:
												/*
		 * mount swap
		 */
		bootswapfile = NULL;
		if (*(int *)&dl_swap.server_address.bp_address == 0)
			goto nfs_noswap;
		swap_vfsp = 
			(struct vfs *)kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
		if (swap_vfsp == NULL) {
			error = EBUSY;
			goto mount_loop;
		}
		VFS_INIT(swap_vfsp);
		if (error = nfs_mount_more("/swap", swap_vfsp, &dl_swap,
					   &swap_rtvp, mycred)) {
			printf("/swap MOUNT FAILED: errno %d.\n", error);
			VFS_FREE(swap_vfsp);
			goto nfs_noswap;
		} 
		else {
			dev_t swpdev;
			if ((minor = vfs_getnum(&nfs_minormap)) == -1) {
				error = ENODEV;
				goto mount_loop;
			}
			swpdev = makedev(nfs_major, minor);
			swap_rtvp->v_rdev = swpdev;
			vfs_add(swap_vfsp->vfs_vnodecovered, swap_vfsp, 0);
		}

		/* swap file opened in main */
		bootswapfile = NFS_SWAPFILE;
		swplo = 0;

nfs_noswap:	
		if (diskless == 1)
			clnt_destroy(client);
		break;

mount_loop:
		bootparam_cleanup(&client);
		printf("\n\nKernel mount failed, check server, bootparams\n");
		printf("	or press reset button !!!\n");
	}
	return 0;
}

int
nfs_rumount()
{
	/* do nothing */
	return 0;
}
