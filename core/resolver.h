#ifndef RESOLVER_H
#define RESOLVER_H

struct CEffectData;
class IGameEvent;

namespace Resolver
{
	void Tick();
	void ImpactEvent(IGameEvent* data, unsigned int name);
	void HandleImpact(const CEffectData& effectData);
}

#endif
