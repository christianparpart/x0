/* <x0/AnsiColor.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/AnsiColor.h>
#include <x0/Buffer.h>

namespace x0 {

/**
 * \class AnsiColor
 * \brief provides an API to ANSI colouring.
 */

/**
 * constructs the ANSII color indicator.
 * \param AColor a bitmask of colors/flags to create the ANSII sequence for
 * \return the ANSII sequence representing the colors/flags passed.
 */
std::string AnsiColor::make(Type AColor)
{
	Buffer sb;
	int i = 0;

	sb.push_back("\x1B["); // XXX: '\e' = 0x1B

	if (AColor)
	{
		// special flags
		if (AColor & AllFlags)
		{
			for (int k = 0; k < 8; ++k)
			{
				if (AColor & (1 << k)) // FIXME
				{
					if (i++) sb.push_back(';');
					sb.push_back(k + 1);
				}
			}
		}

		// forground color
		if (AColor & AnyFg)
		{
			if (i++) sb.push_back(';');
			sb.push_back(((AColor >> 8) & 0x0F) + 29);
		}

		// background color
		if (AColor & AnyBg)
		{
			if (i++) sb.push_back(';');
			sb.push_back(((AColor >> 12) & 0x0F) + 39);
		}
	}
	else
		sb.push_back('0'); // Clear

	sb.push_back("m");

	return sb.str();
}

/**
 * constructs a coloured string.
 * \param AColor the colors/flags bitmask to colorize the given text in.
 * \param AText the text to be colourized.
 * \return the given text colourized in the expected colours/flags.
 */
std::string AnsiColor::colorize(Type AColor, const std::string& AText) {
	return make(AColor) + AText + make(Clear);
}

}
