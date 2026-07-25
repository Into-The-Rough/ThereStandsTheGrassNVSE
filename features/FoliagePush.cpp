#include "core/Common.h"
#include "core/EngineLayout.h"
#include "core/PlayerBend.h"
#include "features/FoliagePush.h"

namespace FoliagePush
{
	Settings settings;

	//statics have no wind or deformation stage in their shaders, and the SLS family that draws
	//them is 148 permutations wide and covers every wall and weapon in the game, so the plant is
	//leaned by rotating its render node instead.
	//nothing here is serialised. only the NiAVObject local transform is written, never the ref's
	//own angles, so nothing can reach the save and every change dies with the cell.
	constexpr UInt32 kMaxPushed = 48;
	constexpr UInt32 kMaxForms = 32;

	struct PushedRef
	{
		UInt32	refID;
		float	original[9];
		float	axisX, axisY;
		float	angle;
		bool	seen;
	};
	static PushedRef s_pushed[kMaxPushed];
	static UInt32 s_pushedCount = 0;
	static UInt32 s_forms[kMaxForms];
	static UInt32 s_formCount = 0;
	static UInt32 s_rescanMS = 0;

	static void WriteRotation(void *node, const float *rot)
	{
		memcpy((UInt8*)node + Engine::kNiAVObject_LocalTransform, rot, sizeof(float) * 9);
		UInt8 updateData[12] = {};
		void **vtbl = *(void***)node;
		((void(__thiscall*)(void*, const void*))vtbl[Engine::kNiAVObject_UpdateTransformAndBounds])(node, updateData);
	}

	static void ApplyLean(void *node, const PushedRef &p)
	{
		if (p.angle <= 0.0001f)
		{
			WriteRotation(node, p.original);
			return;
		}
		float c = cosf(p.angle), s = sinf(p.angle), v = 1.0f - c;
		float kx = p.axisX, ky = p.axisY;
		//rodrigues about the horizontal axis, kz is zero. column major, element(r,c) = m[c*3+r]
		float R[9];
		R[0] = c + kx * kx * v;	R[3] = kx * ky * v;		R[6] = ky * s;
		R[1] = kx * ky * v;		R[4] = c + ky * ky * v;	R[7] = -kx * s;
		R[2] = -ky * s;			R[5] = kx * s;			R[8] = c;
		float out[9];
		for (int col = 0; col < 3; col++)
			for (int row = 0; row < 3; row++)
			{
				float sum = 0.0f;
				for (int k = 0; k < 3; k++)
					sum += R[k * 3 + row] * p.original[col * 3 + k];
				out[col * 3 + row] = sum;
			}
		WriteRotation(node, out);
	}

	static PushedRef* Find(UInt32 refID)
	{
		for (UInt32 i = 0; i < s_pushedCount; i++)
			if (s_pushed[i].refID == refID) return &s_pushed[i];
		return nullptr;
	}

	static bool IsFoliage(UInt32 formID)
	{
		for (UInt32 i = 0; i < s_formCount; i++)
			if (s_forms[i] == formID) return true;
		return false;
	}

	static void Consider(Engine::RefrView *ref, float px, float py, float range)
	{
		if (!ref || !ref->baseForm || !IsFoliage(ref->baseForm->refID)) return;
		float dx = ref->position[0] - px, dy = ref->position[1] - py;
		if ((dx * dx + dy * dy) > range * range) return;
		void *node = Engine::RefRootNode(ref);
		if (!node) return;
		if (PushedRef *existing = Find(ref->refID))
		{
			existing->seen = true;
			return;
		}
		if (s_pushedCount >= kMaxPushed) return;
		PushedRef &p = s_pushed[s_pushedCount++];
		p.refID = ref->refID;
		memcpy(p.original, (UInt8*)node + Engine::kNiAVObject_LocalTransform, sizeof(float) * 9);
		p.angle = 0.0f;
		p.axisX = 0.0f;
		p.axisY = 0.0f;
		p.seen = true;
	}

	static void ScanCell(Engine::CellView *cell, float px, float py, float range)
	{
		if (!cell) return;
		Engine::CellRefNode *n = &cell->objectList;
		for (; n; n = n->next)
			Consider(n->ref, px, py, range);
	}

	//candidates are rebuilt on a timer, the per frame pass only touches what is already tracked
	static void Rescan(float px, float py)
	{
		for (UInt32 i = 0; i < s_pushedCount; i++) s_pushed[i].seen = false;
		float range = settings.radius * 1.5f;	//hysteresis so refs are not added and dropped
		Engine::TESView *tes = *(Engine::TESView**)Engine::kAddr_TES;
		if (tes && tes->grid && tes->grid->cells)
		{
			UInt32 total = tes->grid->gridSize * tes->grid->gridSize;
			for (UInt32 i = 0; i < total; i++)
				ScanCell(tes->grid->cells[i], px, py, range);
		}
		else if (Engine::RefrView *pc = Engine::Player())
			ScanCell((Engine::CellView*)pc->parentCell, px, py, range);
	}

	static void Release(UInt32 index)
	{
		PushedRef &p = s_pushed[index];
		if (Engine::RefrView *ref = Engine::LookupRef(p.refID))
			if (void *node = Engine::RefRootNode(ref))
				WriteRotation(node, p.original);
		s_pushed[index] = s_pushed[--s_pushedCount];
	}

	void ClearState()
	{
		s_pushedCount = 0;		//a load has already taken the scene graph, nothing to restore
	}

	void SetForms(const char *csvHexList)
	{
		s_formCount = 0;
		const char *p = csvHexList;
		while (*p && s_formCount < kMaxForms)
		{
			while (*p == ' ' || *p == ',') p++;
			if (!*p) break;
			s_forms[s_formCount++] = strtoul(p, nullptr, 16);
			while (*p && *p != ',') p++;
		}
	}

	void Update(float dt)
	{
		bool enabled = settings.enabled && s_formCount;
		if (!enabled && !s_pushedCount) return;
		const float *bend = PlayerBend::Position();
		float px = bend[0], py = bend[1];
		bool active = enabled && PlayerBend::Active();

		UInt32 now = GetTickCount();
		if (enabled && now - s_rescanMS >= (UInt32)settings.rescanMS)
		{
			s_rescanMS = now;
			Rescan(px, py);
		}
		float maxAngle = settings.maxDegrees * 0.0174533f;
		float approach = enabled ? dt * settings.speed : 1.0f;
		if (approach > 1.0f) approach = 1.0f;

		for (UInt32 i = 0; i < s_pushedCount; )
		{
			PushedRef &p = s_pushed[i];
			//the ref is re-derived from its id every frame, never held as a pointer across frames
			Engine::RefrView *ref = Engine::LookupRef(p.refID);
			void *node = ref ? Engine::RefRootNode(ref) : nullptr;
			if (!node)
			{
				s_pushed[i] = s_pushed[--s_pushedCount];	//3d gone with the cell
				continue;
			}
			float dx = ref->position[0] - px, dy = ref->position[1] - py;
			float dist = sqrtf(dx * dx + dy * dy);
			float target = 0.0f;
			if (p.seen && active && dist < settings.radius)
			{
				float falloff = 1.0f - dist / settings.radius;
				target = maxAngle * falloff * falloff;
				//the direction to the plant flips through 180 as you cross its centre, which would
				//whip the whole bush round. the axis is slewed rather than snapped and the slew
				//rate falls off toward the centre, so it resists near the middle without locking
				float inv = (dist > 0.01f) ? 1.0f / dist : 0.0f;
				if (inv > 0.0f)
				{
					float ax = -dy * inv, ay = dx * inv;
					bool haveAxis = (p.axisX != 0.0f) || (p.axisY != 0.0f);
					float nearFrac = dist / settings.innerRadius;
					if (nearFrac > 1.0f) nearFrac = 1.0f;
					float turn = haveAxis ? dt * settings.turnSpeed * nearFrac : 1.0f;
					if (turn > 1.0f) turn = 1.0f;
					p.axisX += (ax - p.axisX) * turn;
					p.axisY += (ay - p.axisY) * turn;
					float len = sqrtf(p.axisX * p.axisX + p.axisY * p.axisY);
					if (len > 0.001f)
					{
						p.axisX /= len;
						p.axisY /= len;
					}
					else	//blended straight through a 180 flip, take the new direction
					{
						p.axisX = ax;
						p.axisY = ay;
					}
				}
			}
			p.angle += (target - p.angle) * approach;
			if (p.angle < 0.0005f && target == 0.0f)
			{
				Release(i);
				continue;
			}
			ApplyLean(node, p);
			i++;
		}
	}
}
