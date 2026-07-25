#pragma once

namespace TreeBend
{
	struct Settings
	{
		bool	enabled			= true;
		float	radius			= 160.0f;
		float	strengthScale	= 0.8f;
		float	plantHeight		= 175.0f;
	};
	extern Settings settings;

	bool RegisterPattern();
	bool InstallHooks();
	void RemoveHooks();
}
