/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <base/tl/sorted_array.h>
#include <base/uuid.h>

#include <engine/server.h>
#include <engine/shared/http.h>
#include <engine/shared/memheap.h>

#include <unordered_map>

class CSnapIDPool
{
	enum
	{
		MAX_IDS = 16 * 1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = allocated, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:
	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};

class CServerBan : public CNetBan
{
	class CServer *m_pServer;

	template<class T>
	int BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason);

public:
	class CServer *Server() const { return m_pServer; }

	void InitServerBan(class IConsole *pConsole, class IStorage *pStorage, class CServer *pServer);

	virtual int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason);
	virtual int BanRange(const CNetRange *pRange, int Seconds, const char *pReason);

	static void ConBanExt(class IConsole::IResult *pResult, void *pUser);
};

class CServer : public IServer
{
	class IGameServer *m_pGameServer;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class IRegister *m_pRegister;

public:
	class IGameServer *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IStorage *Storage() { return m_pStorage; }

	enum
	{
		AUTHED_NO = 0,
		AUTHED_MOD,
		AUTHED_ADMIN,

		MAX_RCONCMD_SEND = 16,
		MAX_MAPLISTENTRY_SEND = 32,
		MIN_MAPLIST_CLIENTVERSION = 0x0703, // todo 0.8: remove me
		MAX_RCONCMD_RATIO = 8,
	};

	struct CMapListEntry;

	class CClient
	{
	public:
		enum
		{
			STATE_EMPTY = 0,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_CONNECTING_AS_SPEC,
			STATE_READY,
			STATE_INGAME,

			SNAPRATE_INIT = 0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		char m_aName[MAX_NAME_ARRAY_SIZE];
		char m_aClan[MAX_CLAN_ARRAY_SIZE];
		int m_Version;
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthTries;

		int m_MapChunk;
		bool m_NoRconNote;
		bool m_Quitting;
		const IConsole::CCommandInfo *m_pRconCmdToSend;
		Uuid m_MapID;
		bool m_MapChangeRequest;

		bool IncludedInServerInfo() const
		{
			return m_State != STATE_EMPTY;
		}

		void Reset();
	};

	CClient m_aClients[MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
	CServerBan m_ServerBan;
	CHttp m_Http;

	IEngineMap *m_pMap;

	int64 m_GameStartTime;
	bool m_RunServer;
	bool m_MapReload;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;
	char m_aShutdownReason[128];

	// map
	enum
	{
		MAP_CHUNK_SIZE = NET_MAX_PAYLOAD - NET_MAX_CHUNKHEADERSIZE - 4, // msg type
	};

	class CMapData
	{
	public:
		char m_aName[64];
		SHA256_DIGEST m_Sha256;
		unsigned m_Crc;
		unsigned char *m_pData;
		int m_Size;

		CMapData() { Reset(); }
		void Reset()
		{
			m_aName[0] = '\0';
			m_Crc = 0;
			m_pData = nullptr;
			m_Size = 0;
		}
	};
	std::unordered_map<Uuid, CMapData> m_uMapDatas;
	Uuid m_BaseMapUuid;
	int m_MapChunksPerRequest;

	int m_RconPasswordSet;
	int m_GeneratedRconPassword;

	CDemoRecorder m_DemoRecorder;
	bool m_ServerInfoNeedsUpdate;

	CServer();

	virtual void SetClientName(int ClientID, const char *pName);
	virtual void SetClientClan(int ClientID, char const *pClan);
	virtual void SetClientCountry(int ClientID, int Country);
	virtual void SetClientScore(int ClientID, int Score);

	void Kick(int ClientID, const char *pReason);

	void DemoRecorder_HandleAutoStart();
	bool DemoRecorder_IsRecording();

	int64 TickStartTime(int Tick);

	int Init();

	void InitRconPasswordIfUnset();

	void SetRconCID(int ClientID);
	bool IsAuthed(int ClientID) const;
	bool IsBanned(int ClientID);
	int GetClientInfo(int ClientID, CClientInfo *pInfo) const;
	void GetClientAddr(int ClientID, char *pAddrStr, int Size) const;
	int GetClientVersion(int ClientID) const;
	const char *ClientName(int ClientID) const;
	const char *ClientClan(int ClientID) const;
	int ClientCountry(int ClientID) const;
	bool ClientIngame(int ClientID) const;
	Uuid GetClientMapID(int ClientID) const override;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID);

	void DoSnapshot();

	static int NewClientCallback(int ClientID, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);

	void SendMap(int ClientID);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();

	void ProcessClientPacket(CNetChunk *pPacket);

	void ExpireServerInfo() override;
	void UpdateRegisterServerInfo();
	void UpdateServerInfo(bool Resend = false);

	void SendServerInfo(int ClientID);
	void GenerateServerInfo(CPacker *pPacker, bool IncludeClientInfo);

	void PumpNetwork();

	const char *GetMapName();
	int LoadMap(const char *pMapName, bool Generated = false);

	void InitInterfaces(IKernel *pKernel);
	int Run();
	void Free();

	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConRecord(IConsole::IResult *pResult, void *pUser);
	static void ConStopRecord(IConsole::IResult *pResult, void *pUser);
	static void ConMapReload(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);
	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainModCommandUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconPasswordSet(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void RegisterCommands();

	virtual int SnapNewID();
	virtual void SnapFreeID(int ID);
	virtual void *SnapNewItem(int Type, int ID, int Size);
	void SnapSetStaticsize(int ItemType, int Size);

	void SwitchClientMap(int ClientID, Uuid MapID) override;
	void RequestNewWorld(int ClientID, const char *pWorldName) override;

	Uuid GetBaseMapUuid() const override { return m_BaseMapUuid; }
	const char *GetMapName(Uuid MapID) override;

	void DoSpecialBan(int ClientID, int Time, const char *pBanMsg) override;
};

#endif
