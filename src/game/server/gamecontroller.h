/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/tl/array.h>
#include <base/vmath.h>

#include <game/commands.h>

#include <generated/protocol.h>

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class CGameController
{
	friend class CBotManager;

	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	// activity
	void DoActivityCheck();
	bool GetPlayersReadyState(int WithoutID = -1);
	void SetPlayersReadyState(bool ReadyState);

	// balancing
	enum
	{
		TBALANCE_CHECK = -2,
		TBALANCE_OK,
	};
	int m_RealPlayerNum;
	void ResetGame();

	// spawn
	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100, 100);
		}

		class CGameWorld *m_pWorld;
		vec2 m_Pos;
		bool m_Got;
		bool m_RandomSpawn;
		int m_FriendlyTeam;
		float m_Score;
	};

	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const;
	void EvaluateSpawnType(CSpawnEval *pEval, int Type) const;

	// team
	int ClampTeam(int Team) const;

protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() const { return m_pConfig; }
	IServer *Server() const { return m_pServer; }

	// game
	int m_GameStartTick;

	// info
	int m_GameFlags;
	const char *m_pGameType;

	void SendGameInfo(int ClientID);

	array<vec2> m_aSitPoints;

public:
	CGameController(class CGameContext *pGameServer);
	~CGameController() {}

	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	void OnCharacterSpawn(class CCharacter *pChr);
	void OnFlagReturn(class CFlag *pFlag);
	bool OnEntity(class CGameWorld *pGameWorld, int Index, vec2 Pos);

	void OnPlayerConnect(class CPlayer *pPlayer);
	void OnPlayerDisconnect(class CPlayer *pPlayer);
	void OnPlayerInfoChange(class CPlayer *pPlayer);
	void OnPlayerReadyChange(class CPlayer *pPlayer);

	void OnPlayerSendEmoticon(int Emoticon, class CPlayer *pPlayer);

	void OnReset();

	// general
	void Snap(int SnappingClient);
	void Tick();

	// info
	bool IsFriendlyFire(int ClientID1, int ClientID2) const;
	bool IsFriendlyTeamFire(int Team1, int Team2) const;
	bool IsPlayerReadyMode() const;
	bool IsTeamChangeAllowed() const;
	bool IsTeamplay() const { return false; }
	bool IsSurvival() const { return false; }

	bool OnPlayerChat(int ClientID, const char *pMessage);
	void OnPlayerTeleport(int ClientID, const char *pString);

	const char *GetGameType() const { return m_pGameType; }

	// spawn
	bool CanSpawn(class CGameWorld *pWorld, int Type, vec2 *pPos) const;
	bool GetStartRespawnState() const;

	// team
	bool CanJoinTeam(int Team, int NotThisID) const;
	bool CanChangeTeam(CPlayer *pPplayer, int JoinTeam) const;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true);

	int GetRealPlayerNum() const { return m_RealPlayerNum; }
	int GetStartTeam();

	bool IsInDarkMode() const;
	// static void Com_Example(IConsole::IResult *pResult, void *pContext);
	void RegisterChatCommands(CCommandManager *pManager);
};

#endif
