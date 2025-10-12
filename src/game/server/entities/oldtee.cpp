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
	m_TeeInfos = m_DarkInfo;

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
		if(random_int() % 100 < 12)
		{
			GameServer()->BotManager()->SendChat(ClientID, "语言翻译已生效是么？", GetBotID());
			GameServer()->BotManager()->SendChat(ClientID, "那真是太好了。", GetBotID());
		}
		else if(random_int() % 100 < 12)
		{
			GameServer()->BotManager()->SendChat(ClientID, "我在想", GetBotID());
			GameServer()->BotManager()->SendChat(ClientID, "我们是不是做错了什么。", GetBotID());
		}
		else if(random_int() % 100 < 12)
		{
			GameServer()->BotManager()->SendChat(ClientID, "我的职位?", GetBotID());
			GameServer()->BotManager()->SendChat(ClientID, "我是电解协议来给S.U.L.F.U.R.做指挥的。", GetBotID());
		}
		else if(random_int() % 100 < 12)
		{
			GameServer()->BotManager()->SendChat(ClientID, "电解协议是什么?", GetBotID());
			GameServer()->BotManager()->SendChat(ClientID, "嗯...说来话长", GetBotID());
		}
		else if(random_int() % 100 < 12 && m_DarkMode)
		{
			GameServer()->BotManager()->SendChat(ClientID, "或许你现在应该待在这里先。", GetBotID());
		}
		else
		{
			GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "尽管这里有任务要做" : "嗯...", GetBotID());
			GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "但我不建议你现在就去" : "啊，你在这里？", GetBotID());
			GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "最近又失踪了一些调查员" : "我？我在想它究竟是个什么东西呢。", GetBotID());
		}
	}
	if(m_DarkMode)
	{
		m_Input.m_Fire = 1;
	}
	return false;
}

const char *COldTee::GetName()
{
	return "c29z";
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
	str_format(aBuf, sizeof(aBuf), "你想去'%s'...?", pGoTo);
	GameServer()->BotManager()->SendChat(ClientID, aBuf, GetBotID());
	GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "好吧..." : "现在去确实挺好的", GetBotID());
	GameServer()->BotManager()->SendChat(ClientID, m_DarkMode ? "希望你可以回来，但是先别动" : "如果你确定那是你要去的地方就请原地等待3s!", GetBotID());
	GameServer()->m_apPlayers[ClientID]->TeleTo(pGoTo);
	return true;
}

void COldTee::TriggerDarkMode()
{
	m_DarkMode = true;
	GameServer()->BotManager()->RequestRefreshMap(m_BotID);
}

void COldTee::TriggerDarkModeOver()
{
	m_DarkMode = false;
	GameServer()->BotManager()->RequestRefreshMap(m_BotID);
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
			if(random_int() % 2)
			{
				GameServer()->BotManager()->SendChat(-1, m_DarkMode ? "嘶..." : "...", GetBotID());
			}
			else if(random_int() % 2)
			{
				GameServer()->BotManager()->SendChat(-1, "如果你有什么问题就随时来问吧", GetBotID());
			}
			else if(random_int() % 2)
			{
				GameServer()->BotManager()->SendChat(-1, "唉...", GetBotID());
			}
		}
	}
}
