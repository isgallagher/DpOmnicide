#include "quakedef.h"
#include "utf8lib.h"

/*
================================================================================
Initialization of UTF-8 support and new cvars.
================================================================================
*/
// for compatibility this defaults to 0
cvar_t    utf8_enable = {CVAR_SAVE, "utf8_enable", "0", "Enable UTF-8 support. For compatibility, this is disabled by default in most games."};

void   u8_Init(void)
{
	Cvar_RegisterVariable(&utf8_enable);
}

/*
================================================================================
UTF-8 encoding and decoding functions follow.
================================================================================
*/

unsigned char utf8_lengths[256] = { // 0 = invalid
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // ascii characters
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x80 - 0xBF are within multibyte sequences
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // they could be interpreted as 2-byte starts but
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // the codepoint would be < 127
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // C0 and C1 would also result in overlong encodings
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	// with F5 the codepoint is above 0x10FFFF,
	// F8-FB would start 5-byte sequences
	// FC-FD would start 6-byte sequences
	// ...
};
Uchar utf8_range[5] = {
	1,       // invalid - let's not allow the creation of 0-bytes :P
	1,       // ascii minimum
	0x80,    // 2-byte minimum
	0x800,   // 3-byte minimum
	0x10000, // 4-byte minimum
};

/** Analyze the next character and return various information if requested.
 * @param _s      An utf-8 string.
 * @param _start  Filled with the start byte-offset of the next valid character
 * @param _len    Fileed with the length of the next valid character
 * @param _ch     Filled with the unicode value of the next character
 * @param _maxlen Maximum number of bytes to read from _s
 * @return        Whether or not another valid character is in the string
 */
#define U8_ANALYZE_INFINITY 7
static qboolean u8_analyze(const char *_s, size_t *_start, size_t *_len, Uchar *_ch, size_t _maxlen)
{
	const unsigned char *s = (const unsigned char*)_s;
	size_t i, j;
	size_t bits = 0;
	Uchar ch;

	i = 0;
findchar:
	while (i < _maxlen && s[i] && (bits = utf8_lengths[s[i]]) == 0)
		++i;

	if (i >= _maxlen || !s[i]) {
		if (_start) *_start = i;
		if (_len) *_len = 0;
		return false;
	}
	
	if (bits == 1) { // ascii
		if (_start) *_start = i;
		if (_len) *_len = 1;
		if (_ch) *_ch = (Uchar)s[i];
		return true;
	}

	ch = (s[i] & (0xFF >> bits));
	for (j = 1; j < bits; ++j)
	{
		if ( (s[i+j] & 0xC0) != 0x80 )
		{
			i += j;
			goto findchar;
		}
		ch = (ch << 6) | (s[i+j] & 0x3F);
	}
	if (ch < utf8_range[bits] || ch >= 0x10FFFF)
	{
		i += bits;
		goto findchar;
	}
#if 0
	// <0xC2 is always an overlong encoding, they're invalid, thus skipped
	while (i < _maxlen && s[i] && s[i] >= 0x80 && s[i] < 0xC2) {
		//fprintf(stderr, "skipping\n");
		++i;
	}

	// If we hit the end, well, we're out and invalid
	if(i >= _maxlen || !s[i]) {
		if (_start) *_start = i;
		if (_len) *_len = 0;
		return false;
	}

	// I'll leave that in - if you remove it, also change the part below
	// to support 1-byte chars correctly
	if (s[i] < 0x80)
	{
		if (_start) *_start = i;
		if (_len) *_len = 1;
		if (_ch) *_ch = (Uchar)s[i];
		//fprintf(stderr, "valid ascii\n");
		return true;
	}

	// Figure out the next char's length
	bc = s[i];
	bits = 1;
	// count the 1 bits, they're the # of bytes
	for (bt = 0x40; bt && (bc & bt); bt >>= 1, ++bits);
	if (!bt)
	{
		//fprintf(stderr, "superlong\n");
		++i;
		goto findchar;
	}
	if(i + bits > _maxlen) {
		/*
		if (_start) *_start = i;
		if (_len) *_len = 0;
		return false;
		*/
		++i;
		goto findchar;
	}
	// turn bt into a mask and give ch a starting value
	--bt;
	ch = (s[i] & bt);
	// check the byte sequence for invalid bytes
	for (j = 1; j < bits; ++j)
	{
		// valid bit value: 10xx xxxx
		//if (s[i+j] < 0x80 || s[i+j] >= 0xC0)
		if ( (s[i+j] & 0xC0) != 0x80 )
		{
			//fprintf(stderr, "sequence of %i f'd at %i by %x\n", bits, j, (unsigned int)s[i+j]);
			// this byte sequence is invalid, skip it
			i += j;
			// find a character after it
			goto findchar;
		}
		// at the same time, decode the character
		ch = (ch << 6) | (s[i+j] & 0x3F);
	}

	// Now check the decoded byte for an overlong encoding
	if ( (bits >= 2 && ch < 0x80) ||
	     (bits >= 3 && ch < 0x800) ||
	     (bits >= 4 && ch < 0x10000) ||
	     ch >= 0x10FFFF // RFC 3629
		)
	{
		i += bits;
		//fprintf(stderr, "overlong: %i bytes for %x\n", bits, ch);
		goto findchar;
	}
#endif

	if (_start)
		*_start = i;
	if (_len)
		*_len = bits;
	if (_ch)
		*_ch = ch;
	//fprintf(stderr, "valid utf8\n");
	return true;
}

/** Get the number of characters in an UTF-8 string.
 * @param _s    An utf-8 encoded null-terminated string.
 * @return      The number of unicode characters in the string.
 */
size_t u8_strlen(const char *_s)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
		return strlen(_s);

	while (*s)
	{
		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s < 0xC2)
		{
			++s;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			break;
		// valid character, skip after it
		s += st + ln;
		++len;
	}
	return len;
}

static int colorcode_skipwidth(const unsigned char *s)
{
	if(*s == STRING_COLOR_TAG)
	{
		if(s[1] <= '9' && s[1] >= '0') // ^[0-9] found
		{
			return 2;
		}
		else if(s[1] == STRING_COLOR_RGB_TAG_CHAR &&
			((s[2] >= '0' && s[2] <= '9') || (s[2] >= 'a' && s[2] <= 'f') || (s[2] >= 'A' && s[2] <= 'F')) &&
			((s[3] >= '0' && s[3] <= '9') || (s[3] >= 'a' && s[3] <= 'f') || (s[3] >= 'A' && s[3] <= 'F')) &&
			((s[4] >= '0' && s[4] <= '9') || (s[4] >= 'a' && s[4] <= 'f') || (s[4] >= 'A' && s[4] <= 'F')))
		{
			return 5;
		}
		else if(s[1] == STRING_COLOR_TAG)
		{
			return 1; // special case, do NOT call colorcode_skipwidth for next char
		}
	}
	return 0;
}

/** Get the number of characters in a part of an UTF-8 string.
 * @param _s    An utf-8 encoded null-terminated string.
 * @param n     The maximum number of bytes.
 * @return      The number of unicode characters in the string.
 */
size_t u8_strnlen(const char *_s, size_t n)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
	{
		len = strlen(_s);
		return (len < n) ? len : n;
	}

	while (*s && n)
	{
		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			--n;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s < 0xC2)
		{
			++s;
			--n;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, n))
			break;
		// valid character, see if it's still inside the range specified by n:
		if (n < st + ln)
			return len;
		++len;
		n -= st + ln;
		s += st + ln;
	}
	return len;
}

static size_t u8_strnlen_colorcodes(const char *_s, size_t n)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	while (*s && n)
	{
		int w = colorcode_skipwidth(s);
		n -= w;
		s += w;
		if(w > 1) // == 1 means single caret
			continue;

		// ascii char, skip u8_analyze
		if (*s < 0x80 || !utf8_enable.integer)
		{
			++len;
			++s;
			--n;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s < 0xC2)
		{
			++s;
			--n;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, n))
			break;
		// valid character, see if it's still inside the range specified by n:
		if (n < st + ln)
			return len;
		++len;
		n -= st + ln;
		s += st + ln;
	}
	return len;
}

/** Get the number of bytes used in a string to represent an amount of characters.
 * @param _s    An utf-8 encoded null-terminated string.
 * @param n     The number of characters we want to know the byte-size for.
 * @return      The number of bytes used to represent n characters.
 */
size_t u8_bytelen(const char *_s, size_t n)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer) {
		len = strlen(_s);
		return (len < n) ? len : n;
	}

	while (*s && n)
	{
		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			--n;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s < 0xC2)
		{
			++s;
			++len;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			break;
		--n;
		s += st + ln;
		len += st + ln;
	}
	return len;
}

static size_t u8_bytelen_colorcodes(const char *_s, size_t n)
{
	size_t st, ln;
	size_t len = 0;
	const unsigned char *s = (const unsigned char*)_s;

	while (*s && n)
	{
		int w = colorcode_skipwidth(s);
		len += w;
		s += w;
		if(w > 1) // == 1 means single caret
			continue;

		// ascii char, skip u8_analyze
		if (*s < 0x80 || !utf8_enable.integer)
		{
			++len;
			++s;
			--n;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s < 0xC2)
		{
			++s;
			++len;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			break;
		--n;
		s += st + ln;
		len += st + ln;
	}
	return len;
}

/** Get the byte-index for a character-index.
 * @param _s      An utf-8 encoded string.
 * @param i       The character-index for which you want the byte offset.
 * @param len     If not null, character's length will be stored in there.
 * @return        The byte-index at which the character begins, or -1 if the string is too short.
 */
int u8_byteofs(const char *_s, size_t i, size_t *len)
{
	size_t st, ln;
	size_t ofs = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
	{
		if (strlen(_s) < i)
		{
			if (len) *len = 0;
			return -1;
		}

		if (len) *len = 1;
		return i;
	}

	st = ln = 0;
	do
	{
		ofs += ln;
		if (!u8_analyze((const char*)s + ofs, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			return -1;
		ofs += st;
	} while(i-- > 0);
	if (len)
		*len = ln;
	return ofs;
}

/** Get the char-index for a byte-index.
 * @param _s      An utf-8 encoded string.
 * @param i       The byte offset for which you want the character index.
 * @param len     If not null, the offset within the character is stored here.
 * @return        The character-index, or -1 if the string is too short.
 */
int u8_charidx(const char *_s, size_t i, size_t *len)
{
	size_t st, ln;
	size_t ofs = 0;
	size_t pofs = 0;
	int idx = 0;
	const unsigned char *s = (const unsigned char*)_s;

	if (!utf8_enable.integer)
	{
		if (len) *len = 0;
		return i;
	}

	while (ofs < i && s[ofs])
	{
		// ascii character, skip u8_analyze
		if (s[ofs] < 0x80)
		{
			pofs = ofs;
			++idx;
			++ofs;
			continue;
		}

		// invalid, skip u8_analyze
		if (s[ofs] < 0xC2)
		{
			++ofs;
			continue;
		}

		if (!u8_analyze((const char*)s+ofs, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			return -1;
		// see if next char is after the bytemark
		if (ofs + st > i)
		{
			if (len)
				*len = i - pofs;
			return idx;
		}
		++idx;
		pofs = ofs + st;
		ofs += st + ln;
		// see if bytemark is within the char
		if (ofs > i)
		{
			if (len)
				*len = i - pofs;
			return idx;
		}
	}
	if (len) *len = 0;
	return idx;
}

/** Get the byte offset of the previous byte.
 * The result equals:
 * prevchar_pos = u8_byteofs(text, u8_charidx(text, thischar_pos, NULL) - 1, NULL)
 * @param _s      An utf-8 encoded string.
 * @param i       The current byte offset.
 * @return        The byte offset of the previous character
 */
size_t u8_prevbyte(const char *_s, size_t i)
{
	size_t st, ln;
	const unsigned char *s = (const unsigned char*)_s;
	size_t lastofs = 0;
	size_t ofs = 0;

	if (!utf8_enable.integer)
	{
		if (i > 0)
			return i-1;
		return 0;
	}

	while (ofs < i && s[ofs])
	{
		// ascii character, skip u8_analyze
		if (s[ofs] < 0x80)
		{
			lastofs = ofs++;
			continue;
		}

		// invalid, skip u8_analyze
		if (s[ofs] < 0xC2)
		{
			++ofs;
			continue;
		}

		if (!u8_analyze((const char*)s+ofs, &st, &ln, NULL, U8_ANALYZE_INFINITY))
			return lastofs;
		if (ofs + st > i)
			return lastofs;
		if (ofs + st + ln >= i)
			return ofs + st;

		lastofs = ofs;
		ofs += st + ln;
	}
	return lastofs;
}

Uchar u8_quake2utf8map[256] = {
	0xE000, 0xE001, 0xE002, 0xE003, 0xE004, 0xE005, 0xE006, 0xE007, 0xE008, 0xE009, 0xE00A, 0xE00B, 0xE00C, 0xE00D, 0xE00E, 0xE00F, // specials
	0xE010, 0xE011, 0xE012, 0xE013, 0xE014, 0xE015, 0xE016, 0xE017, 0xE018, 0xE019, 0xE01A, 0xE01B, 0xE01C, 0xE01D, 0xE01E, 0xE01F, // specials
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, // shift+digit line
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, // digits
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, // caps
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F, // caps
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, // small
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F, // small
	0xE080, 0xE081, 0xE082, 0xE083, 0xE084, 0xE085, 0xE086, 0xE087, 0xE088, 0xE089, 0xE08A, 0xE08B, 0xE08C, 0xE08D, 0xE08E, 0xE08F, // specials
	0xE090, 0xE091, 0xE092, 0xE093, 0xE094, 0xE095, 0xE096, 0xE097, 0xE098, 0xE099, 0xE09A, 0xE09B, 0xE09C, 0xE09D, 0xE09E, 0xE09F, // faces
	0xE0A0, 0xE0A1, 0xE0A2, 0xE0A3, 0xE0A4, 0xE0A5, 0xE0A6, 0xE0A7, 0xE0A8, 0xE0A9, 0xE0AA, 0xE0AB, 0xE0AC, 0xE0AD, 0xE0AE, 0xE0AF,
	0xE0B0, 0xE0B1, 0xE0B2, 0xE0B3, 0xE0B4, 0xE0B5, 0xE0B6, 0xE0B7, 0xE0B8, 0xE0B9, 0xE0BA, 0xE0BB, 0xE0BC, 0xE0BD, 0xE0BE, 0xE0BF,
	0xE0C0, 0xE0C1, 0xE0C2, 0xE0C3, 0xE0C4, 0xE0C5, 0xE0C6, 0xE0C7, 0xE0C8, 0xE0C9, 0xE0CA, 0xE0CB, 0xE0CC, 0xE0CD, 0xE0CE, 0xE0CF,
	0xE0D0, 0xE0D1, 0xE0D2, 0xE0D3, 0xE0D4, 0xE0D5, 0xE0D6, 0xE0D7, 0xE0D8, 0xE0D9, 0xE0DA, 0xE0DB, 0xE0DC, 0xE0DD, 0xE0DE, 0xE0DF,
	0xE0E0, 0xE0E1, 0xE0E2, 0xE0E3, 0xE0E4, 0xE0E5, 0xE0E6, 0xE0E7, 0xE0E8, 0xE0E9, 0xE0EA, 0xE0EB, 0xE0EC, 0xE0ED, 0xE0EE, 0xE0EF,
	0xE0F0, 0xE0F1, 0xE0F2, 0xE0F3, 0xE0F4, 0xE0F5, 0xE0F6, 0xE0F7, 0xE0F8, 0xE0F9, 0xE0FA, 0xE0FB, 0xE0FC, 0xE0FD, 0xE0FE, 0xE0FF,
};

/** Fetch a character from an utf-8 encoded string.
 * @param _s      The start of an utf-8 encoded multi-byte character.
 * @param _end    Will point to after the first multi-byte character.
 * @return        The 32-bit integer representation of the first multi-byte character or 0 for invalid characters.
 */
Uchar u8_getchar_utf8_enabled(const char *_s, const char **_end)
{
	size_t st, ln;
	Uchar ch;

	if (!u8_analyze(_s, &st, &ln, &ch, U8_ANALYZE_INFINITY))
		ch = 0;
	if (_end)
		*_end = _s + st + ln;
	return ch;
}

/** Fetch a character from an utf-8 encoded string.
 * @param _s      The start of an utf-8 encoded multi-byte character.
 * @param _end    Will point to after the first multi-byte character.
 * @return        The 32-bit integer representation of the first multi-byte character or 0 for invalid characters.
 */
Uchar u8_getnchar_utf8_enabled(const char *_s, const char **_end, size_t _maxlen)
{
	size_t st, ln;
	Uchar ch;

	if (!u8_analyze(_s, &st, &ln, &ch, _maxlen))
		ch = 0;
	if (_end)
		*_end = _s + st + ln;
	return ch;
}

/** Encode a wide-character into utf-8.
 * @param w        The wide character to encode.
 * @param to       The target buffer the utf-8 encoded string is stored to.
 * @param maxlen   The maximum number of bytes that fit into the target buffer.
 * @return         Number of bytes written to the buffer not including the terminating null.
 *                 Less or equal to 0 if the buffer is too small.
 */
int u8_fromchar(Uchar w, char *to, size_t maxlen)
{
	if (maxlen < 1)
		return 0;

	if (!w)
		return 0;

	if (w >= 0xE000 && !utf8_enable.integer)
		w -= 0xE000;

	if (w < 0x80 || !utf8_enable.integer)
	{
		to[0] = (char)w;
		if (maxlen < 2)
			return -1;
		to[1] = 0;
		return 1;
	}
	// for a little speedup
	if (w < 0x800)
	{
		if (maxlen < 3)
		{
			to[0] = 0;
			return -1;
		}
		to[2] = 0;
		to[1] = 0x80 | (w & 0x3F); w >>= 6;
		to[0] = 0xC0 | w;
		return 2;
	}
	if (w < 0x10000)
	{
		if (maxlen < 4)
		{
			to[0] = 0;
			return -1;
		}
		to[3] = 0;
		to[2] = 0x80 | (w & 0x3F); w >>= 6;
		to[1] = 0x80 | (w & 0x3F); w >>= 6;
		to[0] = 0xE0 | w;
		return 3;
	}

	// RFC 3629
	if (w <= 0x10FFFF)
	{
		if (maxlen < 5)
		{
			to[0] = 0;
			return -1;
		}
		to[4] = 0;
		to[3] = 0x80 | (w & 0x3F); w >>= 6;
		to[2] = 0x80 | (w & 0x3F); w >>= 6;
		to[1] = 0x80 | (w & 0x3F); w >>= 6;
		to[0] = 0xF0 | w;
		return 4;
	}
	return 0;
}

/** uses u8_fromchar on a static buffer
 * @param ch        The unicode character to convert to encode
 * @param l         The number of bytes without the terminating null.
 * @return          A statically allocated buffer containing the character's utf8 representation, or NULL if it fails.
 */
char *u8_encodech(Uchar ch, size_t *l, char *buf16)
{
	size_t len;
	len = u8_fromchar(ch, buf16, 16);
	if (len > 0)
	{
		if (l) *l = len;
		return buf16;
	}
	return NULL;
}

/** Convert a utf-8 multibyte string to a wide character string.
 * @param wcs       The target wide-character buffer.
 * @param mb        The utf-8 encoded multibyte string to convert.
 * @param maxlen    The maximum number of wide-characters that fit into the target buffer.
 * @return          The number of characters written to the target buffer.
 */
size_t u8_mbstowcs(Uchar *wcs, const char *mb, size_t maxlen)
{
	size_t i;
	Uchar ch;
	if (maxlen < 1)
		return 0;
	for (i = 0; *mb && i < maxlen-1; ++i)
	{
		ch = u8_getchar(mb, &mb);
		if (!ch)
			break;
		wcs[i] = ch;
	}
	wcs[i] = 0;
	return i;
}

/** Convert a wide-character string to a utf-8 multibyte string.
 * @param mb      The target buffer the utf-8 string is written to.
 * @param wcs     The wide-character string to convert.
 * @param maxlen  The number bytes that fit into the multibyte target buffer.
 * @return        The number of bytes written, not including the terminating \0
 */
size_t u8_wcstombs(char *mb, const Uchar *wcs, size_t maxlen)
{
	size_t i;
	const char *start = mb;
	if (maxlen < 2)
		return 0;
	for (i = 0; wcs[i] && i < maxlen-1; ++i)
	{
		/*
		int len;
		if ( (len = u8_fromchar(wcs[i], mb, maxlen - i)) < 0)
			return (mb - start);
		mb += len;
		*/
		mb += u8_fromchar(wcs[i], mb, maxlen - i);
	}
	*mb = 0;
	return (mb - start);
}

/*
============
UTF-8 aware COM_StringLengthNoColors

calculates the visible width of a color coded string.

*valid is filled with TRUE if the string is a valid colored string (that is, if
it does not end with an unfinished color code). If it gets filled with FALSE, a
fix would be adding a STRING_COLOR_TAG at the end of the string.

valid can be set to NULL if the caller doesn't care.

For size_s, specify the maximum number of characters from s to use, or 0 to use
all characters until the zero terminator.
============
*/
size_t
COM_StringLengthNoColors(const char *s, size_t size_s, qboolean *valid);
size_t
u8_COM_StringLengthNoColors(const char *_s, size_t size_s, qboolean *valid)
{
	const unsigned char *s = (const unsigned char*)_s;
	const unsigned char *end;
	size_t len = 0;
	size_t st, ln;

	if (!utf8_enable.integer)
		return COM_StringLengthNoColors(_s, size_s, valid);

	end = size_s ? (s + size_s) : NULL;

	for(;;)
	{
		switch((s == end) ? 0 : *s)
		{
			case 0:
				if(valid)
					*valid = TRUE;
				return len;
			case STRING_COLOR_TAG:
				++s;
				switch((s == end) ? 0 : *s)
				{
					case STRING_COLOR_RGB_TAG_CHAR:
						if (s+1 != end && isxdigit(s[1]) &&
							s+2 != end && isxdigit(s[2]) &&
							s+3 != end && isxdigit(s[3]) )
						{
							s+=3;
							break;
						}
						++len; // STRING_COLOR_TAG
						++len; // STRING_COLOR_RGB_TAG_CHAR
						break;
					case 0: // ends with unfinished color code!
						++len;
						if(valid)
							*valid = FALSE;
						return len;
					case STRING_COLOR_TAG: // escaped ^
						++len;
						break;
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9': // color code
						break;
					default: // not a color code
						++len; // STRING_COLOR_TAG
						++len; // the character
						break;
				}
				++s;
				continue;
			default:
				break;
		}

		// ascii char, skip u8_analyze
		if (*s < 0x80)
		{
			++len;
			++s;
			continue;
		}

		// invalid, skip u8_analyze
		if (*s < 0xC2)
		{
			++s;
			continue;
		}

		if (!u8_analyze((const char*)s, &st, &ln, NULL, U8_ANALYZE_INFINITY))
		{
			// we CAN end up here, if an invalid char is between this one and the end of the string
			if(valid)
				*valid = TRUE;
			return len;
		}

		if(end && s + st + ln > end)
		{
			// string length exceeded by new character
			if(valid)
				*valid = TRUE;
			return len;
		}

		// valid character, skip after it
		s += st + ln;
		++len;
	}
	// never get here
}

/** Pads a utf-8 string
 * @param out     The target buffer the utf-8 string is written to.
 * @param outsize The size of the target buffer, including the final NUL
 * @param in      The input utf-8 buffer
 * @param leftalign Left align the output string (by default right alignment is done)
 * @param minwidth The minimum output width
 * @param maxwidth The maximum output width
 * @return        The number of bytes written, not including the terminating \0
 */
size_t u8_strpad(char *out, size_t outsize, const char *in, qboolean leftalign, size_t minwidth, size_t maxwidth)
{
	if(!utf8_enable.integer)
	{
		return dpsnprintf(out, outsize, "%*.*s", leftalign ? -(int) minwidth : (int) minwidth, (int) maxwidth, in);
	}
	else
	{
		size_t l = u8_bytelen(in, maxwidth);
		size_t actual_width = u8_strnlen(in, l);
		int pad = (actual_width >= minwidth) ? 0 : (minwidth - actual_width);
		int prec = l;
		int lpad = leftalign ? 0 : pad;
		int rpad = leftalign ? pad : 0;
		return dpsnprintf(out, outsize, "%*s%.*s%*s", lpad, "", prec, in, rpad, "");
	}
}

size_t u8_strpad_colorcodes(char *out, size_t outsize, const char *in, qboolean leftalign, size_t minwidth, size_t maxwidth)
{
	size_t l = u8_bytelen_colorcodes(in, maxwidth);
	size_t actual_width = u8_strnlen_colorcodes(in, l);
	int pad = (actual_width >= minwidth) ? 0 : (minwidth - actual_width);
	int prec = l;
	int lpad = leftalign ? 0 : pad;
	int rpad = leftalign ? pad : 0;
	return dpsnprintf(out, outsize, "%*s%.*s%*s", lpad, "", prec, in, rpad, "");
}


/*
The two following functions (u8_toupper, u8_tolower) are derived from
ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt and the following license
holds for these:

Copyright © 1991-2011 Unicode, Inc. All rights reserved. Distributed under the
Terms of Use in http://www.unicode.org/copyright.html.

Permission is hereby granted, free of charge, to any person obtaining a copy of
the Unicode data files and any associated documentation (the "Data Files") or
Unicode software and any associated documentation (the "Software") to deal in
the Data Files or Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, and/or sell copies
of the Data Files or Software, and to permit persons to whom the Data Files or
Software are furnished to do so, provided that (a) the above copyright
notice(s) and this permission notice appear with all copies of the Data Files
or Software, (b) both the above copyright notice(s) and this permission notice
appear in associated documentation, and (c) there is clear notice in each
modified Data File or in the Software as well as in the documentation
associated with the Data File(s) or Software that the data or software has been
modified.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD
PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA FILES OR
SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
these Data Files or Software without prior written authorization of the
copyright holder.
*/

Uchar u8_toupper(Uchar ch)
{
	switch(ch)
	{
		case 0x0061: return 0x0041;
		case 0x0062: return 0x0042;
		case 0x0063: return 0x0043;
		case 0x0064: return 0x0044;
		case 0x0065: return 0x0045;
		case 0x0066: return 0x0046;
		case 0x0067: return 0x0047;
		case 0x0068: return 0x0048;
		case 0x0069: return 0x0049;
		case 0x006A: return 0x004A;
		case 0x006B: return 0x004B;
		case 0x006C: return 0x004C;
		case 0x006D: return 0x004D;
		case 0x006E: return 0x004E;
		case 0x006F: return 0x004F;
		case 0x0070: return 0x0050;
		case 0x0071: return 0x0051;
		case 0x0072: return 0x0052;
		case 0x0073: return 0x0053;
		case 0x0074: return 0x0054;
		case 0x0075: return 0x0055;
		case 0x0076: return 0x0056;
		case 0x0077: return 0x0057;
		case 0x0078: return 0x0058;
		case 0x0079: return 0x0059;
		case 0x007A: return 0x005A;
		case 0x00B5: return 0x039C;
		case 0x00E0: return 0x00C0;
		case 0x00E1: return 0x00C1;
		case 0x00E2: return 0x00C2;
		case 0x00E3: return 0x00C3;
		case 0x00E4: return 0x00C4;
		case 0x00E5: return 0x00C5;
		case 0x00E6: return 0x00C6;
		case 0x00E7: return 0x00C7;
		case 0x00E8: return 0x00C8;
		case 0x00E9: return 0x00C9;
		case 0x00EA: return 0x00CA;
		case 0x00EB: return 0x00CB;
		case 0x00EC: return 0x00CC;
		case 0x00ED: return 0x00CD;
		case 0x00EE: return 0x00CE;
		case 0x00EF: return 0x00CF;
		case 0x00F0: return 0x00D0;
		case 0x00F1: return 0x00D1;
		case 0x00F2: return 0x00D2;
		case 0x00F3: return 0x00D3;
		case 0x00F4: return 0x00D4;
		case 0x00F5: return 0x00D5;
		case 0x00F6: return 0x00D6;
		case 0x00F8: return 0x00D8;
		case 0x00F9: return 0x00D9;
		case 0x00FA: return 0x00DA;
		case 0x00FB: return 0x00DB;
		case 0x00FC: return 0x00DC;
		case 0x00FD: return 0x00DD;
		case 0x00FE: return 0x00DE;
		case 0x00FF: return 0x0178;
		case 0x0101: return 0x0100;
		case 0x0103: return 0x0102;
		case 0x0105: return 0x0104;
		case 0x0107: return 0x0106;
		case 0x0109: return 0x0108;
		case 0x010B: return 0x010A;
		case 0x010D: return 0x010C;
		case 0x010F: return 0x010E;
		case 0x0111: return 0x0110;
		case 0x0113: return 0x0112;
		case 0x0115: return 0x0114;
		case 0x0117: return 0x0116;
		case 0x0119: return 0x0118;
		case 0x011B: return 0x011A;
		case 0x011D: return 0x011C;
		case 0x011F: return 0x011E;
		case 0x0121: return 0x0120;
		case 0x0123: return 0x0122;
		case 0x0125: return 0x0124;
		case 0x0127: return 0x0126;
		case 0x0129: return 0x0128;
		case 0x012B: return 0x012A;
		case 0x012D: return 0x012C;
		case 0x012F: return 0x012E;
		case 0x0131: return 0x0049;
		case 0x0133: return 0x0132;
		case 0x0135: return 0x0134;
		case 0x0137: return 0x0136;
		case 0x013A: return 0x0139;
		case 0x013C: return 0x013B;
		case 0x013E: return 0x013D;
		case 0x0140: return 0x013F;
		case 0x0142: return 0x0141;
		case 0x0144: return 0x0143;
		case 0x0146: return 0x0145;
		case 0x0148: return 0x0147;
		case 0x014B: return 0x014A;
		case 0x014D: return 0x014C;
		case 0x014F: return 0x014E;
		case 0x0151: return 0x0150;
		case 0x0153: return 0x0152;
		case 0x0155: return 0x0154;
		case 0x0157: return 0x0156;
		case 0x0159: return 0x0158;
		case 0x015B: return 0x015A;
		case 0x015D: return 0x015C;
		case 0x015F: return 0x015E;
		case 0x0161: return 0x0160;
		case 0x0163: return 0x0162;
		case 0x0165: return 0x0164;
		case 0x0167: return 0x0166;
		case 0x0169: return 0x0168;
		case 0x016B: return 0x016A;
		case 0x016D: return 0x016C;
		case 0x016F: return 0x016E;
		case 0x0171: return 0x0170;
		case 0x0173: return 0x0172;
		case 0x0175: return 0x0174;
		case 0x0177: return 0x0176;
		case 0x017A: return 0x0179;
		case 0x017C: return 0x017B;
		case 0x017E: return 0x017D;
		case 0x017F: return 0x0053;
		case 0x0180: return 0x0243;
		case 0x0183: return 0x0182;
		case 0x0185: return 0x0184;
		case 0x0188: return 0x0187;
		case 0x018C: return 0x018B;
		case 0x0192: return 0x0191;
		case 0x0195: return 0x01F6;
		case 0x0199: return 0x0198;
		case 0x019A: return 0x023D;
		case 0x019E: return 0x0220;
		case 0x01A1: return 0x01A0;
		case 0x01A3: return 0x01A2;
		case 0x01A5: return 0x01A4;
		case 0x01A8: return 0x01A7;
		case 0x01AD: return 0x01AC;
		case 0x01B0: return 0x01AF;
		case 0x01B4: return 0x01B3;
		case 0x01B6: return 0x01B5;
		case 0x01B9: return 0x01B8;
		case 0x01BD: return 0x01BC;
		case 0x01BF: return 0x01F7;
		case 0x01C5: return 0x01C4;
		case 0x01C6: return 0x01C4;
		case 0x01C8: return 0x01C7;
		case 0x01C9: return 0x01C7;
		case 0x01CB: return 0x01CA;
		case 0x01CC: return 0x01CA;
		case 0x01CE: return 0x01CD;
		case 0x01D0: return 0x01CF;
		case 0x01D2: return 0x01D1;
		case 0x01D4: return 0x01D3;
		case 0x01D6: return 0x01D5;
		case 0x01D8: return 0x01D7;
		case 0x01DA: return 0x01D9;
		case 0x01DC: return 0x01DB;
		case 0x01DD: return 0x018E;
		case 0x01DF: return 0x01DE;
		case 0x01E1: return 0x01E0;
		case 0x01E3: return 0x01E2;
		case 0x01E5: return 0x01E4;
		case 0x01E7: return 0x01E6;
		case 0x01E9: return 0x01E8;
		case 0x01EB: return 0x01EA;
		case 0x01ED: return 0x01EC;
		case 0x01EF: return 0x01EE;
		case 0x01F2: return 0x01F1;
		case 0x01F3: return 0x01F1;
		case 0x01F5: return 0x01F4;
		case 0x01F9: return 0x01F8;
		case 0x01FB: return 0x01FA;
		case 0x01FD: return 0x01FC;
		case 0x01FF: return 0x01FE;
		case 0x0201: return 0x0200;
		case 0x0203: return 0x0202;
		case 0x0205: return 0x0204;
		case 0x0207: return 0x0206;
		case 0x0209: return 0x0208;
		case 0x020B: return 0x020A;
		case 0x020D: return 0x020C;
		case 0x020F: return 0x020E;
		case 0x0211: return 0x0210;
		case 0x0213: return 0x0212;
		case 0x0215: return 0x0214;
		case 0x0217: return 0x0216;
		case 0x0219: return 0x0218;
		case 0x021B: return 0x021A;
		case 0x021D: return 0x021C;
		case 0x021F: return 0x021E;
		case 0x0223: return 0x0222;
		case 0x0225: return 0x0224;
		case 0x0227: return 0x0226;
		case 0x0229: return 0x0228;
		case 0x022B: return 0x022A;
		case 0x022D: return 0x022C;
		case 0x022F: return 0x022E;
		case 0x0231: return 0x0230;
		case 0x0233: return 0x0232;
		case 0x023C: return 0x023B;
		case 0x023F: return 0x2C7E;
		case 0x0240: return 0x2C7F;
		case 0x0242: return 0x0241;
		case 0x0247: return 0x0246;
		case 0x0249: return 0x0248;
		case 0x024B: return 0x024A;
		case 0x024D: return 0x024C;
		case 0x024F: return 0x024E;
		case 0x0250: return 0x2C6F;
		case 0x0251: return 0x2C6D;
		case 0x0252: return 0x2C70;
		case 0x0253: return 0x0181;
		case 0x0254: return 0x0186;
		case 0x0256: return 0x0189;
		case 0x0257: return 0x018A;
		case 0x0259: return 0x018F;
		case 0x025B: return 0x0190;
		case 0x0260: return 0x0193;
		case 0x0263: return 0x0194;
		case 0x0265: return 0xA78D;
		case 0x0268: return 0x0197;
		case 0x0269: return 0x0196;
		case 0x026B: return 0x2C62;
		case 0x026F: return 0x019C;
		case 0x0271: return 0x2C6E;
		case 0x0272: return 0x019D;
		case 0x0275: return 0x019F;
		case 0x027D: return 0x2C64;
		case 0x0280: return 0x01A6;
		case 0x0283: return 0x01A9;
		case 0x0288: return 0x01AE;
		case 0x0289: return 0x0244;
		case 0x028A: return 0x01B1;
		case 0x028B: return 0x01B2;
		case 0x028C: return 0x0245;
		case 0x0292: return 0x01B7;
		case 0x0345: return 0x0399;
		case 0x0371: return 0x0370;
		case 0x0373: return 0x0372;
		case 0x0377: return 0x0376;
		case 0x037B: return 0x03FD;
		case 0x037C: return 0x03FE;
		case 0x037D: return 0x03FF;
		case 0x03AC: return 0x0386;
		case 0x03AD: return 0x0388;
		case 0x03AE: return 0x0389;
		case 0x03AF: return 0x038A;
		case 0x03B1: return 0x0391;
		case 0x03B2: return 0x0392;
		case 0x03B3: return 0x0393;
		case 0x03B4: return 0x0394;
		case 0x03B5: return 0x0395;
		case 0x03B6: return 0x0396;
		case 0x03B7: return 0x0397;
		case 0x03B8: return 0x0398;
		case 0x03B9: return 0x0399;
		case 0x03BA: return 0x039A;
		case 0x03BB: return 0x039B;
		case 0x03BC: return 0x039C;
		case 0x03BD: return 0x039D;
		case 0x03BE: return 0x039E;
		case 0x03BF: return 0x039F;
		case 0x03C0: return 0x03A0;
		case 0x03C1: return 0x03A1;
		case 0x03C2: return 0x03A3;
		case 0x03C3: return 0x03A3;
		case 0x03C4: return 0x03A4;
		case 0x03C5: return 0x03A5;
		case 0x03C6: return 0x03A6;
		case 0x03C7: return 0x03A7;
		case 0x03C8: return 0x03A8;
		case 0x03C9: return 0x03A9;
		case 0x03CA: return 0x03AA;
		case 0x03CB: return 0x03AB;
		case 0x03CC: return 0x038C;
		case 0x03CD: return 0x038E;
		case 0x03CE: return 0x038F;
		case 0x03D0: return 0x0392;
		case 0x03D1: return 0x0398;
		case 0x03D5: return 0x03A6;
		case 0x03D6: return 0x03A0;
		case 0x03D7: return 0x03CF;
		case 0x03D9: return 0x03D8;
		case 0x03DB: return 0x03DA;
		case 0x03DD: return 0x03DC;
		case 0x03DF: return 0x03DE;
		case 0x03E1: return 0x03E0;
		case 0x03E3: return 0x03E2;
		case 0x03E5: return 0x03E4;
		case 0x03E7: return 0x03E6;
		case 0x03E9: return 0x03E8;
		case 0x03EB: return 0x03EA;
		case 0x03ED: return 0x03EC;
		case 0x03EF: return 0x03EE;
		case 0x03F0: return 0x039A;
		case 0x03F1: return 0x03A1;
		case 0x03F2: return 0x03F9;
		case 0x03F5: return 0x0395;
		case 0x03F8: return 0x03F7;
		case 0x03FB: return 0x03FA;
		case 0x0430: return 0x0410;
		case 0x0431: return 0x0411;
		case 0x0432: return 0x0412;
		case 0x0433: return 0x0413;
		case 0x0434: return 0x0414;
		case 0x0435: return 0x0415;
		case 0x0436: return 0x0416;
		case 0x0437: return 0x0417;
		case 0x0438: return 0x0418;
		case 0x0439: return 0x0419;
		case 0x043A: return 0x041A;
		case 0x043B: return 0x041B;
		case 0x043C: return 0x041C;
		case 0x043D: return 0x041D;
		case 0x043E: return 0x041E;
		case 0x043F: return 0x041F;
		case 0x0440: return 0x0420;
		case 0x0441: return 0x0421;
		case 0x0442: return 0x0422;
		case 0x0443: return 0x0423;
		case 0x0444: return 0x0424;
		case 0x0445: return 0x0425;
		case 0x0446: return 0x0426;
		case 0x0447: return 0x0427;
		case 0x0448: return 0x0428;
		case 0x0449: return 0x0429;
		case 0x044A: return 0x042A;
		case 0x044B: return 0x042B;
		case 0x044C: return 0x042C;
		case 0x044D: return 0x042D;
		case 0x044E: return 0x042E;
		case 0x044F: return 0x042F;
		case 0x0450: return 0x0400;
		case 0x0451: return 0x0401;
		case 0x0452: return 0x0402;
		case 0x0453: return 0x0403;
		case 0x0454: return 0x0404;
		case 0x0455: return 0x0405;
		case 0x0456: return 0x0406;
		case 0x0457: return 0x0407;
		case 0x0458: return 0x0408;
		case 0x0459: return 0x0409;
		case 0x045A: return 0x040A;
		case 0x045B: return 0x040B;
		case 0x045C: return 0x040C;
		case 0x045D: return 0x040D;
		case 0x045E: return 0x040E;
		case 0x045F: return 0x040F;
		case 0x0461: return 0x0460;
		case 0x0463: return 0x0462;
		case 0x0465: return 0x0464;
		case 0x0467: return 0x0466;
		case 0x0469: return 0x0468;
		case 0x046B: return 0x046A;
		case 0x046D: return 0x046C;
		case 0x046F: return 0x046E;
		case 0x0471: return 0x0470;
		case 0x0473: return 0x0472;
		case 0x0475: return 0x0474;
		case 0x0477: return 0x0476;
		case 0x0479: return 0x0478;
		case 0x047B: return 0x047A;
		case 0x047D: return 0x047C;
		case 0x047F: return 0x047E;
		case 0x0481: return 0x0480;
		case 0x048B: return 0x048A;
		case 0x048D: return 0x048C;
		case 0x048F: return 0x048E;
		case 0x0491: return 0x0490;
		case 0x0493: return 0x0492;
		case 0x0495: return 0x0494;
		case 0x0497: return 0x0496;
		case 0x0499: return 0x0498;
		case 0x049B: return 0x049A;
		case 0x049D: return 0x049C;
		case 0x049F: return 0x049E;
		case 0x04A1: return 0x04A0;
		case 0x04A3: return 0x04A2;
		case 0x04A5: return 0x04A4;
		case 0x04A7: return 0x04A6;
		case 0x04A9: return 0x04A8;
		case 0x04AB: return 0x04AA;
		case 0x04AD: return 0x04AC;
		case 0x04AF: return 0x04AE;
		case 0x04B1: return 0x04B0;
		case 0x04B3: return 0x04B2;
		case 0x04B5: return 0x04B4;
		case 0x04B7: return 0x04B6;
		case 0x04B9: return 0x04B8;
		case 0x04BB: return 0x04BA;
		case 0x04BD: return 0x04BC;
		case 0x04BF: return 0x04BE;
		case 0x04C2: return 0x04C1;
		case 0x04C4: return 0x04C3;
		case 0x04C6: return 0x04C5;
		case 0x04C8: return 0x04C7;
		case 0x04CA: return 0x04C9;
		case 0x04CC: return 0x04CB;
		case 0x04CE: return 0x04CD;
		case 0x04CF: return 0x04C0;
		case 0x04D1: return 0x04D0;
		case 0x04D3: return 0x04D2;
		case 0x04D5: return 0x04D4;
		case 0x04D7: return 0x04D6;
		case 0x04D9: return 0x04D8;
		case 0x04DB: return 0x04DA;
		case 0x04DD: return 0x04DC;
		case 0x04DF: return 0x04DE;
		case 0x04E1: return 0x04E0;
		case 0x04E3: return 0x04E2;
		case 0x04E5: return 0x04E4;
		case 0x04E7: return 0x04E6;
		case 0x04E9: return 0x04E8;
		case 0x04EB: return 0x04EA;
		case 0x04ED: return 0x04EC;
		case 0x04EF: return 0x04EE;
		case 0x04F1: return 0x04F0;
		case 0x04F3: return 0x04F2;
		case 0x04F5: return 0x04F4;
		case 0x04F7: return 0x04F6;
		case 0x04F9: return 0x04F8;
		case 0x04FB: return 0x04FA;
		case 0x04FD: return 0x04FC;
		case 0x04FF: return 0x04FE;
		case 0x0501: return 0x0500;
		case 0x0503: return 0x0502;
		case 0x0505: return 0x0504;
		case 0x0507: return 0x0506;
		case 0x0509: return 0x0508;
		case 0x050B: return 0x050A;
		case 0x050D: return 0x050C;
		case 0x050F: return 0x050E;
		case 0x0511: return 0x0510;
		case 0x0513: return 0x0512;
		case 0x0515: return 0x0514;
		case 0x0517: return 0x0516;
		case 0x0519: return 0x0518;
		case 0x051B: return 0x051A;
		case 0x051D: return 0x051C;
		case 0x051F: return 0x051E;
		case 0x0521: return 0x0520;
		case 0x0523: return 0x0522;
		case 0x0525: return 0x0524;
		case 0x0527: return 0x0526;
		case 0x0561: return 0x0531;
		case 0x0562: return 0x0532;
		case 0x0563: return 0x0533;
		case 0x0564: return 0x0534;
		case 0x0565: return 0x0535;
		case 0x0566: return 0x0536;
		case 0x0567: return 0x0537;
		case 0x0568: return 0x0538;
		case 0x0569: return 0x0539;
		case 0x056A: return 0x053A;
		case 0x056B: return 0x053B;
		case 0x056C: return 0x053C;
		case 0x056D: return 0x053D;
		case 0x056E: return 0x053E;
		case 0x056F: return 0x053F;
		case 0x0570: return 0x0540;
		case 0x0571: return 0x0541;
		case 0x0572: return 0x0542;
		case 0x0573: return 0x0543;
		case 0x0574: return 0x0544;
		case 0x0575: return 0x0545;
		case 0x0576: return 0x0546;
		case 0x0577: return 0x0547;
		case 0x0578: return 0x0548;
		case 0x0579: return 0x0549;
		case 0x057A: return 0x054A;
		case 0x057B: return 0x054B;
		case 0x057C: return 0x054C;
		case 0x057D: return 0x054D;
		case 0x057E: return 0x054E;
		case 0x057F: return 0x054F;
		case 0x0580: return 0x0550;
		case 0x0581: return 0x0551;
		case 0x0582: return 0x0552;
		case 0x0583: return 0x0553;
		case 0x0584: return 0x0554;
		case 0x0585: return 0x0555;
		case 0x0586: return 0x0556;
		case 0x1D79: return 0xA77D;
		case 0x1D7D: return 0x2C63;
		case 0x1E01: return 0x1E00;
		case 0x1E03: return 0x1E02;
		case 0x1E05: return 0x1E04;
		case 0x1E07: return 0x1E06;
		case 0x1E09: return 0x1E08;
		case 0x1E0B: return 0x1E0A;
		case 0x1E0D: return 0x1E0C;
		case 0x1E0F: return 0x1E0E;
		case 0x1E11: return 0x1E10;
		case 0x1E13: return 0x1E12;
		case 0x1E15: return 0x1E14;
		case 0x1E17: return 0x1E16;
		case 0x1E19: return 0x1E18;
		case 0x1E1B: return 0x1E1A;
		case 0x1E1D: return 0x1E1C;
		case 0x1E1F: return 0x1E1E;
		case 0x1E21: return 0x1E20;
		case 0x1E23: return 0x1E22;
		case 0x1E25: return 0x1E24;
		case 0x1E27: return 0x1E26;
		case 0x1E29: return 0x1E28;
		case 0x1E2B: return 0x1E2A;
		case 0x1E2D: return 0x1E2C;
		case 0x1E2F: return 0x1E2E;
		case 0x1E31: return 0x1E30;
		case 0x1E33: return 0x1E32;
		case 0x1E35: return 0x1E34;
		case 0x1E37: return 0x1E36;
		case 0x1E39: return 0x1E38;
		case 0x1E3B: return 0x1E3A;
		case 0x1E3D: return 0x1E3C;
		case 0x1E3F: return 0x1E3E;
		case 0x1E41: return 0x1E40;
		case 0x1E43: return 0x1E42;
		case 0x1E45: return 0x1E44;
		case 0x1E47: return 0x1E46;
		case 0x1E49: return 0x1E48;
		case 0x1E4B: return 0x1E4A;
		case 0x1E4D: return 0x1E4C;
		case 0x1E4F: return 0x1E4E;
		case 0x1E51: return 0x1E50;
		case 0x1E53: return 0x1E52;
		case 0x1E55: return 0x1E54;
		case 0x1E57: return 0x1E56;
		case 0x1E59: return 0x1E58;
		case 0x1E5B: return 0x1E5A;
		case 0x1E5D: return 0x1E5C;
		case 0x1E5F: return 0x1E5E;
		case 0x1E61: return 0x1E60;
		case 0x1E63: return 0x1E62;
		case 0x1E65: return 0x1E64;
		case 0x1E67: return 0x1E66;
		case 0x1E69: return 0x1E68;
		case 0x1E6B: return 0x1E6A;
		case 0x1E6D: return 0x1E6C;
		case 0x1E6F: return 0x1E6E;
		case 0x1E71: return 0x1E70;
		case 0x1E73: return 0x1E72;
		case 0x1E75: return 0x1E74;
		case 0x1E77: return 0x1E76;
		case 0x1E79: return 0x1E78;
		case 0x1E7B: return 0x1E7A;
		case 0x1E7D: return 0x1E7C;
		case 0x1E7F: return 0x1E7E;
		case 0x1E81: return 0x1E80;
		case 0x1E83: return 0x1E82;
		case 0x1E85: return 0x1E84;
		case 0x1E87: return 0x1E86;
		case 0x1E89: return 0x1E88;
		case 0x1E8B: return 0x1E8A;
		case 0x1E8D: return 0x1E8C;
		case 0x1E8F: return 0x1E8E;
		case 0x1E91: return 0x1E90;
		case 0x1E93: return 0x1E92;
		case 0x1E95: return 0x1E94;
		case 0x1E9B: return 0x1E60;
		case 0x1EA1: return 0x1EA0;
		case 0x1EA3: return 0x1EA2;
		case 0x1EA5: return 0x1EA4;
		case 0x1EA7: return 0x1EA6;
		case 0x1EA9: return 0x1EA8;
		case 0x1EAB: return 0x1EAA;
		case 0x1EAD: return 0x1EAC;
		case 0x1EAF: return 0x1EAE;
		case 0x1EB1: return 0x1EB0;
		case 0x1EB3: return 0x1EB2;
		case 0x1EB5: return 0x1EB4;
		case 0x1EB7: return 0x1EB6;
		case 0x1EB9: return 0x1EB8;
		case 0x1EBB: return 0x1EBA;
		case 0x1EBD: return 0x1EBC;
		case 0x1EBF: return 0x1EBE;
		case 0x1EC1: return 0x1EC0;
		case 0x1EC3: return 0x1EC2;
		case 0x1EC5: return 0x1EC4;
		case 0x1EC7: return 0x1EC6;
		case 0x1EC9: return 0x1EC8;
		case 0x1ECB: return 0x1ECA;
		case 0x1ECD: return 0x1ECC;
		case 0x1ECF: return 0x1ECE;
		case 0x1ED1: return 0x1ED0;
		case 0x1ED3: return 0x1ED2;
		case 0x1ED5: return 0x1ED4;
		case 0x1ED7: return 0x1ED6;
		case 0x1ED9: return 0x1ED8;
		case 0x1EDB: return 0x1EDA;
		case 0x1EDD: return 0x1EDC;
		case 0x1EDF: return 0x1EDE;
		case 0x1EE1: return 0x1EE0;
		case 0x1EE3: return 0x1EE2;
		case 0x1EE5: return 0x1EE4;
		case 0x1EE7: return 0x1EE6;
		case 0x1EE9: return 0x1EE8;
		case 0x1EEB: return 0x1EEA;
		case 0x1EED: return 0x1EEC;
		case 0x1EEF: return 0x1EEE;
		case 0x1EF1: return 0x1EF0;
		case 0x1EF3: return 0x1EF2;
		case 0x1EF5: return 0x1EF4;
		case 0x1EF7: return 0x1EF6;
		case 0x1EF9: return 0x1EF8;
		case 0x1EFB: return 0x1EFA;
		case 0x1EFD: return 0x1EFC;
		case 0x1EFF: return 0x1EFE;
		case 0x1F00: return 0x1F08;
		case 0x1F01: return 0x1F09;
		case 0x1F02: return 0x1F0A;
		case 0x1F03: return 0x1F0B;
		case 0x1F04: return 0x1F0C;
		case 0x1F05: return 0x1F0D;
		case 0x1F06: return 0x1F0E;
		case 0x1F07: return 0x1F0F;
		case 0x1F10: return 0x1F18;
		case 0x1F11: return 0x1F19;
		case 0x1F12: return 0x1F1A;
		case 0x1F13: return 0x1F1B;
		case 0x1F14: return 0x1F1C;
		case 0x1F15: return 0x1F1D;
		case 0x1F20: return 0x1F28;
		case 0x1F21: return 0x1F29;
		case 0x1F22: return 0x1F2A;
		case 0x1F23: return 0x1F2B;
		case 0x1F24: return 0x1F2C;
		case 0x1F25: return 0x1F2D;
		case 0x1F26: return 0x1F2E;
		case 0x1F27: return 0x1F2F;
		case 0x1F30: return 0x1F38;
		case 0x1F31: return 0x1F39;
		case 0x1F32: return 0x1F3A;
		case 0x1F33: return 0x1F3B;
		case 0x1F34: return 0x1F3C;
		case 0x1F35: return 0x1F3D;
		case 0x1F36: return 0x1F3E;
		case 0x1F37: return 0x1F3F;
		case 0x1F40: return 0x1F48;
		case 0x1F41: return 0x1F49;
		case 0x1F42: return 0x1F4A;
		case 0x1F43: return 0x1F4B;
		case 0x1F44: return 0x1F4C;
		case 0x1F45: return 0x1F4D;
		case 0x1F51: return 0x1F59;
		case 0x1F53: return 0x1F5B;
		case 0x1F55: return 0x1F5D;
		case 0x1F57: return 0x1F5F;
		case 0x1F60: return 0x1F68;
		case 0x1F61: return 0x1F69;
		case 0x1F62: return 0x1F6A;
		case 0x1F63: return 0x1F6B;
		case 0x1F64: return 0x1F6C;
		case 0x1F65: return 0x1F6D;
		case 0x1F66: return 0x1F6E;
		case 0x1F67: return 0x1F6F;
		case 0x1F70: return 0x1FBA;
		case 0x1F71: return 0x1FBB;
		case 0x1F72: return 0x1FC8;
		case 0x1F73: return 0x1FC9;
		case 0x1F74: return 0x1FCA;
		case 0x1F75: return 0x1FCB;
		case 0x1F76: return 0x1FDA;
		case 0x1F77: return 0x1FDB;
		case 0x1F78: return 0x1FF8;
		case 0x1F79: return 0x1FF9;
		case 0x1F7A: return 0x1FEA;
		case 0x1F7B: return 0x1FEB;
		case 0x1F7C: return 0x1FFA;
		case 0x1F7D: return 0x1FFB;
		case 0x1F80: return 0x1F88;
		case 0x1F81: return 0x1F89;
		case 0x1F82: return 0x1F8A;
		case 0x1F83: return 0x1F8B;
		case 0x1F84: return 0x1F8C;
		case 0x1F85: return 0x1F8D;
		case 0x1F86: return 0x1F8E;
		case 0x1F87: return 0x1F8F;
		case 0x1F90: return 0x1F98;
		case 0x1F91: return 0x1F99;
		case 0x1F92: return 0x1F9A;
		case 0x1F93: return 0x1F9B;
		case 0x1F94: return 0x1F9C;
		case 0x1F95: return 0x1F9D;
		case 0x1F96: return 0x1F9E;
		case 0x1F97: return 0x1F9F;
		case 0x1FA0: return 0x1FA8;
		case 0x1FA1: return 0x1FA9;
		case 0x1FA2: return 0x1FAA;
		case 0x1FA3: return 0x1FAB;
		case 0x1FA4: return 0x1FAC;
		case 0x1FA5: return 0x1FAD;
		case 0x1FA6: return 0x1FAE;
		case 0x1FA7: return 0x1FAF;
		case 0x1FB0: return 0x1FB8;
		case 0x1FB1: return 0x1FB9;
		case 0x1FB3: return 0x1FBC;
		case 0x1FBE: return 0x0399;
		case 0x1FC3: return 0x1FCC;
		case 0x1FD0: return 0x1FD8;
		case 0x1FD1: return 0x1FD9;
		case 0x1FE0: return 0x1FE8;
		case 0x1FE1: return 0x1FE9;
		case 0x1FE5: return 0x1FEC;
		case 0x1FF3: return 0x1FFC;
		case 0x214E: return 0x2132;
		case 0x2170: return 0x2160;
		case 0x2171: return 0x2161;
		case 0x2172: return 0x2162;
		case 0x2173: return 0x2163;
		case 0x2174: return 0x2164;
		case 0x2175: return 0x2165;
		case 0x2176: return 0x2166;
		case 0x2177: return 0x2167;
		case 0x2178: return 0x2168;
		case 0x2179: return 0x2169;
		case 0x217A: return 0x216A;
		case 0x217B: return 0x216B;
		case 0x217C: return 0x216C;
		case 0x217D: return 0x216D;
		case 0x217E: return 0x216E;
		case 0x217F: return 0x216F;
		case 0x2184: return 0x2183;
		case 0x24D0: return 0x24B6;
		case 0x24D1: return 0x24B7;
		case 0x24D2: return 0x24B8;
		case 0x24D3: return 0x24B9;
		case 0x24D4: return 0x24BA;
		case 0x24D5: return 0x24BB;
		case 0x24D6: return 0x24BC;
		case 0x24D7: return 0x24BD;
		case 0x24D8: return 0x24BE;
		case 0x24D9: return 0x24BF;
		case 0x24DA: return 0x24C0;
		case 0x24DB: return 0x24C1;
		case 0x24DC: return 0x24C2;
		case 0x24DD: return 0x24C3;
		case 0x24DE: return 0x24C4;
		case 0x24DF: return 0x24C5;
		case 0x24E0: return 0x24C6;
		case 0x24E1: return 0x24C7;
		case 0x24E2: return 0x24C8;
		case 0x24E3: return 0x24C9;
		case 0x24E4: return 0x24CA;
		case 0x24E5: return 0x24CB;
		case 0x24E6: return 0x24CC;
		case 0x24E7: return 0x24CD;
		case 0x24E8: return 0x24CE;
		case 0x24E9: return 0x24CF;
		case 0x2C30: return 0x2C00;
		case 0x2C31: return 0x2C01;
		case 0x2C32: return 0x2C02;
		case 0x2C33: return 0x2C03;
		case 0x2C34: return 0x2C04;
		case 0x2C35: return 0x2C05;
		case 0x2C36: return 0x2C06;
		case 0x2C37: return 0x2C07;
		case 0x2C38: return 0x2C08;
		case 0x2C39: return 0x2C09;
		case 0x2C3A: return 0x2C0A;
		case 0x2C3B: return 0x2C0B;
		case 0x2C3C: return 0x2C0C;
		case 0x2C3D: return 0x2C0D;
		case 0x2C3E: return 0x2C0E;
		case 0x2C3F: return 0x2C0F;
		case 0x2C40: return 0x2C10;
		case 0x2C41: return 0x2C11;
		case 0x2C42: return 0x2C12;
		case 0x2C43: return 0x2C13;
		case 0x2C44: return 0x2C14;
		case 0x2C45: return 0x2C15;
		case 0x2C46: return 0x2C16;
		case 0x2C47: return 0x2C17;
		case 0x2C48: return 0x2C18;
		case 0x2C49: return 0x2C19;
		case 0x2C4A: return 0x2C1A;
		case 0x2C4B: return 0x2C1B;
		case 0x2C4C: return 0x2C1C;
		case 0x2C4D: return 0x2C1D;
		case 0x2C4E: return 0x2C1E;
		case 0x2C4F: return 0x2C1F;
		case 0x2C50: return 0x2C20;
		case 0x2C51: return 0x2C21;
		case 0x2C52: return 0x2C22;
		case 0x2C53: return 0x2C23;
		case 0x2C54: return 0x2C24;
		case 0x2C55: return 0x2C25;
		case 0x2C56: return 0x2C26;
		case 0x2C57: return 0x2C27;
		case 0x2C58: return 0x2C28;
		case 0x2C59: return 0x2C29;
		case 0x2C5A: return 0x2C2A;
		case 0x2C5B: return 0x2C2B;
		case 0x2C5C: return 0x2C2C;
		case 0x2C5D: return 0x2C2D;
		case 0x2C5E: return 0x2C2E;
		case 0x2C61: return 0x2C60;
		case 0x2C65: return 0x023A;
		case 0x2C66: return 0x023E;
		case 0x2C68: return 0x2C67;
		case 0x2C6A: return 0x2C69;
		case 0x2C6C: return 0x2C6B;
		case 0x2C73: return 0x2C72;
		case 0x2C76: return 0x2C75;
		case 0x2C81: return 0x2C80;
		case 0x2C83: return 0x2C82;
		case 0x2C85: return 0x2C84;
		case 0x2C87: return 0x2C86;
		case 0x2C89: return 0x2C88;
		case 0x2C8B: return 0x2C8A;
		case 0x2C8D: return 0x2C8C;
		case 0x2C8F: return 0x2C8E;
		case 0x2C91: return 0x2C90;
		case 0x2C93: return 0x2C92;
		case 0x2C95: return 0x2C94;
		case 0x2C97: return 0x2C96;
		case 0x2C99: return 0x2C98;
		case 0x2C9B: return 0x2C9A;
		case 0x2C9D: return 0x2C9C;
		case 0x2C9F: return 0x2C9E;
		case 0x2CA1: return 0x2CA0;
		case 0x2CA3: return 0x2CA2;
		case 0x2CA5: return 0x2CA4;
		case 0x2CA7: return 0x2CA6;
		case 0x2CA9: return 0x2CA8;
		case 0x2CAB: return 0x2CAA;
		case 0x2CAD: return 0x2CAC;
		case 0x2CAF: return 0x2CAE;
		case 0x2CB1: return 0x2CB0;
		case 0x2CB3: return 0x2CB2;
		case 0x2CB5: return 0x2CB4;
		case 0x2CB7: return 0x2CB6;
		case 0x2CB9: return 0x2CB8;
		case 0x2CBB: return 0x2CBA;
		case 0x2CBD: return 0x2CBC;
		case 0x2CBF: return 0x2CBE;
		case 0x2CC1: return 0x2CC0;
		case 0x2CC3: return 0x2CC2;
		case 0x2CC5: return 0x2CC4;
		case 0x2CC7: return 0x2CC6;
		case 0x2CC9: return 0x2CC8;
		case 0x2CCB: return 0x2CCA;
		case 0x2CCD: return 0x2CCC;
		case 0x2CCF: return 0x2CCE;
		case 0x2CD1: return 0x2CD0;
		case 0x2CD3: return 0x2CD2;
		case 0x2CD5: return 0x2CD4;
		case 0x2CD7: return 0x2CD6;
		case 0x2CD9: return 0x2CD8;
		case 0x2CDB: return 0x2CDA;
		case 0x2CDD: return 0x2CDC;
		case 0x2CDF: return 0x2CDE;
		case 0x2CE1: return 0x2CE0;
		case 0x2CE3: return 0x2CE2;
		case 0x2CEC: return 0x2CEB;
		case 0x2CEE: return 0x2CED;
		case 0x2D00: return 0x10A0;
		case 0x2D01: return 0x10A1;
		case 0x2D02: return 0x10A2;
		case 0x2D03: return 0x10A3;
		case 0x2D04: return 0x10A4;
		case 0x2D05: return 0x10A5;
		case 0x2D06: return 0x10A6;
		case 0x2D07: return 0x10A7;
		case 0x2D08: return 0x10A8;
		case 0x2D09: return 0x10A9;
		case 0x2D0A: return 0x10AA;
		case 0x2D0B: return 0x10AB;
		case 0x2D0C: return 0x10AC;
		case 0x2D0D: return 0x10AD;
		case 0x2D0E: return 0x10AE;
		case 0x2D0F: return 0x10AF;
		case 0x2D10: return 0x10B0;
		case 0x2D11: return 0x10B1;
		case 0x2D12: return 0x10B2;
		case 0x2D13: return 0x10B3;
		case 0x2D14: return 0x10B4;
		case 0x2D15: return 0x10B5;
		case 0x2D16: return 0x10B6;
		case 0x2D17: return 0x10B7;
		case 0x2D18: return 0x10B8;
		case 0x2D19: return 0x10B9;
		case 0x2D1A: return 0x10BA;
		case 0x2D1B: return 0x10BB;
		case 0x2D1C: return 0x10BC;
		case 0x2D1D: return 0x10BD;
		case 0x2D1E: return 0x10BE;
		case 0x2D1F: return 0x10BF;
		case 0x2D20: return 0x10C0;
		case 0x2D21: return 0x10C1;
		case 0x2D22: return 0x10C2;
		case 0x2D23: return 0x10C3;
		case 0x2D24: return 0x10C4;
		case 0x2D25: return 0x10C5;
		case 0xA641: return 0xA640;
		case 0xA643: return 0xA642;
		case 0xA645: return 0xA644;
		case 0xA647: return 0xA646;
		case 0xA649: return 0xA648;
		case 0xA64B: return 0xA64A;
		case 0xA64D: return 0xA64C;
		case 0xA64F: return 0xA64E;
		case 0xA651: return 0xA650;
		case 0xA653: return 0xA652;
		case 0xA655: return 0xA654;
		case 0xA657: return 0xA656;
		case 0xA659: return 0xA658;
		case 0xA65B: return 0xA65A;
		case 0xA65D: return 0xA65C;
		case 0xA65F: return 0xA65E;
		case 0xA661: return 0xA660;
		case 0xA663: return 0xA662;
		case 0xA665: return 0xA664;
		case 0xA667: return 0xA666;
		case 0xA669: return 0xA668;
		case 0xA66B: return 0xA66A;
		case 0xA66D: return 0xA66C;
		case 0xA681: return 0xA680;
		case 0xA683: return 0xA682;
		case 0xA685: return 0xA684;
		case 0xA687: return 0xA686;
		case 0xA689: return 0xA688;
		case 0xA68B: return 0xA68A;
		case 0xA68D: return 0xA68C;
		case 0xA68F: return 0xA68E;
		case 0xA691: return 0xA690;
		case 0xA693: return 0xA692;
		case 0xA695: return 0xA694;
		case 0xA697: return 0xA696;
		case 0xA723: return 0xA722;
		case 0xA725: return 0xA724;
		case 0xA727: return 0xA726;
		case 0xA729: return 0xA728;
		case 0xA72B: return 0xA72A;
		case 0xA72D: return 0xA72C;
		case 0xA72F: return 0xA72E;
		case 0xA733: return 0xA732;
		case 0xA735: return 0xA734;
		case 0xA737: return 0xA736;
		case 0xA739: return 0xA738;
		case 0xA73B: return 0xA73A;
		case 0xA73D: return 0xA73C;
		case 0xA73F: return 0xA73E;
		case 0xA741: return 0xA740;
		case 0xA743: return 0xA742;
		case 0xA745: return 0xA744;
		case 0xA747: return 0xA746;
		case 0xA749: return 0xA748;
		case 0xA74B: return 0xA74A;
		case 0xA74D: return 0xA74C;
		case 0xA74F: return 0xA74E;
		case 0xA751: return 0xA750;
		case 0xA753: return 0xA752;
		case 0xA755: return 0xA754;
		case 0xA757: return 0xA756;
		case 0xA759: return 0xA758;
		case 0xA75B: return 0xA75A;
		case 0xA75D: return 0xA75C;
		case 0xA75F: return 0xA75E;
		case 0xA761: return 0xA760;
		case 0xA763: return 0xA762;
		case 0xA765: return 0xA764;
		case 0xA767: return 0xA766;
		case 0xA769: return 0xA768;
		case 0xA76B: return 0xA76A;
		case 0xA76D: return 0xA76C;
		case 0xA76F: return 0xA76E;
		case 0xA77A: return 0xA779;
		case 0xA77C: return 0xA77B;
		case 0xA77F: return 0xA77E;
		case 0xA781: return 0xA780;
		case 0xA783: return 0xA782;
		case 0xA785: return 0xA784;
		case 0xA787: return 0xA786;
		case 0xA78C: return 0xA78B;
		case 0xA791: return 0xA790;
		case 0xA7A1: return 0xA7A0;
		case 0xA7A3: return 0xA7A2;
		case 0xA7A5: return 0xA7A4;
		case 0xA7A7: return 0xA7A6;
		case 0xA7A9: return 0xA7A8;
		case 0xFF41: return 0xFF21;
		case 0xFF42: return 0xFF22;
		case 0xFF43: return 0xFF23;
		case 0xFF44: return 0xFF24;
		case 0xFF45: return 0xFF25;
		case 0xFF46: return 0xFF26;
		case 0xFF47: return 0xFF27;
		case 0xFF48: return 0xFF28;
		case 0xFF49: return 0xFF29;
		case 0xFF4A: return 0xFF2A;
		case 0xFF4B: return 0xFF2B;
		case 0xFF4C: return 0xFF2C;
		case 0xFF4D: return 0xFF2D;
		case 0xFF4E: return 0xFF2E;
		case 0xFF4F: return 0xFF2F;
		case 0xFF50: return 0xFF30;
		case 0xFF51: return 0xFF31;
		case 0xFF52: return 0xFF32;
		case 0xFF53: return 0xFF33;
		case 0xFF54: return 0xFF34;
		case 0xFF55: return 0xFF35;
		case 0xFF56: return 0xFF36;
		case 0xFF57: return 0xFF37;
		case 0xFF58: return 0xFF38;
		case 0xFF59: return 0xFF39;
		case 0xFF5A: return 0xFF3A;
		case 0x10428: return 0x10400;
		case 0x10429: return 0x10401;
		case 0x1042A: return 0x10402;
		case 0x1042B: return 0x10403;
		case 0x1042C: return 0x10404;
		case 0x1042D: return 0x10405;
		case 0x1042E: return 0x10406;
		case 0x1042F: return 0x10407;
		case 0x10430: return 0x10408;
		case 0x10431: return 0x10409;
		case 0x10432: return 0x1040A;
		case 0x10433: return 0x1040B;
		case 0x10434: return 0x1040C;
		case 0x10435: return 0x1040D;
		case 0x10436: return 0x1040E;
		case 0x10437: return 0x1040F;
		case 0x10438: return 0x10410;
		case 0x10439: return 0x10411;
		case 0x1043A: return 0x10412;
		case 0x1043B: return 0x10413;
		case 0x1043C: return 0x10414;
		case 0x1043D: return 0x10415;
		case 0x1043E: return 0x10416;
		case 0x1043F: return 0x10417;
		case 0x10440: return 0x10418;
		case 0x10441: return 0x10419;
		case 0x10442: return 0x1041A;
		case 0x10443: return 0x1041B;
		case 0x10444: return 0x1041C;
		case 0x10445: return 0x1041D;
		case 0x10446: return 0x1041E;
		case 0x10447: return 0x1041F;
		case 0x10448: return 0x10420;
		case 0x10449: return 0x10421;
		case 0x1044A: return 0x10422;
		case 0x1044B: return 0x10423;
		case 0x1044C: return 0x10424;
		case 0x1044D: return 0x10425;
		case 0x1044E: return 0x10426;
		case 0x1044F: return 0x10427;
		default: return ch;
	}
}

Uchar u8_tolower(Uchar ch)
{
	switch(ch)
	{
		case 0x0041: return 0x0061;
		case 0x0042: return 0x0062;
		case 0x0043: return 0x0063;
		case 0x0044: return 0x0064;
		case 0x0045: return 0x0065;
		case 0x0046: return 0x0066;
		case 0x0047: return 0x0067;
		case 0x0048: return 0x0068;
		case 0x0049: return 0x0069;
		case 0x004A: return 0x006A;
		case 0x004B: return 0x006B;
		case 0x004C: return 0x006C;
		case 0x004D: return 0x006D;
		case 0x004E: return 0x006E;
		case 0x004F: return 0x006F;
		case 0x0050: return 0x0070;
		case 0x0051: return 0x0071;
		case 0x0052: return 0x0072;
		case 0x0053: return 0x0073;
		case 0x0054: return 0x0074;
		case 0x0055: return 0x0075;
		case 0x0056: return 0x0076;
		case 0x0057: return 0x0077;
		case 0x0058: return 0x0078;
		case 0x0059: return 0x0079;
		case 0x005A: return 0x007A;
		case 0x00C0: return 0x00E0;
		case 0x00C1: return 0x00E1;
		case 0x00C2: return 0x00E2;
		case 0x00C3: return 0x00E3;
		case 0x00C4: return 0x00E4;
		case 0x00C5: return 0x00E5;
		case 0x00C6: return 0x00E6;
		case 0x00C7: return 0x00E7;
		case 0x00C8: return 0x00E8;
		case 0x00C9: return 0x00E9;
		case 0x00CA: return 0x00EA;
		case 0x00CB: return 0x00EB;
		case 0x00CC: return 0x00EC;
		case 0x00CD: return 0x00ED;
		case 0x00CE: return 0x00EE;
		case 0x00CF: return 0x00EF;
		case 0x00D0: return 0x00F0;
		case 0x00D1: return 0x00F1;
		case 0x00D2: return 0x00F2;
		case 0x00D3: return 0x00F3;
		case 0x00D4: return 0x00F4;
		case 0x00D5: return 0x00F5;
		case 0x00D6: return 0x00F6;
		case 0x00D8: return 0x00F8;
		case 0x00D9: return 0x00F9;
		case 0x00DA: return 0x00FA;
		case 0x00DB: return 0x00FB;
		case 0x00DC: return 0x00FC;
		case 0x00DD: return 0x00FD;
		case 0x00DE: return 0x00FE;
		case 0x0100: return 0x0101;
		case 0x0102: return 0x0103;
		case 0x0104: return 0x0105;
		case 0x0106: return 0x0107;
		case 0x0108: return 0x0109;
		case 0x010A: return 0x010B;
		case 0x010C: return 0x010D;
		case 0x010E: return 0x010F;
		case 0x0110: return 0x0111;
		case 0x0112: return 0x0113;
		case 0x0114: return 0x0115;
		case 0x0116: return 0x0117;
		case 0x0118: return 0x0119;
		case 0x011A: return 0x011B;
		case 0x011C: return 0x011D;
		case 0x011E: return 0x011F;
		case 0x0120: return 0x0121;
		case 0x0122: return 0x0123;
		case 0x0124: return 0x0125;
		case 0x0126: return 0x0127;
		case 0x0128: return 0x0129;
		case 0x012A: return 0x012B;
		case 0x012C: return 0x012D;
		case 0x012E: return 0x012F;
		case 0x0130: return 0x0069;
		case 0x0132: return 0x0133;
		case 0x0134: return 0x0135;
		case 0x0136: return 0x0137;
		case 0x0139: return 0x013A;
		case 0x013B: return 0x013C;
		case 0x013D: return 0x013E;
		case 0x013F: return 0x0140;
		case 0x0141: return 0x0142;
		case 0x0143: return 0x0144;
		case 0x0145: return 0x0146;
		case 0x0147: return 0x0148;
		case 0x014A: return 0x014B;
		case 0x014C: return 0x014D;
		case 0x014E: return 0x014F;
		case 0x0150: return 0x0151;
		case 0x0152: return 0x0153;
		case 0x0154: return 0x0155;
		case 0x0156: return 0x0157;
		case 0x0158: return 0x0159;
		case 0x015A: return 0x015B;
		case 0x015C: return 0x015D;
		case 0x015E: return 0x015F;
		case 0x0160: return 0x0161;
		case 0x0162: return 0x0163;
		case 0x0164: return 0x0165;
		case 0x0166: return 0x0167;
		case 0x0168: return 0x0169;
		case 0x016A: return 0x016B;
		case 0x016C: return 0x016D;
		case 0x016E: return 0x016F;
		case 0x0170: return 0x0171;
		case 0x0172: return 0x0173;
		case 0x0174: return 0x0175;
		case 0x0176: return 0x0177;
		case 0x0178: return 0x00FF;
		case 0x0179: return 0x017A;
		case 0x017B: return 0x017C;
		case 0x017D: return 0x017E;
		case 0x0181: return 0x0253;
		case 0x0182: return 0x0183;
		case 0x0184: return 0x0185;
		case 0x0186: return 0x0254;
		case 0x0187: return 0x0188;
		case 0x0189: return 0x0256;
		case 0x018A: return 0x0257;
		case 0x018B: return 0x018C;
		case 0x018E: return 0x01DD;
		case 0x018F: return 0x0259;
		case 0x0190: return 0x025B;
		case 0x0191: return 0x0192;
		case 0x0193: return 0x0260;
		case 0x0194: return 0x0263;
		case 0x0196: return 0x0269;
		case 0x0197: return 0x0268;
		case 0x0198: return 0x0199;
		case 0x019C: return 0x026F;
		case 0x019D: return 0x0272;
		case 0x019F: return 0x0275;
		case 0x01A0: return 0x01A1;
		case 0x01A2: return 0x01A3;
		case 0x01A4: return 0x01A5;
		case 0x01A6: return 0x0280;
		case 0x01A7: return 0x01A8;
		case 0x01A9: return 0x0283;
		case 0x01AC: return 0x01AD;
		case 0x01AE: return 0x0288;
		case 0x01AF: return 0x01B0;
		case 0x01B1: return 0x028A;
		case 0x01B2: return 0x028B;
		case 0x01B3: return 0x01B4;
		case 0x01B5: return 0x01B6;
		case 0x01B7: return 0x0292;
		case 0x01B8: return 0x01B9;
		case 0x01BC: return 0x01BD;
		case 0x01C4: return 0x01C6;
		case 0x01C5: return 0x01C6;
		case 0x01C7: return 0x01C9;
		case 0x01C8: return 0x01C9;
		case 0x01CA: return 0x01CC;
		case 0x01CB: return 0x01CC;
		case 0x01CD: return 0x01CE;
		case 0x01CF: return 0x01D0;
		case 0x01D1: return 0x01D2;
		case 0x01D3: return 0x01D4;
		case 0x01D5: return 0x01D6;
		case 0x01D7: return 0x01D8;
		case 0x01D9: return 0x01DA;
		case 0x01DB: return 0x01DC;
		case 0x01DE: return 0x01DF;
		case 0x01E0: return 0x01E1;
		case 0x01E2: return 0x01E3;
		case 0x01E4: return 0x01E5;
		case 0x01E6: return 0x01E7;
		case 0x01E8: return 0x01E9;
		case 0x01EA: return 0x01EB;
		case 0x01EC: return 0x01ED;
		case 0x01EE: return 0x01EF;
		case 0x01F1: return 0x01F3;
		case 0x01F2: return 0x01F3;
		case 0x01F4: return 0x01F5;
		case 0x01F6: return 0x0195;
		case 0x01F7: return 0x01BF;
		case 0x01F8: return 0x01F9;
		case 0x01FA: return 0x01FB;
		case 0x01FC: return 0x01FD;
		case 0x01FE: return 0x01FF;
		case 0x0200: return 0x0201;
		case 0x0202: return 0x0203;
		case 0x0204: return 0x0205;
		case 0x0206: return 0x0207;
		case 0x0208: return 0x0209;
		case 0x020A: return 0x020B;
		case 0x020C: return 0x020D;
		case 0x020E: return 0x020F;
		case 0x0210: return 0x0211;
		case 0x0212: return 0x0213;
		case 0x0214: return 0x0215;
		case 0x0216: return 0x0217;
		case 0x0218: return 0x0219;
		case 0x021A: return 0x021B;
		case 0x021C: return 0x021D;
		case 0x021E: return 0x021F;
		case 0x0220: return 0x019E;
		case 0x0222: return 0x0223;
		case 0x0224: return 0x0225;
		case 0x0226: return 0x0227;
		case 0x0228: return 0x0229;
		case 0x022A: return 0x022B;
		case 0x022C: return 0x022D;
		case 0x022E: return 0x022F;
		case 0x0230: return 0x0231;
		case 0x0232: return 0x0233;
		case 0x023A: return 0x2C65;
		case 0x023B: return 0x023C;
		case 0x023D: return 0x019A;
		case 0x023E: return 0x2C66;
		case 0x0241: return 0x0242;
		case 0x0243: return 0x0180;
		case 0x0244: return 0x0289;
		case 0x0245: return 0x028C;
		case 0x0246: return 0x0247;
		case 0x0248: return 0x0249;
		case 0x024A: return 0x024B;
		case 0x024C: return 0x024D;
		case 0x024E: return 0x024F;
		case 0x0370: return 0x0371;
		case 0x0372: return 0x0373;
		case 0x0376: return 0x0377;
		case 0x0386: return 0x03AC;
		case 0x0388: return 0x03AD;
		case 0x0389: return 0x03AE;
		case 0x038A: return 0x03AF;
		case 0x038C: return 0x03CC;
		case 0x038E: return 0x03CD;
		case 0x038F: return 0x03CE;
		case 0x0391: return 0x03B1;
		case 0x0392: return 0x03B2;
		case 0x0393: return 0x03B3;
		case 0x0394: return 0x03B4;
		case 0x0395: return 0x03B5;
		case 0x0396: return 0x03B6;
		case 0x0397: return 0x03B7;
		case 0x0398: return 0x03B8;
		case 0x0399: return 0x03B9;
		case 0x039A: return 0x03BA;
		case 0x039B: return 0x03BB;
		case 0x039C: return 0x03BC;
		case 0x039D: return 0x03BD;
		case 0x039E: return 0x03BE;
		case 0x039F: return 0x03BF;
		case 0x03A0: return 0x03C0;
		case 0x03A1: return 0x03C1;
		case 0x03A3: return 0x03C3;
		case 0x03A4: return 0x03C4;
		case 0x03A5: return 0x03C5;
		case 0x03A6: return 0x03C6;
		case 0x03A7: return 0x03C7;
		case 0x03A8: return 0x03C8;
		case 0x03A9: return 0x03C9;
		case 0x03AA: return 0x03CA;
		case 0x03AB: return 0x03CB;
		case 0x03CF: return 0x03D7;
		case 0x03D8: return 0x03D9;
		case 0x03DA: return 0x03DB;
		case 0x03DC: return 0x03DD;
		case 0x03DE: return 0x03DF;
		case 0x03E0: return 0x03E1;
		case 0x03E2: return 0x03E3;
		case 0x03E4: return 0x03E5;
		case 0x03E6: return 0x03E7;
		case 0x03E8: return 0x03E9;
		case 0x03EA: return 0x03EB;
		case 0x03EC: return 0x03ED;
		case 0x03EE: return 0x03EF;
		case 0x03F4: return 0x03B8;
		case 0x03F7: return 0x03F8;
		case 0x03F9: return 0x03F2;
		case 0x03FA: return 0x03FB;
		case 0x03FD: return 0x037B;
		case 0x03FE: return 0x037C;
		case 0x03FF: return 0x037D;
		case 0x0400: return 0x0450;
		case 0x0401: return 0x0451;
		case 0x0402: return 0x0452;
		case 0x0403: return 0x0453;
		case 0x0404: return 0x0454;
		case 0x0405: return 0x0455;
		case 0x0406: return 0x0456;
		case 0x0407: return 0x0457;
		case 0x0408: return 0x0458;
		case 0x0409: return 0x0459;
		case 0x040A: return 0x045A;
		case 0x040B: return 0x045B;
		case 0x040C: return 0x045C;
		case 0x040D: return 0x045D;
		case 0x040E: return 0x045E;
		case 0x040F: return 0x045F;
		case 0x0410: return 0x0430;
		case 0x0411: return 0x0431;
		case 0x0412: return 0x0432;
		case 0x0413: return 0x0433;
		case 0x0414: return 0x0434;
		case 0x0415: return 0x0435;
		case 0x0416: return 0x0436;
		case 0x0417: return 0x0437;
		case 0x0418: return 0x0438;
		case 0x0419: return 0x0439;
		case 0x041A: return 0x043A;
		case 0x041B: return 0x043B;
		case 0x041C: return 0x043C;
		case 0x041D: return 0x043D;
		case 0x041E: return 0x043E;
		case 0x041F: return 0x043F;
		case 0x0420: return 0x0440;
		case 0x0421: return 0x0441;
		case 0x0422: return 0x0442;
		case 0x0423: return 0x0443;
		case 0x0424: return 0x0444;
		case 0x0425: return 0x0445;
		case 0x0426: return 0x0446;
		case 0x0427: return 0x0447;
		case 0x0428: return 0x0448;
		case 0x0429: return 0x0449;
		case 0x042A: return 0x044A;
		case 0x042B: return 0x044B;
		case 0x042C: return 0x044C;
		case 0x042D: return 0x044D;
		case 0x042E: return 0x044E;
		case 0x042F: return 0x044F;
		case 0x0460: return 0x0461;
		case 0x0462: return 0x0463;
		case 0x0464: return 0x0465;
		case 0x0466: return 0x0467;
		case 0x0468: return 0x0469;
		case 0x046A: return 0x046B;
		case 0x046C: return 0x046D;
		case 0x046E: return 0x046F;
		case 0x0470: return 0x0471;
		case 0x0472: return 0x0473;
		case 0x0474: return 0x0475;
		case 0x0476: return 0x0477;
		case 0x0478: return 0x0479;
		case 0x047A: return 0x047B;
		case 0x047C: return 0x047D;
		case 0x047E: return 0x047F;
		case 0x0480: return 0x0481;
		case 0x048A: return 0x048B;
		case 0x048C: return 0x048D;
		case 0x048E: return 0x048F;
		case 0x0490: return 0x0491;
		case 0x0492: return 0x0493;
		case 0x0494: return 0x0495;
		case 0x0496: return 0x0497;
		case 0x0498: return 0x0499;
		case 0x049A: return 0x049B;
		case 0x049C: return 0x049D;
		case 0x049E: return 0x049F;
		case 0x04A0: return 0x04A1;
		case 0x04A2: return 0x04A3;
		case 0x04A4: return 0x04A5;
		case 0x04A6: return 0x04A7;
		case 0x04A8: return 0x04A9;
		case 0x04AA: return 0x04AB;
		case 0x04AC: return 0x04AD;
		case 0x04AE: return 0x04AF;
		case 0x04B0: return 0x04B1;
		case 0x04B2: return 0x04B3;
		case 0x04B4: return 0x04B5;
		case 0x04B6: return 0x04B7;
		case 0x04B8: return 0x04B9;
		case 0x04BA: return 0x04BB;
		case 0x04BC: return 0x04BD;
		case 0x04BE: return 0x04BF;
		case 0x04C0: return 0x04CF;
		case 0x04C1: return 0x04C2;
		case 0x04C3: return 0x04C4;
		case 0x04C5: return 0x04C6;
		case 0x04C7: return 0x04C8;
		case 0x04C9: return 0x04CA;
		case 0x04CB: return 0x04CC;
		case 0x04CD: return 0x04CE;
		case 0x04D0: return 0x04D1;
		case 0x04D2: return 0x04D3;
		case 0x04D4: return 0x04D5;
		case 0x04D6: return 0x04D7;
		case 0x04D8: return 0x04D9;
		case 0x04DA: return 0x04DB;
		case 0x04DC: return 0x04DD;
		case 0x04DE: return 0x04DF;
		case 0x04E0: return 0x04E1;
		case 0x04E2: return 0x04E3;
		case 0x04E4: return 0x04E5;
		case 0x04E6: return 0x04E7;
		case 0x04E8: return 0x04E9;
		case 0x04EA: return 0x04EB;
		case 0x04EC: return 0x04ED;
		case 0x04EE: return 0x04EF;
		case 0x04F0: return 0x04F1;
		case 0x04F2: return 0x04F3;
		case 0x04F4: return 0x04F5;
		case 0x04F6: return 0x04F7;
		case 0x04F8: return 0x04F9;
		case 0x04FA: return 0x04FB;
		case 0x04FC: return 0x04FD;
		case 0x04FE: return 0x04FF;
		case 0x0500: return 0x0501;
		case 0x0502: return 0x0503;
		case 0x0504: return 0x0505;
		case 0x0506: return 0x0507;
		case 0x0508: return 0x0509;
		case 0x050A: return 0x050B;
		case 0x050C: return 0x050D;
		case 0x050E: return 0x050F;
		case 0x0510: return 0x0511;
		case 0x0512: return 0x0513;
		case 0x0514: return 0x0515;
		case 0x0516: return 0x0517;
		case 0x0518: return 0x0519;
		case 0x051A: return 0x051B;
		case 0x051C: return 0x051D;
		case 0x051E: return 0x051F;
		case 0x0520: return 0x0521;
		case 0x0522: return 0x0523;
		case 0x0524: return 0x0525;
		case 0x0526: return 0x0527;
		case 0x0531: return 0x0561;
		case 0x0532: return 0x0562;
		case 0x0533: return 0x0563;
		case 0x0534: return 0x0564;
		case 0x0535: return 0x0565;
		case 0x0536: return 0x0566;
		case 0x0537: return 0x0567;
		case 0x0538: return 0x0568;
		case 0x0539: return 0x0569;
		case 0x053A: return 0x056A;
		case 0x053B: return 0x056B;
		case 0x053C: return 0x056C;
		case 0x053D: return 0x056D;
		case 0x053E: return 0x056E;
		case 0x053F: return 0x056F;
		case 0x0540: return 0x0570;
		case 0x0541: return 0x0571;
		case 0x0542: return 0x0572;
		case 0x0543: return 0x0573;
		case 0x0544: return 0x0574;
		case 0x0545: return 0x0575;
		case 0x0546: return 0x0576;
		case 0x0547: return 0x0577;
		case 0x0548: return 0x0578;
		case 0x0549: return 0x0579;
		case 0x054A: return 0x057A;
		case 0x054B: return 0x057B;
		case 0x054C: return 0x057C;
		case 0x054D: return 0x057D;
		case 0x054E: return 0x057E;
		case 0x054F: return 0x057F;
		case 0x0550: return 0x0580;
		case 0x0551: return 0x0581;
		case 0x0552: return 0x0582;
		case 0x0553: return 0x0583;
		case 0x0554: return 0x0584;
		case 0x0555: return 0x0585;
		case 0x0556: return 0x0586;
		case 0x10A0: return 0x2D00;
		case 0x10A1: return 0x2D01;
		case 0x10A2: return 0x2D02;
		case 0x10A3: return 0x2D03;
		case 0x10A4: return 0x2D04;
		case 0x10A5: return 0x2D05;
		case 0x10A6: return 0x2D06;
		case 0x10A7: return 0x2D07;
		case 0x10A8: return 0x2D08;
		case 0x10A9: return 0x2D09;
		case 0x10AA: return 0x2D0A;
		case 0x10AB: return 0x2D0B;
		case 0x10AC: return 0x2D0C;
		case 0x10AD: return 0x2D0D;
		case 0x10AE: return 0x2D0E;
		case 0x10AF: return 0x2D0F;
		case 0x10B0: return 0x2D10;
		case 0x10B1: return 0x2D11;
		case 0x10B2: return 0x2D12;
		case 0x10B3: return 0x2D13;
		case 0x10B4: return 0x2D14;
		case 0x10B5: return 0x2D15;
		case 0x10B6: return 0x2D16;
		case 0x10B7: return 0x2D17;
		case 0x10B8: return 0x2D18;
		case 0x10B9: return 0x2D19;
		case 0x10BA: return 0x2D1A;
		case 0x10BB: return 0x2D1B;
		case 0x10BC: return 0x2D1C;
		case 0x10BD: return 0x2D1D;
		case 0x10BE: return 0x2D1E;
		case 0x10BF: return 0x2D1F;
		case 0x10C0: return 0x2D20;
		case 0x10C1: return 0x2D21;
		case 0x10C2: return 0x2D22;
		case 0x10C3: return 0x2D23;
		case 0x10C4: return 0x2D24;
		case 0x10C5: return 0x2D25;
		case 0x1E00: return 0x1E01;
		case 0x1E02: return 0x1E03;
		case 0x1E04: return 0x1E05;
		case 0x1E06: return 0x1E07;
		case 0x1E08: return 0x1E09;
		case 0x1E0A: return 0x1E0B;
		case 0x1E0C: return 0x1E0D;
		case 0x1E0E: return 0x1E0F;
		case 0x1E10: return 0x1E11;
		case 0x1E12: return 0x1E13;
		case 0x1E14: return 0x1E15;
		case 0x1E16: return 0x1E17;
		case 0x1E18: return 0x1E19;
		case 0x1E1A: return 0x1E1B;
		case 0x1E1C: return 0x1E1D;
		case 0x1E1E: return 0x1E1F;
		case 0x1E20: return 0x1E21;
		case 0x1E22: return 0x1E23;
		case 0x1E24: return 0x1E25;
		case 0x1E26: return 0x1E27;
		case 0x1E28: return 0x1E29;
		case 0x1E2A: return 0x1E2B;
		case 0x1E2C: return 0x1E2D;
		case 0x1E2E: return 0x1E2F;
		case 0x1E30: return 0x1E31;
		case 0x1E32: return 0x1E33;
		case 0x1E34: return 0x1E35;
		case 0x1E36: return 0x1E37;
		case 0x1E38: return 0x1E39;
		case 0x1E3A: return 0x1E3B;
		case 0x1E3C: return 0x1E3D;
		case 0x1E3E: return 0x1E3F;
		case 0x1E40: return 0x1E41;
		case 0x1E42: return 0x1E43;
		case 0x1E44: return 0x1E45;
		case 0x1E46: return 0x1E47;
		case 0x1E48: return 0x1E49;
		case 0x1E4A: return 0x1E4B;
		case 0x1E4C: return 0x1E4D;
		case 0x1E4E: return 0x1E4F;
		case 0x1E50: return 0x1E51;
		case 0x1E52: return 0x1E53;
		case 0x1E54: return 0x1E55;
		case 0x1E56: return 0x1E57;
		case 0x1E58: return 0x1E59;
		case 0x1E5A: return 0x1E5B;
		case 0x1E5C: return 0x1E5D;
		case 0x1E5E: return 0x1E5F;
		case 0x1E60: return 0x1E61;
		case 0x1E62: return 0x1E63;
		case 0x1E64: return 0x1E65;
		case 0x1E66: return 0x1E67;
		case 0x1E68: return 0x1E69;
		case 0x1E6A: return 0x1E6B;
		case 0x1E6C: return 0x1E6D;
		case 0x1E6E: return 0x1E6F;
		case 0x1E70: return 0x1E71;
		case 0x1E72: return 0x1E73;
		case 0x1E74: return 0x1E75;
		case 0x1E76: return 0x1E77;
		case 0x1E78: return 0x1E79;
		case 0x1E7A: return 0x1E7B;
		case 0x1E7C: return 0x1E7D;
		case 0x1E7E: return 0x1E7F;
		case 0x1E80: return 0x1E81;
		case 0x1E82: return 0x1E83;
		case 0x1E84: return 0x1E85;
		case 0x1E86: return 0x1E87;
		case 0x1E88: return 0x1E89;
		case 0x1E8A: return 0x1E8B;
		case 0x1E8C: return 0x1E8D;
		case 0x1E8E: return 0x1E8F;
		case 0x1E90: return 0x1E91;
		case 0x1E92: return 0x1E93;
		case 0x1E94: return 0x1E95;
		case 0x1E9E: return 0x00DF;
		case 0x1EA0: return 0x1EA1;
		case 0x1EA2: return 0x1EA3;
		case 0x1EA4: return 0x1EA5;
		case 0x1EA6: return 0x1EA7;
		case 0x1EA8: return 0x1EA9;
		case 0x1EAA: return 0x1EAB;
		case 0x1EAC: return 0x1EAD;
		case 0x1EAE: return 0x1EAF;
		case 0x1EB0: return 0x1EB1;
		case 0x1EB2: return 0x1EB3;
		case 0x1EB4: return 0x1EB5;
		case 0x1EB6: return 0x1EB7;
		case 0x1EB8: return 0x1EB9;
		case 0x1EBA: return 0x1EBB;
		case 0x1EBC: return 0x1EBD;
		case 0x1EBE: return 0x1EBF;
		case 0x1EC0: return 0x1EC1;
		case 0x1EC2: return 0x1EC3;
		case 0x1EC4: return 0x1EC5;
		case 0x1EC6: return 0x1EC7;
		case 0x1EC8: return 0x1EC9;
		case 0x1ECA: return 0x1ECB;
		case 0x1ECC: return 0x1ECD;
		case 0x1ECE: return 0x1ECF;
		case 0x1ED0: return 0x1ED1;
		case 0x1ED2: return 0x1ED3;
		case 0x1ED4: return 0x1ED5;
		case 0x1ED6: return 0x1ED7;
		case 0x1ED8: return 0x1ED9;
		case 0x1EDA: return 0x1EDB;
		case 0x1EDC: return 0x1EDD;
		case 0x1EDE: return 0x1EDF;
		case 0x1EE0: return 0x1EE1;
		case 0x1EE2: return 0x1EE3;
		case 0x1EE4: return 0x1EE5;
		case 0x1EE6: return 0x1EE7;
		case 0x1EE8: return 0x1EE9;
		case 0x1EEA: return 0x1EEB;
		case 0x1EEC: return 0x1EED;
		case 0x1EEE: return 0x1EEF;
		case 0x1EF0: return 0x1EF1;
		case 0x1EF2: return 0x1EF3;
		case 0x1EF4: return 0x1EF5;
		case 0x1EF6: return 0x1EF7;
		case 0x1EF8: return 0x1EF9;
		case 0x1EFA: return 0x1EFB;
		case 0x1EFC: return 0x1EFD;
		case 0x1EFE: return 0x1EFF;
		case 0x1F08: return 0x1F00;
		case 0x1F09: return 0x1F01;
		case 0x1F0A: return 0x1F02;
		case 0x1F0B: return 0x1F03;
		case 0x1F0C: return 0x1F04;
		case 0x1F0D: return 0x1F05;
		case 0x1F0E: return 0x1F06;
		case 0x1F0F: return 0x1F07;
		case 0x1F18: return 0x1F10;
		case 0x1F19: return 0x1F11;
		case 0x1F1A: return 0x1F12;
		case 0x1F1B: return 0x1F13;
		case 0x1F1C: return 0x1F14;
		case 0x1F1D: return 0x1F15;
		case 0x1F28: return 0x1F20;
		case 0x1F29: return 0x1F21;
		case 0x1F2A: return 0x1F22;
		case 0x1F2B: return 0x1F23;
		case 0x1F2C: return 0x1F24;
		case 0x1F2D: return 0x1F25;
		case 0x1F2E: return 0x1F26;
		case 0x1F2F: return 0x1F27;
		case 0x1F38: return 0x1F30;
		case 0x1F39: return 0x1F31;
		case 0x1F3A: return 0x1F32;
		case 0x1F3B: return 0x1F33;
		case 0x1F3C: return 0x1F34;
		case 0x1F3D: return 0x1F35;
		case 0x1F3E: return 0x1F36;
		case 0x1F3F: return 0x1F37;
		case 0x1F48: return 0x1F40;
		case 0x1F49: return 0x1F41;
		case 0x1F4A: return 0x1F42;
		case 0x1F4B: return 0x1F43;
		case 0x1F4C: return 0x1F44;
		case 0x1F4D: return 0x1F45;
		case 0x1F59: return 0x1F51;
		case 0x1F5B: return 0x1F53;
		case 0x1F5D: return 0x1F55;
		case 0x1F5F: return 0x1F57;
		case 0x1F68: return 0x1F60;
		case 0x1F69: return 0x1F61;
		case 0x1F6A: return 0x1F62;
		case 0x1F6B: return 0x1F63;
		case 0x1F6C: return 0x1F64;
		case 0x1F6D: return 0x1F65;
		case 0x1F6E: return 0x1F66;
		case 0x1F6F: return 0x1F67;
		case 0x1F88: return 0x1F80;
		case 0x1F89: return 0x1F81;
		case 0x1F8A: return 0x1F82;
		case 0x1F8B: return 0x1F83;
		case 0x1F8C: return 0x1F84;
		case 0x1F8D: return 0x1F85;
		case 0x1F8E: return 0x1F86;
		case 0x1F8F: return 0x1F87;
		case 0x1F98: return 0x1F90;
		case 0x1F99: return 0x1F91;
		case 0x1F9A: return 0x1F92;
		case 0x1F9B: return 0x1F93;
		case 0x1F9C: return 0x1F94;
		case 0x1F9D: return 0x1F95;
		case 0x1F9E: return 0x1F96;
		case 0x1F9F: return 0x1F97;
		case 0x1FA8: return 0x1FA0;
		case 0x1FA9: return 0x1FA1;
		case 0x1FAA: return 0x1FA2;
		case 0x1FAB: return 0x1FA3;
		case 0x1FAC: return 0x1FA4;
		case 0x1FAD: return 0x1FA5;
		case 0x1FAE: return 0x1FA6;
		case 0x1FAF: return 0x1FA7;
		case 0x1FB8: return 0x1FB0;
		case 0x1FB9: return 0x1FB1;
		case 0x1FBA: return 0x1F70;
		case 0x1FBB: return 0x1F71;
		case 0x1FBC: return 0x1FB3;
		case 0x1FC8: return 0x1F72;
		case 0x1FC9: return 0x1F73;
		case 0x1FCA: return 0x1F74;
		case 0x1FCB: return 0x1F75;
		case 0x1FCC: return 0x1FC3;
		case 0x1FD8: return 0x1FD0;
		case 0x1FD9: return 0x1FD1;
		case 0x1FDA: return 0x1F76;
		case 0x1FDB: return 0x1F77;
		case 0x1FE8: return 0x1FE0;
		case 0x1FE9: return 0x1FE1;
		case 0x1FEA: return 0x1F7A;
		case 0x1FEB: return 0x1F7B;
		case 0x1FEC: return 0x1FE5;
		case 0x1FF8: return 0x1F78;
		case 0x1FF9: return 0x1F79;
		case 0x1FFA: return 0x1F7C;
		case 0x1FFB: return 0x1F7D;
		case 0x1FFC: return 0x1FF3;
		case 0x2126: return 0x03C9;
		case 0x212A: return 0x006B;
		case 0x212B: return 0x00E5;
		case 0x2132: return 0x214E;
		case 0x2160: return 0x2170;
		case 0x2161: return 0x2171;
		case 0x2162: return 0x2172;
		case 0x2163: return 0x2173;
		case 0x2164: return 0x2174;
		case 0x2165: return 0x2175;
		case 0x2166: return 0x2176;
		case 0x2167: return 0x2177;
		case 0x2168: return 0x2178;
		case 0x2169: return 0x2179;
		case 0x216A: return 0x217A;
		case 0x216B: return 0x217B;
		case 0x216C: return 0x217C;
		case 0x216D: return 0x217D;
		case 0x216E: return 0x217E;
		case 0x216F: return 0x217F;
		case 0x2183: return 0x2184;
		case 0x24B6: return 0x24D0;
		case 0x24B7: return 0x24D1;
		case 0x24B8: return 0x24D2;
		case 0x24B9: return 0x24D3;
		case 0x24BA: return 0x24D4;
		case 0x24BB: return 0x24D5;
		case 0x24BC: return 0x24D6;
		case 0x24BD: return 0x24D7;
		case 0x24BE: return 0x24D8;
		case 0x24BF: return 0x24D9;
		case 0x24C0: return 0x24DA;
		case 0x24C1: return 0x24DB;
		case 0x24C2: return 0x24DC;
		case 0x24C3: return 0x24DD;
		case 0x24C4: return 0x24DE;
		case 0x24C5: return 0x24DF;
		case 0x24C6: return 0x24E0;
		case 0x24C7: return 0x24E1;
		case 0x24C8: return 0x24E2;
		case 0x24C9: return 0x24E3;
		case 0x24CA: return 0x24E4;
		case 0x24CB: return 0x24E5;
		case 0x24CC: return 0x24E6;
		case 0x24CD: return 0x24E7;
		case 0x24CE: return 0x24E8;
		case 0x24CF: return 0x24E9;
		case 0x2C00: return 0x2C30;
		case 0x2C01: return 0x2C31;
		case 0x2C02: return 0x2C32;
		case 0x2C03: return 0x2C33;
		case 0x2C04: return 0x2C34;
		case 0x2C05: return 0x2C35;
		case 0x2C06: return 0x2C36;
		case 0x2C07: return 0x2C37;
		case 0x2C08: return 0x2C38;
		case 0x2C09: return 0x2C39;
		case 0x2C0A: return 0x2C3A;
		case 0x2C0B: return 0x2C3B;
		case 0x2C0C: return 0x2C3C;
		case 0x2C0D: return 0x2C3D;
		case 0x2C0E: return 0x2C3E;
		case 0x2C0F: return 0x2C3F;
		case 0x2C10: return 0x2C40;
		case 0x2C11: return 0x2C41;
		case 0x2C12: return 0x2C42;
		case 0x2C13: return 0x2C43;
		case 0x2C14: return 0x2C44;
		case 0x2C15: return 0x2C45;
		case 0x2C16: return 0x2C46;
		case 0x2C17: return 0x2C47;
		case 0x2C18: return 0x2C48;
		case 0x2C19: return 0x2C49;
		case 0x2C1A: return 0x2C4A;
		case 0x2C1B: return 0x2C4B;
		case 0x2C1C: return 0x2C4C;
		case 0x2C1D: return 0x2C4D;
		case 0x2C1E: return 0x2C4E;
		case 0x2C1F: return 0x2C4F;
		case 0x2C20: return 0x2C50;
		case 0x2C21: return 0x2C51;
		case 0x2C22: return 0x2C52;
		case 0x2C23: return 0x2C53;
		case 0x2C24: return 0x2C54;
		case 0x2C25: return 0x2C55;
		case 0x2C26: return 0x2C56;
		case 0x2C27: return 0x2C57;
		case 0x2C28: return 0x2C58;
		case 0x2C29: return 0x2C59;
		case 0x2C2A: return 0x2C5A;
		case 0x2C2B: return 0x2C5B;
		case 0x2C2C: return 0x2C5C;
		case 0x2C2D: return 0x2C5D;
		case 0x2C2E: return 0x2C5E;
		case 0x2C60: return 0x2C61;
		case 0x2C62: return 0x026B;
		case 0x2C63: return 0x1D7D;
		case 0x2C64: return 0x027D;
		case 0x2C67: return 0x2C68;
		case 0x2C69: return 0x2C6A;
		case 0x2C6B: return 0x2C6C;
		case 0x2C6D: return 0x0251;
		case 0x2C6E: return 0x0271;
		case 0x2C6F: return 0x0250;
		case 0x2C70: return 0x0252;
		case 0x2C72: return 0x2C73;
		case 0x2C75: return 0x2C76;
		case 0x2C7E: return 0x023F;
		case 0x2C7F: return 0x0240;
		case 0x2C80: return 0x2C81;
		case 0x2C82: return 0x2C83;
		case 0x2C84: return 0x2C85;
		case 0x2C86: return 0x2C87;
		case 0x2C88: return 0x2C89;
		case 0x2C8A: return 0x2C8B;
		case 0x2C8C: return 0x2C8D;
		case 0x2C8E: return 0x2C8F;
		case 0x2C90: return 0x2C91;
		case 0x2C92: return 0x2C93;
		case 0x2C94: return 0x2C95;
		case 0x2C96: return 0x2C97;
		case 0x2C98: return 0x2C99;
		case 0x2C9A: return 0x2C9B;
		case 0x2C9C: return 0x2C9D;
		case 0x2C9E: return 0x2C9F;
		case 0x2CA0: return 0x2CA1;
		case 0x2CA2: return 0x2CA3;
		case 0x2CA4: return 0x2CA5;
		case 0x2CA6: return 0x2CA7;
		case 0x2CA8: return 0x2CA9;
		case 0x2CAA: return 0x2CAB;
		case 0x2CAC: return 0x2CAD;
		case 0x2CAE: return 0x2CAF;
		case 0x2CB0: return 0x2CB1;
		case 0x2CB2: return 0x2CB3;
		case 0x2CB4: return 0x2CB5;
		case 0x2CB6: return 0x2CB7;
		case 0x2CB8: return 0x2CB9;
		case 0x2CBA: return 0x2CBB;
		case 0x2CBC: return 0x2CBD;
		case 0x2CBE: return 0x2CBF;
		case 0x2CC0: return 0x2CC1;
		case 0x2CC2: return 0x2CC3;
		case 0x2CC4: return 0x2CC5;
		case 0x2CC6: return 0x2CC7;
		case 0x2CC8: return 0x2CC9;
		case 0x2CCA: return 0x2CCB;
		case 0x2CCC: return 0x2CCD;
		case 0x2CCE: return 0x2CCF;
		case 0x2CD0: return 0x2CD1;
		case 0x2CD2: return 0x2CD3;
		case 0x2CD4: return 0x2CD5;
		case 0x2CD6: return 0x2CD7;
		case 0x2CD8: return 0x2CD9;
		case 0x2CDA: return 0x2CDB;
		case 0x2CDC: return 0x2CDD;
		case 0x2CDE: return 0x2CDF;
		case 0x2CE0: return 0x2CE1;
		case 0x2CE2: return 0x2CE3;
		case 0x2CEB: return 0x2CEC;
		case 0x2CED: return 0x2CEE;
		case 0xA640: return 0xA641;
		case 0xA642: return 0xA643;
		case 0xA644: return 0xA645;
		case 0xA646: return 0xA647;
		case 0xA648: return 0xA649;
		case 0xA64A: return 0xA64B;
		case 0xA64C: return 0xA64D;
		case 0xA64E: return 0xA64F;
		case 0xA650: return 0xA651;
		case 0xA652: return 0xA653;
		case 0xA654: return 0xA655;
		case 0xA656: return 0xA657;
		case 0xA658: return 0xA659;
		case 0xA65A: return 0xA65B;
		case 0xA65C: return 0xA65D;
		case 0xA65E: return 0xA65F;
		case 0xA660: return 0xA661;
		case 0xA662: return 0xA663;
		case 0xA664: return 0xA665;
		case 0xA666: return 0xA667;
		case 0xA668: return 0xA669;
		case 0xA66A: return 0xA66B;
		case 0xA66C: return 0xA66D;
		case 0xA680: return 0xA681;
		case 0xA682: return 0xA683;
		case 0xA684: return 0xA685;
		case 0xA686: return 0xA687;
		case 0xA688: return 0xA689;
		case 0xA68A: return 0xA68B;
		case 0xA68C: return 0xA68D;
		case 0xA68E: return 0xA68F;
		case 0xA690: return 0xA691;
		case 0xA692: return 0xA693;
		case 0xA694: return 0xA695;
		case 0xA696: return 0xA697;
		case 0xA722: return 0xA723;
		case 0xA724: return 0xA725;
		case 0xA726: return 0xA727;
		case 0xA728: return 0xA729;
		case 0xA72A: return 0xA72B;
		case 0xA72C: return 0xA72D;
		case 0xA72E: return 0xA72F;
		case 0xA732: return 0xA733;
		case 0xA734: return 0xA735;
		case 0xA736: return 0xA737;
		case 0xA738: return 0xA739;
		case 0xA73A: return 0xA73B;
		case 0xA73C: return 0xA73D;
		case 0xA73E: return 0xA73F;
		case 0xA740: return 0xA741;
		case 0xA742: return 0xA743;
		case 0xA744: return 0xA745;
		case 0xA746: return 0xA747;
		case 0xA748: return 0xA749;
		case 0xA74A: return 0xA74B;
		case 0xA74C: return 0xA74D;
		case 0xA74E: return 0xA74F;
		case 0xA750: return 0xA751;
		case 0xA752: return 0xA753;
		case 0xA754: return 0xA755;
		case 0xA756: return 0xA757;
		case 0xA758: return 0xA759;
		case 0xA75A: return 0xA75B;
		case 0xA75C: return 0xA75D;
		case 0xA75E: return 0xA75F;
		case 0xA760: return 0xA761;
		case 0xA762: return 0xA763;
		case 0xA764: return 0xA765;
		case 0xA766: return 0xA767;
		case 0xA768: return 0xA769;
		case 0xA76A: return 0xA76B;
		case 0xA76C: return 0xA76D;
		case 0xA76E: return 0xA76F;
		case 0xA779: return 0xA77A;
		case 0xA77B: return 0xA77C;
		case 0xA77D: return 0x1D79;
		case 0xA77E: return 0xA77F;
		case 0xA780: return 0xA781;
		case 0xA782: return 0xA783;
		case 0xA784: return 0xA785;
		case 0xA786: return 0xA787;
		case 0xA78B: return 0xA78C;
		case 0xA78D: return 0x0265;
		case 0xA790: return 0xA791;
		case 0xA7A0: return 0xA7A1;
		case 0xA7A2: return 0xA7A3;
		case 0xA7A4: return 0xA7A5;
		case 0xA7A6: return 0xA7A7;
		case 0xA7A8: return 0xA7A9;
		case 0xFF21: return 0xFF41;
		case 0xFF22: return 0xFF42;
		case 0xFF23: return 0xFF43;
		case 0xFF24: return 0xFF44;
		case 0xFF25: return 0xFF45;
		case 0xFF26: return 0xFF46;
		case 0xFF27: return 0xFF47;
		case 0xFF28: return 0xFF48;
		case 0xFF29: return 0xFF49;
		case 0xFF2A: return 0xFF4A;
		case 0xFF2B: return 0xFF4B;
		case 0xFF2C: return 0xFF4C;
		case 0xFF2D: return 0xFF4D;
		case 0xFF2E: return 0xFF4E;
		case 0xFF2F: return 0xFF4F;
		case 0xFF30: return 0xFF50;
		case 0xFF31: return 0xFF51;
		case 0xFF32: return 0xFF52;
		case 0xFF33: return 0xFF53;
		case 0xFF34: return 0xFF54;
		case 0xFF35: return 0xFF55;
		case 0xFF36: return 0xFF56;
		case 0xFF37: return 0xFF57;
		case 0xFF38: return 0xFF58;
		case 0xFF39: return 0xFF59;
		case 0xFF3A: return 0xFF5A;
		case 0x10400: return 0x10428;
		case 0x10401: return 0x10429;
		case 0x10402: return 0x1042A;
		case 0x10403: return 0x1042B;
		case 0x10404: return 0x1042C;
		case 0x10405: return 0x1042D;
		case 0x10406: return 0x1042E;
		case 0x10407: return 0x1042F;
		case 0x10408: return 0x10430;
		case 0x10409: return 0x10431;
		case 0x1040A: return 0x10432;
		case 0x1040B: return 0x10433;
		case 0x1040C: return 0x10434;
		case 0x1040D: return 0x10435;
		case 0x1040E: return 0x10436;
		case 0x1040F: return 0x10437;
		case 0x10410: return 0x10438;
		case 0x10411: return 0x10439;
		case 0x10412: return 0x1043A;
		case 0x10413: return 0x1043B;
		case 0x10414: return 0x1043C;
		case 0x10415: return 0x1043D;
		case 0x10416: return 0x1043E;
		case 0x10417: return 0x1043F;
		case 0x10418: return 0x10440;
		case 0x10419: return 0x10441;
		case 0x1041A: return 0x10442;
		case 0x1041B: return 0x10443;
		case 0x1041C: return 0x10444;
		case 0x1041D: return 0x10445;
		case 0x1041E: return 0x10446;
		case 0x1041F: return 0x10447;
		case 0x10420: return 0x10448;
		case 0x10421: return 0x10449;
		case 0x10422: return 0x1044A;
		case 0x10423: return 0x1044B;
		case 0x10424: return 0x1044C;
		case 0x10425: return 0x1044D;
		case 0x10426: return 0x1044E;
		case 0x10427: return 0x1044F;
		default: return ch;
	}
}
