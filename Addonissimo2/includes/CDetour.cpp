// Taken from: http://code.google.com/p/m0d-s0beit-sa/
#include "CDetourReq.h"

void* CDetour::memcpy_s( void* pvAddress, const void* pvBuffer, size_t stLen )
{
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery( pvAddress, &mbi, sizeof( mbi ) );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect );
	void* pvRetn = memcpy( pvAddress, pvBuffer, stLen );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect );
	FlushInstructionCache( GetCurrentProcess( ), pvAddress, stLen );
	return pvRetn;
}

void *CDetour::Create(BYTE *orig, const BYTE *det, int iPatchType, int len)
{
	BYTE *jmp = NULL;
	int iMinLen = 0;

	if(!(iMinLen = GetDetourLen(iPatchType)))
		return 0;

	if(len != 0 && len < iMinLen)
		return 0;

	// Try and find the end of the instruction automatically
	if(len == 0)
	{
		len = GetDetourLenAuto(orig, iMinLen);

		if(len < iMinLen)
			return 0;
	}

	if(!Detour(jmp, orig, det, iPatchType, len))
		return 0;

	return (jmp-len);
}

void *CDetour::Create(char *dllName, char *apiName, const BYTE *det, int iPatchType, int len)
{
	BYTE *jmp = NULL;
	BYTE *orig = NULL;
	int iMinLen = 0;

	if(!(iMinLen = GetDetourLen(iPatchType)))
		return 0;

	if(len != 0 && len < iMinLen)
		return 0;

	// Get the API address
	m_hModule = GetModuleHandle(dllName);
	m_dwAddress = (DWORD)GetProcAddress(m_hModule, apiName);

	if(!m_dwAddress || !det)
		return 0;

	orig = (BYTE*)m_dwAddress;

	// Try and find the end of the instruction automatically
	if(len == 0)
	{
		len = GetDetourLenAuto(orig, iMinLen);

		if(len < iMinLen)
			return 0;
	}

	if(!Detour(jmp, orig, det, iPatchType, len))
		return 0;

	return (jmp-len);
}

bool CDetour::Detour(BYTE *&jmp, BYTE *&orig, const BYTE *&det, int iPatchType, int len)
{
	DWORD dwBack = 0;
	int i = 0;
	BYTE *pPatchBuf = NULL;

	// Allocate space for the jump
	jmp = (BYTE*)malloc(len+5);

	// Force page protection flags to read|write
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery( ( void* )orig, &mbi, sizeof( mbi ) );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &mbi.Protect );

	// Copy the overwritten opcodes at the original to the malloced space
	memcpy(jmp, orig, len);

	// Increment to the end of the opcodes at the malloced space
	jmp += len;

	// Place a jump back to the original at this point
	jmp[0] = 0xE9;												// jmp instruction	(1 byte)
	*(DWORD*)(jmp + 1) = (DWORD)(orig + len - jmp) - 5;

	// Generate a random opcode
	int iTmpRnd = (rand() * 0xFF) + rand();
	BYTE bTmpRnd = (BYTE)iTmpRnd;

	// Place a jump at the original to the detour function
	pPatchBuf = new BYTE[len];

	// Pad out the bytes with NOPs so we don't have ends of intructions
	memset(pPatchBuf, 0x90, len);

	// Write the opcodes to the buffer according to patch type
	switch(iPatchType)
	{
	case DETOUR_TYPE_JMP:
		pPatchBuf[0] = '\xE9';									// jmp instruction	(1 byte)
		*(DWORD*)&pPatchBuf[1] = (DWORD)(det - orig) - 5;		// jmp offset		(4 bytes)
		break;

	case DETOUR_TYPE_PUSH_RET:
		pPatchBuf[0] = '\x68';									// push instruction (1 byte)
		*(DWORD*)&pPatchBuf[1] = (DWORD)det;					// detour location	(4 bytes)
		pPatchBuf[5] = '\xC3';									// retn instruction	(1 byte)
		break;

	case DETOUR_TYPE_PUSH_FUNC:
		pPatchBuf[0] = '\x68';									// push instruction (1 byte)
		*(DWORD*)&pPatchBuf[1] = (DWORD)det;					// detour location	(4 bytes)
		break;

	case DETOUR_TYPE_CALL_FUNC:
		pPatchBuf[0] = '\xE8';									// call instruction	(1 byte)
		*(DWORD*)&pPatchBuf[1] = (DWORD)(det - orig) - 5;		// call location	(4 bytes)
		break;

	default:
		return false;
	}

	// Write the detour
	for(i = 0; i < len; i++)
		orig[i] = pPatchBuf[i];

	// Put the old page protection flags back
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect );

	FlushInstructionCache( GetCurrentProcess( ), orig, len );

	return true;
}

bool CDetour::Remove(BYTE *orig, BYTE *jmp, int iPatchType, int len )
{
	int iMinLen = 0;
	DWORD dwBack = 0;

	if(!(iMinLen = GetDetourLen(iPatchType)))
		return false;

	if(len != 0 && len < iMinLen)
		return false;

	// Try and find the end of the instruction automatically
	if(len == 0)
	{
		len = GetDetourLenAuto(jmp, iMinLen);
		if( len == 0 )len = GetDetourLen(iPatchType);
		if( len == 0 || iMinLen == 0 )return false;
		if(len < iMinLen)return false;
	}

	// Write the bytes @ the jmp back to the orig
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery( ( void* )orig, &mbi, sizeof( mbi ) );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect );
	memcpy( orig, jmp, len );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect );
	FlushInstructionCache( GetCurrentProcess( ), ( void* )orig, len );

	return true;
}


bool CDetour::RestoreFunction( BYTE* func, int len )
{
	MEMORY_BASIC_INFORMATION mbi;
	bool bRet = false;
	VirtualQuery( ( void* )func, &mbi, sizeof( mbi ) );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect );
	memcpy( ( void* )func, ( void* )bBackup, len );
	if( *( BYTE* )func == ( BYTE )bBackup[0] )
	{
		bRet = true;
	}else{
		bRet = false;
	}
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect );
	FlushInstructionCache( GetCurrentProcess( ), ( void* )func, len );
	return bRet;
}

bool CDetour::BackupFunction( BYTE* func, int len )
{
	MEMORY_BASIC_INFORMATION mbi;
	bool bRet = false;
	VirtualQuery( ( void* )func, &mbi, sizeof( mbi ) );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect );
	memcpy( ( void* )bBackup, ( void* )func, len );
	if( ( BYTE )bBackup[0] == *( BYTE* )func )
	{
		bRet = true;
	}else{
		bRet = false;
	}
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect );
	FlushInstructionCache( GetCurrentProcess( ), ( void* )func, len );
	return bRet;
}

bool CDetour::Remove(char *dllName, char *apiName, BYTE *jmp, int iPatchType, int len)
{
	DWORD dwBack = 0;
	BYTE *orig = NULL;
	int iMinLen = 0;

	// Get the API address
	m_hModule = GetModuleHandle(dllName);
	m_dwAddress = (DWORD)GetProcAddress(m_hModule, apiName);

	if(!m_dwAddress || !jmp)
		return false;

	orig = (BYTE*)m_dwAddress;

	if(!(iMinLen = GetDetourLen(iPatchType)))
		return false;

	if(len != 0 && len < iMinLen)
		return false;

	// Try and find the end of the instruction automatically
	if(len == 0)
	{
		len = GetDetourLenAuto(jmp, iMinLen);

		if(len < iMinLen)
			return 0;
	}

	// Write the bytes @ the jmp back to the orig
	VirtualProtect(orig, len, PAGE_READWRITE, &dwBack);
	memcpy(orig, jmp, len);
	VirtualProtect(orig, len, dwBack, &dwBack);

	return true;
}

void * CDetour::DetourClassFunc(BYTE *src, const BYTE *dst, const int len)
{
	BYTE *jmp = (BYTE*)malloc(len+8);

	DWORD dwBack;
	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(jmp + 3, src, len);

	// calculate callback function call
	jmp[0] = 0x58;													// pop eax instruction	(1 byte)
	jmp[1] = 0x59;													// pop ecx instruction	(1 byte)
	jmp[2] = 0x50;													// push eax instruction	(1 byte)
	jmp[len+3] = 0xE9;												// jmp instruction		(1 byte)
	*(DWORD*)(jmp+len+4) = (DWORD)((src+len) - (jmp+len+3)) - 5;	// jmp offset			(4 bytes)

	// detour source function call
	src[0] = 0x58;													// pop eax instruction	(1 byte)
	src[1] = 0x51;													// pop ecx instruction	(1 byte)
	src[2] = 0x50;													// push eax instruction	(1 byte)
	src[3] = 0xE9;													// jmp instruction		(1 byte)
	*(DWORD*)(src+4) = (DWORD)(dst - (src+3)) - 5;					// jmp offset			(4 bytes)					

	for( int i = 8; i < len; i++ )
		src[i] = 0x90;												// NOP instruction		(1 byte)

	VirtualProtect(src, len, dwBack, &dwBack);
	return jmp;
}

void CDetour::RetourClassFunc(BYTE *src, BYTE *restore, const int len)
{
	DWORD dwBack;

	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(src, restore+3, len);

	restore[3] = 0xE9;
	*(DWORD*)(restore+4) = (DWORD)(src - (restore+3)) - 5;

	VirtualProtect(src, len, dwBack, &dwBack);
}

void * CDetour::DetourClassFunc5(BYTE *src, const BYTE *dst, const int len)
{
	BYTE *jmp = (BYTE*)malloc(len+5);
	BYTE *pre = (BYTE*)malloc(len+8);
	DWORD dwBack;
	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(jmp+3, src, len);

	// calculate callback function call
	jmp[0] = 0x58;                            // pop eax
	jmp[1] = 0x59;                            // pop ecx
	jmp[2] = 0x50;                            // push eax
	jmp[len+3] = 0xE9;                        // jmp
	*(DWORD*)(jmp+len+4) = (DWORD)((src+len) - (jmp+len+3)) - 5;

	// detour source function call
	src[0] = 0xE9;
	*(DWORD*)(src+1) = (DWORD)(pre - src) - 5;

	for( int i=5; i < len; i++ )
		src[i] = 0x90;

	// detour source function call
	pre[0] = 0x58;                            // pop eax;
	pre[1] = 0x51;                            // push ecx
	pre[2] = 0x50;                            // push eax
	pre[3] = 0xE9;                            // jmp
	*(DWORD*)(pre+4) = (DWORD)(dst - (pre+3)) - 5;

	VirtualProtect(src, len, dwBack, &dwBack);

	return jmp;
}  

void * CDetour::VTableFunction(void *ClassPtr, DWORD index)
{
	void **pVtable = *(void***)ClassPtr;
	return pVtable[index];
}

int CDetour::GetDetourLen(int iPatchType)
{
	switch(iPatchType)
	{
	case DETOUR_TYPE_JMP:
	case DETOUR_TYPE_PUSH_FUNC:
	case DETOUR_TYPE_CALL_FUNC:
		return 5;

	case DETOUR_TYPE_PUSH_RET:
		return 6;

	default:
		return 0;
	}
}

int CDetour::GetDetourLenAuto(BYTE *&orig, int iMinLen)
{
	int tmpLen = 0;
	BYTE *pCurOp = orig;

	while(tmpLen < iMinLen)
	{
		int i = oplen(pCurOp);

		if(i == 0 || i == -1)
			return false;

		tmpLen += i;
		pCurOp += i;
	}

	return tmpLen;
}