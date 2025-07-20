#include <engine/shared/config.h>

#include <game/server/botmanager.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include <generated/server_data.h>

#include "character.h"
#include "oldtee.h"

// also c29z
COldTee::COldTee(CGameWorld *pWorld, vec2 Pos, Uuid BotID, STeeInfo TeeInfo) :
	CBotEntity(pWorld, Pos, BotID, TeeInfo)
{
	// load teeinfo
	ReadInfoByJson(GameServer()->Storage(), "c29z", m_LightInfo);
	ReadInfoByJson(GameServer()->Storage(), "c29z_dark", m_DarkInfo);

	m_DarkMode = false;
	m_TeeInfos = m_DarkMode ? m_DarkInfo : m_LightInfo;

	m_Emote = EMOTE_NORMAL;
	m_RandomEmoteTimer = random_int() % 500 + 500;
}

bool COldTee::IsFriendlyDamage(CEntity *pFrom)
{
	return true;
}

bool COldTee::TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon)
{
	if(pFrom->GetObjType() == CGameWorld::ENTTYPE_CHARACTER && Weapon == WEAPON_HAMMER)
	{
		int ClientID = ((CCharacter *) pFrom)->GetPlayer()->GetCID();
		GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "Where do YOU want to go?" : "Where do you want to go?", GetBotID());
		GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "If you have decided, YOU CAN tell me." : "If you have decided, just tell me.", GetBotID());
		GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "BUT I WON'T GIVE YOU ANY EXAMPLE." : "For example: /goto FlowerFell-Sans.", GetBotID());
	}
	if(m_DarkMode)
	{
		m_Input.m_Fire = 1;
	}
	return false;
}

const char *COldTee::GetName()
{
	return m_DarkMode ? "c29z" : "Old Tee";
}

void COldTee::Snap(int SnappingClient)
{
	int ClientID = GameServer()->BotManager()->FindClientID(SnappingClient, GetBotID());
	if(ClientID == -1)
	{
		if(NetworkClipped(SnappingClient))
			return;
		CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
		if(!pP)
			return;

		pP->m_X = round_to_int(m_Pos.x);
		pP->m_Y = round_to_int(m_Pos.y);
		pP->m_Type = PICKUP_HAMMER;
		return;
	}

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

	if(GameServer()->GetPlayerChar(SnappingClient))
	{
		vec2 TargetPos = GameServer()->GetPlayerChar(SnappingClient)->GetPos() - GetPos();
		pCharacter->m_Angle = (int) (angle(TargetPos) * 256.0f);
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

bool COldTee::TriggerGo(int ClientID, const char *pGoTo)
{
	if(!GameServer()->GetPlayerChar(ClientID))
		return false;
	if(GameServer()->GetPlayerChar(ClientID)->GameWorld() != GameWorld())
		return false;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), m_DarkMode ? "YOU want to go to '%s'...?" : "You wanna go to '%s'?", pGoTo);
	GameServer()->BotManager()->SendChat(ClientID, aBuf, GetBotID());
	GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "That's a hell..." : "It's quite a good place.", GetBotID());
	GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "SWYgeW91IGRvbid0IHdhbnQgdG8gRElFLCBZT1UgU0hPVUxETidUIE1PVkUu" : "Don't move in 3s if you are sure that is you want to go!", GetBotID());
	GameServer()->m_apPlayers[ClientID]->TeleTo(pGoTo);
	return true;
}

void COldTee::TriggerDarkMode()
{
	m_DarkMode = true;
	m_TeeInfos = m_DarkInfo;
	GameServer()->BotManager()->RequestRefreshMap(GetBotID());
}

void COldTee::TriggerDarkModeOver()
{
	m_DarkMode = false;
	m_TeeInfos = m_LightInfo;
	GameServer()->BotManager()->RequestRefreshMap(GetBotID());
}

void COldTee::Action()
{
	mem_copy(&m_PrevInput, &m_Input, sizeof(m_Input));
	// reset some input
	m_Input.m_Jump = 0;
	m_Input.m_Fire = 0;

	if(m_RandomEmoteTimer)
	{
		m_RandomEmoteTimer--;
		if(!m_RandomEmoteTimer)
		{
			m_RandomEmoteTimer = random_int() % 500 + 500;
			GameServer()->BotManager()->SendEmoticon(random_int() % NUM_EMOTICONS, GetBotID());
			GameServer()->BotManager()->SendChat(-1, m_DarkMode ? "V2h5IGRvIFlPVSBsZWF2ZSBtZSBBTE9ORS4uLi4u" : "Ahh....So strange...", GetBotID());
		}
	}
}
