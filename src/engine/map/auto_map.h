#ifndef ENGINE_MAP_AUTO_MAP_H
#define ENGINE_MAP_AUTO_MAP_H

// from teeworlds 0.6

#include <base/tl/array.h>

class CAutoMapper
{
	struct CPosRule
	{
		int m_X;
		int m_Y;
		int m_Value;
		bool m_IndexValue;

		enum
		{
			EMPTY = 0,
			FULL,
			INDEX,
			NOTINDEX,
		};
	};

	struct CIndexRule
	{
		int m_ID;
		array<CPosRule> m_aRules;
		int m_Flag;
		int m_RandomValue;
		bool m_BaseTile;
	};

	struct CConfiguration
	{
		array<CIndexRule> m_aIndexRules;
		char m_aName[128];
	};

public:
	CAutoMapper(class CMapCreater *pCreater);

	void Load(const char *pTileName);
	void Proceed(struct SLayerTilemap *pLayer, int ConfigID);

	int ConfigNamesNum() { return m_lConfigs.size(); }
	const char *GetConfigName(int Index);

	bool IsLoaded() { return m_FileLoaded; }

private:
	array<CConfiguration> m_lConfigs;
	class CMapCreater *m_pMapCreater;
	bool m_FileLoaded;
};

#endif
