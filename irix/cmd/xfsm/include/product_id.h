/*
 * NetLS Product IDs for SGI products
 *
 * To allocate yourself a product ID, take the following steps:
 *
 *    Create a ptools workarea for yourself that points at
 *    bonnie.wpd:/depot/netls/netls_prod_id
 *
 *    Use p_tupdate to get a local copy of this file (product_id.h).
 *
 *    Use p_modify and p_integrate to lock the file.  Pick an unused
 *    number, and make a #define for your product.  Please keep the
 *    numbers in order.
 *
 *    Use p_finalize to check the file back in.
 *
 *    #include <product_id.h>, and use the #define in your code
 *    instead of hardcoding the number.
 *
 * This file normally lives in /usr/include.  To get the latest
 * 'official' version, install the netls_prod_id ism.
 *
 * These numbers must never be re-used, and we have lots of them,
 * so don't remove any entries.
 */

/*
 * Product IDs for Silicon Graphics software products.  Add yours to
 * this list IN ORDER please.
 */
#define	PRODID_WORKSHOP			1
#define	PRODID_NETWORKER		3	/* NetWorker 1.2 Base version */
#define	PRODID_4DDN			4
#define	PRODID_NETVIS			6
#define	PRODID_VOLMAN			7	/* Volume Manager 1.0/2.0 */
#define	PRODID_WORKSHOP_TESTER		11	/* no longer in active use */
#define	PRODID_WORKSHOP_PRO_MPF		12
#define	PRODID_WORKSHOP_PRO_CXX		13
#define	PRODID_WORKSHOP_PRO_C		14	/* no longer in active use */
#define PRODID_DELTA_CXX                15      /* Delta C++ */
#define	PRODID_TRACKER			23
#define	PRODID_TRACKER_USER		25
#define PRODID_DM_BUILDER               26

#define	PRODID_NETWORKERJUKE		36	/* NetWorker 1.2 Jukebox */
#define	PRODID_NETWKR4_BASE		37	/* NetWorker 4.0 Base version */
#define	PRODID_NETWKR4_ADV		38	/* NetWorker 4.0 Advanced version */
#define	PRODID_NETWKR4_EXTENDED		39	/* NetWorker 4.0 Extended version */
#define	PRODID_NETWKR4_CDS		40	/* NetWorker 4.0 Concurrent Dev */
#define	PRODID_NETWKR4_CLIENTS10	41	/* NetWorker 4.0 add 10 clients */
#define	PRODID_NETWKR4_CLIENTS50	42	/* NetWorker 4.0 add 50 clients */
#define	PRODID_NETWKR4_JUKE16		43	/* NetWorker 4.0 add a 16-slot jukebox */
#define	PRODID_NETWKR4_JUKE64		44	/* NetWorker 4.0 add a 64-slot jukebox */
#define	PRODID_NETWKR4_JUKEBIG		45	/* NetWorker 4.0 add an unlimited jukebox */

#define	PRODID_EPOCH_EPOCHSAVE		50	/* EpochSave enabler */
#define	PRODID_EPOCH_EBACKUP		51	/* EpochBackup enabler */
#define	PRODID_EPOCH_EMIGSRV		52	/* EpochMigration enabler */
#define	PRODID_EPOCH_EBC_1		53	/* EpochBackup 1 client upgrade */
#define	PRODID_EPOCH_EBC_5		54	/* EpochBackup 5 client upgrade */
#define	PRODID_EPOCH_EBC_10		55	/* EpochBackup 10 client upgrade */
#define	PRODID_EPOCH_EBC_15		56	/* EpochBackup 15 client upgrade */
#define	PRODID_EPOCH_EBC_25		57	/* EpochBackup 25 client upgrade */
#define	PRODID_EPOCH_EBC_50		58	/* EpochBackup 50 client upgrade */
#define	PRODID_EPOCH_EBC_100		59	/* EpochBackup 100 client upgrade */
#define	PRODID_EPOCH_EBC_250		60	/* EpochBackup 250 client upgrade */
#define	PRODID_EPOCH_EBC_500		61	/* EpochBackup 500 client upgrade */
#define	PRODID_EPOCH_EMC_1		62	/* EpochMigration 1 client upgrade */
#define	PRODID_EPOCH_EMC_5		63	/* EpochMigration 5 client upgrade */
#define	PRODID_EPOCH_EMC_10		64	/* EpochMigration 10 client upgrade */
#define	PRODID_EPOCH_EMC_25		65	/* EpochMigration 25 client upgrade */
#define	PRODID_EPOCH_OLU_40		66	/* Epoch Library Manager: 40GB OLU */
#define	PRODID_EPOCH_OLU_112		67	/* Epoch Library Manager: 112GB OLU */
#define	PRODID_EPOCH_OLU_180		68	/* Epoch Library Manager: 180GB OLU */
#define	PRODID_EPOCH_TLU_10		69	/* Epoch Library Manager: Tape Storage */
#define	PRODID_EPOCH_OLU_ANY		70	/* Epoch Library Manager: Any Library Unit */
#define PRODID_GRIO_32                  71      /* Guaranteed Rate I/O: 32 streams */
#define PRODID_GRIO_64                  72      /* Guaranteed Rate I/O: 64 streams */
#define PRODID_GRIO_128                 73      /* Guaranteed Rate I/O: 128 streams */
#define PRODID_GRIO_ANY                 74      /* Guaranteed Rate I/O: unlimited streams */
#define PRODID_XLV_PLEXING		75	/* XLV Plexing (Disk Mirroring) */

#define	PRODID_INPERSON			100
#define PRODID_DESKS			101
#define	PRODID_CLEARCASE		14698

/*
 * Product ID aliases.
 */
#define	PRODID_CODEVISION		PRODID_WORKSHOP
