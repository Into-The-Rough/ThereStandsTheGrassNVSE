#pragma once

#include "nvse/PluginAPI.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define PLUGIN_VERSION 1
#define PLUGIN_NAME "ThereStandsTheGrassNVSE"

//xNVSE message ids. the sdk header in use predates several of these
enum
{
	kMsg_PostLoad		= 0,
	kMsg_PreLoadGame	= 6,
	kMsg_NewGame		= 14,
	kMsg_DeferredInit	= 18,
	kMsg_MainGameLoop	= 20,
	kMsg_ReloadConfig	= 25,
};

void Log(const char *fmt, ...);
void OpenLog();

//netimmerse.h fakes the d3d types so the real d3d9.h cannot be included here
struct D3DXMACRO { const char *Name; const char *Definition; };
struct ID3DXBuffer : IUnknown
{
	virtual void* __stdcall GetBufferPointer() = 0;
	virtual DWORD __stdcall GetBufferSize() = 0;
};
typedef HRESULT(WINAPI* _D3DXAssembleShader)(const char*, UInt32, const D3DXMACRO*, void*, DWORD, ID3DXBuffer**, ID3DXBuffer**);
typedef HRESULT(WINAPI* _D3DXDisassembleShader)(const DWORD*, BOOL, const char*, ID3DXBuffer**);
typedef float* (WINAPI* _D3DXMatrixInverse)(float*, float*, const float*);
typedef float* (WINAPI* _D3DXVec3TransformCoord)(float*, const float*, const float*);
