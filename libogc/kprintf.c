#include <string.h>
#include <stdarg.h>

#include <ogc/system.h>
#include <unistd.h>

static void _dietPrintDefaultFunc(const char* buf, size_t size)
{
	if(buf)
	{
		write(STDOUT_FILENO, buf, size);
	}
	else
	{
		for (size_t  i= 0; i < size; i ++)
		{
			char space = ' ';
			write(STDOUT_FILENO,&space,1);
		}
	}
}

DietPrintFn g_dietPrintFn = _dietPrintDefaultFunc;



//-----------------------------------------------------------------------------
// Some of the code below is modelled after:
//   https://github.com/mpaland/printf/blob/master/printf.c
//-----------------------------------------------------------------------------

#define DP_FLAG_ZEROPAD   (1U << 0)
#define DP_FLAG_LEFT      (1U << 1)
#define DP_FLAG_PLUS      (1U << 2)
#define DP_FLAG_SPACE     (1U << 3)
#define DP_FLAG_HASH      (1U << 4)
#define DP_FLAG_PRECISION (1U << 5)
#define DP_FLAG_UPPERCASE (1U << 6)
#define DP_FLAG_SIGNED    (1U << 7)
#define DP_FLAG_NEGATIVE  (1U << 7)
#define DP_FLAG_64        (1U << 8)
#define DP_FLAG_16        0 //(1U << 9)  // See below on why this isn't needed
#define DP_FLAG_8         0 //(1U << 10)

bool _dietIsDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool _dietIsUppercase(char c)
{
	return c >= 'A' && c <= 'Z';
}

unsigned _dietParseDec(const char** ptr)
{
	unsigned ret = 0;
	while (_dietIsDigit(**ptr)) {
		ret = 10 * ret + **ptr - '0';
		(*ptr)++;
	}
	return ret;
}

static void _dietOutput(const char* buf, size_t size, unsigned left_pad, unsigned right_pad)
{
	if (left_pad) {
		g_dietPrintFn(NULL, left_pad);
	}

	if (size) {
		g_dietPrintFn(buf, size);
	}

	if (right_pad) {
		g_dietPrintFn(NULL, right_pad);
	}
}

void _dietPrintWidth(unsigned flags, unsigned width, const char* buf, size_t size)
{
	unsigned padlen = size < width ? (width - size) : 0;

	if (!(flags & DP_FLAG_LEFT)) {
		_dietOutput(buf, size, padlen, 0);
	} else {
		_dietOutput(buf, size, 0, padlen);
	}
}

static unsigned _dietPrintHex32(unsigned flags, unsigned width, unsigned precision, u32 value)
{
	bool wantPrefix = value && (flags & DP_FLAG_HASH);
	if (flags & DP_FLAG_ZEROPAD) {
		precision = width;
		if (wantPrefix) {
			precision -= 2;
		}
	}

	unsigned buflen = 2 + (precision > 8 ? precision : 8);
	char buf[buflen];

	char* end = buf + buflen;
	char* pos = end;
	while (value) {
		u32 quot = value >> 4;
		u32 rem = value & 0xf;

		char c;
		if (rem < 0xa) {
			c = '0' + rem;
		} else if (flags & DP_FLAG_UPPERCASE) {
			c = 'A' + rem - 0xa;
		} else {
			c = 'a' + rem - 0xa;
		}

		*--pos = c;
		value = quot;
	}

	for (unsigned used = end - pos; used < precision; used ++) {
		*--pos = '0';
	}

	if (wantPrefix) {
		pos -= 2;
		pos[0] = '0';
		pos[1] = (flags & DP_FLAG_UPPERCASE) ? 'X' : 'x';
	}

	unsigned final_len = end - pos;
	_dietPrintWidth(flags, width, pos, final_len);
	return final_len;
}

void _dietPrintHex64(unsigned flags, unsigned width, unsigned precision, u64 value)
{
	u32 value_hi = value >> 32; // Guaranteed to be non-zero
	u32 value_lo = value;

	if (precision < 9) {
		precision = 9;
	}

	if (width < 9) {
		width = 9;
	}

	if (!(flags & DP_FLAG_LEFT)) {
		// Padded left
		_dietPrintHex32(flags, width-8, precision-8, value_hi);
		_dietPrintHex32(0, 8, 8, value_lo);
	} else {
		// Padded right
		unsigned used = _dietPrintHex32(0, 0, 0, value_hi);
		_dietPrintHex32(flags, width-used, precision-used, value_lo);
	}
}

static void _dietPrintDec32(unsigned flags, unsigned width, unsigned precision, u32 value)
{
	if (flags & DP_FLAG_ZEROPAD) {
		precision = width;
		if (flags & (DP_FLAG_NEGATIVE|DP_FLAG_PLUS|DP_FLAG_SPACE)) {
			precision--;
		}
	}

	unsigned buflen = 1 + (precision > 10 ? precision : 10);
	char buf[buflen];

	char* end = buf + buflen;
	char* pos = end;
	while (value) {
		// Below code needs to be compiled in ARM mode
		u32 quot = value / 10;
		u32 rem  = value % 10;

		*--pos = '0' + rem;
		value = quot;
	}

	for (unsigned used = end - pos; used < precision; used ++) {
		*--pos = '0';
	}

	if (flags & DP_FLAG_NEGATIVE) {
		*--pos = '-';
	} else if (flags & DP_FLAG_PLUS) {
		*--pos = '+';
	} else if (flags & DP_FLAG_SPACE) {
		*--pos = ' ';
	}

	_dietPrintWidth(flags, width, pos, end - pos);
}

void dietPrintV(const char* fmt, va_list va)
{
	for (const char* span_end = NULL;; fmt = span_end) {
		// Find the end of this immediate span
		for (span_end = fmt; *span_end && *span_end != '%'; span_end ++);

		bool is_literal = span_end[0] == '%' && span_end[1] == '%';
		if (is_literal) {
			span_end ++;
		}

		size_t imm_len = span_end - fmt;
		if (imm_len) {
			g_dietPrintFn(fmt, imm_len);
		}

		if (!*span_end)
			break;

		span_end ++;
		if (is_literal) {
			continue;
		}

		// Retrieve flags
		unsigned flags = 0;
		bool parse_flags = true;
		do {
			switch (*span_end) {
				default:  parse_flags = false;      break;
				case '0': flags |= DP_FLAG_ZEROPAD; break;
				case '-': flags |= DP_FLAG_LEFT;    break;
				case '+': flags |= DP_FLAG_PLUS;    break;
				case ' ': flags |= DP_FLAG_SPACE;   break;
				case '#': flags |= DP_FLAG_HASH;    break;
			}
			if (parse_flags) {
				span_end ++;
			}
		} while (parse_flags);

		// Retrieve width
		unsigned width = 0;
		if (_dietIsDigit(*span_end)) {
			width = _dietParseDec(&span_end);
		} else if (*span_end == '*') {
			int varwidth = va_arg(va, int);
			if (varwidth < 0) {
				flags |= DP_FLAG_LEFT; // The more you know about the C standard...
				varwidth = -varwidth;
			}

			width = varwidth;
			span_end++;
		}

		// Retrieve precision
		unsigned precision = 1;
		if (*span_end == '.') {
			flags |= DP_FLAG_PRECISION;
			span_end++;

			if (_dietIsDigit(*span_end)) {
				precision = _dietParseDec(&span_end);
			} else if (*span_end == '*') {
				int varprec = va_arg(va, int);
				if (varprec < 0) {
					varprec = 0;
				}

				precision = varprec;
				span_end++;
			}
		}

		// Retrieve length modifier
		switch (*span_end) {
			default: break;

			case 'l': { // long
				span_end++;
				if (*span_end == 'l') { // long long
					flags |= DP_FLAG_64;
					span_end++;
				} else if (sizeof(long) == 8) {
					flags |= DP_FLAG_64;
				}
				break;
			}

			case 'h': { // short
				span_end++;
				if (*span_end == 'h') { // char
					flags |= DP_FLAG_8;
					span_end++;
				} else {
					flags |= DP_FLAG_16;
				}
				break;
			}

			case 'j': { // intmax_t
				span_end++;
				flags |= DP_FLAG_64;
				break;
			}

			case 't':   // ptrdiff_t
			case 'z': { // size_t
				span_end++;
				flags |= sizeof(size_t) == 8 ? DP_FLAG_64 : 0;
				break;
			}
		}

		unsigned specifier = *span_end++;
		if (_dietIsUppercase(specifier)) {
			flags |= DP_FLAG_UPPERCASE;
			specifier = specifier - 'A' + 'a';
		}

		if (specifier == 'd' || specifier == 'i') {
			flags |= DP_FLAG_SIGNED;
			specifier = 'u';
		} else if (specifier == 'p') {
			flags = DP_FLAG_HASH | (sizeof(void*) == 8 ? DP_FLAG_64 : 0);
			width = 0;
			precision = sizeof(void*) * 2;
			specifier = 'x';
		}

		if (specifier == 'c' || specifier == 's') {
			flags &= ~(DP_FLAG_ZEROPAD | DP_FLAG_64 | DP_FLAG_16 | DP_FLAG_8);
			if (specifier == 'c') {
				flags |= DP_FLAG_8;
			}
		}

		if (!(flags & DP_FLAG_SIGNED)) {
			flags &= ~(DP_FLAG_PLUS | DP_FLAG_SPACE);
		}
		if (flags & (DP_FLAG_PRECISION|DP_FLAG_LEFT)) {
			flags &= ~DP_FLAG_ZEROPAD;
		}

		union {
			const char* str;
			u64 arg_u64;
			s64 arg_s64;
			u32 arg_u32;
			s32 arg_s32;
		} u;

		if (specifier == 's') {
			u.str = va_arg(va, const char*);
		} else if (!(flags & DP_FLAG_64)) {
			u.arg_u32 = va_arg(va, u32);
			// Explanation why above is correct:
			// - u8/s8/u16/s16/char are promoted to int within variadics (and sign-extended if needed)
			// - int is 32-bit

			if (flags & DP_FLAG_SIGNED) {
				if (u.arg_s32 < 0) {
					u.arg_s32 = -u.arg_s32;
				} else {
					flags &= ~DP_FLAG_NEGATIVE;
				}
			}
		} else {
			u.arg_u64 = va_arg(va, u64);

			if (flags & DP_FLAG_SIGNED) {
				if (u.arg_s64 < 0) {
					u.arg_s64 = -u.arg_s64;
				} else {
					flags &= ~DP_FLAG_NEGATIVE;
				}
			}

			// Fast path when the upper 32 bits aren't actually used
			if ((u.arg_u64 >> 32) == 0) {
				flags &= ~DP_FLAG_64;
				u.arg_u32 = u.arg_u64;
			}
		}

		switch (specifier) {
			default: { // Invalid
				break;
			}

			case 's': { // String
				const char* str = (const char*)u.str;
				str = str ? str : "(null)";
				size_t len = strnlen(str, (flags & DP_FLAG_PRECISION) ? precision : INTPTR_MAX);
				_dietPrintWidth(flags, width, str, len);
				break;
			}

			case 'c': { // Character (UTF-8 code unit)
				char c = u.arg_u32 & 0xff;
				_dietPrintWidth(flags, width, &c, 1);
				break;
			}

			case 'u': { // Decimal
				if (!(flags & DP_FLAG_64)) {
					_dietPrintDec32(flags, width, precision, u.arg_u32);
				} else {
					_dietPrintWidth(flags, width, "#TOOBIG#", 8);
				}
				break;
			}

			case 'x': { // Hex
				if (!(flags & DP_FLAG_64)) {
					_dietPrintHex32(flags, width, precision, u.arg_u32);
				} else {
					_dietPrintHex64(flags, width, precision, u.arg_u64);
				}
				break;
			}

			case 'o': { // Octal
				_dietPrintWidth(flags, width, "sorry", 5);
				break;
			}
		}
	}
}

void kprintf(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	dietPrintV(fmt, va);
	va_end(va);
}