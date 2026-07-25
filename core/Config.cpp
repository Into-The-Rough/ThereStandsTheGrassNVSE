#include "Common.h"
#include "Config.h"
#include "PlayerBend.h"
#include "features/GrassBend.h"
#include "features/TreeBend.h"
#include "features/FoliagePush.h"

namespace Config
{
	bool debugLog = false;
	static char s_path[MAX_PATH] = {};

	const char* Path()
	{
		if (!s_path[0])
		{
			GetModuleFileNameA(nullptr, s_path, MAX_PATH);
			if (char *slash = strrchr(s_path, '\\')) *slash = '\0';
			strcat_s(s_path, "\\Data\\config\\" PLUGIN_NAME ".ini");
		}
		return s_path;
	}

	static float ReadFloat(const char *section, const char *key, float fallback)
	{
		char value[64], defaultValue[64];
		snprintf(defaultValue, sizeof(defaultValue), "%.6f", fallback);
		GetPrivateProfileStringA(section, key, defaultValue, value, sizeof(value), Path());
		return (float)atof(value);
	}

	static bool ReadBool(const char *section, const char *key, bool fallback)
	{
		return GetPrivateProfileIntA(section, key, fallback ? 1 : 0, Path()) != 0;
	}

	static int ReadInt(const char *section, const char *key, int fallback)
	{
		return GetPrivateProfileIntA(section, key, fallback, Path());
	}

	static void Clamp()
	{
		if (GrassBend::settings.radius < 10.0f) GrassBend::settings.radius = 10.0f;
		if (TreeBend::settings.radius < 10.0f) TreeBend::settings.radius = 10.0f;
		if (TreeBend::settings.plantHeight < 1.0f) TreeBend::settings.plantHeight = 1.0f;
		if (FoliagePush::settings.radius < 10.0f) FoliagePush::settings.radius = 10.0f;
		if (FoliagePush::settings.innerRadius < 1.0f) FoliagePush::settings.innerRadius = 1.0f;
		if (FoliagePush::settings.rescanMS < 50) FoliagePush::settings.rescanMS = 50;
		if (PlayerBend::settings.speedForMax < 1.0f) PlayerBend::settings.speedForMax = 1.0f;
		if (PlayerBend::settings.springBackSeconds < 0.05f) PlayerBend::settings.springBackSeconds = 0.05f;
		if (PlayerBend::settings.positionLagSeconds < 0.01f) PlayerBend::settings.positionLagSeconds = 0.01f;
	}

	void Load()
	{
		GrassBend::settings.enabled = ReadBool("General", "bEnabled", GrassBend::settings.enabled);
		GrassBend::settings.radius = ReadFloat("Interaction", "fRadius", GrassBend::settings.radius);
		GrassBend::settings.disableWithNVR = ReadBool("Compat", "bDisableWithNVR", GrassBend::settings.disableWithNVR);

		PlayerBend::settings.strengthStanding = ReadFloat("Interaction", "fStrengthStanding", PlayerBend::settings.strengthStanding);
		PlayerBend::settings.strengthMax = ReadFloat("Interaction", "fStrengthMax", PlayerBend::settings.strengthMax);
		PlayerBend::settings.speedForMax = ReadFloat("Interaction", "fSpeedForMax", PlayerBend::settings.speedForMax);
		PlayerBend::settings.springBackSeconds = ReadFloat("Interaction", "fSpringBackSeconds", PlayerBend::settings.springBackSeconds);
		PlayerBend::settings.positionLagSeconds = ReadFloat("Interaction", "fPositionLagSeconds", PlayerBend::settings.positionLagSeconds);

		TreeBend::settings.enabled = ReadBool("Trees", "bTrees", TreeBend::settings.enabled);
		TreeBend::settings.radius = ReadFloat("Trees", "fTreeRadius", TreeBend::settings.radius);
		TreeBend::settings.strengthScale = ReadFloat("Trees", "fTreeStrengthScale", TreeBend::settings.strengthScale);
		TreeBend::settings.plantHeight = ReadFloat("Trees", "fTreeHeight", TreeBend::settings.plantHeight);

		FoliagePush::settings.enabled = ReadBool("Foliage", "bFoliage", FoliagePush::settings.enabled);
		FoliagePush::settings.radius = ReadFloat("Foliage", "fFoliageRadius", FoliagePush::settings.radius);
		FoliagePush::settings.maxDegrees = ReadFloat("Foliage", "fFoliageMaxDegrees", FoliagePush::settings.maxDegrees);
		FoliagePush::settings.speed = ReadFloat("Foliage", "fFoliageSpeed", FoliagePush::settings.speed);
		FoliagePush::settings.innerRadius = ReadFloat("Foliage", "fFoliageInnerRadius", FoliagePush::settings.innerRadius);
		FoliagePush::settings.turnSpeed = ReadFloat("Foliage", "fFoliageTurnSpeed", FoliagePush::settings.turnSpeed);
		FoliagePush::settings.rescanMS = ReadInt("Foliage", "iFoliageRescanMS", FoliagePush::settings.rescanMS);

		char forms[512];
		GetPrivateProfileStringA("Foliage", "sFoliageForms", "", forms, sizeof(forms), Path());
		if (forms[0]) FoliagePush::SetForms(forms);

		debugLog = ReadBool("Debug", "bDebugLog", debugLog);
		Clamp();
	}
}
