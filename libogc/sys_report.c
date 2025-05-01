/*-------------------------------------------------------------

sys_report.c -- Support for printing to Dolphin debug UART

Copyright (C) 2023
Dave Murphy (WinterMute) <davem@devkitpro.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <stddef.h>
#include <sys/iosupport.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>

#include "exi.h"


static ssize_t __uart_write(const char *buffer,size_t len)
{
	u32 cmd,ret;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return len;
	}

	ret = 0;
	cmd = 0xa0010000;
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_ImmEx(EXI_CHANNEL_0,(void *)buffer,len,EXI_WRITE)==0) ret |= 0x04;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Unlock(EXI_CHANNEL_0)==0) ret |= 0x10;

	return len;
}

#define __outsz 256

static ssize_t __uart_stdio_write(struct _reent *r, void *fd, const char *ptr, size_t len)
{
	// translate \n and \r\n to \r for Dolphin handling
	char *p = (char*)ptr;
	char *buf = (char*)ptr;
	size_t send_len = len;

	for(size_t i = 0; i <= send_len; i++)
	{
		char ch = *p++;
		//skip the one of the 2 win newline bytes
		if(ch=='\r' && *p == '\n')
		{
			send_len--;
			continue;
		}

		//newline should be converted to carriage return
		if (ch=='\n') 
			ch = '\r';

		*buf++ = ch;
	}
	
	__uart_write(ptr, send_len);

	return len;
}
static const devoptab_t dotab_uart = {
	.name    = "uart",
	.write_r = __uart_stdio_write,
};

void SYS_STDIO_Report(bool use_stdout)
{
	fflush(stderr);
	devoptab_list[STD_ERR] = &dotab_uart;
	setvbuf(stderr, NULL, _IONBF, 0);
	if(use_stdout)
	{
		fflush(stdout);
		devoptab_list[STD_OUT] = &dotab_uart;
		setvbuf(stdout, NULL, _IONBF, 0);
	}
}

void SYS_Report (char const *const fmt_, ...)
{
	ssize_t len = 0;
	char* buffer = NULL;
	va_list args;
	size_t write_len = __outsz;
	while(true)
	{
		char* tmp = (char*)realloc(buffer, write_len);
		if(tmp == NULL)
			break;

		buffer = tmp;

		va_start(args, fmt_);
		len = vsnprintf(buffer, write_len, fmt_, args);
		va_end(args);
		if(len > 0 && len >= write_len)
		{
			write_len = len+1;
			continue;
		}
		
		break;
	}

	//only output if we have data to send and no error occured
	if(buffer != NULL && len > 0)
		__uart_stdio_write(NULL, 0, buffer, len);

	if(buffer)
		free(buffer);	
}

