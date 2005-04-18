#include <stdlib.h>
#include "iosupp.h"
#include "stdin_fake.h"
#include "console.h"
#include "netio_fake.h"
#include "sdcardio_fake.h"
#include "dvd_supp.h"

const devoptab_t *devoptab_list[STD_MAX] = {
	&dotab_stdin,
	&dotab_stdout,
	&dotab_stdout,
	&dotab_netfake,
	&dotab_sdcardfake,
	&dotab_dvd
};
