#include <base/uuid.h>

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include <engine/external/perlin-noise/PerlinNoise.hpp>

#include <game/layers.h>
#include <game/mapitems.h>

#include <thread>
#include <time.h>

#include "mapcreater.h"
#include "mapgen.h"

#define MAP_CHUNK_WIDTH 12
#define MAP_CHUNK_HEIGHT 12
#define CHUNK_SIZE 32
#define MAP_WIDTH CHUNK_SIZE *MAP_CHUNK_WIDTH
#define MAP_HEIGHT CHUNK_SIZE *MAP_CHUNK_HEIGHT
#define ISLAND_THRESHOLD 0.3f
#define DIRT_THRESHOLD 0.3f
#define OCTAVES 4
#define PERSISTENCE 0.3f
#define SCALE 40.f
#define SAFE_MARGIN 48

float smoothThreshold(float x, float center = 0.0f, float width = 0.15f)
{
	return 1.0f / (1.0f + exp(-20.0f * (x - center) / width));
}

CMapGen::CMapGen(IStorage *pStorage, IConsole *pConsole) :
	m_pStorage(pStorage),
	m_pConsole(pConsole)
{
	m_pBackGroundTiles = 0;
	m_pGameTiles = 0;
	m_pDoodadsTiles = 0;

	m_HookableReady = false;
	m_UnhookableReady = false;
}

CMapGen::~CMapGen()
{
	delete m_pMapCreater;
}

void CMapGen::GenerateGameLayer()
{
	int Width = CHUNK_SIZE * MAP_CHUNK_WIDTH;
	int Height = CHUNK_SIZE * MAP_CHUNK_HEIGHT;

	// create tiles
	SGroupInfo *pGroup = m_pMapCreater->AddGroup("Game");
	SLayerTilemap *pLayer = pGroup->AddTileLayer("Game");
	pLayer->m_Flags = TILESLAYERFLAG_GAME;

	m_pGameTiles = pLayer->AddTiles(Width, Height);

	int32_t IntSeed;
	mem_copy(&IntSeed, &m_MapUuid.m_aData[0], sizeof(IntSeed));
	const siv::PerlinNoise::seed_type Seed = IntSeed;
	const siv::PerlinNoise Perlin{Seed};
	mem_copy(&IntSeed, &m_MapUuid.m_aData[4], sizeof(IntSeed));
	srand(IntSeed);

	// fill tiles to solid
	auto FillThread = [this, Width, Height, Perlin](int ChunkX, int ChunkY) {
		CTile *pTiles = m_pGameTiles;
		for(int x = ChunkX * CHUNK_SIZE; x < (ChunkX + 1) * CHUNK_SIZE; x++)
		{
			for(int y = ChunkY * CHUNK_SIZE; y < (ChunkY + 1) * CHUNK_SIZE; y++)
			{
				pTiles[y * Width + x].m_Index = 0;
				pTiles[y * Width + x].m_Flags = 0;
				pTiles[y * Width + x].m_Reserved = 0;
				pTiles[y * Width + x].m_Skip = 0;
			}
		}

		{
			// noise create unhookable tiles
			for(int x = ChunkX * CHUNK_SIZE; x < (ChunkX + 1) * CHUNK_SIZE; x++)
			{
				for(int y = ChunkY * CHUNK_SIZE; y < (ChunkY + 1) * CHUNK_SIZE; y++)
				{
					if(x < 1 || x > Width - 1 || y < 1 || y > Height - 1)
					{
						continue;
					}
					float nx = (x - MAP_WIDTH / 2) / SCALE;
					float ny = (y - MAP_HEIGHT / 2) / SCALE;

					float noiseValue = Perlin.octave2D(nx, ny, OCTAVES, PERSISTENCE);

					float distFromCenter = sqrt(nx * nx + ny * ny);
					float noisePerturbation = Perlin.octave2D(nx * 0.5f, ny * 0.5f, OCTAVES, PERSISTENCE) * 1.0f;
					float perturbedDist = distFromCenter + noisePerturbation;
					float radialDecay = maximum(0.0f, 1.0f - perturbedDist / 10.0f);

					float edgeSafety = 1.0f;

					int xDistFromEdge = minimum(x, MAP_WIDTH - x);
					int yDistFromEdge = minimum(y, MAP_HEIGHT - y);
					int minEdgeDist = minimum(xDistFromEdge, yDistFromEdge);

					if(minEdgeDist < SAFE_MARGIN)
					{
						float t = (float) minEdgeDist / SAFE_MARGIN;
						edgeSafety = 0.5f * (1.0f + tanh(t * 10.0f - 5.0f));
					}

					float finalValue = noiseValue * radialDecay * edgeSafety;

					finalValue = smoothThreshold(finalValue, ISLAND_THRESHOLD, 0.1f);
					if(finalValue > DIRT_THRESHOLD)
					{
						m_pGameTiles[y * Width + x].m_Index = TILE_SOLID;
					}
				}
			}
		}
	};

	std::vector<std::thread> vThreads;
	for(int x = 0; x < MAP_CHUNK_WIDTH; x++)
	{
		for(int y = 0; y < MAP_CHUNK_HEIGHT; y++)
		{
			vThreads.push_back(std::thread(FillThread, x, y));
		}
	}

	for(auto &Thread : vThreads)
	{
		Thread.join();
	}

	for(int x = 1; x < MAP_WIDTH - 1; x++)
	{
		for(int y = 1; y < MAP_HEIGHT - 1; y++)
		{
			if(m_pGameTiles[y * Width + x].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x].m_Index == TILE_SOLID && random_int() % 100 < 13)
			{
				m_pGameTiles[y * Width + x].m_Index = ENTITY_OFFSET + 1 + random_int() % ENTITY_SPAWN_BLUE;
			}
		}
	}
}

void CMapGen::GenerateBackground()
{
	SGroupInfo *pGroup = m_pMapCreater->AddGroup("Quads");
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;

	SLayerQuads *pLayer = pGroup->AddQuadsLayer("Quads");
	{
		SQuad *pQuad = pLayer->AddQuad(vec2(0, 0), vec2(1600, 1200));
		pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = random_int() % 155;
		pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = random_int() % 155;
		pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = random_int() % 155;
		pQuad->m_aColors[0].a = pQuad->m_aColors[1].a = 255;
		pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = random_int() % 155;
		pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = random_int() % 155;
		pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = random_int() % 155;
		pQuad->m_aColors[2].a = pQuad->m_aColors[3].a = 255;
	}
}

void CMapGen::GenerateDoodadsLayer()
{
	int Width = CHUNK_SIZE * MAP_CHUNK_WIDTH;
	int Height = CHUNK_SIZE * MAP_CHUNK_HEIGHT;

	SLayerTilemap *pLayer = m_pMainGroup->AddTileLayer("Doodads");

	pLayer->m_pImage = m_pMapCreater->AddEmbeddedImage("grass_doodads");

	m_pDoodadsTiles = pLayer->AddTiles(Width, Height);
	for(int x = 0; x < Width; x++)
	{
		for(int y = 0; y < Height; y++)
		{
			m_pDoodadsTiles[y * Width + x].m_Index = 0;
			m_pDoodadsTiles[y * Width + x].m_Flags = 0;
			m_pDoodadsTiles[y * Width + x].m_Reserved = 0;
			m_pDoodadsTiles[y * Width + x].m_Skip = 0;
		}
	}

	for(int y = 0; y < Height - 3; y++)
	{
		for(int x = 0; x < Width - 9; x++)
		{
			if(random_int() % 100 < 80)
				continue;

			if(m_pDoodadsTiles[(y + 1) * Width + x].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 1].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 2].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 3].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 4].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 5].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 6].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 7].m_Index != 0 || m_pDoodadsTiles[(y + 1) * Width + x + 8].m_Index != 0)
				continue;
			if(m_pDoodadsTiles[(y + 2) * Width + x].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 1].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 2].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 3].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 4].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 5].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 6].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 7].m_Index != 0 || m_pDoodadsTiles[(y + 2) * Width + x + 8].m_Index != 0)
				continue;
			if(m_pDoodadsTiles[(y + 3) * Width + x].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 1].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 2].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 3].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 4].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 5].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 6].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 7].m_Index != 0 || m_pDoodadsTiles[(y + 3) * Width + x + 8].m_Index != 0)
				continue;

			if(m_pGameTiles[(y + 1) * Width + x].m_Index == TILE_AIR &&
				m_pGameTiles[(y + 1) * Width + x + 1].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 2].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 3].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 4].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 5].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 6].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 7].m_Index == TILE_AIR && m_pGameTiles[(y + 1) * Width + x + 8].m_Index == TILE_AIR)
			{
				if(m_pGameTiles[(y + 4) * Width + x].m_Index != TILE_AIR &&
					m_pGameTiles[(y + 4) * Width + x + 1].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 2].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 3].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 4].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 5].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 6].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 7].m_Index != TILE_AIR && m_pGameTiles[(y + 4) * Width + x + 8].m_Index != TILE_AIR)
				{
					for(int i = 0; i < 9; i++)
					{
						for(int j = 0; j < 3; j++)
						{
							m_pDoodadsTiles[(y + 1 + j) * Width + x + i].m_Index = 6 + 16 * j + i;
							m_pDoodadsTiles[(y + 1 + j) * Width + x + i].m_Flags = 0;
						}
					}
				}
			}
		}
	}
}

void CMapGen::GenerateHookable(CMapGen *pParent)
{
	int Width = CHUNK_SIZE * MAP_CHUNK_WIDTH;
	int Height = CHUNK_SIZE * MAP_CHUNK_HEIGHT;

	CTile *pTiles = pParent->m_pHookableLayer->AddTiles(Width, Height);

	for(int x = 0; x < Width; x++)
	{
		for(int y = 0; y < Height; y++)
		{
			pTiles[y * Width + x].m_Flags = 0;
			pTiles[y * Width + x].m_Reserved = 0;
			pTiles[y * Width + x].m_Skip = 0;
			if(pParent->m_pGameTiles[y * Width + x].m_Index == TILE_SOLID)
			{
				pTiles[y * Width + x].m_Index = 1;
			}
			else
			{
				pTiles[y * Width + x].m_Index = 0;
			}
		}
	}

	pParent->m_HookableReady = true;

	pParent->m_pMapCreater->AutoMap(pParent->m_pHookableLayer, "Default");
}

void CMapGen::GenerateUnhookable(CMapGen *pParent)
{
	int Width = CHUNK_SIZE * MAP_CHUNK_WIDTH;
	int Height = CHUNK_SIZE * MAP_CHUNK_HEIGHT;

	CTile *pTiles = pParent->m_pUnhookableLayer->AddTiles(Width, Height);

	for(int x = 0; x < Width; x++)
	{
		for(int y = 0; y < Height; y++)
		{
			pTiles[y * Width + x].m_Flags = 0;
			pTiles[y * Width + x].m_Reserved = 0;
			pTiles[y * Width + x].m_Skip = 0;
			pTiles[y * Width + x].m_Index = pParent->m_pGameTiles[y * Width + x].m_Index == TILE_NOHOOK;
		}
	}

	pParent->m_UnhookableReady = true;

	pParent->m_pMapCreater->AutoMap(pParent->m_pUnhookableLayer, "Random Silver");
}

void CMapGen::GenerateMap(bool CreateCenter)
{
	// Generate background
	GenerateBackground();
	// Generate game tile
	GenerateGameLayer();
	// fast generate
	{
		m_pMainGroup = m_pMapCreater->AddGroup("Tiles");

		// Generate doodads tile
		// GenerateDoodadsLayer();
		{
			m_pHookableLayer = m_pMainGroup->AddTileLayer("Hookable");
			m_pHookableLayer->m_pImage = m_pMapCreater->AddExternalImage("grass_main", 1024, 1024);
		}
		{
			m_pUnhookableLayer = m_pMainGroup->AddTileLayer("Unhookable");
			m_pUnhookableLayer->m_pImage = m_pMapCreater->AddExternalImage("generic_unhookable", 1024, 1024);
		}
		// Generate hookable tile
		std::thread(&CMapGen::GenerateHookable, this).join();
		// Generate unhookable tile
		std::thread(&CMapGen::GenerateUnhookable, this).join();
	}

	while(!m_HookableReady || !m_UnhookableReady) {}
}

bool CMapGen::CreateMap(const char *pFilename, bool CreateCenter)
{
	m_pMapCreater = new CMapCreater(Storage(), Console());
	m_MapUuid = CalculateUuid(pFilename);

	int64_t Time = time_get();

	GenerateMap(CreateCenter);

	float UseTime = (time_get() - Time) / (float) time_freq();
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "generate map in %.02f seconds", UseTime);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "mapgen", aBuf, UseTime);

	return m_pMapCreater->SaveMap(EMapType::MAPTYPE_NORMAL, pFilename);
}
