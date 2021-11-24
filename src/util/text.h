#pragma once

#include "types.h"

String from_utf16 (Str16);

String16 to_utf16 (Str);

 // Must be 0..15
char to_hex_digit (uint8 nyb);

 // Returns -1 if not a hex digit
int8 from_hex_digit (char c);

String escape_url (Str);
String make_url_utf8 (Str);
