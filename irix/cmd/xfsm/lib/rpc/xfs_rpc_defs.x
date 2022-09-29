
typedef string nametype<>;

struct xfs_info_res {
	nametype info;
	nametype warnmsg;
	int errno;
};

struct xfs_result {
	nametype warnmsg;
	int errno;
};
	
program XFSPROG {
	version XFSVERS {
		xfs_info_res
		XFSGETOBJ(nametype) = 1;
		xfs_info_res
		XFSGETINFO(nametype) = 2;
		xfs_result
		XFSCREATE(nametype) = 3;
		xfs_result
		XFSEDIT(nametype) = 4;
		xfs_result
		XFSMOUNT(nametype) = 5;
		xfs_result
		XFSUNMOUNT(nametype) = 6;
		xfs_result
		XFSDELETE(nametype) = 7;
		xfs_info_res
		XFSGETPARTITIONTABLE(nametype) = 8;
		xfs_result
		XFSSETPARTITIONTABLE(nametype) = 9;
		xfs_info_res
		XFSGROUP(nametype) = 10;
		xfs_info_res
		XFSGETHOSTS(nametype) = 11;
		xfs_info_res
		XFSGETDEFAULTS(nametype) = 12;
		xfs_result
		XFSEXPORT(nametype) = 13;
		xfs_result
		XFSUNEXPORT(nametype) = 14;
		xfs_info_res
		XFSXLVCMD(nametype) = 15;
                xfs_result
                XLVDELETE(nametype) = 16;
                xfs_result
                XLVCREATE(nametype) = 17;
                xfs_result
                XLVDETACH(nametype) = 18;
                xfs_result
                XLVATTACH(nametype) = 19;
                xfs_result
                XLVREMOVE(nametype) = 20;
                xfs_result
                XLVSYNOPSIS(nametype) = 21;
                xfs_result
                XLVINFO(nametype) = 22;
		xfs_result
		GETPARTSINUSE(nametype) = 23;
		xfs_info_res
		XFSGETEXPORTINFO(nametype) = 24;
		xfs_info_res
		XFSSIMPLEFSINFO(nametype) = 25;
		xfs_info_res
		XFSGETBBLK(nametype) = 26;
		xfs_info_res
		XFSCHKMOUNTS(nametype) = 27;

	} = 1;
} = 391016;

