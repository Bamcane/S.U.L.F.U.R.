/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEWORLD_H
#define GAME_SERVER_GAMEWORLD_H

#include <base/uuid.h>
#include <game/collision.h>
#include <game/gamecore.h>

#define MAX_CHECK_ENTITY 128

enum class EEntityFlag : int
{
	ENTFLAG_NONE = 0,
	ENTFLAG_OWNER = 1 << 0,
	ENTFLAG_DAMAGE = 1 << 1,
};

inline EEntityFlag operator|(const EEntityFlag &A, const EEntityFlag &B)
{
	return static_cast<EEntityFlag>(static_cast<int>(A) | static_cast<int>(B));
}

inline bool operator&(const EEntityFlag &A, const EEntityFlag &B)
{
	return static_cast<int>(A) & static_cast<int>(B);
}

class CEntity;

/*
	Class: Game World
		Tracks all entities in the game. Propagates tick and
		snap calls to all entities.
*/
class CGameWorld
{
public:
	enum
	{
		ENTTYPE_PROJECTILE = 0,
		ENTTYPE_LASER,
		ENTTYPE_PICKUP,
		ENTTYPE_CHARACTER,
		ENTTYPE_FLAG,
		ENTTYPE_BOTENTITY,
		ENTTYPE_PORTAL,
		NUM_ENTTYPES
	};

private:
	void Reset();
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	CCollision m_Collision;

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }
	CCollision *Collision() { return &m_Collision; }

	bool m_ResetRequested;
	bool m_Paused;
	CWorldCore m_Core;

	CGameWorld();
	~CGameWorld();

	vec2 m_aaSpawnPoints[3][128];
	unsigned m_aNumSpawnPoints[3];
	Uuid m_WorldUuid;

	void SetGameServer(CGameContext *pGameServer);

	CEntity *FindFirst(int Type);

	/*
		Function: find_entities
			Finds entities close to a position and returns them in a list.

		Arguments:
			pos - Position.
			radius - How close the entities have to be.
			ents - Pointer to a list that should be filled with the pointers
				to the entities.
			max - Number of entities that fits into the ents array.
			type - Type of the entities to find.

		Returns:
			Number of entities found and added to the ents array.
	*/
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, EEntityFlag Flag);

	/*
		Function: closest_CEntity
			Finds the closest CEntity of a type to a specific point.

		Arguments:
			pos - The center position.
			radius - How far off the CEntity is allowed to be
			type - Type of the entities to find.
			notthis - Entity to ignore

		Returns:
			Returns a pointer to the closest CEntity or NULL if no CEntity is close enough.
	*/
	CEntity *ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis);
	CEntity *ClosestEntity(vec2 Pos, float Radius, EEntityFlag Flag, CEntity *pNotThis);

	/*
		Function: interserct_CCharacter
			Finds the closest CCharacter that intersects the line.

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be.
			new_pos - Intersection position
			notthis - Entity to ignore intersecting with

		Returns:
			Returns a pointer to the closest hit or NULL of there is no intersection.
	*/
	CEntity *IntersectEntity(vec2 Pos0, vec2 Pos1, float Radius, int Type, vec2 &NewPos, class CEntity *pNotThis = 0);
	CEntity *IntersectEntity(vec2 Pos0, vec2 Pos1, float Radius, EEntityFlag Flag, vec2 &NewPos, class CEntity *pNotThis = 0);

	/*
		Function: insert_entity
			Adds an entity to the world.

		Arguments:
			entity - Entity to add
	*/
	void InsertEntity(CEntity *pEntity);

	/*
		Function: remove_entity
			Removes an entity from the world.

		Arguments:
			entity - Entity to remove
	*/
	void RemoveEntity(CEntity *pEntity);

	/*
		Function: destroy_entity
			Destroys an entity in the world.

		Arguments:
			entity - Entity to destroy
	*/
	void DestroyEntity(CEntity *pEntity);

	/*
		Function: snap
			Calls snap on all the entities in the world to create
			the snapshot.

		Arguments:
			snapping_client - ID of the client which snapshot
			is being created.
	*/
	void Snap(int SnappingClient);

	void PostSnap();

	/*
		Function: tick
			Calls tick on all the entities in the world to progress
			the world to the next tick.

	*/
	void Tick();

	int64 CmaskAllInWorld();
	int64 CmaskAllInWorldExceptOne(int ClientID);

	void TriggerDarkMode();
	void TriggerDarkModeOver();
};

#endif
