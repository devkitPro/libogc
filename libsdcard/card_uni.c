#include <stdlib.h>
#include <string.h>
#include <gctypes.h>

u32 card_convertStrToUni(u16 *dest,u8 *src,u32 len)
{
	u32 i;
	
	for(i=0;i<len;i++) {
		*dest = *src;
		dest++; src++;
	}
	return (len*2);
}

void card_uniToUpper(u16 *dest,u16 *src,u32 len)
{
	u32 i;
	u16 uni_char;

	for(i=0;i<len;i++) {
		uni_char = *(src+i);
		if((uni_char>=(u16)'a') && (uni_char<=(u16)'z'))
			*(dest+i) = *(src+i)-(u16)('a'-'A');
		else
			*(dest+i) = *(src+i);

		if(!uni_char) break;
	}
}
