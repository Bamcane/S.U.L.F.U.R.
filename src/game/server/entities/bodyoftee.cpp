#include <engine/shared/config.h>

#include <game/server/botmanager.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include <generated/server_data.h>

#include "character.h"
#include "bodyoftee.h"

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
			case 0: GameServer()->SendChatTarget(ClientID, (m_BotID.m_aData[1] % 2) ? "She has been dead" : "He has been dead...."); break;
			case 1: GameServer()->SendChatTarget(ClientID, "Quite scary..."); break;
			case 2: GameServer()->SendChatTarget(ClientID, "Why?..."); break;
			case 3: GameServer()->SendChatTarget(ClientID, (m_BotID.m_aData[1] % 2) ? "Who killed her?..." : "Who killed him?..."); break;
			case 4: GameServer()->SendChatTarget(ClientID, "Oh, there is a note, read it (/read)?...");
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
	unsigned char NoteType = m_BotID.m_aData[2] % 9;
	switch (NoteType)
	{
		case 0: *ppMessage = "I saw it.\nIt is attacking my defense shield\nI'm writing this note in hurry.\nIf any investigator read this,\nremember that, S.U.L.F"; break;
		case 1: *ppMessage = "If I could go back,\nI must play the 'InfClass' after work\nSadly, I'm here to face its back.\nOh wait, why that I don't want to play InfClass anymore?"; break;
		case 2: *ppMessage = "I shouldn't go to xX_ajdo_Xx!!!!"; break;
		case 3: *ppMessage = "I had been lost in a dark place now, where am I?"; break;
		case 4: *ppMessage = "Firewall! Firewall! Firewall!\nRemove the Internet and other connections!"; break;
		case 5: *ppMessage = "I'm not fine.\nI can't sense anything."; break;
		case 6: *ppMessage = "c29z said that I was glad to do my work before.\nBut I feel boring now. That's quite weird."; break;
		case 7: *ppMessage = "I guess you have known that, its name is Majd (there are some weird characters on the note)"; break;
		case 8: *ppMessage = "Deflate compression algorithm is great."; break;
	}
}

void CBodyOfTee::Action()
{
	mem_copy(&m_PrevInput, &m_Input, sizeof(m_Input));
	mem_zero(&m_Input, sizeof(m_Input));
}
