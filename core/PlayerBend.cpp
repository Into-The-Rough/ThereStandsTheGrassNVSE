#include "Common.h"
#include "EngineLayout.h"
#include "PlayerBend.h"

namespace PlayerBend
{
	Settings settings;

	static _D3DXMatrixInverse D3DXMatrixInverse = nullptr;
	static _D3DXVec3TransformCoord D3DXVec3TransformCoord = nullptr;

	static float s_pos[3] = {};
	static float s_strength = 0.0f;
	static bool s_have = false;
	static float s_prevPos[3] = {};
	static bool s_havePrev = false;
	static bool s_active = false;

	bool Active() { return s_active && s_have; }
	const float* Position() { return s_pos; }
	float Strength() { return s_have ? s_strength : 0.0f; }

	bool Init()
	{
		HMODULE d3dx = GetModuleHandleA("d3dx9_38.dll");
		if (!d3dx) return false;
		D3DXMatrixInverse = (_D3DXMatrixInverse)GetProcAddress(d3dx, "D3DXMatrixInverse");
		D3DXVec3TransformCoord = (_D3DXVec3TransformCoord)GetProcAddress(d3dx, "D3DXVec3TransformCoord");
		return D3DXMatrixInverse && D3DXVec3TransformCoord;
	}

	void Reset()
	{
		s_have = false;
		s_havePrev = false;
		s_strength = 0.0f;
		s_active = false;
	}

	void Update(float dt)
	{
		Engine::RefrView *player = Engine::Player();
		Engine::CellView *cell = player ? (Engine::CellView*)player->parentCell : nullptr;
		s_active = cell && cell->worldSpace;	//worldSpace null means interior

		float target = 0.0f, px = 0.0f, py = 0.0f, pz = 0.0f;
		if (s_active)
		{
			px = player->position[0]; py = player->position[1]; pz = player->position[2];
			float speed = 0.0f;
			if (s_havePrev)
			{
				float dx = px - s_prevPos[0], dy = py - s_prevPos[1];
				speed = sqrtf(dx * dx + dy * dy) / dt;
				if (speed > 2000.0f) speed = 0.0f;		//teleport or load
			}
			s_prevPos[0] = px; s_prevPos[1] = py; s_prevPos[2] = pz;
			s_havePrev = true;
			float frac = speed / settings.speedForMax;
			if (frac > 1.0f) frac = 1.0f;
			target = settings.strengthStanding + (settings.strengthMax - settings.strengthStanding) * frac;
		}
		else s_havePrev = false;

		if (!s_have && s_active)
		{
			s_pos[0] = px; s_pos[1] = py; s_pos[2] = pz;
			s_have = true;
		}
		if (!s_have) return;

		float posAlpha = dt / settings.positionLagSeconds;
		if (posAlpha > 1.0f) posAlpha = 1.0f;
		if (s_active)
		{
			s_pos[0] += (px - s_pos[0]) * posAlpha;
			s_pos[1] += (py - s_pos[1]) * posAlpha;
			s_pos[2] += (pz - s_pos[2]) * posAlpha;
		}
		float strAlpha = dt / settings.springBackSeconds;
		if (strAlpha > 1.0f) strAlpha = 1.0f;
		s_strength += (target - s_strength) * strAlpha;
		if (!s_active && s_strength < 0.05f)
		{
			s_have = false;
			s_strength = 0.0f;
		}
	}

	void WorldToObject(const float *xform, float *outLocal)
	{
		if (!s_have)
		{
			outLocal[0] = outLocal[1] = outLocal[2] = 0.0f;
			return;
		}
		float sc = xform[12];
		//grass batches are placed with an identity transform, so the common case is a subtract
		if (sc == 1.0f
			&& xform[0] == 1.0f && xform[4] == 1.0f && xform[8] == 1.0f
			&& xform[1] == 0.0f && xform[2] == 0.0f && xform[3] == 0.0f
			&& xform[5] == 0.0f && xform[6] == 0.0f && xform[7] == 0.0f)
		{
			outLocal[0] = s_pos[0] - xform[9];
			outLocal[1] = s_pos[1] - xform[10];
			outLocal[2] = s_pos[2] - xform[11];
			return;
		}
		float m[16], inv[16];
		m[0] = xform[0] * sc; m[1] = xform[3] * sc; m[2] = xform[6] * sc; m[3] = 0.0f;
		m[4] = xform[1] * sc; m[5] = xform[4] * sc; m[6] = xform[7] * sc; m[7] = 0.0f;
		m[8] = xform[2] * sc; m[9] = xform[5] * sc; m[10] = xform[8] * sc; m[11] = 0.0f;
		m[12] = xform[9]; m[13] = xform[10]; m[14] = xform[11]; m[15] = 1.0f;
		D3DXMatrixInverse(inv, nullptr, m);
		D3DXVec3TransformCoord(outLocal, s_pos, inv);
	}
}
