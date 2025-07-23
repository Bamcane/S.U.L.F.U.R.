#ifndef ENGINE_MAP_MAPITEMS_H
#define ENGINE_MAP_MAPITEMS_H

#include <base/color.h>
#include <base/tl/array.h>
#include <base/tl/string.h>

#include <game/mapitems.h>

enum class EMapType
{
	MAPTYPE_NORMAL = 0
};

enum class ELayerType
{
	TILES = 0,
	QUADS,
	TEXT,
};

enum class EEnvType
{
	None = -1,
	Pos = 0,
	Color,
	Sound,
};

struct SImage
{
	char m_aName[32];

	bool m_External;

	int m_Width;
	int m_Height;

	int m_ImageID;

	unsigned char *m_pImageData;
};

struct SEnvPoint
{
	int m_Time; // in ms
	int m_Curvetype;
	float m_aValues[4];
};

struct SEnvelope
{
	char m_aName[32];
	bool m_Synchronized;

	int m_EnvID;

	array<SEnvPoint *> m_apEnvPoints;

	SEnvPoint *AddEnvPoint(int Time, int CurveType);

	virtual EEnvType Type() const = 0;
	virtual int Channels() const = 0;
	virtual ~SEnvelope() {};
};

struct ILayerInfo
{
	char m_aName[32];

	ELayerType m_Type;
	int m_Flags;
	bool m_UseInMinimap;

	SImage *m_pImage;

	ILayerInfo()
	{
		m_pImage = nullptr;
		m_Flags = 0;
		m_UseInMinimap = false;
	}
};

struct SText
{
	ivec2 m_Pos;

	string m_Text;

	int m_Size;
	bool m_Outline;
	bool m_Center;

	SEnvelope *m_pColorEnv;
	SEnvelope *m_pPosEnv;
};

struct SLayerText : public ILayerInfo
{
	array<SText *> m_apText;

	SLayerText()
	{
		m_Type = ELayerType::TEXT;
	}

	SText *AddText(const char *pText, int Size, ivec2 Pos, bool Outline = true, bool Center = false);
};

struct SLayerTilemap : public ILayerInfo
{
	CTile *m_pTiles;

	int m_Width;
	int m_Height;
	int m_Flags;

	SEnvelope *m_pColorEnv;
	ColorRGBA m_Color;

	SLayerTilemap() :
		ILayerInfo()
	{
		m_Type = ELayerType::TILES;
		SLayerTilemap::m_Flags = 0;
	}

	CTile *AddTiles(int Width, int Height);
};

struct SQuad
{
	ivec2 m_aPoints[4];
	ivec2 m_aTexcoords[4];

	ivec2 m_Pos;

	ColorRGBA m_aColors[4];

	SEnvelope *m_pColorEnv;
	SEnvelope *m_pPosEnv;
};

struct SLayerQuads : public ILayerInfo
{
	ColorRGBA m_Color;

	array<SQuad *> m_apQuads;

	SLayerQuads() :
		ILayerInfo()
	{
		m_Type = ELayerType::QUADS;
	}

	SQuad *AddQuad(vec2 Pos, vec2 Size, ColorRGBA Color = ColorRGBA(255, 255, 255, 255));
};

struct SGroupInfo
{
	char m_aName[32];
	bool m_UseClipping;

	int m_OffsetX;
	int m_OffsetY;
	int m_ParallaxX;
	int m_ParallaxY;

	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	array<ILayerInfo *> m_apLayers;

	SLayerTilemap *AddTileLayer(const char *pName);
	SLayerQuads *AddQuadsLayer(const char *pName);
	SLayerText *AddTextLayer(const char *pName);
};

struct SPosEnvelope : public SEnvelope
{
	EEnvType Type() const override { return EEnvType::Pos; }
	int Channels() const override { return 3; }
	~SPosEnvelope() override = default;
};

struct SColorEnvelope : public SEnvelope
{
	EEnvType Type() const override { return EEnvType::Color; }
	int Channels() const override { return 4; }
	~SColorEnvelope() override = default;
};

struct SSoundEnvelope : public SEnvelope
{
	EEnvType Type() const override { return EEnvType::Sound; }
	int Channels() const override { return 1; }
	~SSoundEnvelope() override = default;
};

#endif // ENGINE_MAP_MAPITEMS_H
