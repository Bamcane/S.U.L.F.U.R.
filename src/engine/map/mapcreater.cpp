// Thanks ddnet
#include <engine/console.h>
#include <engine/storage.h>

#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/imageinfo.h>
#include <engine/shared/linereader.h>

#include <game/gamecore.h>
#include <game/layers.h>
#include <game/mapitems.h>

#include <engine/gfx/image_loader.h>

#include <base/color.h>

// sync for std::thread
#include <mutex>

#include "auto_map.h"
#include "mapcreater.h"

void FreePNG(CImageInfo *pImg)
{
	free(pImg->m_pData);
	pImg->m_pData = nullptr;
}

int LoadPNG(CImageInfo *pImg, IStorage *pStorage, const char *pFilename)
{
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_seek(File, 0, IOSEEK_END);
		unsigned int FileSize = io_tell(File);
		io_seek(File, 0, IOSEEK_START);

		TImageByteBuffer ByteBuffer;
		SImageByteBuffer ImageByteBuffer(&ByteBuffer);

		ByteBuffer.resize(FileSize);
		io_read(File, &ByteBuffer.front(), FileSize);

		io_close(File);

		uint8_t *pImgBuffer = NULL;
		EImageFormat ImageFormat;
		int PngliteIncompatible;
		if(::LoadPNG(ImageByteBuffer, pFilename, PngliteIncompatible, pImg->m_Width, pImg->m_Height, pImgBuffer, ImageFormat))
		{
			pImg->m_pData = pImgBuffer;

			if(ImageFormat == IMAGE_FORMAT_RGB) // ignore_convention
				pImg->m_Format = CImageInfo::FORMAT_RGB;
			else if(ImageFormat == IMAGE_FORMAT_RGBA) // ignore_convention
				pImg->m_Format = CImageInfo::FORMAT_RGBA;
			else
			{
				free(pImgBuffer);
				return 0;
			}

			if(PngliteIncompatible != 0)
			{
				dbg_msg("game/png", "\"%s\" is not compatible with pnglite and cannot be loaded by old DDNet versions: ", pFilename);
			}
		}
		else
		{
			dbg_msg("game/png", "image had unsupported image format. filename='%s'", pFilename);
			return 0;
		}
	}
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	return 1;
}

CMapCreater::CMapCreater(IStorage *pStorage, IConsole *pConsole) :
	m_pStorage(pStorage),
	m_pConsole(pConsole)
{
	m_vpGroups.clear();
	m_vpImages.clear();

	FT_Error Error = FT_Init_FreeType(&m_Library);
	if(Error)
	{
		dbg_msg("mapcreater", "failed to init freetype");
		return;
	}

	FT_Stroker_New(m_Library, &m_FtStroker);

	void *pBuf;
	unsigned Length;
	if(!Storage()->ReadFile("fonts/DejaVuSans.ttf", IStorage::TYPE_ALL, &pBuf, &Length))
		return;

	FT_Face FtFace;

	FT_New_Memory_Face(m_Library, (FT_Bytes) pBuf, Length, 0, &FtFace);

	m_vFontFaces.add(FtFace);

	if(!Storage()->ReadFile("fonts/SourceHanSans.ttc", IStorage::TYPE_ALL, &pBuf, &Length))
		return;

	FT_New_Memory_Face(m_Library, (FT_Bytes) pBuf, Length, 0, &FtFace);

	int NumFaces = FtFace->num_faces;
	FT_Done_Face(FtFace);

	for(int i = 0; i < NumFaces; ++i)
	{
		if(FT_New_Memory_Face(m_Library, (FT_Bytes) pBuf, Length, i, &FtFace))
		{
			FT_Done_Face(FtFace);
			break;
		}
		m_vFontFaces.add(FtFace);
	}
}

CMapCreater::~CMapCreater()
{
	for(auto &pImage : m_vpImages)
	{
		if(pImage->m_pImageData)
		{
			free(pImage->m_pImageData);
		}
		delete pImage;
	}
	m_vpImages.clear();

	for(auto &pGroup : m_vpGroups)
	{
		for(auto &pLayer : pGroup->m_vpLayers)
		{
			switch(pLayer->m_Type)
			{
			case ELayerType::TILES:
			{
				delete[]((SLayerTilemap *) pLayer)->m_pTiles;
			}
			break;
			case ELayerType::QUADS:
			{
				for(auto &pQuad : ((SLayerQuads *) pLayer)->m_vpQuads)
					delete pQuad;
			}
			break;
			case ELayerType::TEXT:
			{
				for(auto &pText : ((SLayerText *) pLayer)->m_vpText)
					delete pText;
			}
			break;
			default:
				dbg_assert(false, "Invalid layer type");
			}
			delete pLayer;
		}
		pGroup->m_vpLayers.clear();
		delete pGroup;
	}
	m_vpGroups.clear();

	for(auto Face : m_vFontFaces)
	{
		FT_Done_Face(Face);
	}

	FT_Stroker_Done(m_FtStroker);
	FT_Done_FreeType(m_Library);
}

static std::mutex s_ImageMutex;
SImage *CMapCreater::AddEmbeddedImage(const char *pImageName, bool Flag)
{
	for(auto &pImage : m_vpImages)
	{
		if(str_comp(pImage->m_aName, pImageName) == 0)
		{
			return pImage;
		}
	}

	CImageInfo img;
	CImageInfo *pImg = &img;

	char aBuf[IO_MAX_PATH_LENGTH];
	if(Flag)
	{
		str_format(aBuf, sizeof(aBuf), "flags/%s.png", pImageName);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "mapres/%s.png", pImageName);
	}

	if(!LoadPNG(pImg, Storage(), aBuf))
	{
		dbg_msg("mapcreater", "failed to load image '%s'", aBuf);
		return nullptr;
	}

	s_ImageMutex.lock();

	m_vpImages.add(new SImage());
	SImage *pImage = m_vpImages[m_vpImages.size() - 1];

	s_ImageMutex.unlock();

	str_copy(pImage->m_aName, pImageName, sizeof(pImage->m_aName));

	pImage->m_External = false;
	pImage->m_pImageData = (unsigned char *) malloc((size_t) pImg->m_Width * pImg->m_Height * 4);

	pImage->m_Width = pImg->m_Width;
	pImage->m_Height = pImg->m_Height;

	pImage->m_ImageID = -1;

	unsigned char *pDataRGBA = pImage->m_pImageData;
	if(pImg->m_Format == CImageInfo::FORMAT_RGB)
	{
		// Convert to RGBA
		unsigned char *pDataRGB = (unsigned char *) pImg->m_pData;
		for(int i = 0; i < pImg->m_Width * pImg->m_Height; i++)
		{
			pDataRGBA[i * 4] = pDataRGB[i * 3];
			pDataRGBA[i * 4 + 1] = pDataRGB[i * 3 + 1];
			pDataRGBA[i * 4 + 2] = pDataRGB[i * 3 + 2];
			pDataRGBA[i * 4 + 3] = 255;
		}
	}
	else
	{
		mem_copy(pDataRGBA, pImg->m_pData, (size_t) pImg->m_Width * pImg->m_Height * 4);
	}

	FreePNG(pImg);

	return pImage;
}

SImage *CMapCreater::AddExternalImage(const char *pImageName, int Width, int Height)
{
	s_ImageMutex.lock();

	m_vpImages.add(new SImage());
	SImage *pImage = m_vpImages[m_vpImages.size() - 1];

	s_ImageMutex.unlock();

	str_copy(pImage->m_aName, pImageName, sizeof(pImage->m_aName));

	pImage->m_External = true;
	pImage->m_pImageData = nullptr;

	pImage->m_Width = Width;
	pImage->m_Height = Height;

	pImage->m_ImageID = -1;

	return pImage;
}

static std::mutex s_GroupMutex;
SGroupInfo *CMapCreater::AddGroup(const char *pName)
{
	s_GroupMutex.lock();

	m_vpGroups.add(new SGroupInfo());
	SGroupInfo *pGroup = m_vpGroups[m_vpGroups.size() - 1];

	s_GroupMutex.unlock();

	str_copy(pGroup->m_aName, pName, sizeof(pGroup->m_aName));

	// default
	pGroup->m_UseClipping = false;

	pGroup->m_ParallaxX = 100;
	pGroup->m_ParallaxY = 100;
	pGroup->m_OffsetX = 0;
	pGroup->m_OffsetY = 0;
	pGroup->m_ClipX = 0;
	pGroup->m_ClipY = 0;
	pGroup->m_ClipW = 0;
	pGroup->m_ClipH = 0;

	return pGroup;
}

void CMapCreater::AutoMap(SLayerTilemap *pTilemap, const char* pConfigName)
{
    CAutoMapper AutoMapper(this);

    AutoMapper.Load(pTilemap->m_pImage->m_aName);

    if(!AutoMapper.IsLoaded())
        return;

    int ConfigID = -1;
    for(int i = 0; i < AutoMapper.ConfigNamesNum(); i ++)
    {
        if(str_comp(AutoMapper.GetConfigName(i), pConfigName) == 0)
        {
            ConfigID = i;
            break;
        }
    }

    if(ConfigID == -1)
    {
        return;
    }

    AutoMapper.Proceed(pTilemap, ConfigID);
}

static std::mutex s_LayerMutex;
SLayerTilemap *SGroupInfo::AddTileLayer(const char *pName)
{
	s_LayerMutex.lock();

	m_vpLayers.add(new SLayerTilemap());
	SLayerTilemap *pLayer = (SLayerTilemap *) m_vpLayers[m_vpLayers.size() - 1];

	s_LayerMutex.unlock();

	str_copy(pLayer->m_aName, pName, sizeof(pLayer->m_aName));
	pLayer->m_pTiles = nullptr;
	pLayer->m_pImage = nullptr;

	pLayer->m_Color = ColorRGBA(255, 255, 255, 255);

	return pLayer;
}

SLayerQuads *SGroupInfo::AddQuadsLayer(const char *pName)
{
	s_LayerMutex.lock();

	m_vpLayers.add(new SLayerQuads());
	SLayerQuads *pLayer = (SLayerQuads *) m_vpLayers[m_vpLayers.size() - 1];

	s_LayerMutex.unlock();

	str_copy(pLayer->m_aName, pName, sizeof(pLayer->m_aName));

	return pLayer;
}

SLayerText *SGroupInfo::AddTextLayer(const char *pName)
{
	s_LayerMutex.lock();

	m_vpLayers.add(new SLayerText());
	SLayerText *pLayer = (SLayerText *) m_vpLayers[m_vpLayers.size() - 1];

	s_LayerMutex.unlock();

	str_copy(pLayer->m_aName, pName, sizeof(pLayer->m_aName));

	return pLayer;
}

CTile *SLayerTilemap::AddTiles(int Width, int Height)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "add tiles to the layer '%s' twice", m_aName);

	dbg_assert(!m_pTiles, aBuf);

	m_pTiles = new CTile[Width * Height];
	m_Width = Width;
	m_Height = Height;

	return m_pTiles;
}

static std::mutex s_QuadMutex;
SQuad *SLayerQuads::AddQuad(vec2 Pos, vec2 Size, ColorRGBA Color)
{
	s_QuadMutex.lock();

	m_vpQuads.add(new SQuad());
	SQuad *pQuad = m_vpQuads[m_vpQuads.size() - 1];

	s_QuadMutex.unlock();

	int X0 = f2fx(Pos.x - Size.x / 2.0f);
	int X1 = f2fx(Pos.x + Size.x / 2.0f);
	int XC = f2fx(Pos.x);
	int Y0 = f2fx(Pos.y - Size.y / 2.0f);
	int Y1 = f2fx(Pos.y + Size.y / 2.0f);
	int YC = f2fx(Pos.y);

	pQuad->m_Pos = ivec2(XC, YC);

	pQuad->m_aPoints[0].x = pQuad->m_aPoints[2].x = X0;
	pQuad->m_aPoints[1].x = pQuad->m_aPoints[3].x = X1;
	pQuad->m_aPoints[0].y = pQuad->m_aPoints[1].y = Y0;
	pQuad->m_aPoints[2].y = pQuad->m_aPoints[3].y = Y1;

	for(int i = 0; i < 4; i++)
	{
		pQuad->m_aColors[i] = Color;
	}

	pQuad->m_aTexcoords[0].x = pQuad->m_aTexcoords[2].x = 0;
	pQuad->m_aTexcoords[1].x = pQuad->m_aTexcoords[3].x = 1024;
	pQuad->m_aTexcoords[0].y = pQuad->m_aTexcoords[1].y = 0;
	pQuad->m_aTexcoords[2].y = pQuad->m_aTexcoords[3].y = 1024;

	return pQuad;
}

static std::mutex s_TextMutex;
SText *SLayerText::AddText(const char *pText, int Size, ivec2 Pos, bool Outline, bool Center)
{
	s_TextMutex.lock();
	m_vpText.add(new SText());
	SText *pTextObj = m_vpText[m_vpText.size() - 1];

	s_TextMutex.unlock();

	str_copy(pTextObj->m_aText, pText, sizeof(pTextObj->m_aText));
	pTextObj->m_Size = Size;
	pTextObj->m_Pos = Pos;
	pTextObj->m_Outline = Outline;
	pTextObj->m_Center = Center;

	return pTextObj;
}

static int AdjustOutlineThicknessToFontSize(int OutlineThickness, int FontSize)
{
	if(FontSize > 36)
		OutlineThickness *= 4;
	else if(FontSize >= 18)
		OutlineThickness *= 2;
	return OutlineThickness;
}

void CMapCreater::GenerateQuadsFromTextLayer(SLayerText *pText, array<CQuad> &vpQuads)
{
	for(auto &pText : pText->m_vpText)
	{
		int MaxHeight = 0;
		int MaxWidth = 0;

		for(int Step = 0; Step < 2; Step++) // 0 = get width, 1 = render
		{
			if(Step == 0 && !pText->m_Center)
				continue;

			ivec2 Beginning = pText->m_Pos;
			ivec2 Pos = Beginning;

			const char *pTextStr = pText->m_aText;
			int Char;

			while((Char = str_utf8_decode(&pTextStr)) > 0)
			{
				if(Char == '\0')
					break;

				// find face
				FT_Face Face = NULL;
				FT_ULong GlyphIndex = 0;

				for(auto FtFace : m_vFontFaces)
				{
					FT_ULong FtChar = Char;
					if(FtChar == '\n')
						FtChar = ' ';
					GlyphIndex = FT_Get_Char_Index(FtFace, (FT_ULong) FtChar);
					if(GlyphIndex)
					{
						Face = FtFace;
						break;
					}
				}

				FT_Set_Char_Size(Face, 0, pText->m_Size * 64, 0, 96);

				// render
				FT_BitmapGlyph Glyph;
				FT_Load_Glyph(Face, GlyphIndex, FT_LOAD_NO_BITMAP);
				FT_Get_Glyph(Face->glyph, (FT_Glyph *) &Glyph);
				FT_Glyph_To_Bitmap((FT_Glyph *) &Glyph, FT_RENDER_MODE_NORMAL, 0, true);

				FT_Bitmap *pBitmap;
				pBitmap = &Glyph->bitmap;

				int Width, Height, BitmapLeft, BitmapTop;
				Width = pBitmap->width;
				Height = pBitmap->rows;
				BitmapLeft = Face->glyph->bitmap_left;
				BitmapTop = Face->glyph->bitmap_top;

				MaxHeight = maximum(MaxHeight, Height);

				if(Char == '\n')
				{
					Beginning.y += MaxHeight * 1.2f;
					Pos = Beginning;
					if(Step == 0)
					{
						MaxWidth = 0;
					}
					continue;
				}

				if(Step == 0)
				{
					MaxWidth += Face->glyph->advance.x / 64;
					continue;
				}

				ivec2 StartPos = ivec2(Pos.x - MaxWidth / 2, Pos.y);

				StartPos.x += BitmapLeft;
				StartPos.y -= BitmapTop;

				// create outline
				if(!pText->m_Outline)
				{
					for(int x = 0; x < Width; x++)
					{
						for(int y = 0; y < Height; y++)
						{
							unsigned char Alpha = pBitmap->buffer[y * Width + x];
							if(Alpha == 0)
								continue;

							ivec2 Pos = ivec2(StartPos.x + x, StartPos.y + y);
							CQuad Quad;

							for(int i = 0; i < 4; i++)
							{
								Quad.m_aColors[i].r = 255;
								Quad.m_aColors[i].g = 255;
								Quad.m_aColors[i].b = 255;
								Quad.m_aColors[i].a = Alpha;

								Quad.m_aTexcoords[i].x = 0;
								Quad.m_aTexcoords[i].y = 0;
							}
							Quad.m_aPoints[0].x = f2fx(Pos.x);
							Quad.m_aPoints[0].y = f2fx(Pos.y);

							Quad.m_aPoints[1].x = f2fx(Pos.x + 1);
							Quad.m_aPoints[1].y = f2fx(Pos.y);

							Quad.m_aPoints[2].x = f2fx(Pos.x);
							Quad.m_aPoints[2].y = f2fx(Pos.y + 1);

							Quad.m_aPoints[3].x = f2fx(Pos.x + 1);
							Quad.m_aPoints[3].y = f2fx(Pos.y + 1);

							Quad.m_aPoints[4].x = f2fx(Pos.x);
							Quad.m_aPoints[4].y = f2fx(Pos.y);

							Quad.m_ColorEnv = -1;
							Quad.m_ColorEnvOffset = 0;

							Quad.m_PosEnv = -1;
							Quad.m_PosEnvOffset = 0;

							vpQuads.add(Quad);
						}
					}
					Pos.x += Face->glyph->advance.x / 64;
					Pos.y += Face->glyph->advance.y / 64;

					FT_Done_Glyph((FT_Glyph) Glyph);

					continue;
				}
				FT_BitmapGlyph OutlineGlyph;
				int OutlineThickness = AdjustOutlineThicknessToFontSize(1, pText->m_Size);

				FT_Stroker_Set(m_FtStroker, (OutlineThickness) * 64 + 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
				FT_Get_Glyph(Face->glyph, (FT_Glyph *) &OutlineGlyph);
				FT_Glyph_Stroke((FT_Glyph *) &OutlineGlyph, m_FtStroker, true);
				FT_Glyph_To_Bitmap((FT_Glyph *) &OutlineGlyph, FT_RENDER_MODE_NORMAL, 0, true);

				int OutlinedPositionX = Pos.x - MaxWidth / 2 + Face->glyph->bitmap_left - OutlineThickness;
				int OutlinedPositionY = Pos.y - Face->glyph->bitmap_top - OutlineThickness;

				Width = OutlineGlyph->bitmap.width;
				Height = OutlineGlyph->bitmap.rows;

				for(int x = 0; x < Width; x++)
				{
					for(int y = 0; y < Height; y++)
					{
						int Alpha = (int) (OutlineGlyph->bitmap.buffer[y * Width + x]) * 3 / 10;
						if(Alpha == 0)
							continue;

						ivec2 Pos = ivec2(OutlinedPositionX + x, OutlinedPositionY + y);
						CQuad Quad;

						for(int i = 0; i < 4; i++)
						{
							Quad.m_aColors[i].r = 0;
							Quad.m_aColors[i].g = 0;
							Quad.m_aColors[i].b = 0;
							Quad.m_aColors[i].a = Alpha;

							Quad.m_aTexcoords[i].x = 0;
							Quad.m_aTexcoords[i].y = 0;
						}
						Quad.m_aPoints[0].x = f2fx(Pos.x);
						Quad.m_aPoints[0].y = f2fx(Pos.y);

						Quad.m_aPoints[1].x = f2fx(Pos.x + 1);
						Quad.m_aPoints[1].y = f2fx(Pos.y);

						Quad.m_aPoints[2].x = f2fx(Pos.x);
						Quad.m_aPoints[2].y = f2fx(Pos.y + 1);

						Quad.m_aPoints[3].x = f2fx(Pos.x + 1);
						Quad.m_aPoints[3].y = f2fx(Pos.y + 1);

						Quad.m_aPoints[4].x = f2fx(Pos.x);
						Quad.m_aPoints[4].y = f2fx(Pos.y);

						Quad.m_ColorEnv = -1;
						Quad.m_ColorEnvOffset = 0;

						Quad.m_PosEnv = -1;
						Quad.m_PosEnvOffset = 0;

						vpQuads.add(Quad);
					}
				}

				Width = pBitmap->width;
				Height = pBitmap->rows;
				for(int x = 0; x < Width; x++)
				{
					for(int y = 0; y < Height; y++)
					{
						unsigned char Alpha = pBitmap->buffer[y * Width + x];
						if(Alpha == 0)
							continue;

						ivec2 Pos = ivec2(StartPos.x + x, StartPos.y + y);
						CQuad Quad;

						for(int i = 0; i < 4; i++)
						{
							Quad.m_aColors[i].r = 255;
							Quad.m_aColors[i].g = 255;
							Quad.m_aColors[i].b = 255;
							Quad.m_aColors[i].a = Alpha;

							Quad.m_aTexcoords[i].x = 0;
							Quad.m_aTexcoords[i].y = 0;
						}
						Quad.m_aPoints[0].x = f2fx(Pos.x);
						Quad.m_aPoints[0].y = f2fx(Pos.y);

						Quad.m_aPoints[1].x = f2fx(Pos.x + 1);
						Quad.m_aPoints[1].y = f2fx(Pos.y);

						Quad.m_aPoints[2].x = f2fx(Pos.x);
						Quad.m_aPoints[2].y = f2fx(Pos.y + 1);

						Quad.m_aPoints[3].x = f2fx(Pos.x + 1);
						Quad.m_aPoints[3].y = f2fx(Pos.y + 1);

						Quad.m_aPoints[4].x = f2fx(Pos.x);
						Quad.m_aPoints[4].y = f2fx(Pos.y);

						Quad.m_ColorEnv = -1;
						Quad.m_ColorEnvOffset = 0;

						Quad.m_PosEnv = -1;
						Quad.m_PosEnvOffset = 0;

						vpQuads.add(Quad);
					}
				}

				Pos.x += Face->glyph->advance.x / 64;
				Pos.y += Face->glyph->advance.y / 64;

				FT_Done_Glyph((FT_Glyph) Glyph);
				FT_Done_Glyph((FT_Glyph) OutlineGlyph);
			}
		}
	}
}

static const char *GetMapByMapType(EMapType MapType)
{
	switch(MapType)
	{
	case EMapType::MAPTYPE_NORMAL: return "generatedmaps";
	}
	return "maps";
}

bool CMapCreater::SaveMap(EMapType MapType, const char *pMap)
{
	CDataFileWriter DataFile;

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s.map", GetMapByMapType(MapType), pMap);

	if(!DataFile.Open(Storage(), aPath))
	{
		dbg_msg("mapcreater", "failed to open file '%s'...", aPath);
		return false;
	}

	// save version
	{
		CMapItemVersion Item;
		Item.m_Version = CMapItemVersion::CURRENT_VERSION;
		DataFile.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(CMapItemVersion), &Item);
	}

	// save map info
	{
		CMapItemInfo Item;
		Item.m_Version = 1;
		Item.m_Author = -1;
		Item.m_MapVersion = -1;
		Item.m_Credits = -1;
		Item.m_License = -1;

		DataFile.AddItem(MAPITEMTYPE_INFO, 0, sizeof(CMapItemInfo), &Item);
	}

	int NumGroups, NumLayers, NumImages;
	NumGroups = 0;
	NumLayers = 0;
	NumImages = 0;

	for(auto &pImage : m_vpImages)
	{
		CMapItemImage Item;
		Item.m_Version = CMapItemImage::CURRENT_VERSION;
		Item.m_MustBe1 = 1;

		Item.m_External = pImage->m_External;
		Item.m_Width = pImage->m_Width;
		Item.m_Height = pImage->m_Height;
		Item.m_ImageName = DataFile.AddData(str_length(pImage->m_aName) + 1, pImage->m_aName);

		if(pImage->m_pImageData)
		{
			Item.m_ImageData = DataFile.AddData(pImage->m_Width * pImage->m_Height * 4, pImage->m_pImageData);
		}
		else
		{
			Item.m_ImageData = -1;
		}
		pImage->m_ImageID = NumImages;

		DataFile.AddItem(MAPITEMTYPE_IMAGE, NumImages++, sizeof(CMapItemImage), &Item);
	}

	for(auto &pGroup : m_vpGroups)
	{
		// add group
		{
			CMapItemGroup Item;
			Item.m_Version = CMapItemGroup::CURRENT_VERSION;
			Item.m_ParallaxX = pGroup->m_ParallaxX;
			Item.m_ParallaxY = pGroup->m_ParallaxY;
			Item.m_OffsetX = pGroup->m_OffsetX;
			Item.m_OffsetY = pGroup->m_OffsetY;
			Item.m_StartLayer = NumLayers;
			Item.m_NumLayers = (int) pGroup->m_vpLayers.size();
			Item.m_UseClipping = pGroup->m_UseClipping ? 1 : 0;
			Item.m_ClipX = pGroup->m_ClipX;
			Item.m_ClipY = pGroup->m_ClipY;
			Item.m_ClipW = pGroup->m_ClipW;
			Item.m_ClipH = pGroup->m_ClipH;
			StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pGroup->m_aName);

			DataFile.AddItem(MAPITEMTYPE_GROUP, NumGroups++, sizeof(CMapItemGroup), &Item);
		}

		for(auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == ELayerType::TILES)
			{
				SLayerTilemap *pTilemap = (SLayerTilemap *) pLayer;

				CMapItemLayerTilemap Item;

				Item.m_Version = CMapItemLayerTilemap::CURRENT_VERSION;

				Item.m_Layer.m_Version = 0;
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = LAYERTYPE_TILES;

				Item.m_Color.r = pTilemap->m_Color.r;
				Item.m_Color.g = pTilemap->m_Color.g;
				Item.m_Color.b = pTilemap->m_Color.b;
				Item.m_Color.a = pTilemap->m_Color.a;

				Item.m_ColorEnv = -1;
				Item.m_ColorEnvOffset = 0;

				Item.m_Width = pTilemap->m_Width;
				Item.m_Height = pTilemap->m_Height;
				Item.m_Flags = pTilemap->m_Flags;
				Item.m_Image = pTilemap->m_pImage ? pTilemap->m_pImage->m_ImageID : -1;

				Item.m_Data = DataFile.AddData(Item.m_Width * Item.m_Height * sizeof(CTile), pTilemap->m_pTiles);

				StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pTilemap->m_aName);

				DataFile.AddItem(MAPITEMTYPE_LAYER, NumLayers++, sizeof(CMapItemLayerTilemap), &Item);
			}
			else if(pLayer->m_Type == ELayerType::QUADS)
			{
				SLayerQuads *pQuads = (SLayerQuads *) pLayer;

				CMapItemLayerQuads Item;

				Item.m_Version = CMapItemLayerQuads::CURRENT_VERSION;

				Item.m_Layer.m_Version = 0;
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = LAYERTYPE_QUADS;

				Item.m_Image = pQuads->m_pImage ? pQuads->m_pImage->m_ImageID : -1;
				Item.m_NumQuads = pQuads->m_vpQuads.size();

				std::vector<CQuad> vQuads;
				for(auto &pOriginQuad : pQuads->m_vpQuads)
				{
					vQuads.push_back(CQuad());
					CQuad *pQuad = &vQuads[vQuads.size() - 1];

					for(int i = 0; i < 4; i++)
					{
						pQuad->m_aColors[i].r = pOriginQuad->m_aColors[i].r;
						pQuad->m_aColors[i].g = pOriginQuad->m_aColors[i].g;
						pQuad->m_aColors[i].b = pOriginQuad->m_aColors[i].b;
						pQuad->m_aColors[i].a = pOriginQuad->m_aColors[i].a;

						pQuad->m_aPoints[i].x = pOriginQuad->m_aPoints[i].x;
						pQuad->m_aPoints[i].y = pOriginQuad->m_aPoints[i].y;

						pQuad->m_aTexcoords[i].x = pOriginQuad->m_aTexcoords[i].x;
						pQuad->m_aTexcoords[i].y = pOriginQuad->m_aTexcoords[i].y;
					}

					pQuad->m_aPoints[4].x = pOriginQuad->m_Pos.x;
					pQuad->m_aPoints[4].y = pOriginQuad->m_Pos.y;

					pQuad->m_ColorEnv = -1;
					pQuad->m_ColorEnvOffset = 0;

					pQuad->m_PosEnv = -1;
					pQuad->m_PosEnvOffset = 0;
				}

				StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pQuads->m_aName);
				Item.m_Data = DataFile.AddDataSwapped((int) vQuads.size() * sizeof(CQuad), vQuads.data());

				DataFile.AddItem(MAPITEMTYPE_LAYER, NumLayers++, sizeof(CMapItemLayerQuads), &Item);
			}
			else if(pLayer->m_Type == ELayerType::TEXT)
			{
				SLayerText *pText = (SLayerText *) pLayer;

				CMapItemLayerQuads Item;

				Item.m_Version = CMapItemLayerQuads::CURRENT_VERSION;

				Item.m_Layer.m_Version = 0;
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = LAYERTYPE_QUADS;

				array<CQuad> vQuads;
				GenerateQuadsFromTextLayer(pText, vQuads);

				Item.m_Image = -1;
				Item.m_NumQuads = (int) vQuads.size();

				StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pText->m_aName);
				Item.m_Data = DataFile.AddDataSwapped((int) vQuads.size() * sizeof(CQuad), vQuads.base_ptr());

				DataFile.AddItem(MAPITEMTYPE_LAYER, NumLayers++, sizeof(CMapItemLayerQuads), &Item);
			}
		}
	}

	DataFile.Finish();

	return true;
}
