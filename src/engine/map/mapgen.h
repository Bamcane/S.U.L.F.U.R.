#ifndef ENGINE_MAP_MAPGEN_H
#define ENGINE_MAP_MAPGEN_H

#include <base/color.h>
#include <base/uuid.h>

#include <engine/console.h>
#include <engine/map.h>
#include <engine/shared/datafile.h>
#include <engine/shared/imageinfo.h>
#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <game/gamecore.h>
#include <game/mapitems.h>

class CServer;

class CMapGen
{
	static void GenerateHookable(CMapGen *pParent);
	static void GenerateUnhookable(CMapGen *pParent);

	void GenerateBackground();
	void GenerateGameLayer();
	void GenerateDoodadsLayer();
	void UseDarkMode();

	void GenerateMap(bool CreateCenter);

	Uuid m_MapUuid;

public:
	IStorage *m_pStorage;
	IConsole *m_pConsole;

	struct SLayerTilemap *m_pHookableLayer;
	struct SLayerTilemap *m_pUnhookableLayer;

	CTile *m_pBackGroundTiles;
	CTile *m_pGameTiles;
	CTile *m_pDoodadsTiles;

	class SGroupInfo *m_pMainGroup;
	class SImage *m_pSpaceImage;

	class CMapCreater *m_pMapCreater;

	bool m_HookableReady;
	bool m_UnhookableReady;

	IStorage *Storage() { return m_pStorage; };
	IConsole *Console() { return m_pConsole; };

	CMapGen(IStorage *pStorage, IConsole *pConsole);
	~CMapGen();

	bool CreateMap(const char *pFilename, bool CreateCenter);
};

#endif
