#include "nosmoke.h"
#include "../core/engine.h"
#include "../core/hooks.h"

static RecvVarProxyFn originalProxy = nullptr;
extern NetvarHook netvarHooks[];
extern size_t netvarCount;

static bool noSmoke = true;

void NoSmoke::HandleProxy(const CRecvProxyData* data, void* ent, void* out)
{
	//TODO: Do this cleaner
	if (!originalProxy) {
		for (size_t i = 0; i < netvarCount; i++) {
			if (netvarHooks[i].hook == CSGOHooks::DidSmokeEffectProxy) {
				originalProxy = netvarHooks[i].original;
				break;
			}
		}
	}

	if (noSmoke)
		*(bool*)((uintptr_t)out + 1) = true;

	if (originalProxy)
		originalProxy(data, ent, out);
}

void NoSmoke::OnRenderStart()
{
	if (noSmoke)
		*smokeCount = 0;
}
