#include "hexdump.h"

int hexdump(void const *data, size_t length, int linelen, int split)
{
	char buffer[512];
	char *ptr;
	const void *inptr;
	int pos;
	int remaining = length;

	inptr = data;

	/*
	 *	Assert that the buffer is large enough. This should pretty much
	 *	always be the case...
	 *
	 *	hex/ascii gap (2 chars) + closing \0 (1 char)
	 *	split = 4 chars (2 each for hex/ascii) * number of splits
	 *
	 *	(hex = 3 chars, ascii = 1 char) * linelen number of chars
	 */
	assert(sizeof(buffer) >= (3 + (4 * (linelen / split)) + (linelen * 4)));

	/*
	 *	Loop through each line remaining
	 */
	while (remaining > 0) {
		int lrem;
		int splitcount;
		ptr = buffer;

		/*
		 *	Loop through the hex chars of this line
		 */
		lrem = remaining;
		splitcount = 0;
		for (pos = 0; pos < linelen; pos++) {

			/* Split hex section if required */
			if (split == splitcount++) {
				sprintf(ptr, "  ");
				ptr += 2;
				splitcount = 1;
			}

			/* If still remaining chars, output, else leave a space */
			if (lrem) {
				sprintf(ptr, "%0.2x ", *((unsigned char *) inptr + pos));
				lrem--;
			} else {
				sprintf(ptr, "   ");
			}
			ptr += 3;
		}

		*ptr++ = ' ';
		*ptr++ = ' ';

		/*
		 *	Loop through the ASCII chars of this line
		 */
		lrem = remaining;
		splitcount = 0;
		for (pos = 0; pos < linelen; pos++) {
			unsigned char c;

			/* Split ASCII section if required */
			if (split == splitcount++) {
				sprintf(ptr, "  ");
				ptr += 2;
				splitcount = 1;
			}

			if (lrem) {
				c = *((unsigned char *) inptr + pos);
				if (c > 31 && c < 127) {
					sprintf(ptr, "%c", c);
				} else {
					sprintf(ptr, ".");
				}
				lrem--;
			}
			ptr++;
		}

		*ptr = '\0';
		printf("%s\n", buffer);

		inptr += linelen;
		remaining -= linelen;
	}

	return 0;
}
