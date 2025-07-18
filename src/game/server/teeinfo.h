#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

#include <engine/shared/protocol.h>
#include <generated/protocol.h>

struct STeeInfo
{
	char m_aaSkinPartNames[NUM_SKINPARTS][MAX_SKIN_ARRAY_SIZE];
	int m_aUseCustomColors[NUM_SKINPARTS];
	int m_aSkinPartColors[NUM_SKINPARTS];
};

void ReadInfoByJson(class IStorage *pStorage, const char *pSkinName, STeeInfo &TeeInfos);

STeeInfo GenerateRandomSkin();

#endif // GAME_SERVER_TEEINFO_H
