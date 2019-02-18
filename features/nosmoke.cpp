#include "nosmoke.h"
#include "../core/fw_bridge.h"
#include "../core/settings.h"
#include "../core/hooks.h"

static RecvVarProxyFn originalProxy = nullptr;
extern NetvarHook netvarHooks[];
extern size_t netvarCount;

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

	if (Settings::noSmoke)
		*(bool*)((uintptr_t)out + 1) = true;

	if (originalProxy)
		originalProxy(data, ent, out);
}

void NoSmoke::OnRenderStart()
{
	if (Settings::noSmoke)
		*smokeCount = 0;
}
