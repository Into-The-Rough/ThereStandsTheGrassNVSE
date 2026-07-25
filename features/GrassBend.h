#pragma once

namespace GrassBend
{
	struct Settings
	{
		bool	enabled		= true;
		float	radius		= 100.0f;
		bool	disableWithNVR = true;
	};
	extern Settings settings;

	bool RegisterPattern();
	bool InstallHooks();
	void RemoveHooks();
}
