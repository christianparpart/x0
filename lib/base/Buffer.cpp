/* <x0/Buffer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Buffer.h>
#include <cstdio>

namespace x0 {

bool Buffer::setCapacity(std::size_t value)
{
	if (value > capacity_) {
		// pad up to CHUNK_SIZE
		value = value - 1;
		value = value + CHUNK_SIZE - (value % CHUNK_SIZE);
	} else if (value < capacity_) {
		// possibly adjust the actual used size
		if (value < size_)
			size_ = value;
	} else
		// nothing changed
		return true;

	capacity_ = value;

	if (capacity_)
		data_ = static_cast<value_type *>(std::realloc(data_, capacity_));
	else if (data_)
		std::free(data_);

	return true;
}

void Buffer::dump(const void *bytes, std::size_t length, const char *description)
{
	static char hex[] = "0123456789ABCDEF";
	const int BLOCK_SIZE = 8;
	const int BLOCK_COUNT = 4;

	//12 34 56 78   12 34 56 78   12 34 56 78   12 34 56 78   12345678abcdef
	const int HEX_WIDTH = (BLOCK_SIZE + 1) * 3 * BLOCK_COUNT;
	const int PLAIN_WIDTH = BLOCK_SIZE * BLOCK_COUNT;
	char line[HEX_WIDTH + PLAIN_WIDTH + 1];

	const char *p = (const char *)bytes;

	if (description && *description)
		printf("%s (%ld bytes):\n", description, length);
	else
		printf("Memory dump (%ld bytes):\n", length);

	while (length > 0)
	{
		char *u = line;
		char *v = u + HEX_WIDTH;

		int blockNr = 0;
		for (; blockNr < BLOCK_COUNT && length > 0; ++blockNr)
		{
			// block
			int i = 0;
			for (; i < BLOCK_SIZE && length > 0; ++i)
			{
				// hex frame
				*u++ = hex[(*p>>4) & 0x0F];
				*u++ = hex[*p & 0x0F];
				*u++ = ' '; // byte separator

				// plain text frame
				*v++ = std::isprint(*p) ? *p : '.';

				++p;
				--length;
			}

			for (; i < BLOCK_SIZE; ++i)
			{
				*u++ = ' ';
				*u++ = ' ';
				*u++ = ' ';
			}

			// block separator
			*u++ = ' ';
			*u++ = ' ';
			*u++ = ' ';
		}

		for (; blockNr < BLOCK_COUNT; ++blockNr)
		{
			for (int i = 0; i < BLOCK_SIZE; ++i)
			{
				*u++ = ' ';
				*u++ = ' ';
				*u++ = ' ';
			}
			*u++ = ' ';
			*u++ = ' ';
			*u++ = ' ';
		}

		// EOS
		*v = '\0';

		printf("%s\n", line);
	}
}

} // namespace x0
