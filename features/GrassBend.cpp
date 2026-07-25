#include "core/Common.h"
#include "core/EngineLayout.h"
#include "core/PlayerBend.h"
#include "core/ShaderPatch.h"
#include "features/GrassBend.h"
#include "internal/Detours.h"

namespace GrassBend
{
	Settings settings;

	static Detours::VtableDetour s_setupDetour;
	typedef int(__thiscall* _Setup)(void*, int, int, int, int, int, int, float*, int);
	constexpr float kStrengthReferenceHeight = 90.0f;
	constexpr float kHeightResponseScale = 0.5f;

	//inserts the bend after the line that adds the instance offset, the last position write
	//before the final modelviewproj transform in every vanilla permutation.
	//the displacement follows the rendered vertex height above its own instance anchor. that keeps
	//the root planted and scales the same bend angle across mesh and per-instance height differences
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

		int colorReg = -1, anchorLine = -1, posReg = -1, lastDp4Line = -1, dp4Reg = -1;
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
			if (const char *dc = strstr(s, "dcl_color v"))
				colorReg = atoi(dc + 11);
			if (!strncmp(s, "add r", 5) && strstr(s, ".xyz,"))
			{
				const char *tail = s + strlen(s);
				const char *c20 = "c20[a0.x]";
				if (strlen(s) > strlen(c20) && !strcmp(tail - strlen(c20), c20))
				{
					anchorLine = i;
					posReg = atoi(s + 5);
				}
			}
			if (!strncmp(s, "dp4 ", 4))
			{
				bool mvpRow = strstr(s, ", c9,") || strstr(s, ", c10,") || strstr(s, ", c11,") || strstr(s, ", c12,");
				if (mvpRow)
					if (const char *r = strrchr(s, 'r'))
					{
						lastDp4Line = i;
						dp4Reg = atoi(r + 1);
					}
			}
		}
		if (colorReg < 0 || anchorLine < 0 || lastDp4Line < anchorLine || dp4Reg != posReg)
			return nullptr;

		char block[1024];
		snprintf(block, sizeof(block),
			"    add r10.xy, r%d, -c248\n"
			"    mul r11.x, r10.x, r10.x\n"
			"    mad r11.x, r10.y, r10.y, r11.x\n"
			"    add r11.x, r11.x, c249.z\n"
			"    rsq r11.y, r11.x\n"
			"    rcp r11.z, r11.y\n"
			"    mul r11.w, r11.z, c248.w\n"
			"    add r11.w, -r11.w, c249.z\n"
			"    max r11.w, r11.w, c249.w\n"
			"    mul r11.w, r11.w, r11.w\n"
			"    mul r11.w, r11.w, c249.x\n"
			"    add r10.z, r%d.z, -c20[a0.x].z\n"
			"    max r10.z, r10.z, c249.w\n"
			"    mul r11.w, r11.w, r10.z\n"
			"    mul r10.xy, r10, r11.y\n"
			"    mad r%d.xy, r10, r11.w, r%d\n",
			posReg, posReg, posReg, posReg);

		size_t outCap = strlen(text) + strlen(block) + 16;
		char *out = (char*)malloc(outCap);
		if (!out) return nullptr;
		char *w = out;
		for (int i = 0; i < lineCount; i++)
		{
			memcpy(w, lines[i], lineLen[i]);
			w += lineLen[i];
			*w++ = '\n';
			if (i == anchorLine)
			{
				size_t bl = strlen(block);
				memcpy(w, block, bl);
				w += bl;
			}
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
			strength = PlayerBend::Strength();
		}
		float c[8] =
		{
			local[0], local[1], local[2], 1.0f / settings.radius,
			strength / kStrengthReferenceHeight * kHeightResponseScale, 0.0f, 1.0f, 0.0f
		};
		Engine::SetVertexShaderConstants(device, Engine::kConst_Grass, c, 2);
	}

	static int __fastcall SetupHook(void *self, void*, int a2, int a3, int a4, int a5, int a6, int a7, float *xform, int a9)
	{
		Upload(xform);
		return ((_Setup)s_setupDetour.GetOverwrittenAddr())(self, a2, a3, a4, a5, a6, a7, xform, a9);
	}

	bool RegisterPattern()
	{
		return ShaderPatch::RegisterPattern("GRASS2", Patch);
	}

	bool InstallHooks()
	{
		UInt32 slot = Engine::kVtbl_TallGrassShader + Engine::kShaderSetupSlot;
		if (!s_setupDetour.Write(slot, SetupHook))
		{
			Log("grass setup vtbl hook failed");
			return false;
		}
		Log("grass setup vtbl hook installed, orig %08X", s_setupDetour.GetOverwrittenAddr());
		return true;
	}

	void RemoveHooks()
	{
		s_setupDetour.Remove();
	}
}
