#include "Common.h"
#include "EngineLayout.h"
#include "ShaderPatch.h"
#include "internal/Detours.h"

namespace ShaderPatch
{
	struct Pattern
	{
		const char	*prefix;
		size_t		prefixLen;
		PatchFn		patch;
	};

	constexpr UInt32 kMaxPatterns = 8;
	static Pattern s_patterns[kMaxPatterns];
	static UInt32 s_patternCount = 0;
	static int s_patched = 0;
	static int s_skipped = 0;
	static bool s_dump = false;

	static _D3DXAssembleShader D3DXAssembleShader = nullptr;
	static _D3DXDisassembleShader D3DXDisassembleShader = nullptr;

	static Detours::CallDetour s_grassLowDetailCall;
	static Detours::CallDetour s_grassHighDetailCall;
	static Detours::CallDetour s_leafCall;
	static Detours::CallDetour s_frondCall;
	typedef void* (__thiscall* _CreateVS)(void*, const char*, D3DXMACRO*, const char*, const char*);

	bool RegisterPattern(const char *namePrefix, PatchFn patch)
	{
		if (s_patternCount >= kMaxPatterns) return false;
		Pattern &p = s_patterns[s_patternCount++];
		p.prefix = namePrefix;
		p.prefixLen = strlen(namePrefix);
		p.patch = patch;
		return true;
	}

	void SetDumpDisassembly(bool dump) { s_dump = dump; }

	static PatchFn FindPattern(const char *filename)
	{
		if (!filename) return nullptr;
		for (UInt32 i = 0; i < s_patternCount; i++)
			if (!strncmp(filename, s_patterns[i].prefix, s_patterns[i].prefixLen))
				return s_patterns[i].patch;
		return nullptr;
	}

	static void DumpDisassembly(const char *name, const char *text)
	{
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%s.d3dx.txt", name);
		FILE *f = nullptr;
		if (!fopen_s(&f, path, "w"))
		{
			fputs(text, f);
			fclose(f);
		}
	}

	//takes the bytecode the engine just loaded, returns a patched d3d shader or null
	static void* BuildPatched(void *vanilla, const char *name, PatchFn patch)
	{
		auto getFunction = (HRESULT(__stdcall*)(void*, void*, UInt32*))
			Engine::ComVtbl(vanilla)[Engine::kD3DVS_GetFunction];
		UInt32 size = 0;
		if (FAILED(getFunction(vanilla, nullptr, &size)) || !size) return nullptr;
		DWORD *code = (DWORD*)malloc(size);
		if (!code) return nullptr;

		void *result = nullptr;
		ID3DXBuffer *disasm = nullptr, *assembled = nullptr, *errors = nullptr;
		char *patched = nullptr;
		if (SUCCEEDED(getFunction(vanilla, code, &size))
			&& SUCCEEDED(D3DXDisassembleShader(code, FALSE, nullptr, &disasm)))
		{
			const char *text = (const char*)disasm->GetBufferPointer();
			if (s_dump) DumpDisassembly(name, text);
			patched = patch(text);
			if (patched)
			{
				if (SUCCEEDED(D3DXAssembleShader(patched, (UInt32)strlen(patched), nullptr, nullptr, 0, &assembled, &errors)))
				{
					if (void *device = Engine::GetD3DDevice())
						((HRESULT(__stdcall*)(void*, const DWORD*, void**))
							Engine::ComVtbl(device)[Engine::kD3DDev_CreateVertexShader])
							(device, (const DWORD*)assembled->GetBufferPointer(), &result);
				}
				else if (errors)
					Log("assemble failed for %s: %s", name, (const char*)errors->GetBufferPointer());
			}
			else Log("pattern mismatch in %s, left vanilla", name);
		}
		if (patched) free(patched);
		if (disasm) disasm->Release();
		if (assembled) assembled->Release();
		if (errors) errors->Release();
		free(code);
		return result;
	}

	static void* PatchCreatedShader(void *niShader, const char *filename)
	{
		PatchFn patch = niShader ? FindPattern(filename) : nullptr;
		if (!patch) return niShader;

		void **handleSlot = (void**)((UInt8*)niShader + Engine::kNiD3DVertexShader_Handle);
		void *vanilla = *handleSlot;
		if (!vanilla) return niShader;

		if (void *replacement = BuildPatched(vanilla, filename, patch))
		{
			*handleSlot = replacement;
			Engine::ComRelease(vanilla);
			s_patched++;
			Log("patched %s", filename);
		}
		else s_skipped++;
		return niShader;
	}

	static void* CallOriginal(Detours::CallDetour &detour, void *self, const char *path,
		D3DXMACRO *macros, const char *version, const char *filename)
	{
		void *niShader = ((_CreateVS)detour.GetOverwrittenAddr())(self, path, macros, version, filename);
		return PatchCreatedShader(niShader, filename);
	}

	static void* __fastcall GrassLowDetailHook(void *self, void*, const char *path,
		D3DXMACRO *macros, const char *version, const char *filename)
	{
		return CallOriginal(s_grassLowDetailCall, self, path, macros, version, filename);
	}

	static void* __fastcall GrassHighDetailHook(void *self, void*, const char *path,
		D3DXMACRO *macros, const char *version, const char *filename)
	{
		return CallOriginal(s_grassHighDetailCall, self, path, macros, version, filename);
	}

	static void* __fastcall LeafHook(void *self, void*, const char *path,
		D3DXMACRO *macros, const char *version, const char *filename)
	{
		return CallOriginal(s_leafCall, self, path, macros, version, filename);
	}

	static void* __fastcall FrondHook(void *self, void*, const char *path,
		D3DXMACRO *macros, const char *version, const char *filename)
	{
		return CallOriginal(s_frondCall, self, path, macros, version, filename);
	}

	bool Install()
	{
		HMODULE d3dx = GetModuleHandleA("d3dx9_38.dll");
		if (!d3dx)
		{
			Log("d3dx9_38.dll not loaded, disabled");
			return false;
		}
		D3DXAssembleShader = (_D3DXAssembleShader)GetProcAddress(d3dx, "D3DXAssembleShader");
		D3DXDisassembleShader = (_D3DXDisassembleShader)GetProcAddress(d3dx, "D3DXDisassembleShader");
		if (!D3DXAssembleShader || !D3DXDisassembleShader)
		{
			Log("d3dx9 exports missing, disabled");
			return false;
		}

		if (!s_grassLowDetailCall.WriteRelCall(0xBABC8B, GrassLowDetailHook)
			|| !s_grassHighDetailCall.WriteRelCall(0xBABD9B, GrassHighDetailHook)
			|| !s_leafCall.WriteRelCall(0xBB1590, LeafHook)
			|| !s_frondCall.WriteRelCall(0xBD28F0, FrondHook))
		{
			s_frondCall.Remove();
			s_leafCall.Remove();
			s_grassHighDetailCall.Remove();
			s_grassLowDetailCall.Remove();
			Log("shader call hooks failed, disabled");
			return false;
		}
		Log("shader call hooks installed");
		return true;
	}

	void ReportCounts()
	{
		Log("shaders patched: %d, skipped: %d", s_patched, s_skipped);
	}
}
