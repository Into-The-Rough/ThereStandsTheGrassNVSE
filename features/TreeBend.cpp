#include "core/Common.h"
#include "core/EngineLayout.h"
#include "core/PlayerBend.h"
#include "core/ShaderPatch.h"
#include "features/TreeBend.h"
#include "internal/Detours.h"

namespace TreeBend
{
	Settings settings;

	static Detours::VtableDetour s_leafDetour;
	static Detours::VtableDetour s_frondDetour;
	typedef int(__thiscall* _Setup)(void*, int, int, int, int, int, int, float*, int);

	//speedtree stages its object space position into one register and transforms it by c0-c3.
	//the odd variants stage that through a temp and only mov it to oPos at the end, so there is
	//no dp4 oPos.w to anchor on in half of them - the first modelviewproj row is the form that
	//appears in all eight.
	//the lean weight is v0.z over fTreeHeight, not speedtree's own per vertex stiffness, which
	//reads about zero on WastelandShrub01
	char* Patch(const char *text)
	{
		const char *lines[512];
		int lineLen[512];
		int lineCount = 0;
		const char *p = text;
		while (*p && lineCount < 512)
		{
			const char *nl = strchr(p, '\n');
			int len = nl ? (int)(nl - p) : (int)strlen(p);
			lines[lineCount] = p;
			lineLen[lineCount] = len;
			lineCount++;
			if (!nl) break;
			p = nl + 1;
		}
		if (lineCount >= 512) return nullptr;
		if (strstr(text, "r10") || strstr(text, "r11")) return nullptr;

		int flexReg = -1, anchorLine = -1, posReg = -1;
		char buf[256];
		for (int i = 0; i < lineCount; i++)
		{
			if (lineLen[i] >= (int)sizeof(buf)) continue;
			memcpy(buf, lines[i], lineLen[i]);
			buf[lineLen[i]] = '\0';
			char *end = buf + lineLen[i];
			while (end > buf && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
			const char *s = buf;
			while (*s == ' ' || *s == '\t') s++;
			if (const char *dc = strstr(s, "dcl_blendindices v"))
				flexReg = atoi(dc + 18);
			if (anchorLine < 0 && !strncmp(s, "dp4 ", 4)
				&& (strstr(s, ", c0,") || strstr(s, ", c1,") || strstr(s, ", c2,") || strstr(s, ", c3,")))
			{
				if (const char *r = strrchr(s, 'r'))
				{
					anchorLine = i;
					posReg = atoi(r + 1);
				}
			}
		}
		if (flexReg < 0 || anchorLine < 0 || posReg < 0)
			return nullptr;

		char block[1024];
		snprintf(block, sizeof(block),
			"    add r10.xy, r%d, -c90\n"
			"    mul r11.x, r10.x, r10.x\n"
			"    mad r11.x, r10.y, r10.y, r11.x\n"
			"    add r11.x, r11.x, c91.z\n"
			"    rsq r11.y, r11.x\n"
			"    rcp r11.z, r11.y\n"
			"    mul r11.w, r11.z, c90.w\n"
			"    add r11.w, -r11.w, c91.z\n"
			"    max r11.w, r11.w, c91.w\n"
			"    mul r11.w, r11.w, r11.w\n"
			"    mul r11.w, r11.w, c91.x\n"
			"    mul r10.z, v0.z, c91.y\n"
			"    min r10.z, r10.z, c91.z\n"
			"    max r10.z, r10.z, c91.w\n"
			"    mul r11.w, r11.w, r10.z\n"
			"    mul r10.xy, r10, r11.y\n"
			"    mad r%d.xy, r10, r11.w, r%d\n",
			posReg, posReg, posReg);

		size_t outCap = strlen(text) + strlen(block) + 16;
		char *out = (char*)malloc(outCap);
		if (!out) return nullptr;
		char *w = out;
		for (int i = 0; i < lineCount; i++)
		{
			if (i == anchorLine)
			{
				size_t bl = strlen(block);
				memcpy(w, block, bl);
				w += bl;
			}
			memcpy(w, lines[i], lineLen[i]);
			w += lineLen[i];
			*w++ = '\n';
		}
		*w = '\0';
		return out;
	}

	static void Upload(float *xform)
	{
		void *device = Engine::GetD3DDevice();
		if (!device) return;
		float local[3] = {};
		float strength = 0.0f;
		if (settings.enabled)
		{
			PlayerBend::WorldToObject(xform, local);
			strength = PlayerBend::Strength() * settings.strengthScale;
		}
		float c[8] =
		{
			local[0], local[1], local[2], 1.0f / settings.radius,
			strength, 1.0f / settings.plantHeight, 1.0f, 0.0f
		};
		Engine::SetVertexShaderConstants(device, Engine::kConst_Tree, c, 2);
	}

	static int __fastcall LeafSetupHook(void *self, void*, int a2, int a3, int a4, int a5, int a6, int a7, float *xform, int a9)
	{
		Upload(xform);
		return ((_Setup)s_leafDetour.GetOverwrittenAddr())(self, a2, a3, a4, a5, a6, a7, xform, a9);
	}

	static int __fastcall FrondSetupHook(void *self, void*, int a2, int a3, int a4, int a5, int a6, int a7, float *xform, int a9)
	{
		Upload(xform);
		return ((_Setup)s_frondDetour.GetOverwrittenAddr())(self, a2, a3, a4, a5, a6, a7, xform, a9);
	}

	bool RegisterPattern()
	{
		return ShaderPatch::RegisterPattern("STFROND", Patch)
			&& ShaderPatch::RegisterPattern("STLEAF", Patch);
	}

	bool InstallHooks()
	{
		UInt32 leafSlot = Engine::kVtbl_SpeedTreeLeafShader + Engine::kShaderSetupSlot;
		UInt32 frondSlot = Engine::kVtbl_SpeedTreeFrondShader + Engine::kShaderSetupSlot;
		if (!s_leafDetour.Write(leafSlot, LeafSetupHook))
		{
			Log("leaf setup vtbl hook failed");
			return false;
		}
		if (!s_frondDetour.Write(frondSlot, FrondSetupHook))
		{
			s_leafDetour.Remove();
			Log("frond setup vtbl hook failed");
			return false;
		}
		Log("tree setup vtbl hooks installed, leaf orig %08X frond orig %08X",
			s_leafDetour.GetOverwrittenAddr(), s_frondDetour.GetOverwrittenAddr());
		return true;
	}

	void RemoveHooks()
	{
		s_frondDetour.Remove();
		s_leafDetour.Remove();
	}
}
