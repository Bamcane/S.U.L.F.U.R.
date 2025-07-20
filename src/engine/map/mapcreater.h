#ifndef ENGINE_MAP_MAPCREATER_H
#define ENGINE_MAP_MAPCREATER_H

#include "mapitems.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

class CServer;

class CMapCreater
{
	class IStorage *m_pStorage;
	class IConsole *m_pConsole;

	array<SGroupInfo *> m_apGroups;

	array<SImage *> m_apImages;
	array<SEnvelope *> m_apEnvelopes;

	FT_Library m_Library;
	FT_Stroker m_FtStroker;
	array<FT_Face> m_aFontFaces;

	void GenerateQuadsFromTextLayer(SLayerText *pText, array<CQuad> &vpQuads);

public:
	class IStorage *Storage() { return m_pStorage; };
	class IConsole *Console() { return m_pConsole; };

	CMapCreater(class IStorage *pStorage, class IConsole *pConsole);
	~CMapCreater();

	SImage *AddExternalImage(const char *pImageName, int Width, int Height);
	SImage *AddEmbeddedImage(const char *pImageName, bool Flag = false);
	
	SEnvelope *AddEnvelope(const char *pEnvName, EEnvType Type, bool Synchronized);

	SGroupInfo *AddGroup(const char *pName);

	void AutoMap(SLayerTilemap *pTilemap, const char *pConfigName);
	void AddMiniMap();

	bool SaveMap(EMapType MapType, const char *pMap);
};

#endif // ENGINE_MAP_MAPCREATER_H
