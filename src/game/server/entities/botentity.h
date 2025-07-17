#ifndef GAME_SERVER_ENTITIES_BOTENTITY_H
#define GAME_SERVER_ENTITIES_BOTENTITY_H

#include <base/uuid.h>

#include <generated/protocol.h>

#include <game/gamecore.h>
#include <game/server/entity.h>
#include <game/server/teeinfo.h>

class CBotEntity : public CDamageEntity<CEntity>
{
public:
	// same as character's size
	static const int ms_PhysSize = 28;
	CBotEntity(CGameWorld *pWorld, vec2 Pos, Uuid BotID, STeeInfo TeeInfos);

	void Tick() override;
	void TickDefered() override;
	void Snap(int SnappingClient) override;
	void PostSnap() override;

	bool IsFriendlyDamage(CEntity *pFrom) override;
	bool TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon) override;
	void Die(CEntity *pKiller, int Weapon) override;

	Uuid GetBotID() const { return m_BotID; }
	STeeInfo *GetTeeInfos() { return &m_TeeInfos; }
	virtual const char *GetName() { return ""; }

	virtual bool TriggerGo(int ClientID, const char *pGoTo) { return false; }

protected:
	STeeInfo m_TeeInfos;
	Uuid m_BotID;
	int m_Emote;

	int m_TriggeredEvents;
	// the core for the physics
	CCharacterCore m_Core;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	int m_AttackTick;
	int m_ReloadTimer;

	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_PrevInput;

	vec2 m_CursorTarget;

	virtual void RandomAction();
	virtual void TargetAction(CDamageEntity *pTarget);

	virtual void Action();
	virtual void DoWeapon();
};

#endif // GAME_SERVER_ENTITIES_BOTENTITY_H
