#pragma once

namespace gdipp
{
namespace server_config
{

enum RENDERER_TYPE
{
	RENDERER_CLEARTYPE = 0,
	RENDERER_FREETYPE = 10,
	RENDERER_GETGLYPHOUTLINE = 20,
	RENDERER_DIRECTWRITE = 30,
	RENDERER_WIC = 31
};

static const unsigned char AUTO_HINTING = 1;
static const unsigned int CACHE_SIZE = 8;
static const bool EMBEDDED_BITMAP = false;
static const long EMBOLDEN = 0;
static const FT_LcdFilter LCD_FILTER = FT_LCD_FILTER_DEFAULT;
static const unsigned char HINTING = 1;
static const bool KERNING = false;
static const unsigned char RENDER_MODE_MONO = 0;
static const unsigned char RENDER_MODE_GRAY = 1;
static const unsigned char RENDER_MODE_SUBPIXEL = 1;
static const bool RENDER_MODE_ALIASED = false;
static const RENDERER_TYPE RENDERER = RENDERER_FREETYPE;

}
}
