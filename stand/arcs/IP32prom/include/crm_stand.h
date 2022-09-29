/*
 * crm_stand.h - initialization graphics routine header for Moosehead
 *
 *
 */


/*
 * Things to do:
 * 1) Clean out temporary space variables.
 *
 */


/************************************************************************/
/* Temporary Space							*/
/************************************************************************/

#define CRM_RE_BASE_ADDRESS		0x14100000
#define CRM_CPU_INTERFACE_BASE_ADDRESS	0x14000000

#define CRMTEST_GBE_PETTY_CRIME		0x00000001
#define CRMTEST_GBE_CAPITAL_CRIME	0x00000002

/***********************************/
/* Definitions needed for crm_tp.c */
/***********************************/

#ifndef CURSOR_XOFF
  #define CURSOR_XOFF
#endif

#ifndef CURSOR_YOFF
  #define CURSOR_YOFF
#endif

/************************************/
/* Definitions needed for crm_i2c.c */
/************************************/

/* Crime Extended Display Identification Data */
struct crmEDID {
	char Header[8];
	char ManufacturerName[2];		/* EISA compressed 3-char ID */
	char ProductCode[2];
	char SerialNum[4];
	char WeekOfManufacture;
	char YearOfManufacture;
	char Version;
	char Revision;
	char VideoInputDefinition;
	char MaxHImageSize;			/* in cm. */
	char MaxVImageSize;			/* in cm. */
	char DisplayTransferChar;
	char FeatureSupport;
	char RedGreenLowBits;
	char BlueWhiteLowBits;
	char RedX;
	char RedY;
	char GreenX;
	char GreenY;
	char BlueX;
	char BlueY;
	char WhiteX;
	char WhiteY;
	char EstablishedTimings1;
	char EstablishedTimings2;
	char MfgReservedTimings;
	char StandardTimingID1[2];
	char StandardTimingID2[2];
	char StandardTimingID3[2];
	char StandardTimingID4[2];
	char StandardTimingID5[2];
	char StandardTimingID6[2];
	char StandardTimingID7[2];
	char StandardTimingID8[2];
	char DetailedTimingDescriptions1[18];
	char DetailedTimingDescriptions2[18];
	char DetailedTimingDescriptions3[18];
	char DetailedTimingDescriptions4[18];
	char ExtensionFlag;
	char Checksum;
};

/*******************************************/
/* Definitions for the CRIME CPU Interface */
/*******************************************/

typedef struct crm_cpu_interface
{
   unsigned long long crimeID;
   unsigned long long crimeControl;
   unsigned long long crimeIntStatus;
   unsigned long long crimeIntEnable;
   unsigned long long crimeSwInterrupt;
   unsigned long long crimeHwInterrupt;
   unsigned long long crimeWatchdogTimer;
   unsigned long long crimeCrimeTime;
   unsigned long long crimeCPUErrorAddr;
   unsigned long long crimeVICEErrorStatus;
   unsigned long long crimeVICEErrorEnable;
   unsigned long long crimeVICEErrorAddr;
} CrimeCPUInterface;


/*****************************************************/
/* Definitions for reading and writing GBE registers */
/*****************************************************/

#define GBE_OVR_RHS_MASK                0x1F
#define GBE_OVR_WIDTH_TILE_MASK         0x1FE0
#define GBE_OVR_WIDTH_TILE_SHIFT        5

#define GBE_OVR_TILE_PTR_MASK           0xFFFFFFE0
#define GBE_OVR_DMA_ENABLE_MASK         0x1

#define GBE_FRM_WIDTH_TILE_MASK         0x1FE0
#define GBE_FRM_WIDTH_TILE_SHIFT        5
#define GBE_FRM_DEPTH_MASK              0x6000
#define GBE_FRM_DEPTH_SHIFT             13

#define GBE_FRM_DEPTH_8                 0
#define GBE_FRM_DEPTH_16                1
#define GBE_FRM_DEPTH_32                2

#define GBE_FRM_HEIGHT_PIX_MASK         0xFFFF0000
#define GBE_FRM_HEIGHT_PIX_SHIFT        16

#define GBE_FRM_TILE_PTR_MASK           0xFFFFFFE0
#define GBE_FRM_DMA_ENABLE_MASK         0x1

#define GBE_DID_DMA_ENABLE_MASK         0x10000
#define GBE_DID_DMA_ENABLE_SHIFT        16

#define GBE_NUM_WIDS                    32
#define GBE_WID_BUF_MASK                0x3
#define GBE_WID_TYPE_MASK               0x1C
#define GBE_WID_TYPE_SHIFT              2
#define GBE_WID_CM_MASK                 0x3E0
#define GBE_WID_CM_SHIFT                5
#define GBE_WID_GM_MASK                 0x400
#define GBE_WID_GM_SHIFT                10

#define GBE_BMODE_BOTH                  3

#define GBE_CMODE_I8                    0

#define GBE_CRS_POSX_MASK               0xFFF
#define GBE_CRS_POSY_MASK               0xFFF0000
#define GBE_CRS_POSY_SHIFT              16

#define GBE_CRS_ENABLE_MASK             0x1

#define gbeGetReg(hwpage, reg, val) \
   (val) = (hwpage)->reg;

#define gbeGetTable(hwpage, reg, index, val) \
   (val) = (hwpage)->reg[index];
