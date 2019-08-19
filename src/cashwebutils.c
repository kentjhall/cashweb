#include "cashwebutils.h"

void initDynamicMemory(struct DynamicMemory *dm) {
	dm->data = NULL;
	dm->size = 0;
}

void freeDynamicMemory(struct DynamicMemory *dm) {
	if (dm->data) { free(dm->data); }
	initDynamicMemory(dm);
}

void resizeDynamicMemory(struct DynamicMemory *dm, size_t newSize) {
	char *newDataAlloc;
	// sets data pointer to NULL on failure; this should be error-checked on use
	if ((newDataAlloc = realloc(dm->data, newSize)) == NULL) { perror("realloc failed"); free(dm->data); dm->data = NULL; return; }
	dm->data = newDataAlloc;
	dm->size = newSize;
}

long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { perror("fstat() failed"); return -1; }
	return st.st_size;
}

int safeReadLine(struct DynamicMemory *line, size_t lineBufStart, FILE *fp) {
	resizeDynamicMemory(line, lineBufStart);
	if (line->data == NULL) { return READLINE_ERR; }
	bzero(line->data, line->size);

	int lineLen;
	char lastChar;
	int offset = 0;
	while (fgets(line->data+offset, line->size-1-offset, fp) != NULL) {
		lineLen = strlen(line->data);
		lastChar = line->data[lineLen-1];
		if (lastChar != '\n' && !feof(fp)) {
			offset = lineLen;
			resizeDynamicMemory(line, line->size*2);
			if (line->data == NULL) { return READLINE_ERR; }
			bzero(line->data+offset, line->size-offset);
			continue;
		}
		if (lastChar == '\n') { line->data[lineLen-1] = 0; }
		return READLINE_OK;
	}

	return READLINE_NO;
}

bool copyStreamData(FILE *dest, FILE *source) {
	char buf[FILE_DATA_BUF];
	int n;
	while ((n = fread(buf, 1, sizeof(buf), source)) > 0) {
		if (fwrite(buf, 1, n, dest) < n) { perror("fwrite() failed"); return false; }
	}
	if (ferror(source)) { perror("fread() failed"); return false; }
	return true;
}

void byteArrToHexStr(const char *byteArr, int n, char *hexStr) {
	for (int i=0; i<n; i++) {
		hexStr[i*2] = "0123456789abcdef"[((uint8_t)byteArr[i]) >> 4];
		hexStr[i*2+1] = "0123456789abcdef"[((uint8_t)byteArr[i]) & 0x0F];
	}
	hexStr[n*2] = 0;
}

int hexStrToByteArr(const char *hexStr, int suffixLen, char *byteArr) {
	int hexDataLen = strlen(hexStr);
	if (hexDataLen % 2 != 0) { return -1; }

	char hexByte[2+1];
	hexByte[2] = 0;
	int bytesRead = 0;
	for (int i=0; i<hexDataLen-suffixLen; i+=2) { 
		strncpy(hexByte, hexStr+i, 2);
		byteArr[i/2] = (char)strtoul(hexByte, NULL, 16);
		++bytesRead;
	}

	return bytesRead;
}

void int32ToNetByteArr(uint32_t uint, unsigned char *byteArr) {
	uint32_t netUInt = htonl(uint);
	for (int i=0; i<sizeof(netUInt); i++) {
		byteArr[i] = (netUInt >> i*8) & 0xFF;
	}
}

void int16ToNetByteArr(uint16_t uint, unsigned char *byteArr) {
	uint16_t netUInt = htons(uint);
	for (int i=0; i<sizeof(netUInt); i++) {
		byteArr[i] = (netUInt >> i*8) & 0xFF;
	}
}

bool intToNetByteArr(void *uintPtr, int numBytes, unsigned char *byteArr) {
	switch (numBytes) {
		case sizeof(uint16_t):
		{
			uint16_t uint16 = *(uint16_t *)uintPtr;
			int16ToNetByteArr(uint16, byteArr);
			break;
		}
		case sizeof(uint32_t):
		{
			uint32_t uint32 = *(uint32_t *)uintPtr;
			int32ToNetByteArr(uint32, byteArr);
			break;
		}
		default:
			fprintf(stderr, "invalid byte count specified for intToNetByteArr, int must be 2 or 4 bytes; problem with cashwebtools\n");
			return false;
	}
	return true;
}

bool intToNetHexStr(void *uintPtr, int numBytes, char *hexStr) {
	unsigned char bytes[numBytes];
		
	if (!intToNetByteArr(uintPtr, numBytes, bytes)) { return false; }
	byteArrToHexStr((const char *)bytes, numBytes, hexStr);

	return true;
}

bool netByteArrToInt(const char *byteData, int numBytes, void *uintPtr) {
	uint16_t uint16 = 0; uint32_t uint32 = 0;
	bool isShort = false;
	switch (numBytes) {
		case sizeof(uint16_t):
			isShort = true;
			break;
		case sizeof(uint32_t):
			break;
		default:
			fprintf(stderr, "unsupported number of bytes read for network integer value; probably problem with cashwebtools\n");
			return false;
	}

	for (int i=0; i<numBytes; i++) {
		if (isShort) { uint16 |= (uint16_t)byteData[i] << i*8; } else { uint32 |= (uint32_t)byteData[i] << i*8; }
	}
	if (isShort) { *(uint16_t *)uintPtr = ntohs(uint16); }
	else { *(uint32_t *)uintPtr = ntohl(uint32); }

	return true;
}

bool netHexStrToInt(const char *hex, int numBytes, void *uintPtr) {
	char byteData[numBytes];
	if (numBytes != strlen(hex)/2 || hexStrToByteArr(hex, 0, byteData) < 0) { fprintf(stderr, "invalid hex data passed for network integer value; probably problem with cashwebtools\n"); return false; }	

	return netByteArrToInt(byteData, numBytes, uintPtr);
}
