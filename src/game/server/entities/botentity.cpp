#include <engine/shared/config.h>

#include <game/server/botmanager.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include <generated/server_data.h>

#include "botentity.h"

CBotEntity::CBotEntity(CGameWorld *pWorld, vec2 Pos, Uuid BotID, STeeInfo TeeInfos) :
	CDamageEntity(pWorld, CGameWorld::ENTTYPE_BOTENTITY, Pos, ms_PhysSize)
{
	m_BotID = BotID;
	m_Emote = EMOTE_ANGRY;
	m_TeeInfos = TeeInfos;

	m_AttackTick = 0;
	m_ReloadTimer = 0;

	m_Core.Reset();
	m_Core.Init(GameServer()->BotManager()->BotWorldCore(), GameWorld()->Collision());
	m_Core.m_Pos = m_Pos;

	m_CursorTarget = vec2(100.f, 0.f);

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	mem_zero(&m_Input, sizeof(m_Input));
	mem_zero(&m_PrevInput, sizeof(m_PrevInput));

	GameServer()->CreatePlayerSpawn(Pos, CmaskAllInWorld());
	GameWorld()->InsertEntity(this);
}

void CBotEntity::Tick()
{
	Action();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	// handle leaving gamelayer
	if(GameLayerClipped(m_Pos))
	{
		Die(this, WEAPON_WORLD);
		return;
	}

	DoWeapon();
}

void CBotEntity::TickDefered()
{
	static const vec2 ColBox(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE);
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameWorld()->Collision());
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	// apply drag velocity
	// and set it back to 0 for the next tick
	m_Core.AddDragVelocity();
	m_Core.ResetDragVelocity();

	// lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameWorld()->Collision()->TestBox(m_Core.m_Pos, ColBox);

	m_Core.Move();

	bool StuckAfterMove = GameWorld()->Collision()->TestBox(m_Core.m_Pos, ColBox);
	m_Core.Quantize();
	bool StuckAfterQuant = GameWorld()->Collision()->TestBox(m_Core.m_Pos, ColBox);
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		} StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	m_TriggeredEvents |= m_Core.m_TriggeredEvents;

	if(m_Core.m_Death)
	{
		// handle death-tiles
		Die(this, WEAPON_WORLD);
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reckoning for a top of 3 seconds
		if(m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CBotEntity::Snap(int SnappingClient)
{
	int ClientID = GameServer()->BotManager()->FindClientID(SnappingClient, GetBotID());
	if(ClientID == -1)
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_PlayerFlags = 0;
	pPlayerInfo->m_Latency = 0;
	pPlayerInfo->m_Score = 0;

	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ClientID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// write down the m_Core
	if(!m_ReckoningTick || GameWorld()->m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	pCharacter->m_Emote = m_Emote;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;

	pCharacter->m_Weapon = WEAPON_HAMMER;
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	if(ClientID == SnappingClient || SnappingClient == -1 ||
		(!Config()->m_SvStrictSpectateMode && ClientID == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()))
	{
		pCharacter->m_Health = clamp(round_to_int(GetHealth() / (float) GetMaxHealth() * 10), 0, 10);
		pCharacter->m_Armor = clamp(round_to_int(GetArmor() / (float) GetMaxArmor() * 10), 0, 10);
		pCharacter->m_AmmoCount = 0;
	}
}

void CBotEntity::PostSnap()
{
	m_TriggeredEvents = 0;
}

void CBotEntity::Die(CEntity *pKiller, int Weapon)
{
	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, CmaskAllInWorld());

	CDamageEntity::Die(pKiller, Weapon);
	GameServer()->BotManager()->CreateDeath(m_Pos, GetBotID());

	if(GameServer()->BotManager())
		GameServer()->BotManager()->OnBotDeath(GetBotID());
}

bool CBotEntity::IsFriendlyDamage(CEntity *pFrom)
{
	if(!pFrom)
		return true;

	return false;
}

bool CBotEntity::TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon)
{
	m_Core.m_Vel += Force;
	int Owner = -1;
	if(pFrom && (pFrom->GetObjFlag() & EEntityFlag::ENTFLAG_OWNER))
		Owner = ((CBaseOwnerEntity *) pFrom)->GetOwner();

	int OldHealth = m_Health, OldArmor = m_Armor;
	bool Return = CDamageEntity::TakeDamage(Force, Source, Dmg, pFrom, Weapon);
	// create healthmod indicator
	GameServer()->BotManager()->CreateDamage(m_Pos, m_BotID, Source, OldHealth - m_Health, OldArmor - m_Armor, pFrom == this);

	// do damage Hit sound
	if(pFrom && pFrom != this && Owner > -1 && GameServer()->m_apPlayers[Owner])
	{
		int64 Mask = CmaskOne(Owner);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
				GameServer()->m_apPlayers[i]->GetSpectatorID() == Owner)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[Owner]->m_ViewPos, SOUND_HIT, Mask);
	}

	if(Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG, CmaskAllInWorld());
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT, CmaskAllInWorld());

	return Return;
}

void CBotEntity::RandomAction()
{
	// random jump
	if(random_int() % 1000 < 25)
		m_Input.m_Jump = 1;

	// random move
	if(random_int() % 1000 < 55)
	{
		m_Input.m_Direction = random_int() % 3 - 1;
		if(m_Input.m_Direction)
			m_CursorTarget = vec2(length(m_CursorTarget) * m_Input.m_Direction, 0.f);
	}
}

void CBotEntity::TargetAction(CDamageEntity *pTarget)
{
	vec2 MoveTo = pTarget->GetPos();
	if(abs(MoveTo.x - m_Pos.x) > 48.f)
		m_Input.m_Direction = (int) sign(MoveTo.x - m_Pos.x);
	else
		m_Input.m_Direction = 0;

	if((MoveTo.y - m_Pos.y < -48.f) && !(m_Core.m_Jumped & 1) && (m_Core.m_Vel.y > -1.25f))
	{
		m_Input.m_Jump = 1;
	}

	if(distance(MoveTo, m_Pos) < 48.f && ((random_int() % 100) < 8) && m_ReloadTimer <= 0)
		m_Input.m_Fire = 1;

	m_CursorTarget = MoveTo - m_Pos;
}

void CBotEntity::Action()
{
	mem_copy(&m_PrevInput, &m_Input, sizeof(m_Input));
	// reset some input
	m_Input.m_Jump = 0;
	m_Input.m_Fire = 0;

	CDamageEntity *pTarget = (CDamageEntity *) GameWorld()->ClosestEntity(GetPos(), 320.f, EEntityFlag::ENTFLAG_DAMAGE, this);
	if(pTarget && GameWorld()->Collision()->IntersectLine(GetPos(), pTarget->GetPos(), nullptr, nullptr))
		pTarget = nullptr;

	if(pTarget)
		TargetAction(pTarget);
	else
		RandomAction();

	// move cursor
	vec2 Target = vec2(m_Input.m_TargetX, m_Input.m_TargetY);
	float MouseSpeed = random_float() * 32.f + 8.f;
	if(distance(Target, m_CursorTarget) > MouseSpeed)
	{
		vec2 Direction = normalize(m_CursorTarget - Target);
		Target += Direction * MouseSpeed;
		Target = normalize(Target) * clamp(length(m_CursorTarget), 0.f, 400.f);
		m_Input.m_TargetX = Target.x;
		m_Input.m_TargetY = Target.y;
	}
}

void CBotEntity::DoWeapon()
{
	if(m_ReloadTimer > 0)
	{
		m_ReloadTimer--;
		return;
	}

	vec2 Direction = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_PrevInput.m_Fire, m_Input.m_Fire).m_Presses)
		WillFire = true;

	if(!WillFire)
		return;

	vec2 ProjStartPos = m_Pos + Direction * GetProximityRadius() * 0.75f;

	GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, CmaskAllInWorld());

	CDamageEntity *apEnts[MAX_CHECK_ENTITY];
	int Hits = 0;
	int Num = GameWorld()->FindEntities(ProjStartPos, GetProximityRadius() * 0.5f, (CEntity **) apEnts,
		MAX_CHECK_ENTITY, EEntityFlag::ENTFLAG_DAMAGE);

	for(int i = 0; i < Num; ++i)
	{
		CDamageEntity *pTarget = apEnts[i];

		if((pTarget == this) || GameWorld()->Collision()->IntersectLine(ProjStartPos, pTarget->GetPos(), NULL, NULL))
			continue;

		// set his velocity to fast upward (for now)
		if(length(pTarget->GetPos() - ProjStartPos) > 0.0f)
			GameServer()->CreateHammerHit(pTarget->GetPos() - normalize(pTarget->GetPos() - ProjStartPos) * GetProximityRadius() * 0.5f, CmaskAllInWorld());
		else
			GameServer()->CreateHammerHit(ProjStartPos, CmaskAllInWorld());

		vec2 Dir;
		if(length(pTarget->GetPos() - GetPos()) > 0.0f)
			Dir = normalize(pTarget->GetPos() - GetPos());
		else
			Dir = vec2(0.f, -1.f);

		pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, Dir * -1, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
			this, WEAPON_HAMMER);
		Hits++;
	}

	// if we Hit anything, we have to wait for the reload
	if(Hits)
		m_ReloadTimer = Server()->TickSpeed() / 3;

	m_AttackTick = Server()->Tick();

	if(!m_ReloadTimer)
		m_ReloadTimer = g_pData->m_Weapons.m_aId[WEAPON_HAMMER].m_Firedelay * Server()->TickSpeed() / 1000;
}
