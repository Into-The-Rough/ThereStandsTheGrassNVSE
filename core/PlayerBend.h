#pragma once

//the shared bend origin. one smoothed position and one strength, driven from player movement,
//consumed by every feature so grass, shrubs and statics all lean from the same point.

namespace PlayerBend
{
	struct Settings
	{
		float	strengthStanding	= 120.0f;
		float	strengthMax			= 150.0f;
		float	speedForMax			= 300.0f;
		float	springBackSeconds	= 0.5f;
		float	positionLagSeconds	= 0.08f;
	};
	extern Settings settings;

	bool Init();
	void Update(float dt);
	void Reset();

	bool Active();				//outdoors with a live bend origin
	const float* Position();	//world space
	float Strength();

	//puts the bend origin into the space a piece of geometry is drawn in, using the same transform
	//layout the engine builds in its own shader setup: rows scaled by xform[12], translate at
	//xform[9..11]. identity transforms skip the inverse, which is most grass batches.
	void WorldToObject(const float *xform, float *outLocal);
}
