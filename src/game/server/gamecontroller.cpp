/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <generated/server_data.h>

#include <game/mapitems.h>
#include <game/version.h>

#include "botmanager.h"
#include "entities/botentity.h"
#include "entities/character.h"
#include "entities/pickup.h"
#include "entities/portal.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"

CGameController::CGameController(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();

	// game
	m_RealPlayerNum = 0;
	m_GameStartTick = Server()->Tick();

	// info
	m_GameFlags = 0;
	m_pGameType = MOD_NAME;
}

// activity
void CGameController::DoActivityCheck()
{
	if(Config()->m_SvInactiveKickTime == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->IsDummy() && (GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS || Config()->m_SvInactiveKick > 0) &&
			!Server()->IsAuthed(i) && (GameServer()->m_apPlayers[i]->m_InactivityTickCounter > Config()->m_SvInactiveKickTime * Server()->TickSpeed() * 60))
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
			{
				if(Config()->m_SvInactiveKickSpec)
					Server()->Kick(i, "Kicked for inactivity");
			}
			else
			{
				switch(Config()->m_SvInactiveKick)
				{
				case 1:
				{
					// move player to spectator
					DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 2:
				{
					/*
					// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
					int Spectators = 0;
					for(int j = 0; j < MAX_CLIENTS; ++j)
						if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == TEAM_SPECTATORS)
							++Spectators;
					if(Spectators >= Config()->m_SvMaxClients)
						Server()->Kick(i, "Kicked for inactivity");
					else
					*/
					DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 3:
				{
					// kick the player
					Server()->Kick(i, "Kicked for inactivity");
				}
				}
			}
		}
	}
}

bool CGameController::GetPlayersReadyState(int WithoutID)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == WithoutID)
			continue; // skip
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !GameServer()->m_apPlayers[i]->m_IsReadyToPlay)
			return false;
	}

	return true;
}

void CGameController::SetPlayersReadyState(bool ReadyState)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && (ReadyState || !GameServer()->m_apPlayers[i]->m_DeadSpecMode))
			GameServer()->m_apPlayers[i]->m_IsReadyToPlay = ReadyState;
	}
}

// event
int CGameController::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;

	if(Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3.0f;

	// update spectator modes for dead players in survival
	if(m_GameFlags & GAMEFLAG_SURVIVAL)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_DeadSpecMode)
				GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
	}

	return 0;
}

void CGameController::OnCharacterSpawn(CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
}

void CGameController::OnFlagReturn(CFlag *pFlag)
{
}

bool CGameController::OnEntity(CGameWorld *pGameWorld, int Index, vec2 Pos)
{
	// don't add pickups in survival
	if(m_GameFlags & GAMEFLAG_SURVIVAL)
	{
		if(Index < ENTITY_SPAWN || Index > ENTITY_SPAWN_BLUE)
			return false;
	}

	int Type = -1;

	switch(Index)
	{
	case ENTITY_SPAWN: // Player
		pGameWorld->m_aaSpawnPoints[0][pGameWorld->m_aNumSpawnPoints[0]++] = Pos;
		break;
	case ENTITY_SPAWN_RED: // NPC1
		pGameWorld->m_aaSpawnPoints[1][pGameWorld->m_aNumSpawnPoints[1]++] = Pos;
		break;
	case ENTITY_SPAWN_BLUE:
		pGameWorld->m_aaSpawnPoints[2][pGameWorld->m_aNumSpawnPoints[2]++] = Pos;
		break;
	case ENTITY_ARMOR_1:
		Type = PICKUP_ARMOR;
		break;
	case ENTITY_HEALTH_1:
		Type = PICKUP_HEALTH;
		break;
	case ENTITY_WEAPON_SHOTGUN:
		Type = PICKUP_SHOTGUN;
		break;
	case ENTITY_WEAPON_GRENADE:
		Type = PICKUP_GRENADE;
		break;
	case ENTITY_WEAPON_LASER:
		Type = PICKUP_LASER;
		break;
	case ENTITY_POWERUP_NINJA:
		Type = PICKUP_NINJA;
		break;
	case ENTITY_PORT_PORTAL:
		new CPortal(pGameWorld, Pos);
		return true;
	}

	if(Type != -1)
	{
		new CPickup(pGameWorld, Type, Pos);
		return true;
	}

	return false;
}

void CGameController::OnPlayerConnect(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();
	pPlayer->Respawn();

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam());
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// update game info
	SendGameInfo(ClientID);
}

void CGameController::OnPlayerDisconnect(CPlayer *pPlayer)
{
	pPlayer->OnDisconnect();

	int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
		m_RealPlayerNum--;
}

void CGameController::OnPlayerInfoChange(CPlayer *pPlayer)
{
}

void CGameController::OnPlayerReadyChange(CPlayer *pPlayer)
{
	if(Config()->m_SvPlayerReadyMode && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->m_DeadSpecMode)
	{
		// change players ready state
		pPlayer->m_IsReadyToPlay ^= 1;
	}
}

void CGameController::OnPlayerSendEmoticon(int Emoticon, CPlayer *pPlayer)
{
}

void CGameController::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
			GameServer()->m_apPlayers[i]->Respawn();
			GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
			GameServer()->m_apPlayers[i]->m_IsReadyToPlay = true;
		}
	}
}

// game
void CGameController::ResetGame()
{
	m_GameStartTick = Server()->Tick();
}

// general
void CGameController::Snap(int SnappingClient)
{
	CNetObj_GameData *pGameData = static_cast<CNetObj_GameData *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData)));
	if(!pGameData)
		return;

	pGameData->m_GameStartTick = m_GameStartTick;
	pGameData->m_GameStateFlags = 0;
	pGameData->m_GameStateEndTick = 0; // no timer/infinite = 0, on end = GameEndTick, otherwise = GameStateEndTick

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_GameInfo *pGameInfo = static_cast<CNetObj_De_GameInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_GAMEINFO, 0, sizeof(CNetObj_De_GameInfo)));
		if(!pGameInfo)
			return;

		pGameInfo->m_GameFlags = m_GameFlags;
		pGameInfo->m_ScoreLimit = 0;
		pGameInfo->m_TimeLimit = 0;
		pGameInfo->m_MatchNum = 0;
		pGameInfo->m_MatchCurrent = 0;
	}
}

void CGameController::Tick()
{
	// check for inactive players
	DoActivityCheck();

	GameServer()->BotManager()->Tick();
	// do special loop
	for(auto &pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;
		if(!pPlayer->GetCharacter())
			continue;
		CCharacter *pChr = pPlayer->GetCharacter();
		if(pChr->GetPos().x < -1200.f)
		{
			float x = -pChr->GetPos().x + pChr->GameWorld()->Collision()->GetWidth() * 32.f;
			float y = pChr->GetPos().y;

			GameServer()->CreatePlayerSpawn(pChr->GetPos(), pChr->GameWorld()->CmaskAllInWorld());
			pChr->SetPos(vec2(x, y));
			GameServer()->CreatePlayerSpawn(pChr->GetPos(), pChr->GameWorld()->CmaskAllInWorld());
		}
		else if(pChr->GetPos().x > pChr->GameWorld()->Collision()->GetWidth() * 32.f + 1200.f)
		{
			float x = -(pChr->GetPos().x - pChr->GameWorld()->Collision()->GetWidth() * 32.f);
			float y = pChr->GetPos().y;

			GameServer()->CreatePlayerSpawn(pChr->GetPos(), pChr->GameWorld()->CmaskAllInWorld());
			pChr->SetPos(vec2(x, y));
			GameServer()->CreatePlayerSpawn(pChr->GetPos(), pChr->GameWorld()->CmaskAllInWorld());
		}
	}

	if((Server()->Tick() - m_GameStartTick) % (1200 * Server()->TickSpeed()) == (610 * Server()->TickSpeed()))
	{
		for(auto &[Uuid, pWorld] : GameServer()->m_upWorlds)
		{
			pWorld->TriggerDarkMode();
		}
		GameServer()->SendChatTarget(-1, "⚠ Experiencing severe signal interference ⚠");
		GameServer()->SendChatTarget(-1, "⚠ Anomaly alert activated ⚠");
		GameServer()->SendChatTarget(-1, "S.U.L.F.U.R. will protect your safety.");
	}
	else if((Server()->Tick() - m_GameStartTick) % (1200 * Server()->TickSpeed()) == 0)
	{
		for(auto &[Uuid, pWorld] : GameServer()->m_upWorlds)
		{
			pWorld->TriggerDarkModeOver();
		}
	}
}

// info
bool CGameController::IsFriendlyFire(int ClientID1, int ClientID2) const
{
	if(ClientID1 == ClientID2)
		return false;

	if(IsTeamplay())
	{
		if(!GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
			return false;

		if(!Config()->m_SvTeamdamage && GameServer()->m_apPlayers[ClientID1]->GetTeam() == GameServer()->m_apPlayers[ClientID2]->GetTeam())
			return true;
	}

	return false;
}

bool CGameController::IsFriendlyTeamFire(int Team1, int Team2) const
{
	return IsTeamplay() && !Config()->m_SvTeamdamage && Team1 == Team2;
}

bool CGameController::IsPlayerReadyMode() const
{
	return Config()->m_SvPlayerReadyMode != 0;
}

bool CGameController::IsTeamChangeAllowed() const
{
	return true;
}

bool CGameController::OnPlayerChat(int ClientID, const char *pMessage, char *pBuffer, int BufferSize)
{
	if(IsInDarkMode())
	{
		// special censor
		int Length = 0;
		const char *p = pMessage;
		*pBuffer = 0;
		while(*p)
		{
			int Code = str_utf8_decode(&p);

			static const int s_aRandomCharacter[6] = {'*', '&', '.', '#', '%', '^'};
			if(random_int() % 100 > 47)
			{
				char aEncoded[4];
				int Size = str_utf8_encode(aEncoded, s_aRandomCharacter[random_int() % 6]);
				aEncoded[Size] = 0;
				str_append(pBuffer, aEncoded, BufferSize);
				Length += Size;
			}
			else
			{
				char aEncoded[4];
				int Size = str_utf8_encode(aEncoded, Code);
				aEncoded[Size] = 0;
				str_append(pBuffer, aEncoded, BufferSize);
				Length += Size;
			}

			if(++Length >= BufferSize)
			{
				break;
			}
		}
	}
	else
		str_copy(pBuffer, pMessage, BufferSize);
	return true;
}

void CGameController::OnPlayerTeleport(int ClientID, const char *pString)
{
	if(str_comp(pString, "Void") == 0)
		return;
	GameServer()->BotManager()->m_pOldTee->TriggerGo(ClientID, pString);
}

void CGameController::OnPlayerDeathWhenDarkMode(int ClientID)
{
	int Time = 1200 - round_to_int((Server()->Tick() - m_GameStartTick) % (1200 * Server()->TickSpeed()) / Server()->TickSpeed());
	// S.U.L.F.U.R. Scientific Unconventional Laboratory Field Unit Response 科学非常规现象调查局
	char aBanMsg[128];
	str_format(aBanMsg, sizeof(aBanMsg), "S.U.L.F.U.R. protected your safety.\nPlease re-execute the survey task after %d seconds", Time);
	Server()->DoSpecialBan(ClientID, Time, aBanMsg);
}

void CGameController::OnPlayerSwitchMap(int ClientID, Uuid OldMapID, Uuid NewMapID)
{
	if(NewMapID == CalculateUuid("Void"))
	{
		char aMsg[128];
		str_format(aMsg, sizeof(aMsg), "⚠ An investigator has lost in '%s'", Server()->GetMapName(OldMapID));
		GameServer()->SendChatTarget(-1, aMsg);
	}
}

void CGameController::SendGameInfo(int ClientID)
{
	CNetMsg_Sv_GameInfo GameInfoMsg;
	GameInfoMsg.m_GameFlags = m_GameFlags;
	GameInfoMsg.m_ScoreLimit = 0;
	GameInfoMsg.m_TimeLimit = 0;
	GameInfoMsg.m_MatchNum = 0;
	GameInfoMsg.m_MatchCurrent = 0;

	CNetMsg_Sv_GameInfo GameInfoMsgNoRace = GameInfoMsg;
	GameInfoMsgNoRace.m_GameFlags &= ~GAMEFLAG_RACE;

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(!GameServer()->m_apPlayers[i] || !Server()->ClientIngame(i))
				continue;

			CNetMsg_Sv_GameInfo *pInfoMsg = (Server()->GetClientVersion(i) < CGameContext::MIN_RACE_CLIENTVERSION) ? &GameInfoMsgNoRace : &GameInfoMsg;
			Server()->SendPackMsg(pInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		CNetMsg_Sv_GameInfo *pInfoMsg = (Server()->GetClientVersion(ClientID) < CGameContext::MIN_RACE_CLIENTVERSION) ? &GameInfoMsgNoRace : &GameInfoMsg;
		Server()->SendPackMsg(pInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}

// spawn
bool CGameController::CanSpawn(class CGameWorld *pWorld, int Type, vec2 *pOutPos) const
{
	CSpawnEval Eval;
	Eval.m_RandomSpawn = true;
	Eval.m_FriendlyTeam = 0;
	Eval.m_pWorld = pWorld;

	// first try own team spawn, then normal spawn and then enemy
	EvaluateSpawnType(&Eval, Type);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

float CGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const
{
	float Score = 0.0f;
	CCharacter *pC = (CCharacter *) pEval->m_pWorld->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pC; pC = (CCharacter *) pC->TypeNext())
	{
		// team mates are not as dangerous as enemies
		float Scoremod = 1.0f;
		if(pEval->m_FriendlyTeam != -1 && pC->GetPlayer()->GetTeam() == pEval->m_FriendlyTeam)
			Scoremod = 0.5f;

		float d = distance(Pos, pC->GetPos());
		Score += Scoremod * (d == 0 ? 1000000000.0f : 1.0f / d);
	}

	return Score;
}

void CGameController::EvaluateSpawnType(CSpawnEval *pEval, int Type) const
{
	// get spawn point
	for(unsigned i = 0; i < pEval->m_pWorld->m_aNumSpawnPoints[Type]; i++)
	{
		// check if the position is occupado
		CBaseDamageEntity *apEnts[MAX_CHECK_ENTITY];
		int Num = pEval->m_pWorld->FindEntities(pEval->m_pWorld->m_aaSpawnPoints[Type][i], 64, (CEntity **) apEnts, MAX_CHECK_ENTITY, EEntityFlag::ENTFLAG_DAMAGE);
		vec2 Positions[5] = {vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f)}; // start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			for(int c = 0; c < Num; ++c)
				if(pEval->m_pWorld->Collision()->CheckPoint(pEval->m_pWorld->m_aaSpawnPoints[Type][i] + Positions[Index]) ||
					distance(apEnts[c]->GetPos(), pEval->m_pWorld->m_aaSpawnPoints[Type][i] + Positions[Index]) <= apEnts[c]->GetProximityRadius())
				{
					Result = -1;
					break;
				}
		}
		if(Result == -1)
			continue; // try next spawn point

		vec2 P = pEval->m_pWorld->m_aaSpawnPoints[Type][i] + Positions[Result];
		float S = pEval->m_RandomSpawn ? (Result + random_float()) : EvaluateSpawnPos(pEval, P);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

bool CGameController::GetStartRespawnState() const
{
	return false;
}

// team
bool CGameController::CanChangeTeam(CPlayer *pPlayer, int JoinTeam) const
{
	return true;
}

bool CGameController::CanJoinTeam(int Team, int NotThisID) const
{
	return true;
}

int CGameController::ClampTeam(int Team) const
{
	if(Team < TEAM_RED)
		return TEAM_SPECTATORS;
	if(IsTeamplay())
		return Team & 1;
	return TEAM_RED;
}

void CGameController::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	int OldTeam = pPlayer->GetTeam();
	pPlayer->SetTeam(Team);

	int ClientID = pPlayer->GetCID();

	// notify clients
	CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Team = Team;
	Msg.m_Silent = DoChatMsg ? 0 : 1;
	Msg.m_CooldownTick = pPlayer->m_TeamChangeTick;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d->%d", ClientID, Server()->ClientName(ClientID), OldTeam, Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// update effected game settings
	if(OldTeam != TEAM_SPECTATORS)
	{
		m_RealPlayerNum--;
	}
	if(Team != TEAM_SPECTATORS)
	{
		m_RealPlayerNum++;
		pPlayer->m_IsReadyToPlay = !IsPlayerReadyMode();
		pPlayer->m_RespawnDisabled = GetStartRespawnState();
	}
	OnPlayerInfoChange(pPlayer);
	GameServer()->OnClientTeamChange(ClientID);

	// reset inactivity counter when joining the game
	if(OldTeam == TEAM_SPECTATORS)
		pPlayer->m_InactivityTickCounter = 0;
}

int CGameController::GetStartTeam()
{
	if(Config()->m_SvTournamentMode)
		return TEAM_SPECTATORS;

	// determine new team
	int Team = TEAM_RED;
	return Team;
}

int CGameController::GetWeaponDamage(int WeaponID, Uuid WorldID)
{
	if(WorldID == Server()->GetBaseMapUuid() && !IsInDarkMode())
		return 0;
	switch(WeaponID)
	{
	case WEAPON_HAMMER: return g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage;
	case WEAPON_GUN: return g_pData->m_Weapons.m_Gun.m_pBase->m_Damage;
	case WEAPON_SHOTGUN: return g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage;
	case WEAPON_GRENADE: return g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage;
	case WEAPON_LASER: return g_pData->m_Weapons.m_Laser.m_pBase->m_Damage;
	case WEAPON_NINJA: return g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage;
	}
	return 0;
}

int CGameController::GetWeaponDamage(int WeaponID, CGameWorld *pWorld)
{
	return GetWeaponDamage(WeaponID, pWorld->m_WorldUuid);
}

bool CGameController::IsInDarkMode() const
{
	return ((Server()->Tick() - m_GameStartTick) % (1200 * Server()->TickSpeed()) >= (610 * Server()->TickSpeed())) && ((Server()->Tick() - m_GameStartTick) % (1200 * Server()->TickSpeed()) <= (1200 * Server()->TickSpeed()));
}

void CGameController::RegisterChatCommands(CCommandManager *pManager)
{
}
