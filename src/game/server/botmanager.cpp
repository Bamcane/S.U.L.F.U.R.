#include <engine/shared/config.h>

#include <game/server/entities/botentity.h>
#include <game/server/entities/oldtee.h>

#include "gamecontext.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"

#include "botmanager.h"

#include <algorithm>

CConfig *CBotManager::Config() const { return GameServer()->Config(); }
CGameContext *CBotManager::GameServer() const { return m_pGameServer; }
CGameController *CBotManager::GameController() const { return GameServer()->GameController(); }
IServer *CBotManager::Server() const { return GameServer()->Server(); }
CWorldCore *CBotManager::BotWorldCore() const { return m_pWorldCore; }

CBotManager::CBotManager(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pWorldCore = new CWorldCore();
	m_OldTeeSpawned = false;

	m_vMarkedAsDestroy.clear();
	m_vpBots.clear();
	ClearPlayerMap(-1);
}

CBotManager::~CBotManager()
{
	delete m_pWorldCore;
	m_pWorldCore = nullptr;
}

void CBotManager::ClearPlayerMap(int ClientID)
{
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			ClearPlayerMap(i);
		}
		return;
	}

	for(int i = 0; i < MAX_BOTS; i++)
	{
		m_aaBotIDMaps[ClientID][i] = UUID_ZEROED;
	}
}

bool DistanceCompare(std::pair<float, Uuid> a, std::pair<float, Uuid> b)
{
	return (a.first < b.first);
}

void CBotManager::UpdatePlayerMap(int ClientID)
{
	Uuid aLastMap[MAX_BOTS];
	mem_copy(aLastMap, m_aaBotIDMaps[ClientID], sizeof(m_aaBotIDMaps[ClientID]));

	Uuid *pMap = m_aaBotIDMaps[ClientID];
	for(int i = 0; i < MAX_BOTS; i++)
	{
		if(pMap[i] != UUID_ZEROED && !m_vpBots.count(pMap[i]))
			pMap[i] = UUID_ZEROED;
	}

	std::vector<std::pair<float, Uuid>> Distances;
	for(auto &[BotID, pBot] : m_vpBots)
	{
		if(!pBot)
			continue;
		if(pBot->GameWorld() != GameServer()->m_apPlayers[ClientID]->GameWorld())
			continue;

		std::pair<float, Uuid> Temp;
		Temp.first = distance(GameServer()->m_apPlayers[ClientID]->m_ViewPos, pBot->GetPos());
		Temp.second = BotID;

		Distances.push_back(Temp);
	}
	std::sort(Distances.begin(), Distances.end(), DistanceCompare);

	for(int i = 0; i < MAX_BOTS; i++)
	{
		if(pMap[i] == UUID_ZEROED)
			continue;

		bool Found = false;
		int FoundID;

		for(FoundID = 0; FoundID < minimum((int) Distances.size(), (int) MAX_BOTS); FoundID++)
		{
			if(Distances[FoundID].second == pMap[i])
			{
				Found = true;
				break;
			}
		}

		if(Found)
		{
			Distances.erase(Distances.begin() + FoundID);
		}
		else
		{
			pMap[i] = UUID_ZEROED;
		}
	}

	int Index = 0;
	for(int i = 0; i < MAX_BOTS; i++)
	{
		if(Index >= (int) Distances.size())
			break;

		if(pMap[i] == UUID_ZEROED)
		{
			pMap[i] = Distances[Index].second;
			Index++;
		}
	}

	// skip self
	for(int i = 0; i < MAX_BOTS; i++)
	{
		if(aLastMap[i] != pMap[i])
		{
			// the id is removed, we need to send drop message
			bool SendDrop = pMap[i] != UUID_ZEROED;
			if(!SendDrop || aLastMap[i] != UUID_ZEROED)
			{
				CNetMsg_Sv_ClientDrop DropInfo;
				DropInfo.m_ClientID = i + MAX_CLIENTS;
				DropInfo.m_pReason = "";
				DropInfo.m_Silent = true;

				Server()->SendPackMsg(&DropInfo, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);

				if(!SendDrop)
					continue;
			}

			if(!m_vpBots.count(pMap[i]))
				continue;
			CBotEntity *pBot = m_vpBots[pMap[i]];

			CNetMsg_Sv_ClientInfo NewInfo;
			NewInfo.m_ClientID = i + MAX_CLIENTS;
			NewInfo.m_Local = 0;
			// do not show bot
			NewInfo.m_Team = TEAM_BLUE;

			NewInfo.m_pName = pBot->GetName();
			NewInfo.m_pClan = "";
			NewInfo.m_Country = -1;
			NewInfo.m_Silent = true;

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				NewInfo.m_apSkinPartNames[p] = pBot->GetTeeInfos()->m_aaSkinPartNames[p];
				NewInfo.m_aUseCustomColors[p] = pBot->GetTeeInfos()->m_aUseCustomColors[p];
				NewInfo.m_aSkinPartColors[p] = pBot->GetTeeInfos()->m_aSkinPartColors[p];
			}

			Server()->SendPackMsg(&NewInfo, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}
}

bool CBotManager::CreateBot(CGameWorld *pWorld, bool OldTee)
{
	// find first free bot id
	Uuid FreeID = RandomUuid();
	for(; m_vpBots.count(FreeID); FreeID = RandomUuid()) {}

	vec2 SpawnPos;
	if(!GameServer()->GameController()->CanSpawn(pWorld, OldTee ? 1 : 2, &SpawnPos))
		return false;

	CBotEntity *pBot = OldTee ? new COldTee(pWorld, SpawnPos, FreeID, GenerateRandomSkin()) : new CBotEntity(pWorld, SpawnPos, FreeID, GenerateRandomSkin());
	pBot->SetMaxHealth(10);
	pBot->SetMaxArmor(10);
	pBot->IncreaseHealth(10);
	pBot->IncreaseArmor(5);
	m_vpBots[FreeID] = pBot;

	if(OldTee)
		m_OldTeeSpawned = true;
	return true;
}

void CBotManager::Tick()
{
	for(auto &[WorldID, pWorld] : GameServer()->m_upWorlds)
	{
		if(pWorld->m_aNumSpawnPoints[1] && !m_OldTeeSpawned)
			CreateBot(pWorld, true);

		while(m_vpBots.size() < pWorld->m_aNumSpawnPoints[2])
		{
			if(!CreateBot(pWorld))
				break;
		}
	}
}

void CBotManager::SendEmoticon(int Emoticon, Uuid BotID)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int ClientID = FindClientID(i, BotID);
		if(ClientID == -1)
			continue;

		CNetMsg_Sv_Emoticon Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Emoticon = Emoticon;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
}

void CBotManager::SendChat(int To, const char *pChat, Uuid BotID)
{
	if(To == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			int ClientID = FindClientID(i, BotID);
			if(ClientID == -1)
				continue;
			CNetMsg_Sv_Chat Msg;
			Msg.m_Mode = CHAT_ALL;
			Msg.m_ClientID = ClientID;
			Msg.m_pMessage = pChat;
			Msg.m_TargetID = -1;

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		int ClientID = FindClientID(To, BotID);
		if(ClientID == -1)
			return;
		CNetMsg_Sv_Chat Msg;
		Msg.m_Mode = CHAT_ALL;
		Msg.m_ClientID = ClientID;
		Msg.m_pMessage = pChat;
		Msg.m_TargetID = -1;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
}

void CBotManager::CreateDamage(vec2 Pos, Uuid BotID, vec2 Source, int HealthAmount, int ArmorAmount, bool Self)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int ClientID = FindClientID(i, BotID);
		if(ClientID == -1)
			continue;

		GameServer()->CreateDamage(Pos, ClientID, Source, HealthAmount, ArmorAmount, Self, CmaskOne(i));
	}
}

void CBotManager::CreateDeath(vec2 Pos, Uuid BotID)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int ClientID = FindClientID(i, BotID);
		if(ClientID == -1)
			continue;

		GameServer()->CreateDeath(Pos, ClientID, CmaskOne(i));
	}
}

int CBotManager::FindClientID(int ClientID, Uuid BotID)
{
	dbg_assert(ClientID >= 0, "Server demo is hard-coded disabled now.");

	int FindID = -1 - MAX_CLIENTS;
	for(int i = 0; i < MAX_BOTS; i++)
	{
		if(m_aaBotIDMaps[ClientID][i] == BotID)
		{
			FindID = i;
			break;
		}
	}
	return FindID + MAX_CLIENTS;
}

void CBotManager::OnBotDeath(Uuid BotID)
{
	m_vMarkedAsDestroy.push_back(BotID);
	m_vpBots[BotID] = nullptr;
}

void CBotManager::OnClientRefresh(int ClientID)
{
	if(ClientID == -1)
		return;

	ClearPlayerMap(ClientID);
}

void CBotManager::PostSnap()
{
	if(m_vMarkedAsDestroy.size())
	{
		for(auto &DestroyID : m_vMarkedAsDestroy)
		{
			m_vpBots.erase(DestroyID);
		}
		m_vMarkedAsDestroy.clear();
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Server()->ClientIngame(i) && GameServer()->m_apPlayers[i])
			UpdatePlayerMap(i);
	}
}
