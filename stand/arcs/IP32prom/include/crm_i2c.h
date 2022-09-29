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
