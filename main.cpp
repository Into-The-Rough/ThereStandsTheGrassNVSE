#include "core/Common.h"
#include "core/Config.h"
#include "core/PlayerBend.h"
#include "core/ShaderPatch.h"
#include "features/GrassBend.h"
#include "features/TreeBend.h"
#include "features/FoliagePush.h"

static FILE *g_log = nullptr;
static PluginHandle g_pluginHandle = kPluginHandle_Invalid;
static NVSEMessagingInterface *g_messaging = nullptr;
static bool g_hooked = false;
static bool g_installAttempted = false;

void OpenLog()
{
	if (Config::debugLog && !g_log)
		fopen_s(&g_log, PLUGIN_NAME ".log", "w");
}

void Log(const char *fmt, ...)
{
	if (!g_log) return;
	va_list args;
	va_start(args, fmt);
	vfprintf(g_log, fmt, args);
	va_end(args);
	fputc('\n', g_log);
	fflush(g_log);
}

static bool InstallEverything()
{
	if (GrassBend::settings.disableWithNVR && (GetModuleHandleA("NewVegasReloaded.dll") || GetModuleHandleA("nvr.dll")))
	{
		Log("NVR detected, disabled");
		return false;
	}
	if (!PlayerBend::Init())
	{
		Log("d3dx9 unavailable, disabled");
		return false;
	}
	if (!GrassBend::RegisterPattern() || !TreeBend::RegisterPattern())
	{
		Log("shader pattern registration failed, disabled");
		return false;
	}
	if (!GrassBend::InstallHooks())
		return false;
	if (!TreeBend::InstallHooks())
	{
		GrassBend::RemoveHooks();
		return false;
	}
	if (!ShaderPatch::Install())
	{
		TreeBend::RemoveHooks();
		GrassBend::RemoveHooks();
		return false;
	}
	return true;
}

static void MessageHandler(NVSEMessagingInterface::Message *msg)
{
	switch (msg->type)
	{
	case kMsg_PostLoad:
		if (!g_installAttempted)
		{
			g_installAttempted = true;
			g_hooked = InstallEverything();
		}
		break;
	case kMsg_DeferredInit:
		if (g_hooked) ShaderPatch::ReportCounts();
		break;
	case kMsg_MainGameLoop:
		if (!g_hooked) break;
		{
			static UInt32 lastMS = 0;
			UInt32 now = GetTickCount();
			float dt = (now - lastMS) * 0.001f;
			lastMS = now;
			if (dt <= 0.0f || dt > 0.25f) dt = 0.05f;
			PlayerBend::Update(dt);
			FoliagePush::Update(dt);
		}
		break;
	case kMsg_PreLoadGame:
	case kMsg_NewGame:
		FoliagePush::ClearState();
		PlayerBend::Reset();
		break;
	case kMsg_ReloadConfig:
		Config::Load();
		OpenLog();
		break;
	}
}

extern "C"
{
	__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface *nvse, PluginInfo *info)
	{
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = PLUGIN_VERSION;
		return !nvse->isEditor;
	}

	__declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface *nvse)
	{
		g_pluginHandle = nvse->GetPluginHandle();
		Config::Load();
		OpenLog();
		Log(PLUGIN_NAME " v%d", PLUGIN_VERSION);
		g_messaging = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
		if (!g_messaging || !g_messaging->RegisterListener(g_pluginHandle, "NVSE", MessageHandler))
			Log("NVSE messaging unavailable, disabled");
		return true;
	}
}
