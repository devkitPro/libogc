#include "iosupp.h"
#include "stdin_fake.h"
#include "console.h"
#include "netio_fake.h"
#include "sdcardio_fake.h"

const devoptab_t *devoptab_list[] = {
	&dotab_stdin,
	&dotab_con,
	&dotab_netfake,
	&dotab_sdcardfake,
	0
};
