#ifndef GAME_SERVER_ENTITIES_PORTAL_H
#define GAME_SERVER_ENTITIES_PORTAL_H

#include <game/server/entity.h>

class CPortal : public CEntity
{
	int m_aPortalIDs[4];
public:
	CPortal(CGameWorld *pGameWorld, vec2 Pos);
	~CPortal();

	void Tick();
	void Snap(int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_PORTAL_H
