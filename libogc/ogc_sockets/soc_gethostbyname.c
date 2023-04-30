#include "soc_common.h"
#include <netdb.h>

struct hostent* gethostbyname(const char *name)
{

	return net_gethostbyname(name);

}