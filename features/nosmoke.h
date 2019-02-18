#ifndef NOSMOKE_H
#define NOSMOKE_H

class CRecvProxyData;

namespace NoSmoke
{
	void HandleProxy(const CRecvProxyData* data, void* ent, void* out);
	void OnRenderStart();
}

#endif
