#include "visuals.h"
#include "fw_bridge.h"

bool Visuals::shouldDraw = false;

//Temporary debugging of the hit shot resolver
constexpr int HISTORY_COUNT = 2;
HistoryList<zvec3, HISTORY_COUNT> starts;
HistoryList<zvec3, HISTORY_COUNT> ends;
vec3_t start;
vec3_t end;
int best = 0;
int besti = 0;

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

	for (int i = 0; i < HISTORY_COUNT; i++) {

		bool flags[16];
		zvec3 screenStartPos = w2s.WorldToScreen(starts.GetLastItem(i), screen, flags);
		bool flags2[16];
		zvec3 screenEndPos = w2s.WorldToScreen(ends.GetLastItem(i), screen, flags2);

		for (size_t u = 0; u < 16; u++) {
			if (!flags[u] || !flags2[u])
				continue;
			vec3 screenStart = (vec3)screenStartPos.acc[u];
			vec3 screenEnd = (vec3)screenEndPos.acc[u];
			if (best == u && besti == i)
				surface->DrawSetColor(Color(1.f, 0.f, 1.f, 1.f));
			else
				surface->DrawSetColor(Color(0.f, 0.f, 1.f, 1.f));
			surface->DrawLine(screenStart[0], screenStart[1], screenEnd[0], screenEnd[1]);
		}
	}


	{
		bool flag1, flag2;

		vec3_t startPos = w2s.WorldToScreen(start, screen, flag1);
		vec3_t endPos = w2s.WorldToScreen(end, screen, flag2);

		if (!flag1 || !flag2)
			return;

		surface->DrawSetColor(Color(0.f, 1.f, 0.f, 1.f));
		surface->DrawLine(startPos[0], startPos[1], endPos[0], endPos[1]);
	}
}
#endif

void Visuals::PassColliders(vec3soa<float, 16> start, vec3soa<float, 16> end)
{
	starts.Push(start);
	ends.Push(end);
}


void Visuals::PassStart(vec3_t start, vec3_t end)
{
	::start = start;
	::end = end;
}

void Visuals::PassBest(int o, int i)
{
	besti = i;
	best = o;
}
