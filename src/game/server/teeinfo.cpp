#include <base/math.h>

#include <engine/console.h>
#include <engine/shared/jsonparser.h>

#include "teeinfo.h"

#include <array>

enum
{
	NUM_COLOR_COMPONENTS = 4,
};

static STeeInfo g_aStdSkins[] = {
	{{"standard", "", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {1798004, 0, 0, 1799582, 1869630, 0}},
	{{"kitty", "whisker", "", "standard", "standard", "negative"}, {true, true, false, true, true, true}, {8681144, -8229413, 0, 7885547, 8868585, 9043712}},
	{{"standard", "stripes", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {10187898, 0, 0, 750848, 1944919, 0}},
	{{"bear", "bear", "hair", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {1082745, -15634776, 0, 1082745, 1147174, 0}},
	{{"standard", "cammo2", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {5334342, -11771603, 0, 750848, 1944919, 0}},
	{{"standard", "cammostripes", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {5334342, -14840320, 0, 750848, 1944919, 0}},
	{{"koala", "twinbelly", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {184, -15397662, 0, 184, 9765959, 0}},
	{{"kitty", "whisker", "", "standard", "standard", "negative"}, {true, true, false, true, true, true}, {4612803, -12229920, 0, 3827951, 3827951, 8256000}},
	{{"standard", "whisker", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {15911355, -801066, 0, 15043034, 15043034, 0}},
	{{"standard", "donny", "unibop", "standard", "standard", "standard"}, {true, true, true, true, true, false}, {16177260, -16590390, 16177260, 16177260, 7624169, 0}},
	{{"standard", "stripe", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {16307835, 0, 0, 184, 9765959, 0}},
	{{"standard", "saddo", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {7171455, -9685436, 0, 3640746, 5792119, 0}},
	{{"standard", "toptri", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {6119331, 0, 0, 3640746, 5792119, 0}},
	{{"standard", "duodonny", "twinbopp", "standard", "standard", "standard"}, {true, true, true, true, true, false}, {15310519, -1600806, 15310519, 15310519, 37600, 0}},
	{{"standard", "twintri", "", "standard", "standard", "standard"}, {true, true, false, true, true, false}, {3447932, -14098717, 0, 185, 9634888, 0}},
	{{"standard", "warpaint", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {1944919, 0, 0, 750337, 1944919, 0}}};

static const char *const s_apSkinPartNames[NUM_SKINPARTS] = {"body", "marking", "decoration", "hands", "feet", "eyes"};
static const char *const s_apColorComponents[NUM_COLOR_COMPONENTS] = {"hue", "sat", "lgt", "alp"};

void ReadInfoByJson(IStorage *pStorage, const char *pSkinName, STeeInfo &TeeInfos)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "skins/%s.json", pSkinName);
	CJsonParser JsonParser;
	const json_value *pJsonData = JsonParser.ParseFile(aBuf, pStorage);
	if(pJsonData == 0)
	{
		dbg_msg("skins", "failed to load skin '%s': %s", pSkinName, JsonParser.Error());
		return;
	}

	mem_zero(&TeeInfos, sizeof(TeeInfos));
	// extract data
	const json_value &rStart = (*pJsonData)["skin"];
	if(rStart.type == json_object)
	{
		for(int PartIndex = 0; PartIndex < NUM_SKINPARTS; ++PartIndex)
		{
			const json_value &rPart = rStart[(const char *) s_apSkinPartNames[PartIndex]];
			if(rPart.type != json_object)
				continue;

			// name
			const json_value &rFilename = rPart["name"];
			str_copy(TeeInfos.m_aaSkinPartNames[PartIndex], rFilename, sizeof(TeeInfos.m_aaSkinPartNames[PartIndex]));

			// use custom colors
			bool UseCustomColors = false;
			const json_value &rColor = rPart["custom_colors"];
			if(rColor.type == json_string)
				UseCustomColors = str_comp((const char *) rColor, "true") == 0;
			else if(rColor.type == json_boolean)
				UseCustomColors = rColor.u.boolean;
			TeeInfos.m_aUseCustomColors[PartIndex] = UseCustomColors;

			// color components
			if(!UseCustomColors)
				continue;

			for(int i = 0; i < NUM_COLOR_COMPONENTS; i++)
			{
				if(PartIndex != SKINPART_MARKING && i == 3)
					continue;

				const json_value &rComponent = rPart[(const char *) s_apColorComponents[i]];
				if(rComponent.type == json_integer)
				{
					switch(i)
					{
					case 0: TeeInfos.m_aSkinPartColors[PartIndex] = (TeeInfos.m_aSkinPartColors[PartIndex] & 0xFF00FFFF) | (rComponent.u.integer << 16); break;
					case 1: TeeInfos.m_aSkinPartColors[PartIndex] = (TeeInfos.m_aSkinPartColors[PartIndex] & 0xFFFF00FF) | (rComponent.u.integer << 8); break;
					case 2: TeeInfos.m_aSkinPartColors[PartIndex] = (TeeInfos.m_aSkinPartColors[PartIndex] & 0xFFFFFF00) | rComponent.u.integer; break;
					case 3: TeeInfos.m_aSkinPartColors[PartIndex] = (TeeInfos.m_aSkinPartColors[PartIndex] & 0x00FFFFFF) | (rComponent.u.integer << 24); break;
					}
				}
			}
		}
	}
}

STeeInfo GenerateRandomSkin()
{
	return g_aStdSkins[random_int() % std::size(g_aStdSkins)];
}
