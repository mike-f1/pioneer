// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs/utils.h"

#include <cmath>
#include <cstdio>

#include <SDL_messagebox.h>

void Error(const char *format, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	Output("error: %s\n", buf);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pioneer guru meditation error", buf, 0);

	exit(1);
}

void Warning(const char *format, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	Output("warning: %s\n", buf);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Pioneer warning", buf, 0);
}

void Output(const char *format, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	fputs(buf, stderr);
}

void OpenGLDebugMsg(const char *format, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	fputs(buf, stderr);
}

static unsigned int __indentationLevel = 0;
void IndentIncrease() { __indentationLevel++; }
void IndentDecrease()
{
	assert(__indentationLevel > 0);
	__indentationLevel--;
}
void IndentedOutput(const char *format, ...)
{
	std::string indentation(__indentationLevel, '\t');
	char buf[1024];
	va_list ap;
	va_start(ap, format);
	strcpy(buf, indentation.c_str());
	int indentationSize = indentation.size();
	vsnprintf(buf + indentationSize, sizeof(buf) - indentationSize, format, ap);
	va_end(ap);

	fputs(buf, stderr);
}

static const int HEXDUMP_CHUNK = 16;
void hexdump(const unsigned char *buf, int len)
{
	int count;

	for (int i = 0; i < len; i += HEXDUMP_CHUNK) {
		Output("0x%06x  ", i);

		count = ((len - i) > HEXDUMP_CHUNK ? HEXDUMP_CHUNK : len - i);

		for (int j = 0; j < count; j++) {
			if (j == HEXDUMP_CHUNK / 2) Output(" ");
			Output("%02x ", buf[i + j]);
		}

		for (int j = count; j < HEXDUMP_CHUNK; j++) {
			if (j == HEXDUMP_CHUNK / 2) Output(" ");
			Output("   ");
		}

		Output(" ");

		for (int j = 0; j < count; j++)
			Output("%c", isprint(buf[i + j]) ? buf[i + j] : '.');

		Output("\n");
	}
}
