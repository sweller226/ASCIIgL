#pragma once

// just a bunch of enums for different colours, taken from a series on OneLoneCoder.com
enum COLOUR
{	
	FG_WHITE = 0x000F,
	FG_GREY = 0x0007,
	FG_DARK_GREY = 0x0008,
	FG_BLACK = 0x0000,

	FG_DARK_BLUE = 0x0001,
	FG_DARK_GREEN = 0x0002,
	FG_DARK_CYAN = 0x0003,
	FG_DARK_RED = 0x0004,
	FG_DARK_MAGENTA = 0x0005,
	FG_DARK_YELLOW = 0x0006,
	FG_BLUE = 0x0009,
	FG_GREEN = 0x000A,
	FG_CYAN = 0x000B,
	FG_RED = 0x000C,
	FG_MAGENTA = 0x000D,
	FG_YELLOW = 0x000E,

	BG_WHITE = 0x00F0,
	BG_GREY = 0x0070,
	BG_DARK_GREY = 0x0080,
	BG_BLACK = 0x0000,

	BG_DARK_BLUE = 0x0010,
	BG_DARK_GREEN = 0x0020,
	BG_DARK_CYAN = 0x0030,
	BG_DARK_RED = 0x0040,
	BG_DARK_MAGENTA = 0x0050,
	BG_DARK_YELLOW = 0x0060,
	BG_BLUE = 0x0090,
	BG_GREEN = 0x00A0,
	BG_CYAN = 0x00B0,
	BG_RED = 0x00C0,
	BG_MAGENTA = 0x00D0,
	BG_YELLOW = 0x00E0,
};

// just a bunch of pixels types defined for easy use, pixel solid is percieved as brighter than pixel quarter
// Using hashtag (#) character which is universally available in all fonts
enum PIXEL_TYPE
{
    PIXEL_FULL = 219,         // █ (Full block - DOS char 219)
    PIXEL_THREEQUARTERS = 178, // ▓ (Dark shade - DOS char 178)
    PIXEL_HALF = 177,          // ▒ (Medium shade - DOS char 177) 
    PIXEL_QUARTER = 176,       // ░ (Light shade - DOS char 176)
};
