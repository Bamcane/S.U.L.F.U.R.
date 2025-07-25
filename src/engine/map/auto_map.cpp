#include <cstdio>

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/mapitems.h>

#include "auto_map.h"
#include "mapcreater.h"

CAutoMapper::CAutoMapper(class CMapCreater *pCreater)
{
	m_pMapCreater = pCreater;
	m_FileLoaded = false;
}

void CAutoMapper::Load(const char *pTileName)
{
	char aPath[256];
	str_format(aPath, sizeof(aPath), "automap/%s.rules", pTileName);
	IOHANDLE RulesFile = m_pMapCreater->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!RulesFile)
		return;

	CLineReader LineReader;
	LineReader.Init(RulesFile);

	CConfiguration *pCurrentConf = 0;
	CIndexRule *pCurrentIndex = 0;

	char aBuf[256];

	// read each line
	while(const char *pLine = LineReader.Get())
	{
		// skip blank/empty lines as well as comments
		if(str_length(pLine) > 0 && pLine[0] != '#' && pLine[0] != '\n' && pLine[0] != '\r' && pLine[0] != '\t' && pLine[0] != '\v' && pLine[0] != ' ')
		{
			if(pLine[0] == '[')
			{
				// new configuration, get the name
				pLine++;

				CConfiguration NewConf;
				int ID = m_lConfigs.add(NewConf);
				pCurrentConf = &m_lConfigs[ID];

				str_copy(pCurrentConf->m_aName, pLine, str_length(pLine));
			}
			else
			{
				if(!str_comp_num(pLine, "Index", 5))
				{
					// new index
					int ID = 0;
					char aFlip[128] = "";

					sscanf(pLine, "Index %d %127s", &ID, aFlip);

					CIndexRule NewIndexRule;
					NewIndexRule.m_ID = ID;
					NewIndexRule.m_Flag = 0;
					NewIndexRule.m_RandomValue = 0;
					NewIndexRule.m_BaseTile = false;

					if(str_length(aFlip) > 0)
					{
						if(!str_comp(aFlip, "XFLIP"))
							NewIndexRule.m_Flag = TILEFLAG_XFLIP;
						else if(!str_comp(aFlip, "YFLIP"))
							NewIndexRule.m_Flag = TILEFLAG_YFLIP;
					}

					// add the index rule object and make it current
					int ArrayID = pCurrentConf->m_aIndexRules.add(NewIndexRule);
					pCurrentIndex = &pCurrentConf->m_aIndexRules[ArrayID];
				}
				else if(!str_comp_num(pLine, "BaseTile", 8) && pCurrentIndex)
				{
					pCurrentIndex->m_BaseTile = true;
				}
				else if(!str_comp_num(pLine, "Pos", 3) && pCurrentIndex)
				{
					int x = 0, y = 0;
					char aValue[128];
					int Value = CPosRule::EMPTY;
					bool IndexValue = false;

					sscanf(pLine, "Pos %d %d %127s", &x, &y, aValue);

					if(!str_comp(aValue, "FULL"))
						Value = CPosRule::FULL;
					else if(!str_comp_num(aValue, "INDEX", 5) || !str_comp_num(aValue, "NOTINDEX", 8))
					{
						if(!str_comp_num(aValue, "INDEX", 5))
						{
							sscanf(pLine, "Pos %*d %*d INDEX %d", &Value);
							Value = CPosRule::INDEX;
						}
						else
						{
							sscanf(pLine, "Pos %*d %*d NOTINDEX %d", &Value);
							Value = CPosRule::NOTINDEX;
						}
						IndexValue = true;
					}

					CPosRule NewPosRule = {x, y, Value, IndexValue};
					pCurrentIndex->m_aRules.add(NewPosRule);
				}
				else if(!str_comp_num(pLine, "Random", 6) && pCurrentIndex)
				{
					sscanf(pLine, "Random %d", &pCurrentIndex->m_RandomValue);
				}
			}
		}
	}

	io_close(RulesFile);

	str_format(aBuf, sizeof(aBuf), "loaded %s", aPath);
	m_pMapCreater->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);

	m_FileLoaded = true;
}

const char *CAutoMapper::GetConfigName(int Index)
{
	if(Index < 0 || Index >= m_lConfigs.size())
		return "";

	return m_lConfigs[Index].m_aName;
}

void CAutoMapper::Proceed(SLayerTilemap *pLayer, int ConfigID)
{
	if(!m_FileLoaded || ConfigID < 0 || ConfigID >= m_lConfigs.size())
		return;

	CConfiguration *pConf = &m_lConfigs[ConfigID];

	if(!pConf->m_aIndexRules.size())
		return;

	int BaseTile = 1;

	// find base tile if there is one
	for(int i = 0; i < pConf->m_aIndexRules.size(); ++i)
	{
		if(pConf->m_aIndexRules[i].m_BaseTile)
		{
			BaseTile = pConf->m_aIndexRules[i].m_ID;
			break;
		}
	}

	// auto map !
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
		{
			CTile *pTile = &(pLayer->m_pTiles[y * pLayer->m_Width + x]);
			if(pTile->m_Index == 0)
				continue;

			pTile->m_Index = BaseTile;

			for(int i = 0; i < pConf->m_aIndexRules.size(); ++i)
			{
				if(pConf->m_aIndexRules[i].m_BaseTile)
					continue;

				bool RespectRules = true;
				for(int j = 0; j < pConf->m_aIndexRules[i].m_aRules.size() && RespectRules; ++j)
				{
					CPosRule *pRule = &pConf->m_aIndexRules[i].m_aRules[j];
					int CheckIndex;
					int CheckX = x + pRule->m_X;
					int CheckY = y + pRule->m_Y;

					if(CheckX >= 0 && CheckX < pLayer->m_Width && CheckY >= 0 && CheckY < pLayer->m_Height)
					{
						int CheckTile = CheckY * pLayer->m_Width + CheckX;
						CheckIndex = pLayer->m_pTiles[CheckTile].m_Index;
					}
					else
					{
						CheckIndex = -1;
					}

					if(pRule->m_IndexValue)
					{
						if(pRule->m_Value == CPosRule::INDEX)
						{
							if(CheckIndex != pRule->m_Value)
								RespectRules = false;
						}
						else
						{
							if(CheckIndex == pRule->m_Value)
								RespectRules = false;
						}
					}
					else
					{
						if(CheckIndex != 0 && pRule->m_Value == CPosRule::EMPTY)
							RespectRules = false;

						if(CheckIndex == 0 && pRule->m_Value == CPosRule::FULL)
							RespectRules = false;
					}
				}

				if(RespectRules &&
					(pConf->m_aIndexRules[i].m_RandomValue <= 1 || (int) ((float) rand() / ((float) RAND_MAX + 1) * pConf->m_aIndexRules[i].m_RandomValue) == 1))
				{
					pTile->m_Index = pConf->m_aIndexRules[i].m_ID;
					pTile->m_Flags = pConf->m_aIndexRules[i].m_Flag;
				}
			}
		}
}
