/*
 *	scrlog_VC.cpp:
 *	     This file contains the hookers for GTA Vice City (all versions);
 *
 *  Issues:
 *		*No CollectGlobalVariableIndex or CollectVariablePointer (game fault)
 *      *If using CLEO, this must be loaded after CLEO.asi (not really a issue, just a advice),
 *        hopefully, this may happen always since scrlog.asi is after CLEO.asi in alphabetic order
 *      *CLEO hooks all commands with label as parameter, so... yeah, we cant show label params if CLEO is installed
 *      *Many of the Vice's script engine stuff is inlined, and when inlined we wont have access to the values since
 *        no func is called.
 *
 *  LICENSE:
 *		 (c) 2013 - LINK/2012 - <dma_2012@hotmail.com>
 *
 *		 Permission is hereby granted, free of charge, to any person obtaining a copy
 *		 of this plugin and source code, to copy, modify, merge, publish and/or distribute
 *		 as long as you leave the original credits (above copyright notice)
 *		 together with your derived work. Please not you are NOT permited to sell or get money from it
 *
 *		 THE SOFTWARE AND SOURCE CODE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *		 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *		 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *		 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *		 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *		 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *		 THE SOFTWARE.
 *
 */
#include "GameInfo.h"
#include "Injector.h"
#include "scrlog.h"

// Some macros to help in saving important registers being used
#define push_regs(a, b)	_asm push a _asm push b
#define pop_regs(a, b)  _asm pop  b _asm pop  a


static void PatchALL(bool bSteam);
void VC_Patch(GameInfo& info)
{
	PatchALL(info.IsSteam());
}

static void* CRunningScript__ProcessOneCommand;
static void HOOK_RegisterCommand();
static void HOOK_RegisterScript();
static void HOOK_CalledCollectParameters();
static void HOOK_RegisterCollectedDatatype();
static void HOOK_RegisterCollectedParam();
static void HOOK_CalledStoreParameters();
static void HOOK_CalledCollectVariablePointer();
static void HOOK_RegisterCollectVariablePointer();

// Structure to store addresses for each version, used in CommonPatch() function
struct CommonPatchInfo
{
	uint32_t phRegisterScript;
	uint32_t phRegisterCommand;
	uint32_t phCollectParameters;
	uint32_t phCollectedDatatype;
	uint32_t phCollectedValue;
	uint32_t phStoreParameters;
	uint32_t phUpdateCompareFlag;
	uint32_t CollectiveArray;
	uint32_t ScriptsUpdated;
	uint32_t ScriptSpace;
	uint32_t MissionSpace;
	size_t   ScriptSpaceSize;
	size_t   MissionSpaceSize;
};

// Patches based on the sent CommonPatchInfo structure
static void CommonPatch(CommonPatchInfo& c)
{
	uint32_t ptr;
	
	c.ScriptSpaceSize = 225512;
	c.MissionSpaceSize = 35000;

	// Get pointers for scrlog
	{
		SCRLog::ScriptSpace      = ReadMemory<char*>(c.ScriptSpace, true);
		SCRLog::MissionSpace     = SCRLog::ScriptSpace + c.ScriptSpaceSize;
		SCRLog::CollectiveArray  = ReadMemory<ScriptVar*>(c.CollectiveArray, true);
		SCRLog::ScriptsUpdated   = ReadMemory<short*>(c.ScriptsUpdated, true);

		SCRLog::ScriptSpaceEnd =  SCRLog::ScriptSpace + c.ScriptSpaceSize;
		SCRLog::MissionSpaceEnd = SCRLog::MissionSpace+ c.MissionSpaceSize;
	}

	// RegisterScript
	if(SCRLog::bHookRegisterScript)
	{
		ptr = c.phRegisterScript;
		MakeCALL(ptr, &HOOK_RegisterScript);
		MakeNOP(ptr+5, 1);
	}

	// RegisterCommand
	if(SCRLog::bHookRegisterCommand)
	{
		ptr = c.phRegisterCommand;
		CRunningScript__ProcessOneCommand = (void*) GetAbsoluteOffset(ReadMemory<int>(ptr+1, true), ptr+5);
		MakeCALL(ptr, &HOOK_RegisterCommand);
	}

	// Called CollectParameters
	if(SCRLog::bHookCollectParam)
	{
		ptr = c.phCollectParameters;
		MakeCALL(ptr, &HOOK_CalledCollectParameters);
	}

	// Register collected datatype
	if(SCRLog::bHookFindDatatype)
	{
		ptr = c.phCollectedDatatype;
		MakeCALL(ptr, &HOOK_RegisterCollectedDatatype);
	}

	// Register collected value
	if(SCRLog::bHookCollectParam)
	{
		ptr = c.phCollectedValue;
		MakeCALL(ptr, &HOOK_RegisterCollectedParam);
		MakeNOP(ptr+5, 2);
	}

	// Register call to store parameters
	if(SCRLog::bHookStoreParam)
	{
		ptr = c.phStoreParameters;
		MakeCALL(ptr, &HOOK_CalledStoreParameters);
	}

	// Replace UpdateCompareFlag
	if(SCRLog::bHookUpdateCompareFlag)
	{
		ptr = c.phUpdateCompareFlag;
		MakeJMP(ptr, SCRLog::New_CRunningScript__UpdateCompareFlag);
		MakeNOP(ptr+5, 2);
	}
}


// Patch all Vice City's versions
// 1.0 and 1.1 have similar addresses (at least in .text segment)
// Steam has a difference of -240 in the addresses compared to retail version
static void PatchALL(bool bSteam)
{
	CommonPatchInfo c;
	unsigned int off = bSteam? -240 : 0;

	c.phRegisterScript = 0x44FD71 + off;
	c.phRegisterCommand = 0x44FDB5 + off;
	c.phCollectParameters = 0x451025 + off;
	c.phCollectedDatatype = 0x451030 + off;
	c.phCollectedValue = 0x4510F0 + off;
	c.phStoreParameters = 0x450E57 + off;
	c.phUpdateCompareFlag = !bSteam? 0x463F00 : 0x463DE0;		// this one differ
	
	c.ScriptsUpdated = 0x450239 + off;
	c.CollectiveArray = c.phCollectParameters + 1;
	c.ScriptSpace = c.phCollectedDatatype + 1;
	c.MissionSpace = 0;

	CommonPatch(c);
}


void __declspec(naked) HOOK_RegisterScript()
{
	_asm
	{
		mov ebx, ecx			// Replaced code

		push ecx
		call SCRLog::RegisterScript

		// Run replaced code and go back to normal flow
		mov ecx, ebx
		cmp byte ptr [ecx+0x7A], 0
		retn
	}
}

void __declspec(naked) HOOK_RegisterCommand()
{
	_asm
	{
		call SCRLog::RegisterCommand

		// Back
		mov ecx, ebx
		call CRunningScript__ProcessOneCommand
		retn
	}
}

void __declspec(naked) HOOK_CalledCollectParameters()
{
	_asm
	{
		push edx

		push edi
		call SCRLog::RegisterCallToCollectParameters

		pop edx

		// Run replaced code and go back to normal flow
		mov ecx, SCRLog::CollectiveArray
		retn
	}
}


void __declspec(naked) HOOK_RegisterCollectedDatatype()
{
	_asm
	{
		xor eax, eax
		mov ebx, dword ptr [edx]		// CRunningScript.ip
		add ebx, dword ptr [SCRLog::ScriptSpace]
		mov al, byte ptr [ebx]
		mov SCRLog::Datatype, eax

		// Run replaced code and go back to normal flow
		mov eax, SCRLog::ScriptSpace
		retn
	}
}

void __declspec(naked) HOOK_RegisterCollectedParam()
{
	_asm
	{
		push_regs(ecx, edx)

		mov eax, ecx				// ecx is current value in iteration over CollectiveArray
		sub eax, SCRLog::CollectiveArray
		shr eax, 2					// (eax >>= 2) = (eax /= 4)
		inc eax						// the index starts from 1
		push eax
		call SCRLog::RegisterCollectionAtIndex

		pop_regs(ecx, edx)

		// Jump back
		add ecx, 4
		sub di, 1
		retn
	}
}

void __declspec(naked) HOOK_CalledStoreParameters()
{
	_asm
	{
		push ecx
		push edi
		mov  SCRLog::Datatype, 1
		call SCRLog::RegisterCallToStoreParameters
		pop ecx

		// Run replaced code and go back to normal flow
		xor esi, esi
		test di, di
		retn
	}
}
