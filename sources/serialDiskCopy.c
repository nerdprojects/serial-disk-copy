// many thx to stephan brumme for the nice crc32 implementation: http://create.stephan-brumme.com/crc32/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <Devices.h>
#include <Serial.h>
#include <Files.h>
#include <StandardFile.h>
#include <OSUtils.h>

enum {
	logicalBlockSize = 512 // are different values on an old mac even possible?
};

struct diskInfo {
	char name[255];
	short driver;
	short volume;
	unsigned int bytes;
};

short gDiskInputRefNum;
short gOutputRefNum;
short gDiskVRefNum;
short gInputRefNum;
long gDiskTotalBytes;
Handle gInputBufHandle;
int gFileBufferSize;
char doInfo;
char doCRC;
unsigned int crcTable[256];


unsigned char *p2c(unsigned char *str) {
	int j, len;
	len = str[0];			
	for(j = 1; j <= len; j++)	/* memcpy() works for this too */
		str[j - 1] = str[j];
	str[len] = 0;			/* Store the null terminator */
	return str;
}

char *c2p(char *str) {
	int j, len;
	len = strlen(str);
	if(len > 255) len = 255;
	for(j = 1; j <= len; j++)	/* memcpy() works for this too */
		str[j] = str[j - 1];
	str[0] = len;			/* Record the string length */
	return str;
}

void crcInit () {

	int tmp;
	unsigned int crc, i, j;

	for (i = 0; i <= 0xFF; i++) {
		crc = i;
		for (j = 0; j < 8; j++) {
			tmp = -(crc & 1);
			crc = (crc >> 1) ^ (tmp & 0xedb88320);
		}
		crcTable[i] = crc;
	}
}

unsigned int crcCalc(char* data, long length) {

	unsigned int crc;
	unsigned char* current;
  
	crc = ~0;
	current = (unsigned char*) data;
	while (length--) {
		crc = (crc >> 8) ^ crcTable[(crc & 0xFF) ^ *current++];
	}
	return ~crc;
	
}

void myOpenSerialDriver () {

	OSErr myErr;

	myErr = OpenDriver("\p.AOut", &gOutputRefNum);
	if( myErr == noErr ){
		myErr = OpenDriver("\p.AIn", &gInputRefNum);
		if( myErr != noErr ){
			printf("error opening input driver %d\n", myErr);
		}
	}else{
		printf("error opening output driver %d\n", myErr);
	}
	
}

void myCloseSerialDriver() {

	OSErr myErr;

	myErr = KillIO(gOutputRefNum);
	if( myErr == noErr ){
	
		myErr = CloseDriver(gInputRefNum);
		if( myErr != noErr ){
			printf("error closing input driver %d\n", myErr);
		}
		
		myErr = CloseDriver(gOutputRefNum);
		if( myErr != noErr ){
			printf("error closing output driver %d\n", myErr);
		}
		
	}else{
		printf("error killing io %d\n", myErr);
	}
	
}

void myChangeInputBuffer() {
	
	short kInputBufSize = 1024;
	
	gInputBufHandle = NewHandle(kInputBufSize);
	HLock(gInputBufHandle);
	SerSetBuf(gInputRefNum, *gInputBufHandle, kInputBufSize);

}

void myRestoreInputBuffer() {

	SerSetBuf(gInputRefNum, *gInputBufHandle, 0);
	HUnlock(gInputBufHandle);
	
}

void mySetHandshakeOptions() {

	SerShk mySerShkRec;
	OSErr myErr;
	
	mySerShkRec.fXOn = 0; // no output XOn/XOff
	mySerShkRec.fCTS = 0; // no output CTS/DTR
	mySerShkRec.errs = 0; // clear error mask
	mySerShkRec.evts = 0; // clear event mask
	mySerShkRec.fInX = 0; // no input XOn/XOff
	mySerShkRec.fDTR = 0; // no input CTR/DTR
	
	myErr = Control(gOutputRefNum, 14, &mySerShkRec);
	if( myErr != noErr ){
		printf("error setting serial handshake options %d\n", myErr);
	}

}

void myConfigureThePort() {

	OSErr myErr;
	short kConfigParam;
	
	kConfigParam = baud57600 + data8 + noParity + stop10;
	myErr = SerReset(gOutputRefNum, kConfigParam);
	if( myErr != noErr ){
		printf("error setting serial transfer options %d\n", myErr);
	}
	
}

void sendSerialData(char* dataBuffer, long bufferSize) {

	OSErr myErr;
	IOParam myParamBlock;
	
	myParamBlock.ioRefNum = gOutputRefNum;
	myParamBlock.ioBuffer = (Ptr)dataBuffer;
	myParamBlock.ioReqCount = bufferSize;
	myParamBlock.ioCompletion = nil;
	myParamBlock.ioVRefNum = 0;
	myParamBlock.ioPosMode = 0;
		
	myErr = PBWrite((ParmBlkPtr)&myParamBlock, false);
	if( myErr != noErr ){
		printf("error sending data %d\n", myErr);
	}
}

long readDiskBlock(char* fileBuffer, long count, long offset) {
	
	OSErr osError;
	IOParam ioParamBlock;
	
	ioParamBlock.ioRefNum = gDiskInputRefNum; // SCSI ID 0 = -33
	ioParamBlock.ioBuffer = (Ptr)fileBuffer;
	ioParamBlock.ioReqCount = count;
	ioParamBlock.ioVRefNum = gDiskVRefNum; // volume nummer (DrvNum)
	ioParamBlock.ioCompletion = nil;
	ioParamBlock.ioPosMode = fsFromStart;
	ioParamBlock.ioPosOffset = offset;
	ioParamBlock.ioActCount = 0;
	
	osError = PBReadSync((ParmBlkPtr)&ioParamBlock);
	if( osError != noErr ){
		printf("error reading data %d, driver error %d\n", osError, ioParamBlock.ioResult);
	}
	
	return ioParamBlock.ioActCount;
	
}	

void getDiskInfos (struct diskInfo **diskInfos, short *diskCount) {

	OSErr osError;
	QHdrPtr driverQueueHeader;
	DrvQElPtr headerElement;
	short driverNumber;
	unsigned char volumeName[255];
	short volumeNumber;
	short oldVolumeNumber;
	unsigned int volumeBlocks;
	unsigned int volumeBytes;
	long freeBytes;
	unsigned short i;

	driverQueueHeader = GetDrvQHdr();
	headerElement = (DrvQElPtr)driverQueueHeader->qHead;
		
	// count the drives
	*diskCount = 0;
	while (headerElement != nil) {
		driverNumber = headerElement->dQRefNum;
		volumeNumber = headerElement->dQDrive;
		osError = GetVInfo(volumeNumber, volumeName, &oldVolumeNumber, &freeBytes);
		if(osError == noErr) {
			(*diskCount)++;
		}
		headerElement = (DrvQElPtr)headerElement->qLink;
	}
		
	// allocate and fill the stuff
	(*diskInfos) = malloc(*diskCount*sizeof(struct diskInfo));
	i = 0;
	headerElement = (DrvQElPtr)driverQueueHeader->qHead;
	while (headerElement != nil) {
		
		driverNumber = headerElement->dQRefNum;
		volumeNumber = headerElement->dQDrive;
		if (headerElement->qType == 1) {
			volumeBlocks = headerElement->dQDrvSz2;
			volumeBlocks = volumeBlocks << 16;
			volumeBlocks = volumeBlocks | headerElement->dQDrvSz;
		} else {
			volumeBlocks = headerElement->dQDrvSz;
		}
		
		// ugly hack for floppies, because the dQDrvSz is always 65535 blocks (which is too mutch)
		// this should be corrected some time, 
		// maybe by quering the size of the floppy with a api call or someting
		if (driverNumber == -5) {
			volumeBlocks = 2880; // 2880 blocks is a 1440k floppy
		}
		
		volumeBytes = volumeBlocks*logicalBlockSize;

		osError = GetVInfo(volumeNumber, volumeName, &oldVolumeNumber, &freeBytes);
		if(osError != noErr) {
			printf("error %d occured when getting volume info for volume %d\n", osError, volumeNumber);
			headerElement = (DrvQElPtr)headerElement->qLink;
			continue;
		}
				
		strcpy((*diskInfos)[i].name, (char*)p2c(volumeName));
		(*diskInfos)[i].driver = driverNumber;
		(*diskInfos)[i].volume = volumeNumber;
		(*diskInfos)[i].bytes = volumeBytes;
		i++;
		
		headerElement = (DrvQElPtr)headerElement->qLink;
		
	}
	
}

void getOptions() {

	struct diskInfo* diskInfos;
	short diskCount;
	short i;
	char inputChar;
	short input;
	int bufferSizes[8] = {512,1024,4096,8192,65536,262144,1048576,4194304};
	
	getDiskInfos(&diskInfos, &diskCount);
	
	printf("found %d drives, choose one:\n", diskCount);
	for(i = 0; i < diskCount; i++) {
		printf("%d - %s (%d/%d) (%d bytes)\n", i, diskInfos[i].name, diskInfos[i].volume, diskInfos[i].driver, diskInfos[i].bytes);
	}

	input = -1;
	while(input < 0 || input > diskCount - 1) {
		inputChar = getchar();
		fflush(stdin);
		input = (short)inputChar - 48;
	}
	gDiskVRefNum = diskInfos[input].volume;
	gDiskInputRefNum = diskInfos[input].driver;
	gDiskTotalBytes = (long)diskInfos[input].bytes;
	printf("disk set to %s\n\n", diskInfos[input].name);
	
	
	printf("choose transfer buffer size:\n");
	for(i = 0; i < 8; i++) {
		printf("%d - %d\n", i, bufferSizes[i]);
	}

	input = -1;
	while (input < 0 || input > 7) {
		inputChar = getchar();
		fflush(stdin);
		input = (short)inputChar - 48;
	}
	gFileBufferSize = bufferSizes[input];
	printf("buffer size set to %d\n\n", bufferSizes[input]);

	/*
	printf("do Info (Y/n):\n");
	inputChar = getchar();
	fflush(stdin);
	if (inputChar == 'n') {
		doInfo = 0;
		printf("no info\n\n");
	} else {
		doInfo = 1;
		printf("print info\n\n");
	}
	
	printf("do CRC (Y/n):\n");
	inputChar = getchar();
	fflush(stdin);
	if (inputChar == 'n') {
		doCRC = 0;
		printf("no CRC\n\n");
	} else {
		doCRC = 1;
		printf("calc CRC\n\n");
	}
	*/
	
	doInfo = 1;
	doCRC = 1;
	
	// maybe also let the user choose the baud rate?

}

void startSerial() {
	//printf("\nopen driver\n");
	myOpenSerialDriver();	
	//printf("\ncreate input buffer\n");
	myChangeInputBuffer();
	//printf("\nsetting handshake options\n");
	mySetHandshakeOptions();
	//printf("\nsetting transfer options\n");
	myConfigureThePort();
}

void stopSerial() {
	//printf("\nrestoring buffer\n");
	myRestoreInputBuffer();
	//printf("\nclosing driver\n");
	myCloseSerialDriver();
}

void sendBackup() {

	long readBytes;
	long totalReadBytes;
	unsigned int crc;
	char* fileBuffer;
	int progress;
	long bytesToRead;
	
	fileBuffer = malloc(gFileBufferSize);

	
	// header
	sendSerialData("DISK", 4);
	// size of disk
	sendSerialData((char*)&gDiskTotalBytes, sizeof(gDiskTotalBytes));
	
	// send all blocks and their crc
	totalReadBytes = 0;
	progress = 0;
	crcInit();
	crc = 0;
	while (totalReadBytes < gDiskTotalBytes) {
		
		sendSerialData("DATA", 4);
		// check if we are over the last block, ajust bytesToRead accordingly
		bytesToRead = gFileBufferSize;
		if (totalReadBytes + bytesToRead > gDiskTotalBytes) {
			bytesToRead = gDiskTotalBytes - totalReadBytes;
		}
		//printf("going to read %d bytes from %d \n", bytesToRead, totalReadBytes);
		readBytes = readDiskBlock((char*)fileBuffer, bytesToRead,totalReadBytes);
		if (doInfo) {
			progress = (float)100 / (float)gDiskTotalBytes * (float)totalReadBytes;
			printf("%d%% - sending %d bytes from %d to %d of total %d\n", progress, readBytes, totalReadBytes, totalReadBytes+readBytes, gDiskTotalBytes);
		}
		
		sendSerialData((char*)&readBytes, sizeof(readBytes));
		sendSerialData(fileBuffer, readBytes);
		
		if (doCRC) {
			crc = crcCalc(fileBuffer, readBytes);
		}
		sendSerialData((char*)&crc, 4);

		
		totalReadBytes += readBytes;

	}
	
	// footer
	sendSerialData("DONE", 4);
}

int main () {

	getOptions();

	printf("hit any key to start the transfer\n\n");
	getchar();
	fflush(stdin);

	startSerial();
	
	sendBackup();

	stopSerial();

	printf("\n\ndone...\n");

	return 0;
}

