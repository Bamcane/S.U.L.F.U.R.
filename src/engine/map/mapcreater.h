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

	array<SGroupInfo *> m_vpGroups;

	array<SImage *> m_vpImages;

	FT_Library m_Library;
	FT_Stroker m_FtStroker;
	array<FT_Face> m_vFontFaces;

	void GenerateQuadsFromTextLayer(SLayerText *pText, array<CQuad> &vpQuads);

public:
	class IStorage *Storage() { return m_pStorage; };
	class IConsole *Console() { return m_pConsole; };

	CMapCreater(class IStorage *pStorage, class IConsole *pConsole);
	~CMapCreater();

	SImage *AddExternalImage(const char *pImageName, int Width, int Height);
	SImage *AddEmbeddedImage(const char *pImageName, bool Flag = false);

	SGroupInfo *AddGroup(const char *pName);

	void AutoMap(SLayerTilemap *pTilemap, const char *pConfigName);

	bool SaveMap(EMapType MapType, const char *pMap);
};

#endif // ENGINE_MAP_MAPCREATER_H
