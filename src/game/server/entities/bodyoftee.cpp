#include <engine/shared/config.h>

#include <game/server/botmanager.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include <generated/server_data.h>

#include "character.h"
#include "bodyoftee.h"

#include <algorithm>

// also c29z
CBodyOfTee::CBodyOfTee(CGameWorld *pWorld, vec2 Pos, Uuid BotID) :
	CBotEntity(pWorld, Pos, BotID, GenerateRandomSkin())
{
	m_Emote = EMOTE_PAIN;
}

bool CBodyOfTee::IsFriendlyDamage(CEntity *pFrom)
{
	return true;
}

bool CBodyOfTee::TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon)
{
	if(pFrom->GetObjType() == CGameWorld::ENTTYPE_CHARACTER && Weapon == WEAPON_HAMMER)
	{
		int ClientID = ((CCharacter *) pFrom)->GetPlayer()->GetCID();
		unsigned char Type = m_BotID.m_aData[0] % 5;
		switch (Type)
		{
			case 0: GameServer()->SendChatTarget(ClientID, (m_BotID.m_aData[1] % 2) ? "她已经死了" : "他已经死了"); break;
			case 1: GameServer()->SendChatTarget(ClientID, "毛骨悚然..."); break;
			case 2: GameServer()->SendChatTarget(ClientID, "为什么..."); break;
			case 3: GameServer()->SendChatTarget(ClientID, (m_BotID.m_aData[1] % 2) ? "谁杀了她?..." : "谁杀了他?..."); break;
			case 4: GameServer()->SendChatTarget(ClientID, "找到了一篇笔记，读它么?(输入/read)");
		}
	}
	return false;
}

void CBodyOfTee::Snap(int SnappingClient)
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

	pCharacter->m_Emote = m_Emote;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;

	pCharacter->m_Weapon = -1;
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

static const char *s_pMessage[] = {
	"S.U.L.F.U.R.\n暂无发现任何异常，除了似乎我看见了一个激光笼子",
	"c29z说我最近有些消极\n呃啊，医院也没什么检查出来的问题\n我也不知道怎么办好\n脑子烧成一团了\n我到底要去哪",
	"这里的地形黑成了一片，似乎是Port的反色版本\n但是这里没有z92c\n不过我好像看见个什么东西...?",
	"E8BF99E79C9FE79A84E69C89E6848FE4B989E4B988",
	"-..----.--..... -..---.---.--.. ---.---...----- ---.--.-....-.. -.......------.- -.-...------.-- -...--.-..-..-.- --...-....-...- -..---..-..-...",
	"That's not useful",
	"Give up",
	"我还不能放弃...c29z博士还在等着...",
	"Password is....ELECTROLYSIS16"
};
void CBodyOfTee::TriggerRead(int ClientID, const char **ppMessage)
{
	if((m_BotID.m_aData[0] % 5) != 4)
		return;
	if(!GameServer()->m_apPlayers[ClientID]->GetCharacter())
		return;
	if(GameWorld() != GameServer()->m_apPlayers[ClientID]->GameWorld())
		return;
	CCharacter *pChr = GameServer()->m_apPlayers[ClientID]->GetCharacter();
	if(distance(pChr->GetPos(), m_Pos) > GetProximityRadius() * 1.75f)
		return;
	unsigned char NoteType = m_BotID.m_aData[2] % std::size(s_pMessage);
	*ppMessage = s_pMessage[NoteType];
}

void CBodyOfTee::Action()
{
	mem_copy(&m_PrevInput, &m_Input, sizeof(m_Input));
	mem_zero(&m_Input, sizeof(m_Input));
}
