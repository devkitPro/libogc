#include <stdlib.h>
#include "iosupp.h"
#include "stdin_fake.h"
#include "console.h"
#include "netio_fake.h"
#include "sdcardio_fake.h"

const devoptab_t *devoptab_list[STD_MAX] = {
	&dotab_stdin,
	&dotab_stdout,
	NULL,
	&dotab_netfake,
	&dotab_sdcardfake
};
