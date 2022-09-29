/*
 * This just makes sure that all the structs are the size we said they were.
 * This is essential for the backwards-compatibility structs!  It doesn't
 * matter as much for the current structs.
 *
 * The first release we used typedefs like uid_t, etc. which can change in
 * size.  This program makes sure the structs are the right size.
 *
 * DON'T SHIP THIS!
 */

#include <stdio.h>
#include <sys/sat.h>
#include <sys/sat_compat.h>

void
hdr_check() {

	printf("Headers Sizes\n");
        printf("\n");
        printf("Common Record Header...\n\n");
	printf("   V1.0:    %d\n",sizeof(struct sat_hdr_1_0));
	printf("   V1.1:    %d\n",sizeof(struct sat_hdr_1_1));
	printf("   V1.2:    %d\n",sizeof(struct sat_hdr_1_2));
	printf("   V2.0:    %d\n",sizeof(struct sat_hdr_2_0));
	printf("   Curr:    %d\n",sizeof(struct sat_hdr));
        printf("\n");
        printf("Audit File Header...\n\n");
	printf("   V1.0:    %d\n",sizeof(struct sat_filehdr_1_0));
	printf("   V1.1:    %d\n",sizeof(struct sat_filehdr_1_1));
	printf("   V1.2:    %d\n",sizeof(struct sat_filehdr_1_2));
	printf("   V2.0:    %d\n",sizeof(struct sat_filehdr_2_0));
	printf("   Curr:    %d\n",sizeof(struct sat_filehdr));

}

main()
{
    printf("Compiled under version %d.%d\n\n",
        SAT_VERSION_MAJOR,SAT_VERSION_MINOR);

    hdr_check();

    /*
     * version 1.0 compatible records
     */

    if (sizeof(struct sat_hdr_1_0) != 48)
        printf("sizeof(struct sat_hdr_1_0) = %d, must be 48\n",
        	sizeof(struct sat_hdr_1_0));

    if (sizeof(struct sat_pathname_1_0) != 16)
        printf("sizeof(struct sat_pathname_1_0) = %d, must be 16\n",
        	sizeof(struct sat_pathname_1_0));

    if (sizeof(struct sat_filehdr_1_0) != 36)
        printf("sizeof(struct sat_filehdr_1_0) = %d, must be 36\n",
        	sizeof(struct sat_filehdr_1_0));

    if (sizeof(struct sat_user_ent_1_0) != 4)
        printf("sizeof(struct sat_user_ent_1_0) = %d, must be 4\n",
        	sizeof(struct sat_user_ent_1_0));

    if (sizeof(struct sat_group_ent_1_0) != 4)
        printf("sizeof(struct sat_group_ent_1_0) = %d, must be 4\n",
        	sizeof(struct sat_group_ent_1_0));

    if (sizeof(struct sat_mount_1_0) != 4)
        printf("sizeof(struct sat_mount_1_0) = %d, must be 4\n",
        	sizeof(struct sat_mount_1_0));

    if (sizeof(struct sat_file_attr_write_1_0) != 8)
        printf("sizeof(struct sat_file_attr_write_1_0) = %d, must be 8\n",
        	sizeof(struct sat_file_attr_write_1_0));

    if (sizeof(struct sat_exec_1_0) != 6)
        printf("sizeof(struct sat_exec_1_0) = %d, must be 6\n",
        	sizeof(struct sat_exec_1_0));

    if (sizeof(struct sat_fd_read2_1_0) != 32)
        printf("sizeof(struct sat_fd_read2_1_0) = %d, must be 32\n",
        	sizeof(struct sat_fd_read2_1_0));

    if (sizeof(struct sat_fd_attr_write_1_0) != 8)
        printf("sizeof(struct sat_fd_attr_write_1_0) = %d, must be 8\n",
        	sizeof(struct sat_fd_attr_write_1_0));

    if (sizeof(struct sat_proc_access_1_0) != 12)
        printf("sizeof(struct sat_proc_access_1_0) = %d, must be 12\n",
        	sizeof(struct sat_proc_access_1_0));

    if (sizeof(struct sat_proc_own_attr_write_1_0) != 4)
        printf("sizeof(struct sat_proc_own_attr_write_1_0) = %d, must be 4\n",
        	sizeof(struct sat_proc_own_attr_write_1_0));

    if (sizeof(struct sat_svipc_change_1_0) != 16)
        printf("sizeof(struct sat_svipc_change_1_0) = %d, must be 16\n",
        	sizeof(struct sat_svipc_change_1_0));

    /*
     * current records
     */

    if (sizeof(struct sat_hdr) != 64)
        printf("sizeof(struct sat_hdr) = %d, should be 64\n",
        	sizeof(struct sat_hdr));

    if (sizeof(struct sat_pathname) != 24)
        printf("sizeof(struct sat_pathname) = %d, should be 24\n",
        	sizeof(struct sat_pathname));

    if (sizeof(struct sat_filehdr) != 40)
        printf("sizeof(struct sat_filehdr) = %d, should be 40\n",
        	sizeof(struct sat_filehdr));

    if (sizeof(struct sat_list_ent) != 8)
        printf("sizeof(struct sat_list_ent) = %d, should be 8\n",
        	sizeof(struct sat_list_ent));

    if (sizeof(struct sat_wd) != 6)
        printf("sizeof(struct sat_wd) = %d, should be 6\n",
        	sizeof(struct sat_wd));

    if (sizeof(struct sat_open) != 8)
        printf("sizeof(struct sat_open) = %d, should be 8\n",
        	sizeof(struct sat_open));

    if (sizeof(struct sat_file_attr_write) != 8)
        printf("sizeof(struct sat_file_attr_write) = %d, should be 8\n",
        	sizeof(struct sat_file_attr_write));

    if (sizeof(struct sat_exec) != 12)
        printf("sizeof(struct sat_exec) = %d, should be 12\n",
        	sizeof(struct sat_exec));

    if (sizeof(struct sat_sysacct) != 4)
        printf("sizeof(struct sat_sysacct) = %d, should be 4\n",
        	sizeof(struct sat_sysacct));

    if (sizeof(struct sat_fchdir) != 4)
        printf("sizeof(struct sat_fchdir) = %d, should be 4\n",
        	sizeof(struct sat_fchdir));

    if (sizeof(struct sat_fd_read) != 4)
        printf("sizeof(struct sat_fd_read) = %d, should be 4\n",
        	sizeof(struct sat_fd_read));

    if (sizeof(struct sat_fd_read2) != 4)
        printf("sizeof(struct sat_fd_read2) = %d, should be 4\n",
        	sizeof(struct sat_fd_read2));

    if (sizeof(struct sat_tty_setlabel) != 4)
        printf("sizeof(struct sat_tty_setlabel) = %d, should be 4\n",
        	sizeof(struct sat_tty_setlabel));

    if (sizeof(struct sat_fd_write) != 4)
        printf("sizeof(struct sat_fd_write) = %d, should be 4\n",
        	sizeof(struct sat_fd_write));

    if (sizeof(struct sat_fd_attr_write) != 12)
        printf("sizeof(struct sat_fd_attr_write) = %d, should be 12\n",
        	sizeof(struct sat_fd_attr_write));

    if (sizeof(struct sat_pipe) != 4)
        printf("sizeof(struct sat_pipe) = %d, should be 4\n",
        	sizeof(struct sat_pipe));

    if (sizeof(struct sat_dup) != 4)
        printf("sizeof(struct sat_dup) = %d, should be 4\n",
        	sizeof(struct sat_dup));

    if (sizeof(struct sat_close) != 4)
        printf("sizeof(struct sat_close) = %d, should be 4\n",
        	sizeof(struct sat_close));

    if (sizeof(struct sat_fork) != 4)
        printf("sizeof(struct sat_fork) = %d, should be 4\n",
        	sizeof(struct sat_fork));

    if (sizeof(struct sat_exit) != 4)
        printf("sizeof(struct sat_exit) = %d, should be 4\n",
        	sizeof(struct sat_exit));

    if (sizeof(struct sat_proc_access) != 16)
        printf("sizeof(struct sat_proc_access) = %d, should be 16\n",
        	sizeof(struct sat_proc_access));

    if (sizeof(struct sat_proc_own_attr_write) != 8)
        printf("sizeof(struct sat_proc_own_attr_write) = %d, should be 8\n",
        	sizeof(struct sat_proc_own_attr_write));

    if (sizeof(struct sat_svipc_access) != 8)
        printf("sizeof(struct sat_svipc_access) = %d, should be 8\n",
        	sizeof(struct sat_svipc_access));

    if (sizeof(struct sat_svipc_create) != 12)
        printf("sizeof(struct sat_svipc_create) = %d, should be 12\n",
        	sizeof(struct sat_svipc_create));

    if (sizeof(struct sat_svipc_remove) != 4)
        printf("sizeof(struct sat_svipc_remove) = %d, should be 4\n",
        	sizeof(struct sat_svipc_remove));

    if (sizeof(struct sat_svipc_change) != 28)
        printf("sizeof(struct sat_svipc_change) = %d, should be 28\n",
        	sizeof(struct sat_svipc_change));

    if (sizeof(struct sat_bsdipc_create) != 12)
        printf("sizeof(struct sat_bsdipc_create) = %d, should be 12\n",
        	sizeof(struct sat_bsdipc_create));

    if (sizeof(struct sat_bsdipc_create_pair) != 16)
        printf("sizeof(struct sat_bsdipc_create_pair) = %d, should be 16\n",
        	sizeof(struct sat_bsdipc_create_pair));

    if (sizeof(struct sat_bsdipc_shutdown) != 8)
        printf("sizeof(struct sat_bsdipc_shutdown) = %d, should be 8\n",
        	sizeof(struct sat_bsdipc_shutdown));

    if (sizeof(struct sat_bsdipc_mac_change) != 8)
        printf("sizeof(struct sat_bsdipc_mac_change) = %d, should be 8\n",
        	sizeof(struct sat_bsdipc_mac_change));

    if (sizeof(struct sat_bsdipc_address) != 8)
        printf("sizeof(struct sat_bsdipc_address) = %d, should be 8\n",
        	sizeof(struct sat_bsdipc_address));

    if (sizeof(struct sat_bsdipc_resvport) != 8)
        printf("sizeof(struct sat_bsdipc_resvport) = %d, should be 8\n",
        	sizeof(struct sat_bsdipc_resvport));

    if (sizeof(struct sat_bsdipc_if_setlabel) != 36)
        printf("sizeof(struct sat_bsdipc_if_setlabel) = %d, should be 36\n",
        	sizeof(struct sat_bsdipc_if_setlabel));

    if (sizeof(struct sat_bsdipc_if_config) != 12)
        printf("sizeof(struct sat_bsdipc_if_config) = %d, should be 12\n",
        	sizeof(struct sat_bsdipc_if_config));

    if (sizeof(struct sat_bsdipc_match) != 8)
        printf("sizeof(struct sat_bsdipc_match) = %d, should be 8\n",
        	sizeof(struct sat_bsdipc_match));

    if (sizeof(struct sat_bsdipc_snoop) != 8)
        printf("sizeof(struct sat_bsdipc_snoop) = %d, should be 8\n",
        	sizeof(struct sat_bsdipc_snoop));

    if (sizeof(struct sat_bsdipc_range) != 20)
        printf("sizeof(struct sat_bsdipc_range) = %d, should be 20\n",
        	sizeof(struct sat_bsdipc_range));

    if (sizeof(struct sat_bsdipc_missing) != 18)
        printf("sizeof(struct sat_bsdipc_missing) = %d, should be 18\n",
        	sizeof(struct sat_bsdipc_missing));

    if (sizeof(struct sat_clock_set) != 4)
        printf("sizeof(struct sat_clock_set) = %d, should be 4\n",
        	sizeof(struct sat_clock_set));

    if (sizeof(struct sat_hostid_set) != 4)
        printf("sizeof(struct sat_hostid_set) = %d, should be 4\n",
        	sizeof(struct sat_hostid_set));

    if (sizeof(struct sat_check_priv) != 4)
        printf("sizeof(struct sat_check_priv) = %d, should be 4\n",
        	sizeof(struct sat_check_priv));

    if (sizeof(struct sat_control) != 12)
        printf("sizeof(struct sat_control) = %d, should be 12\n",
        	sizeof(struct sat_control));
}
