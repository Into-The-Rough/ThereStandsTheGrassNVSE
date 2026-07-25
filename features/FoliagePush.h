#pragma once

namespace FoliagePush
{
	struct Settings
	{
		bool	enabled		= true;
		float	radius		= 200.0f;
		float	maxDegrees	= 18.0f;
		float	speed		= 6.0f;
		float	innerRadius	= 45.0f;
		float	turnSpeed	= 3.0f;
		int		rescanMS	= 400;
	};
	extern Settings settings;

	void SetForms(const char *csvHexList);
	void Update(float dt);
	void ClearState();
}
