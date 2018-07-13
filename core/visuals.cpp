#include "visuals.h"
#include "fw_bridge.h"

bool Visuals::shouldDraw = false;

#ifdef PT_VISUALS
void Visuals::Draw()
{
	if (!engine->IsInGame()) {
		shouldDraw = false;
		return;
	}

	if (!shouldDraw)
		return;

	auto& pl = FwBridge::playerTrack.GetLastItem(0);
	static matrix4x4& w2s = (matrix4x4&)engine->WorldToScreenMatrix();

	vec2 screen;
	int w, h;
	engine->GetScreenSize(w, h);
	screen[0] = w;
	screen[1] = h;

	int count = pl.count;

	for (int i = 0; i < count; i++) {
		for (int o = 0; o < MAX_HITBOXES; o++) {
			mvec3 mpVec = pl.hitboxes[i].mpOffset[o];
			mvec3 ptVec = pl.hitboxes[i].mpDir[o] * pl.hitboxes[i].radius[o];
			mpVec += ptVec;
			mpVec = pl.hitboxes[i].wm[o].VecSoaTransform(mpVec);

			bool flags[mpVec.Yt];
			mvec3 screenPos = w2s.WorldToScreen(mpVec, screen, flags);

			for (size_t u = 0; u < MULTIPOINT_COUNT; u++) {
				if (!flags[u])
					continue;
				vec3 screen = (vec3)screenPos.acc[u];
				surface->DrawSetColor(Color(1.f, 0.f, 0.f, 1.f));
				surface->DrawFilledRect(screen[0]-2, screen[1]-2, screen[0]+2, screen[1]+2);
			}
		}
	}
}
#endif
