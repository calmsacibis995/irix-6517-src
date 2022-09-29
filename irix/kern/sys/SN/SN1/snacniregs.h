/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1996-1998, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/



/**************************************************************************
 *                                                                        *
 * WARNING!!!  WARNING!!!  WARNING!!!  WARNING!!!  WARNING!!!  WARNING!!! *
 *                                                                        *
 * This file is created by an automated script. Any changes made          *
 * manually to this  file will be lost.					  *
 *                                                                        *
 *               DO NOT EDIT THIS FILE MANUALLY			          *
 *                                                                        *
 **************************************************************************/



#ifndef __SYS_SN_SN1_SNACNIREGS_H__
#define __SYS_SN_SN1_SNACNIREGS_H__



#define    NI_STATUS_REV_ID          0x000000    /*
                                                  * Hub network status,
                                                  * revision. ID
                                                  */
#define    NI_PORT_RESET             0x000008    /*
                                                  * Reset the Network
                                                  * Interface
                                                  */
#define    NI_PROTECTION             0x000010    /*
                                                  * Region register access
                                                  * permission
                                                  */
#define    NI_GLOBAL_PARMS           0x000018    /* LLP Parameters          */
#define    NI_SCRATCH_REG0           0x000100    /* Scratch Register 0      */
#define    NI_SCRATCH_REG1           0x000108    /* Scratch Register 1      */
#define    NI_DIAG_PARMS             0x000110    /* Diagnostic Parameters   */
#define    NI_VECTOR_PARMS           0x000200    /* Vector PIO parameters   */
#define    NI_VECTOR_A               0x000220    /*
                                                  * Vector PIO Vector Route
                                                  * low bits
                                                  */
#define    NI_VECTOR_B               0x000228    /*
                                                  * Vector PIO Vector Route
                                                  * mid bits
                                                  */
#define    NI_VECTOR_C               0x000230    /*
                                                  * Vector PIO Vector Route
                                                  * high bits
                                                  */
#define    NI_VECTOR_DATA            0x000240    /* Vector PIO Write Data   */
#define    NI_VECTOR_STATUS          0x000300    /*
                                                  * Vector PIO Return
                                                  * Status
                                                  */
#define    NI_RETURN_VECTOR_A        0x000320    /*
                                                  * Vector PIO Return
                                                  * Vector first bits
                                                  */
#define    NI_RETURN_VECTOR_B        0x000328    /*
                                                  * Vector PIO Return
                                                  * Vector mid bits
                                                  */
#define    NI_RETURN_VECTOR_C        0x000330    /*
                                                  * Vector PIO Return
                                                  * Vector last bits
                                                  */
#define    NI_VECTOR_READ_DATA       0x000340    /* Vector PIO Read Data    */
#define    NI_STATUS_CLEAR           0x000380    /*
                                                  * Clear Vector PIO Return
                                                  * Status
                                                  */
#define    NI_IO_PROTECT             0x000400    /* PIO protection bits     */
#define    NI_IO_PROT_OVRRD          0x000408    /*
                                                  * PIO protection bit
                                                  * override
                                                  */
#define    NI_AGE_CPU0_MEMORY        0x000500    /* CPU 0 Memory agecontrol */
#define    NI_AGE_CPU0_PIO           0x000508    /* CPU 0 PIO age control   */
#define    NI_AGE_CPU1_MEMORY        0x000510    /*
                                                  * CPU 1 Memory age
                                                  * control
                                                  */
#define    NI_AGE_CPU1_PIO           0x000518    /* CPU 1 PIO age control   */
#define    NI_AGE_GBR_MEMORY         0x000520    /* GBR Memory age control  */
#define    NI_AGE_GBR_PIO            0x000528    /* GBR PIO age control     */
#define    NI_AGE_IO_MEMORY          0x000530    /* IO Memory age control   */
#define    NI_AGE_IO_PIO             0x000538    /* IO PIO age control      */
#define    NI_PORT_PARMS             0x008000    /* LLP Parameters          */
#define    NI_PORT_ERRORS            0x008008    /* Errors                  */
#define    NI_PORT_HEADER_A          0x008010    /* Error Header first half */
#define    NI_PORT_HEADER_B          0x008018    /*
                                                  * Error Header second
                                                  * half
                                                  */
#define    NI_PORT_SIDEBAND          0x008020    /* Error Sideband          */
#define    NI_PORT_ERROR_CLEAR       0x008088    /* Clear the Error bits    */
#define    NI_META_TABLE_0           0x038000    /* Meta Mapping Table 0-31 */
#define    NI_META_TABLE_1           0x038008    
#define    NI_META_TABLE_2           0x038010    
#define    NI_META_TABLE_3           0x038018    
#define    NI_META_TABLE_4           0x038020    
#define    NI_META_TABLE_5           0x038028    
#define    NI_META_TABLE_6           0x038030    
#define    NI_META_TABLE_7           0x038038    
#define    NI_META_TABLE_8           0x038040    
#define    NI_META_TABLE_9           0x038048    
#define    NI_META_TABLE_10          0x038050    
#define    NI_META_TABLE_11          0x038058    
#define    NI_META_TABLE_12          0x038060    
#define    NI_META_TABLE_13          0x038068    
#define    NI_META_TABLE_14          0x038070    
#define    NI_META_TABLE_15          0x038078    
#define    NI_META_TABLE_16          0x038080    
#define    NI_META_TABLE_17          0x038088    
#define    NI_META_TABLE_18          0x038090    
#define    NI_META_TABLE_19          0x038098    
#define    NI_META_TABLE_20          0x0380A0    
#define    NI_META_TABLE_21          0x0380A8    
#define    NI_META_TABLE_22          0x0380B0    
#define    NI_META_TABLE_23          0x0380B8    
#define    NI_META_TABLE_24          0x0380C0    
#define    NI_META_TABLE_25          0x0380C8    
#define    NI_META_TABLE_26          0x0380D0    
#define    NI_META_TABLE_27          0x0380D8    
#define    NI_META_TABLE_28          0x0380E0    
#define    NI_META_TABLE_29          0x0380E8    
#define    NI_META_TABLE_30          0x0380F0    
#define    NI_META_TABLE_31          0x0380F8    
#define    NI_META_TABLE_32          0x038100    
#define    NI_META_TABLE_33          0x038108    
#define    NI_META_TABLE_34          0x038110    
#define    NI_META_TABLE_35          0x038118    
#define    NI_META_TABLE_36          0x038120    
#define    NI_META_TABLE_37          0x038128    
#define    NI_META_TABLE_38          0x038130    
#define    NI_META_TABLE_39          0x038138    
#define    NI_META_TABLE_40          0x038140    
#define    NI_META_TABLE_41          0x038148    
#define    NI_META_TABLE_42          0x038150    
#define    NI_META_TABLE_43          0x038158    
#define    NI_META_TABLE_44          0x038160    
#define    NI_META_TABLE_45          0x038168    
#define    NI_META_TABLE_46          0x038170    
#define    NI_META_TABLE_47          0x038178    
#define    NI_META_TABLE_48          0x038180    
#define    NI_META_TABLE_49          0x038188    
#define    NI_META_TABLE_50          0x038190    
#define    NI_META_TABLE_51          0x038198    
#define    NI_META_TABLE_52          0x0381A0    
#define    NI_META_TABLE_53          0x0381A8    
#define    NI_META_TABLE_54          0x0381B0    
#define    NI_META_TABLE_55          0x0381B8    
#define    NI_META_TABLE_56          0x0381C0    
#define    NI_META_TABLE_57          0x0381C8    
#define    NI_META_TABLE_58          0x0381D0    
#define    NI_META_TABLE_59          0x0381D8    
#define    NI_META_TABLE_60          0x0381E0    
#define    NI_META_TABLE_61          0x0381E8    
#define    NI_META_TABLE_62          0x0381F0    
#define    NI_META_TABLE_63          0x0381F8    
#define    NI_LOCAL_TABLE_0          0x038200    /*
                                                  * Local Mapping Table
                                                  * 0-63
                                                  */
#define    NI_LOCAL_TABLE_1          0x038208    
#define    NI_LOCAL_TABLE_2          0x038210    
#define    NI_LOCAL_TABLE_3          0x038218    
#define    NI_LOCAL_TABLE_4          0x038220    
#define    NI_LOCAL_TABLE_5          0x038228    
#define    NI_LOCAL_TABLE_6          0x038230    
#define    NI_LOCAL_TABLE_7          0x038238    
#define    NI_LOCAL_TABLE_8          0x038240    
#define    NI_LOCAL_TABLE_9          0x038248    
#define    NI_LOCAL_TABLE_10         0x038250    
#define    NI_LOCAL_TABLE_11         0x038258    
#define    NI_LOCAL_TABLE_12         0x038260    
#define    NI_LOCAL_TABLE_13         0x038268    
#define    NI_LOCAL_TABLE_14         0x038270    
#define    NI_LOCAL_TABLE_15         0x038278    
#define    NI_LOCAL_TABLE_16         0x038280    
#define    NI_LOCAL_TABLE_17         0x038288    
#define    NI_LOCAL_TABLE_18         0x038290    
#define    NI_LOCAL_TABLE_19         0x038298    
#define    NI_LOCAL_TABLE_20         0x0382A0    
#define    NI_LOCAL_TABLE_21         0x0382A8    
#define    NI_LOCAL_TABLE_22         0x0382B0    
#define    NI_LOCAL_TABLE_23         0x0382B8    
#define    NI_LOCAL_TABLE_24         0x0382C0    
#define    NI_LOCAL_TABLE_25         0x0382C8    
#define    NI_LOCAL_TABLE_26         0x0382D0    
#define    NI_LOCAL_TABLE_27         0x0382D8    
#define    NI_LOCAL_TABLE_28         0x0382E0    
#define    NI_LOCAL_TABLE_29         0x0382E8    
#define    NI_LOCAL_TABLE_30         0x0382F0    
#define    NI_LOCAL_TABLE_31         0x0382F8    
#define    NI_LOCAL_TABLE_32         0x038300    
#define    NI_LOCAL_TABLE_33         0x038308    
#define    NI_LOCAL_TABLE_34         0x038310    
#define    NI_LOCAL_TABLE_35         0x038318    
#define    NI_LOCAL_TABLE_36         0x038320    
#define    NI_LOCAL_TABLE_37         0x038328    
#define    NI_LOCAL_TABLE_38         0x038330    
#define    NI_LOCAL_TABLE_39         0x038338    
#define    NI_LOCAL_TABLE_40         0x038340    
#define    NI_LOCAL_TABLE_41         0x038348    
#define    NI_LOCAL_TABLE_42         0x038350    
#define    NI_LOCAL_TABLE_43         0x038358    
#define    NI_LOCAL_TABLE_44         0x038360    
#define    NI_LOCAL_TABLE_45         0x038368    
#define    NI_LOCAL_TABLE_46         0x038370    
#define    NI_LOCAL_TABLE_47         0x038378    
#define    NI_LOCAL_TABLE_48         0x038380    
#define    NI_LOCAL_TABLE_49         0x038388    
#define    NI_LOCAL_TABLE_50         0x038390    
#define    NI_LOCAL_TABLE_51         0x038398    
#define    NI_LOCAL_TABLE_52         0x0383A0    
#define    NI_LOCAL_TABLE_53         0x0383A8    
#define    NI_LOCAL_TABLE_54         0x0383B0    
#define    NI_LOCAL_TABLE_55         0x0383B8    
#define    NI_LOCAL_TABLE_56         0x0383C0    
#define    NI_LOCAL_TABLE_57         0x0383C8    
#define    NI_LOCAL_TABLE_58         0x0383D0    
#define    NI_LOCAL_TABLE_59         0x0383D8    
#define    NI_LOCAL_TABLE_60         0x0383E0    
#define    NI_LOCAL_TABLE_61         0x0383E8    
#define    NI_LOCAL_TABLE_62         0x0383F0    
#define    NI_LOCAL_TABLE_63         0x0383F8    


#ifdef _LANGUAGE_C

typedef union ni_status_rev_id_u {
	__uint64_t	ni_status_rev_id_regval;
	struct  {
		__uint64_t	nsri_reserved             :	33;
		__uint64_t	nsri_rsvd_0               :	 1;
		__uint64_t	nsri_port_status          :	 2;
		__uint64_t	nsri_reserved_2           :	 9;
		__uint64_t	nsri_region_size          :	 2;
		__uint64_t	nsri_node_id              :	 9;
		__uint64_t	nsri_chiprevision         :	 4;
		__uint64_t	nsri_chipid               :	 4;
	} ni_status_rev_id_fld_s;
} ni_status_rev_id_u_t;


typedef union ni_port_reset_u {
	__uint64_t	ni_port_reset_regval;
	struct  {
		__uint64_t	npr_reserved              :	55;
		__uint64_t	npr_rsvd_0                :	 1;
		__uint64_t	npr_portreset             :	 1;
		__uint64_t	npr_reserved_2            :	 5;
		__uint64_t	npr_linkreset             :	 1;
		__uint64_t	npr_localreset            :	 1;
	} ni_port_reset_fld_s;
} ni_port_reset_u_t;


typedef union ni_protection_config_u {
	__uint64_t	ni_protection_config_regval;
	struct  {
		__uint64_t	npc_reserved              :	63;
		__uint64_t	npc_reset_ok              :	 1;
	} ni_protection_config_fld_s;
} ni_protection_config_u_t;


typedef union ni_global_parms_u {
	__uint64_t	ni_global_parms_regval;
	struct  {
		__uint64_t	ngp_reserved              :	 6;
		__uint64_t	ngp_maxretry              :	10;
		__uint64_t	ngp_tailtowrap            :	16;
		__uint64_t	ngp_reserved_2            :	12;
		__uint64_t	ngp_credit_to_val         :	 4;
		__uint64_t	ngp_reserved_4            :	 8;
		__uint64_t	ngp_tailtoval             :	 4;
		__uint64_t	ngp_reserved_6            :	 4;
	} ni_global_parms_fld_s;
} ni_global_parms_u_t;


typedef union ni_scratch_reg0_u {
	__uint64_t	ni_scratch_reg0_regval;
	struct  {
		__uint64_t	nsr_scratchbits           :	64;
	} ni_scratch_reg0_fld_s;
} ni_scratch_reg0_u_t;


typedef union ni_scratch_reg1_u {
	__uint64_t	ni_scratch_reg1_regval;
	struct  {
		__uint64_t	nsr_scratchbits           :	64;
	} ni_scratch_reg1_fld_s;
} ni_scratch_reg1_u_t;


typedef union ni_diag_parms_u {
	__uint64_t	ni_diag_parms_regval;
	struct  {
		__uint64_t	ndp_reserved              :	45;
		__uint64_t	ndp_port_to_reset         :	 1;
		__uint64_t	ndp_reserved_1            :	11;
		__uint64_t	ndp_portdisable           :	 1;
		__uint64_t	ndp_reserved_3            :	 5;
		__uint64_t	ndp_senddataerror         :	 1;
	} ni_diag_parms_fld_s;
} ni_diag_parms_u_t;


typedef union ni_vector_parms_u {
	__uint64_t	ni_vector_parms_regval;
	struct  {
		__uint64_t	nvp_reserved              :	11;
		__uint64_t	nvp_pio_id                :	13;
		__uint64_t	nvp_write_id              :	 8;
		__uint64_t	nvp_reserved_2            :	12;
		__uint64_t	nvp_address               :	17;
		__uint64_t	nvp_reserved_4            :	 1;
		__uint64_t	nvp_type                  :	 2;
	} ni_vector_parms_fld_s;
} ni_vector_parms_u_t;


typedef union ni_vector_x_u {
	__uint64_t	ni_vector_x_regval;
	struct  {
		__uint64_t	nvx_vector                :	64;
	} ni_vector_x_fld_s;
} ni_vector_x_u_t;


typedef union ni_vector_data_u {
	__uint64_t	ni_vector_data_regval;
	struct  {
		__uint64_t	nvd_write_data            :	64;
	} ni_vector_data_fld_s;
} ni_vector_data_u_t;


typedef union ni_vector_status_u {
	__uint64_t	ni_vector_status_regval;
	struct  {
		__uint64_t	nvs_status_valid          :	 1;
		__uint64_t	nvs_overrun               :	 1;
		__uint64_t	nvs_source                :	11;
		__uint64_t	nvs_pio_id                :	11;
		__uint64_t	nvs_write_id              :	 8;
		__uint64_t	nvs_address               :	29;
		__uint64_t	nvs_type                  :	 3;
	} ni_vector_status_fld_s;
} ni_vector_status_u_t;


typedef union ni_return_vector_x_u {
	__uint64_t	ni_return_vector_x_regval;
	struct  {
		__uint64_t	nrvx_return_vector        :	64;
	} ni_return_vector_x_fld_s;
} ni_return_vector_x_u_t;


typedef union ni_vector_read_data_u {
	__uint64_t	ni_vector_read_data_regval;
	struct  {
		__uint64_t	nvrd_read_data            :	64;
	} ni_vector_read_data_fld_s;
} ni_vector_read_data_u_t;


typedef union ni_io_protect_u {
	__uint64_t	ni_io_protect_regval;
	struct  {
		__uint64_t	nip_io_protect            :	64;
	} ni_io_protect_fld_s;
} ni_io_protect_u_t;


typedef union ni_io_prot_ovrrd_u {
	__uint64_t	ni_io_prot_ovrrd_regval;
	struct  {
		__uint64_t	nipo_io_prot_ovr          :	64;
	} ni_io_prot_ovrrd_fld_s;
} ni_io_prot_ovrrd_u_t;


typedef union ni_age_xxx_control_u {
	__uint64_t	ni_age_xxx_control_regval;
	struct  {
		__uint64_t	naxc_reserved             :	46;
		__uint64_t	naxc_vch_rp_long          :	 2;
		__uint64_t	naxc_vch_rp               :	 2;
		__uint64_t	naxc_vch_rq_long          :	 2;
		__uint64_t	naxc_vch_rq               :	 2;
		__uint64_t	naxc_cc                   :	 2;
		__uint64_t	naxc_age                  :	 8;
	} ni_age_xxx_control_fld_s;
} ni_age_xxx_control_u_t;


typedef union ni_port_parms_u {
	__uint64_t	ni_port_parms_regval;
	struct  {
		__uint64_t	npp_reserved              :	48;
		__uint64_t	npp_nulltimeout           :	 6;
		__uint64_t	npp_maxburst              :	10;
	} ni_port_parms_fld_s;
} ni_port_parms_u_t;


typedef union ni_port_error_u {
	__uint64_t	ni_port_error_regval;
	struct  {
		__uint64_t	npe_reserved              :	 5;
		__uint64_t	npe_link_reset            :	 1;
		__uint64_t	npe_internal_bad          :	 2;
		__uint64_t	npe_internal_long         :	 4;
		__uint64_t	npe_internal_short        :	 4;
		__uint64_t	npe_bad_header            :	 4;
		__uint64_t	npe_external_long         :	 4;
		__uint64_t	npe_external_short        :	 4;
		__uint64_t	npe_fifooverflow          :	 4;
		__uint64_t	npe_credit_timeout        :	 4;
		__uint64_t	npe_tail_timeout          :	 4;
		__uint64_t	npe_retrycount            :	 8;
		__uint64_t	npe_cberrorcount          :	 8;
		__uint64_t	npe_snerrorcount          :	 8;
	} ni_port_error_fld_s;
} ni_port_error_u_t;


typedef union ni_port_header_a_u {
	__uint64_t	ni_port_header_a_regval;
	struct  {
		__uint64_t	npha_source               :	13;
		__uint64_t	npha_supplement           :	13;
		__uint64_t	npha_reserved             :	13;
		__uint64_t	npha_destination          :	11;
		__uint64_t	npha_direction            :	 4;
		__uint64_t	npha_age                  :	 8;
		__uint64_t	npha_cc                   :	 2;
	} ni_port_header_a_fld_s;
} ni_port_header_a_u_t;


typedef union ni_port_header_b_u {
	__uint64_t	ni_port_header_b_regval;
	struct  {
		__uint64_t	nphb_prexsel              :	 2;
		__uint64_t	nphb_command              :	 8;
		__uint64_t	nphb_address              :	54;
	} ni_port_header_b_fld_s;
} ni_port_header_b_u_t;


typedef union ni_port_sideband_u {
	__uint64_t	ni_port_sideband_regval;
	struct  {
		__uint64_t	nps_reserved              :	52;
		__uint64_t	nps_fifo_full             :	 1;
		__uint64_t	nps_error                 :	 1;
		__uint64_t	nps_bad_length            :	 1;
		__uint64_t	nps_bad_dest              :	 1;
		__uint64_t	nps_sideband              :	 8;
	} ni_port_sideband_fld_s;
} ni_port_sideband_u_t;


typedef union ni_port_error_clear_u {
	__uint64_t	ni_port_error_clear_regval;
	struct  {
		__uint64_t	npec_reserved             :	60;
		__uint64_t	npec_next_exit_port       :	 4;
	} ni_port_error_clear_fld_s;
} ni_port_error_clear_u_t;




#endif /* _LANGUAGE_C */




#endif  /* __SYS_SN_SN1_SNACNIREGS_H__ */
