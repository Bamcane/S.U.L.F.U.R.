#include <base/math.h>

#include "teeinfo.h"

#include <array>

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

STeeInfo GenerateRandomSkin()
{
	return g_aStdSkins[random_int() % std::size(g_aStdSkins)];
}
