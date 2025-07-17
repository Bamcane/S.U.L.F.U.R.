/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include "eventhandler.h"
#include "gamecontext.h"
#include "player.h"

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = 0;
	Clear();
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void CEventHandler::Create(void *pData, int Type, int Size, int ClientID)
{
	int &NumEvent = m_aNumEvents[ClientID];
	if(m_aNumEvents[ClientID] == MAX_EVENTS)
		return;
	if(m_aCurrentOffset[ClientID] + Size >= MAX_DATASIZE)
		return;

	void *p = &m_aaData[ClientID][m_aCurrentOffset[ClientID]];
	m_aaOffsets[ClientID][NumEvent] = m_aCurrentOffset[ClientID];
	m_aaTypes[ClientID][NumEvent] = Type;
	m_aaSizes[ClientID][NumEvent] = Size;
	m_aCurrentOffset[ClientID] += Size;
	NumEvent++;
	mem_copy(p, pData, Size);
}

void CEventHandler::Create(void *pData, int Type, int Size, int64 Mask)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(CmaskIsSet(Mask, i))
		{
			Create(pData, Type, Size, i);
		}
	}
}

void CEventHandler::Clear()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aNumEvents[i] = 0;
		m_aCurrentOffset[i] = 0;
	}
}

void CEventHandler::Snap(int SnappingClient)
{
	for(int i = 0; i < m_aNumEvents[SnappingClient]; i++)
	{
		CNetEvent_Common *pEvent = (CNetEvent_Common *) &m_aaData[SnappingClient][m_aaOffsets[SnappingClient][i]];
		if(!NetworkClipped(SnappingClient, vec2(pEvent->m_X, pEvent->m_Y), GameServer(), GameServer()->m_apPlayers[SnappingClient]->GameWorld()))
		{
			void *pData = GameServer()->Server()->SnapNewItem(m_aaTypes[SnappingClient][i], i, m_aaSizes[SnappingClient][i]);
			if(pData)
				mem_copy(pData, &m_aaData[SnappingClient][m_aaOffsets[SnappingClient][i]], m_aaSizes[SnappingClient][i]);
		}
	}
}
