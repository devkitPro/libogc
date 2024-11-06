#include <netdb.h>

static const struct
{
	const int code;
	const char *error_string;
}
gai_errors[] =
{
	{ EAI_ADDRFAMILY, "address family for hostname not supported" },
	{ EAI_AGAIN,      "temporary failure in name resolution" },
	{ EAI_BADFLAGS,   "invalid value for ai_flags" },
	{ EAI_FAIL,       "non-recoverable failure in name resolution" },
	{ EAI_FAMILY,     "ai_family not supported" },
	{ EAI_MEMORY,     "memory allocation failure" },
	{ EAI_NODATA,     "no address associated with hostname" },
	{ EAI_NONAME,     "hostname nor servname provided, or not known" }, 
	{ EAI_SERVICE,    "servname not supported for ai_socktype" }, 
	{ EAI_SOCKTYPE,   "ai_socktype not supported" }, 
	{ EAI_SYSTEM,     "system error returned in errno" }, 
	{ EAI_BADHINTS,   "invalid value for hints" }, 
	{ EAI_PROTOCOL,   "resolved protocol is unknown" }, 
	{ EAI_OVERFLOW,   "argument buffer overflow" }, 
};

const char *gai_strerror (int code)
{
	const char *result = "unknown error";
	for (size_t i = 0; i < sizeof (gai_errors) / sizeof (gai_errors[0]); ++i)
	if (gai_errors[i].code == code)
	{
		result = gai_errors[i].error_string;
		break;
	}

	return result;
}
