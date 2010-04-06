#ifndef sw_x0_ansi_color_hpp
#define sw_x0_ansi_color_hpp (1)

#include <x0/api.hpp>
#include <string>

namespace x0 {

class X0_API ansi_color {
public:
	enum color_type {
		ccClear 	= 0,
		ccReset 	= ccClear,

		ccBold 		= 0x0001, 		// 1
		ccDark 		= 0x0002, 		// 2
		ccUndef1 	= 0x0004,
		ccUnderline = 0x0008, 		// 4
		ccBlink 	= 0x0010, 		// 5
		ccUndef2    = 0x0020,
		ccReverse 	= 0x0040, 		// 7
		ccConcealed = 0x0080, 		// 8
		ccAllFlags	= 0x00FF,

		ccBlack 	= 0x0100,
		ccRed 		= 0x0200,
		ccGreen 	= 0x0300,
		ccYellow 	= 0x0400,
		ccBlue 		= 0x0500,
		ccMagenta 	= 0x0600,
		ccCyan 		= 0x0700,
		ccWhite 	= 0x0800,
		ccAnyFg 	= 0x0F00,

		ccOnBlack 	= 0x1000,
		ccOnRed 	= 0x2000,
		ccOnGreen 	= 0x3000,
		ccOnYellow 	= 0x4000,
		ccOnBlue 	= 0x5000,
		ccOnMagenta = 0x6000,
		ccOnCyan 	= 0x7000,
		ccOnWhite 	= 0x8000,
		ccAnyBg 	= 0xF000
	};

	static std::string make(color_type AColor);
	static std::string colorize(color_type AColor, const std::string& AText);
};

inline X0_API ansi_color::color_type operator|(ansi_color::color_type a, ansi_color::color_type b) {
	return ansi_color::color_type(int(a) | int(b));
}

} // namespace x0

#endif
