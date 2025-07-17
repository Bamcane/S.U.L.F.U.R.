/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include "player.h"

CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius, EEntityFlag ObjFlag)
{
	m_pGameWorld = pGameWorld;

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;

	m_ID = Server()->SnapNewID();
	m_ObjType = ObjType;

	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_Pos = Pos;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
}

int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	return ::NetworkClipped(SnappingClient, CheckPos, GameServer());
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	int rx = round_to_int(CheckPos.x) / 32;
	int ry = round_to_int(CheckPos.y) / 32;
	return (rx < -200 || rx >= GameWorld()->Collision()->GetWidth() + 200) || (ry < -200 || ry >= GameWorld()->Collision()->GetHeight() + 200);
}

template<class IBaseEntity>
COwnerEntity<IBaseEntity>::COwnerEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, int ProximityRadius) :
	IBaseEntity(pGameWorld, Objtype, Pos, ProximityRadius)
{
	m_Owner = -1;
	IBaseEntity::m_ObjFlag = EEntityFlag::ENTFLAG_OWNER | IBaseEntity::m_ObjFlag;
}

template<class IBaseEntity>
CDamageEntity<IBaseEntity>::CDamageEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius) :
	IBaseEntity(pGameWorld, ObjType, Pos, ProximityRadius)
{
	m_Health = 0;
	m_Armor = 0;
	m_MaxHealth = 0;
	m_MaxArmor = 0;
	IBaseEntity::m_ObjFlag = EEntityFlag::ENTFLAG_DAMAGE | IBaseEntity::m_ObjFlag;
}

template<class IBaseEntity>
CDamageEntity<IBaseEntity>::~CDamageEntity()
{
}

template<class IBaseEntity>
void CDamageEntity<IBaseEntity>::Die(CEntity *pKiller, int Weapon)
{
	m_Alive = false;
	(IBaseEntity::GameWorld())->DestroyEntity(this);
}

template<class IBaseEntity>
bool CDamageEntity<IBaseEntity>::IsFriendlyDamage(CEntity *pFrom)
{
	if(!pFrom)
		return false;
	return true;
}

template<class IBaseEntity>
bool CDamageEntity<IBaseEntity>::TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon)
{
	if(IsFriendlyDamage(pFrom))
		return false;

	if(pFrom == this)
		Dmg = maximum(1, Dmg / 2);

	if(Dmg)
	{
		if(m_Armor)
		{
			if(Dmg > 1)
			{
				m_Health--;
				Dmg--;
			}

			if(Dmg > m_Armor)
			{
				Dmg -= m_Armor;
				m_Armor = 0;
			}
			else
			{
				m_Armor -= Dmg;
				Dmg = 0;
			}
		}

		m_Health -= Dmg;
	}

	// check for death
	if(m_Health <= 0)
	{
		Die(pFrom, Weapon);
		return false;
	}
	return true;
}

template class CDamageEntity<CEntity>;
template class COwnerEntity<CEntity>;
template class CDamageEntity<COwnerEntity<CEntity>>;
