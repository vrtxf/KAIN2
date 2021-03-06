#include "LOAD3D.H"

#include "MEMPACK.H"
#include "G2TYPES.H"

#include <SYS\TYPES.H>
#include <LIBCD.H>
#include <LIBETC.H>
#include <LIBGPU.H>
#include <LIBSN.H>
#include <STDLIB.H>
#include <STDIO.H>

struct LoadStatus loadStatus;

void LOAD_InitCd()
{
	CdInit();
	ResetCallback();
	CdSetDebug(0);
}

void LOAD_CdSeekCallback(unsigned char intr, unsigned char* result)
{
	if (loadStatus.waitingForSeek)
	{
		loadStatus.waitingForSeek = 0;
	}
}

void LOAD_CdDataReady()
{
	if (loadStatus.currentSector > loadStatus.currentQueueFile->blocks)
	{
		loadStatus.currentQueueFile->bufferFlags |= 0x1;
	}
}

void LOAD_CdReadReady(unsigned char intr, unsigned char* result)
{
	if (loadStatus.waitingForData != NULL)
	{
		if ((intr & 0xFF) == 1)
		{
			if (loadStatus.currentSector < loadStatus.currentQueueFile->blocks)
			{
				CdGetSector((void*)loadStatus.currentQueueFile->dest[loadStatus.currentSector++ << 9], 1 << 9);

				if (loadStatus.currentSector > loadStatus.currentQueueFile->blocks)
				{
					CdControlF(9, NULL);
				}
			}
		}
	}
}

void LOAD_UpdateCheckSum(struct FileAccessInfo* currentQueueFile, long sectors)
{
	long i;

	if (currentQueueFile->checksumType == 1)
	{
		i = 0;
		if ((sectors << 9) <= 0)
		{
			return;
		}

		++i;

		//loc_8003775C
		while (i < sectors)
		{
			loadStatus.checksum += *loadStatus.checkAddr;
			loadStatus.checkAddr++;
			i++;
		}
	}//loc_8003778C
	else if (currentQueueFile->checksumType == 2)
	{
		i = 0;

		if (sectors <= 0)
		{
			return;
		}

		++i;

		//loc_800377A0
		while (i < sectors)
		{
			loadStatus.checksum += *loadStatus.checkAddr;
			loadStatus.checkAddr += 512;
			i++;
		}
	}
}

void LOAD_DoCDReading(struct FileAccessInfo* currentQueueFile /*$s1*/)
{
	long sectorsLoaded; // $s0
	long decompressLen; // $s0

	sectorsLoaded = 0;

	if (*loadStatus.waitingSector != 0xABBA1234)
	{
		//loc_80037804
		if (loadStatus.remSectors != 0)
		{
			while (*loadStatus.waitingSector != 0xABBA1234)
			{
				sectorsLoaded++;
				loadStatus.remSectors--;
				loadStatus.waitingSector += 512;
			}
		}
		//loc_80037838
	}
	//loc_80037838
	if (sectorsLoaded != 0)
	{
		if (currentQueueFile->checksumType != 0)
		{
			LOAD_UpdateCheckSum(currentQueueFile, sectorsLoaded);
		}
		//loc_80037858
		if (currentQueueFile->compressedLen != 0)
		{
			decompressLen = (sectorsLoaded << 11) + loadStatus.compEndBytes;
			if (loadStatus.decompressLen < decompressLen)
			{
				decompressLen = loadStatus.decompressLen;
			}
			//loc_80037884
			loadStatus.compEndBytes = LOAD_lzrw1_decompress(decompressLen, 4);
			loadStatus.decompressLen = (loadStatus.decompressLen + loadStatus.compEndBytes) - decompressLen;
		}//loc_800378A4
	}
	//loc_800378A4
	if (*loadStatus.waitingForData != 0xABBA1234 && loadStatus.remSectors == 0)
	{
		if (loadStatus.compEndBytes != 0)
		{
			loadStatus.compEndBytes = LOAD_lzrw1_decompress(loadStatus.compEndBytes, 1);
		}//loc_800378E8

		if (currentQueueFile->checksumType != 0 && loadStatus.checksum != currentQueueFile->checksum)
		{
			currentQueueFile->status = 4;
		}
		else
		{
			//loc_80037918
			currentQueueFile->status = 0;
		}
	}//loc_80037954

	loadStatus.waitingForData = 0;
	loadStatus.currentQueueFile = &loadStatus.loadQueue[(++loadStatus.currentQueueFileIndex) & 3];
}

/*
 * Offset 0x80037968
 * C:\kain2\game\LOAD3D.C (line 549)
 * Stack frame base $sp, size 40
 * Saved registers at offset -8: s0 s1 ra
 */
void /*$ra*/ LOAD_DoCDBufferedReading(struct FileAccessInfo* currentQueueFile /*$s0*/)
{ // line 1, offset 0x80037968
    struct NonBlockLoadEntry *loadEntry; // $s1
    long actualReadSize; // $a1
    long status; // $a2
} // line 91, offset 0x80037b34
/*
 * Offset 0x80037B48
 * C:\kain2\game\LOAD3D.C (line 642)
 * Stack frame base $sp, size 24
 * Saved registers at offset -8: ra
 */
void /*$ra*/ LOAD_SetupFileToDoCDReading(struct FileAccessInfo* currentQueueFile /*$a1*/)
{ // line 1, offset 0x80037b48
    long i; // $a0


} // line 34, offset 0x80037c1c
/*
 * Offset 0x80037C68
 * C:\kain2\game\LOAD3D.C (line 754)
 * Stack frame base $sp, size 24
 * Saved registers at offset -8: ra
 */
void /*$ra*/ LOAD_SetupFileToDoBufferedCDReading(struct FileAccessInfo* currentQueueFile /*$a1*/)
{
}

void LOAD_ProcessReadQueue()
{
	struct FileAccessInfo* currentQueueFile = loadStatus.currentQueueFile;

	if (currentQueueFile->status == 3)
	{
		LOAD_DoCDReading(currentQueueFile);
	}
	else if (currentQueueFile->status == 6)
	{
		LOAD_DoCDBufferedReading(currentQueueFile);
	}
	else if (currentQueueFile->status == 1)
	{
		LOAD_SetupFileToDoCDReading(currentQueueFile);
	}
	else if (currentQueueFile->status == 5)
	{
		LOAD_SetupFileToDoBufferedCDReading(currentQueueFile);
	}

	//loc_80037D90
	if (currentQueueFile->status == 4)
	{
		//loc_80037DE0
		currentQueueFile->status = 1;
	}
	else if (currentQueueFile->status == 3 || currentQueueFile->status == 6)
	{
		if (0x401640 < TIMER_TimeDiff2(loadStatus.cdWaitTime))
		{
			currentQueueFile->status = 1;
		}
	}
}

long LOAD_CdReadFromBigFile(long fileOffset, unsigned long* loadAddr, unsigned long* finalDest, long blocks, long chksumLevel, long checksum, long compressed)
{
	struct FileAccessInfo* currentQueueReq;
	long oldQueueReqIndex;

	currentQueueReq = &loadStatus.loadQueue[loadStatus.currentQueueReqIndex];

	if (currentQueueReq->status != 0)
	{
		while (currentQueueReq->status != 0)
		{
			LOAD_ProcessReadQueue();
		}
	}//loc_80037E68

	CdIntToPos(loadStatus.bigFile.bigfileBaseOffset + fileOffset, &currentQueueReq->loc);

	currentQueueReq->blocks = blocks;
	currentQueueReq->dest = loadAddr;
	currentQueueReq->finalDest = finalDest;
	currentQueueReq->fileOffset = fileOffset;
	currentQueueReq->status = 1;
	currentQueueReq->checksumType = chksumLevel;
	currentQueueReq->checksum = checksum;
	currentQueueReq->bufferBlocks = 0;
	currentQueueReq->compressedLen = compressed;

	oldQueueReqIndex = loadStatus.currentQueueReqIndex;

	loadStatus.currentQueueReqIndex = (oldQueueReqIndex + 1) & 3;

	LOAD_ProcessReadQueue();

	return oldQueueReqIndex;
}

/*
 * Offset 0x80037EF4
 * C:\kain2\game\LOAD3D.C (line 874)
 * Stack frame base $sp, size 80
 * Saved registers at offset -8: s0 s1 ra
 */
void /*$ra*/ LOAD_InitCdLoader(char* bigFileName /*$s0*/, char* voiceFileName /*$a1*/)
{ // line 1, offset 0x80037ef4
	CdlFILE fp; // stack offset -48
	long i; // $a0
	long fileId; // $a0
	long sizeOfContents; // $s0
	char *bigFileContents; // $s1
	unsigned char cdMode; // stack offset -24

	//s0 = bigFileName
	//a0 = 0x80030000
	//v0 = 0x80
	cdMode = 0x80;
	loadStatus.waitingForData = 0;

	CdReadyCallback(LOAD_CdReadReady);
	//a0 = LOAD_CdSeekCallback;
	loadStatus.waitingForSeek = 0;
	CdSyncCallback(LOAD_CdSeekCallback);
	CdDataCallback(LOAD_CdDataReady);

	if (CdSearchFile(&fp, bigFileName) == NULL)
	{
		return;
	}

	CdControl(0xE, &cdMode, NULL);
	loadStatus.bigFile.bigfileBaseOffset = CdPosToInt(&fp.pos);

	//loc_80037F78
	for (i = 0; i < 4; i++)
	{
		loadStatus.loadQueue[i].status = 0;
	}

	loadStatus.currentQueueFile = &loadStatus.loadQueue[0];
	loadStatus.currentQueueFileIndex = 0;
	loadStatus.currentQueueReqIndex = 0;

	bigFileContents = MEMPACK_Malloc(4096, 8);

	fileId = LOAD_CdReadFromBigFile(0, (unsigned long*)bigFileContents, (unsigned long*)bigFileContents, 1, 0, 0, 0);

	if (loadStatus.loadQueue[fileId].status != 0)
	{
		while (loadStatus.loadQueue[fileId].status != 0)
		{
			LOAD_ProcessReadQueue();
		}
	}//loc_80038014
	loadStatus.bigFile.numFiles = ((long*)bigFileContents)[0];
	MEMPACK_Free(bigFileContents);
	//v1 = loadStatus.bigFile.numFiles;
	//a1 = 8;
	//v0 = ((loadStatus.bigFile.numFiles << 1) + loadStatus.bigFile.numFiles) << 3;
	sizeOfContents = ((loadStatus.bigFile.numFiles << 1) + loadStatus.bigFile.numFiles) << 3;
	sizeOfContents = ((sizeOfContents & 0x7FF) >> 11) << 11;

	loadStatus.bigFile.contents = (struct BigFileFileInfo*)MEMPACK_Malloc(sizeOfContents);
	if (loadStatus.bigFile.contents == NULL)
	{
		PSYQpause();
	}
	//loc_80038064
	fileId = LOAD_CdReadFromBigFile(0, (unsigned long*)loadStatus.bigFile.contents, (unsigned long*)loadStatus.bigFile.contents, sizeOfContents >> 11, 0, 0, 0);
	loadStatus.bigFile.contents = &((long*)loadStatus.bigFile.contents)[1];

	if (loadStatus.loadQueue[fileId].status != 0)
	{
		while (loadStatus.loadQueue[fileId].status != 0);
		{
			LOAD_ProcessReadQueue();
		}
	}
	//loc_800380DC
}

long* LOAD_ReadFile(char* fileName, unsigned char memType)
{
	long bigFileIndex;
	long length;
	long checksum;
	long compressed;
	long compressedLength;
	char* finalDest;
	struct BigFileFileInfo* fileInfo;

	bigFileIndex = LOAD_GetBigFileFileIndex(fileName);
	fileInfo = &loadStatus.bigFile.contents[bigFileIndex];
	length = (fileInfo->fileLen + 0x7FF) >> 11;

	if (fileInfo->compressedLen != 0)
	{
		length++;
		compressedLength = (fileInfo->compressedLen + 0x7FF) >> 11;
	}
	else
	{
		//loc_80038154
		compressedLength = length;
	}

	//loc_80038158
	finalDest = MEMPACK_Malloc(length << 11, memType);
	LOAD_LoadingScreen(LOAD_CdReadFromBigFile(fileInfo->filePos >> 11, &finalDest[(length - compressedLength) << 11], finalDest, compressedLength, 1, loadStatus.bigFile.contents[bigFileIndex].checkSumFull, compressed));

	return (long*)finalDest;
}

/*
 * Offset 0x800381E0
 * C:\kain2\game\LOAD3D.C (line 992)
 * Stack frame base $sp, size 32
 * Saved registers at offset -4: s0 s1 s2 ra
 */
long * /*$ra*/ LOAD_SetupNonBlockingReadFile(char* fileName /*$a0*/, unsigned char memType /*$s2*/, struct NonBlockLoadEntry* loadEntry /*$s0*/, long forceSize /*$s1*/)
{ // line 1, offset 0x800381e0
    long bigFileIndex; // $a0
    long length; // $a1
    long checksum; // $a0
    long compressed; // $a2
    long compressedLength; // $t0
    struct BigFileFileInfo *fileInfo; // $a3

	return NULL;
} // line 44, offset 0x800382bc

long LOAD_NonBlockingReadFile(struct NonBlockLoadEntry* loadEntry)
{
	///loadEntry->finalDest = MEMPACK_Malloc(loadEntry->mallocedSize, loadEntry->memType);
	loadEntry->loadAddr = &loadEntry->finalDest[((loadEntry->mallocedSize >> 11) - loadEntry->compressedLength) << 11];
	return LOAD_CdReadFromBigFile(loadEntry->filePos, loadEntry->loadAddr, loadEntry->finalDest, loadEntry->compressedLength, loadEntry->checksumType, loadEntry->checksum, loadEntry->compressed);
}

long LOAD_CD_ReadPartOfFile(struct NonBlockLoadEntry* loadEntry)
{
    struct FileAccessInfo* currentQueueReq;
    long oldQueueReqIndex;

	currentQueueReq = &loadStatus.loadQueue[loadStatus.currentQueueReqIndex];
	if (currentQueueReq->status != 0)
	{
		//loc_800383A4
		while (currentQueueReq->status != 0)
		{
			LOAD_ProcessReadQueue();
		}
	}

	//loc_800383BC
	CdIntToPos(loadStatus.bigFile.bigfileBaseOffset + loadEntry->filePos, &currentQueueReq->loc);
	currentQueueReq->blocks = loadEntry->compressedLength;
	currentQueueReq->dest = loadEntry->finalDest;
	currentQueueReq->finalDest = loadEntry->finalDest;
	currentQueueReq->status = 5;
	currentQueueReq->checksumType = 0;
	currentQueueReq->fileOffset = loadEntry->filePos;
	currentQueueReq->checksum = loadEntry->checksum;
	currentQueueReq->compressedLen = loadEntry->compressed;
	currentQueueReq->curBufferOffset = 0;
	currentQueueReq->bufferBlocks = loadEntry->bufferSize >> 11;
	currentQueueReq->buffer1 = loadEntry->finalDest;
	currentQueueReq->bufferFlags = 0;
	currentQueueReq->buffer2 = loadEntry->finalDest2;
	currentQueueReq->retFunc = loadEntry->retFunc;
	currentQueueReq->retData = loadEntry->retData;
	currentQueueReq->loadEntry = loadEntry;
	currentQueueReq->retData2 = loadEntry->retData2;

	oldQueueReqIndex = loadStatus.currentQueueReqIndex;
	loadStatus.currentQueueReqIndex = (oldQueueReqIndex + 1) & 3;

	LOAD_ProcessReadQueue();

	return oldQueueReqIndex;
}

int LOAD_IsFileLoading(long fileId /*$s0*/)
{
	long loopFlag;

	loopFlag = -1;

	if (fileId == loopFlag)
	{
		loopFlag = 1;
	}
	else
	{
		//loc_800384C0
		loopFlag = 0 < loadStatus.loadQueue[fileId].status ? 1 : 0;
	}

	if (loopFlag != 0)
	{
		loopFlag = 0;
		if (fileId != -1)
		{
			LOAD_ProcessReadQueue();
			loopFlag = 0 < loadStatus.loadQueue[fileId].status ? 1 : 0;
		}//loc_80038534
	}//loc_80038534

	return loopFlag;
}

long LOAD_HashName(char* string)
{
	long sum;
	long xor;
	long length;
	long ext;
	char c;
	long strl;
	long endPos;
	long i;
	char* pos;

	sum = 0;
	xor = 0;
	length = 0;
	ext = 0;

	strl = strlen(string) - 1;
	pos = strchr(string, '.');

	endPos = 0;
	if (pos++ != NULL)
	{
		//loc_800385B4
		for (i = 0; i < 7; i++)
		{
			if (strcmpi(pos, &HashExtensions[i][0]) == 0)
			{
				ext = i;
				break;
			}
		}

		//loc_800385E0
		if (i < 7)
		{
			strl -= 4;
		}
	}
	//loc_800385F0
	if (strl > endPos)
	{
		while (strl > endPos)
		{
			//loc_800385FC
			c = string[strl];

			if ((c - 0x61) < 0x1A)
			{
				c &= 0xDF;
			}
			//loc_80038624
			c -= 0x1A;
			sum += (c & 0xFF);
			strl--;
			length++;
			xor ^= (c & 0xFF) * length;
		}
	}
	//loc_80038650
	return (length << 27) | (sum << 15) | (xor << 3) | ext;
}

long LOAD_GetBigFileFileIndex(char* fileName)
{
	long hash;
	long bigFileIndex;

	hash = LOAD_HashName(fileName);
	bigFileIndex = 0;

	if (loadStatus.bigFile.numFiles > 0)
	{
		for (; bigFileIndex < loadStatus.bigFile.numFiles; bigFileIndex++)
		{
			if (loadStatus.bigFile.contents[bigFileIndex].fileHash == hash)
				break;
		}
	}

	//loc_800386E0
	return bigFileIndex;
}

long LOAD_DoesFileExist(char* fileName)
{
	long hash;
	long bigFileIndex;
	long result;

	result = 1;
	hash = LOAD_HashName(fileName);
	bigFileIndex = 0;

	if (loadStatus.bigFile.numFiles > 0)
	{
		//loc_8003871C
		for (; bigFileIndex < loadStatus.bigFile.numFiles; bigFileIndex++)
		{
			if (loadStatus.bigFile.contents[0].fileHash == hash)
				break;
		}
	}

	//loc_80038740
	if (bigFileIndex == loadStatus.bigFile.numFiles)
	{
		result = 0;
	}

	return result;
}

void LOAD_LoadTIM(long* addr, long x_pos, long y_pos, long clut_x, long clut_y)
{
    RECT rect;

	addr = &addr[2];

	if (addr[-1] == 8)
	{
		rect.x = clut_x;
		rect.y = clut_y;
		rect.w = 16;
		rect.h = 1;
		DrawSync(0);

		LoadImage(&rect, (u_long*)&addr[3]);
		addr = &addr[11];
	}

	//loc_800387C8:
	rect.x = x_pos;
	rect.y = y_pos;
	rect.w = ((short*)addr)[4];
	rect.h = ((short*)addr)[5];
	
	DrawSync(0);
	LoadImage(&rect, &addr[3]);
	DrawSync(0);
}

void LOAD_LoadTIM2(long* addr, long x_pos, long y_pos, long width, long height)
{
	RECT rect;

	rect.x = x_pos;
	addr = &addr[2];
	rect.y = y_pos;
	rect.w = ((short*)addr)[4];
	addr = &addr[3];
	rect.h = ((short*)addr)[5];
	LoadImage(&rect, (u_long*)addr);
	DrawSync(0);
}

/*
 * Offset 0x80038860
 * C:\kain2\game\LOAD3D.C (line 1314)
 * Stack frame base $sp, size 48
 * Saved registers at offset -8: s0 s1 s2 s3 ra
 */
void* LOAD_RelocBinaryData(long* data /*$s0*/)
{
    long* dataAddr; // $s1
    long* lastMoveDest; // $v1
    long tableSize; // $s3
    long fileSize; // $v0
    struct RedirectList redirectListX; // stack offset -32
    struct RedirectList* redirectList; // $a0

	//a0 = &redirectListX;
	dataAddr = data;
	//v0 = &dataAddr[1];
	redirectListX.data = &dataAddr[1];
	//v1 = *dataAddr
	fileSize = *dataAddr + 512;
	redirectListX.numPointers = *dataAddr;

	if (fileSize < 0)
	{
		fileSize = *dataAddr + 1023;
	}

	//loc_800388A4
	fileSize >>= 9;
	tableSize = fileSize << 9;
	RESOLVE_Pointers(&redirectListX, &dataAddr[filesize << 9], dataAddr);
	fileSize = MEMPACK_Size(dataAddr);

	fileSize >>= 2;
	fileSize -= tableSize;
	lastMoveDest = dataAddr - fileSize;

	//v0 = dataAddr < lastMoveDest ? 1 : 0;
		sltu    $v0, $s1, $v1
		beqz    $v0, loc_80038900
		move    $a0, $s2
		addu    $v0, $a0, $s0

		loc_800388E4 :
		lw      $v0, 0($v0)
		nop
		sw      $v0, 0($s0)
		addiu   $s0, 4
		sltu    $v0, $s0, $v1
		bnez    $v0, loc_800388E4
		addu    $v0, $a0, $s0

		loc_80038900 :
		move    $a0, $s1
		jal     sub_8005047C
		sll     $a1, $s3, 2
		move    $v0, $s1
		lw      $ra, 0x30 + var_8($sp)
		lw      $s3, 0x30 + var_C($sp)
		lw      $s2, 0x30 + var_10($sp)
		lw      $s1, 0x30 + var_14($sp)
		lw      $s0, 0x30 + var_18($sp)
		jr      $ra
		addiu   $sp, 0x30
		# End of function sub_80038860

	return NULL;
} // line 21, offset 0x80038900

long LOAD_lzrw1_decompress(unsigned long src_len, long overBufferTolerance)
{
    unsigned char* p_src; // $a2
    unsigned char* p_dst; // $a3
    unsigned long controlbits; // $t1
    unsigned long control; // $t0
    unsigned char* p_src_post; // $t3
    unsigned long offset; // $v0
    unsigned long len; // $a0
    long remainingBytes; // $v0
    unsigned char* p; // $v1

	p_src = loadStatus.compSrcAddr;
	p_dst = loadStatus.compDstAddr;
	controlbits = loadStatus.compControlbits;
	control = loadStatus.compControl;
	p_src_post = &p_src[len];
	p = p_src_post - overBufferTolerance;
	offset = p_src_post - p_src;

	if (p_src > p)
	{
		//loc_80038958
		while (p > p_src)
		{
			if (controlbits == 0)
			{
				control = *p_src++ | (*p_src++ << 8);
				controlbits = 16;
			}

			if ((control & 1))
			{
				//loc_80038980
				len = *p_src++;
				p = p_dst - ((len & 0xF0) << 4) + *p_src++;
				if ((len & 0xF) != -1)
				{
					//loc_800389B4
					while (len-- != -1)
					{
						*p_dst++ = *p++;
					}
				}//loc_800389E4
			}
			else
			{
				//loc_800389D4
				*p_dst++ = *p_src++;
			}

			//loc_800389E4
			control >>= 1;

			//loc_800389E8
			controlbits--;
		}

		remainingBytes = p_src_post - p_src;
	}//loc_800389F8

	loadStatus.compSrcAddr = p_src;
	loadStatus.compDstAddr = p_dst;
	loadStatus.compControlbits = controlbits;
	loadStatus.compControl = control;

	return remainingBytes > 0 ? remainingBytes : 0;
}

void LOAD_LoadingScreen(long fileId)
{
    long loopFlag;

	if (fileId == -1)
	{
		loopFlag = 1;
	}
	else
	{
		loopFlag = 0 < loadStatus.loadQueue[fileId].status ? 1 : 0;
	}

	//loc_80038A70
	if (loopFlag != 0)
	{
		while (loopFlag != 0)
		{
			loopFlag = 0;
			if (fileId != -1)
			{
				LOAD_ProcessReadQueue();
				loopFlag = 0 < loadStatus.loadQueue[fileId].status ? 1 : 0;
			}
		}
	}

	DrawSync(0);
}