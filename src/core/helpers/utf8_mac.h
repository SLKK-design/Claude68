#pragma once
/* utf8_mac.h — UTF-8 → Mac Roman conversion helpers. */

/* Read one UTF-8 code point from s[0..len-1].
   Returns bytes consumed (0 if len<=0). Sets *cp to the code point.
   4-byte sequences (emoji) and unknowns yield cp=0.              */
int ReadUTF8(const char* s, int len, unsigned long* cp);

/* Map a Unicode code point to a Mac Roman byte.
   Returns 0 for unmappable codepoints (caller should drop them). */
unsigned char ToMacRoman(unsigned long cp);

/* Convert a Mac Roman byte (0x80–0xFF) to UTF-8.
   Writes up to 3 bytes into out[]. Returns bytes written (0 if unmappable). */
int MacRomanToUTF8(unsigned char c, char out[4]);
