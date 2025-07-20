#include <engine/server.h>

#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>

#include "character.h"
#include "portal.h"

// top, left, bottom, right，center
static const vec2 s_aPortalCheckPoints[5] = {{0.f, -48.f}, {-24.f, 0.f}, {0.f, 49.f}, {24.f, 0.f}, {0.f, 0.f}};

CPortal::CPortal(CGameWorld *pGameWorld, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTAL, Pos)
{
	for(int i = 0; i < 4; i++)
	{
		m_aPortalIDs[i] = Server()->SnapNewID();
	}
	GameWorld()->InsertEntity(this);
}

CPortal::~CPortal()
{
	for(int i = 0; i < 4; i++)
	{
		Server()->SnapFreeID(m_aPortalIDs[i]);
	}
}

void CPortal::Tick()
{
	for(CCharacter *pChr = (CCharacter *) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChr; pChr = (CCharacter *) pChr->TypeNext())
	{
		for(auto& Pos : s_aPortalCheckPoints)
		{
			float Len = distance(GetPos() + Pos, pChr->GetPos());
			if(Len < pChr->GetProximityRadius() + 2.f)
			{
				if(random_int() % 100 > 77 && GameServer()->GameController()->IsInDarkMode())
				{
					Server()->SwitchClientMap(pChr->GetCID(), CalculateUuid("Void"));
					break;
				}
				Server()->SwitchClientMap(pChr->GetCID(), Server()->GetBaseMapUuid());
				break;
			}
		}
	}
}

void CPortal::Snap(int SnappingClient)
{
	for(int i = 0; i < 4; i++)
	{
		vec2 Pos = m_Pos + s_aPortalCheckPoints[i];
		vec2 From = m_Pos + s_aPortalCheckPoints[(i == 3) ? 0 : (i + 1)];
		if(NetworkClippedLine(SnappingClient, Pos, From))
			continue;

		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aPortalIDs[i], sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = round_to_int(Pos.x);
		pObj->m_Y = round_to_int(Pos.y);
		pObj->m_FromX = round_to_int(From.x);
		pObj->m_FromY = round_to_int(From.y);
		pObj->m_StartTick = Server()->Tick();
	}

	if(NetworkClipped(SnappingClient, m_Pos))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = round_to_int(m_Pos.x);
	pObj->m_Y = round_to_int(m_Pos.y);
	pObj->m_FromX = round_to_int(m_Pos.x);
	pObj->m_FromY = round_to_int(m_Pos.y);
	pObj->m_StartTick = Server()->Tick();
}
