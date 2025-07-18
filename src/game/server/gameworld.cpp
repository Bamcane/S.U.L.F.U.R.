/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entities/character.h"
#include "entity.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	m_pGameServer = nullptr;
	m_pConfig = nullptr;
	m_pServer = nullptr;

	m_aNumSpawnPoints[0] = 0;
	m_aNumSpawnPoints[1] = 0;
	m_aNumSpawnPoints[2] = 0;

	m_Paused = false;
	m_ResetRequested = false;
	for(int i = 0; i < NUM_ENTTYPES; i++)
		m_apFirstEntityTypes[i] = nullptr;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
}

void CGameWorld::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius + pEnt->m_ProximityRadius)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, EEntityFlag Flag)
{
	int Num = 0;
	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; pEnt = pEnt->TypeNext())
		{
			if((pEnt->GetObjFlag() & Flag) && distance(pEnt->m_Pos, Pos) < Radius + pEnt->m_ProximityRadius)
			{
				if(ppEnts)
					ppEnts[Num] = pEnt;
				Num++;
				if(Num == Max)
					break;
			}
		}
	}

	return Num;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = 0x0;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::DestroyEntity(CEntity *pEnt)
{
	pEnt->MarkForDestroy();
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = 0;
	pEnt->m_pPrevTypeEntity = 0;
}

//
void CGameWorld::Snap(int SnappingClient)
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::PostSnap()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->PostSnap();
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	GameServer()->GameController()->OnReset();
	RemoveEntities();

	m_ResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->IsMarkedForDestroy())
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	if(m_Paused)
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}
	else
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}

		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();
}

int64 CGameWorld::CmaskAllInWorld()
{
	int64 Mask = 0LL;
	for(auto &pPlayer : GameServer()->m_apPlayers)
	{
		if(pPlayer && pPlayer->GameWorld() == this)
		{
			Mask |= CmaskOne(pPlayer->GetCID());
		}
	}
	return Mask;
}

int64 CGameWorld::CmaskAllInWorldExceptOne(int ClientID)
{
	return CmaskAllInWorld() ^ CmaskOne(ClientID);
}

// TODO: should be more general
CEntity *CGameWorld::IntersectEntity(vec2 Pos0, vec2 Pos1, float Radius, int Type, vec2 &NewPos, CEntity *pNotThis)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CEntity *pClosest = 0;

	CEntity *p = (CEntity *) FindFirst(Type);
	for(; p; p = (CEntity *) p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		float Len = distance(p->m_Pos, IntersectPos);
		if(Len < p->m_ProximityRadius + Radius)
		{
			Len = distance(Pos0, IntersectPos);
			if(Len < ClosestLen)
			{
				NewPos = IntersectPos;
				ClosestLen = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

CEntity *CGameWorld::IntersectEntity(vec2 Pos0, vec2 Pos1, float Radius, EEntityFlag Flag, vec2 &NewPos, CEntity *pNotThis)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CEntity *pClosest = 0;

	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; pEnt = pEnt->TypeNext())
		{
			if(!(pEnt->GetObjFlag() & Flag) || pEnt == pNotThis)
				continue;

			vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, pEnt->m_Pos);
			float Len = distance(pEnt->m_Pos, IntersectPos);
			if(Len < pEnt->m_ProximityRadius + Radius)
			{
				Len = distance(Pos0, IntersectPos);
				if(Len < ClosestLen)
				{
					NewPos = IntersectPos;
					ClosestLen = Len;
					pClosest = pEnt;
				}
			}
		}
	}

	return pClosest;
}

CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis)
{
	// Find other players
	float ClosestRange = Radius * 2;
	CEntity *pClosest = 0;

	CEntity *p = FindFirst(Type);
	for(; p; p = p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius + Radius)
		{
			if(Len < ClosestRange)
			{
				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, EEntityFlag Flag, CEntity *pNotThis)
{
	// Find other players
	float ClosestRange = Radius * 2;
	CEntity *pClosest = 0;

	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; pEnt = pEnt->TypeNext())
		{
			if(!(pEnt->GetObjFlag() & Flag) || pEnt == pNotThis)
				continue;

			float Len = distance(Pos, pEnt->m_Pos);
			if(Len < pEnt->m_ProximityRadius + Radius)
			{
				if(Len < ClosestRange)
				{
					ClosestRange = Len;
					pClosest = pEnt;
				}
			}
		}
	}

	return pClosest;
}

void CGameWorld::TriggerDarkMode()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->TriggerDarkMode();
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::TriggerDarkModeOver()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->TriggerDarkModeOver();
			pEnt = m_pNextTraverseEntity;
		}
}
