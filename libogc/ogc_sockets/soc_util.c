#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <sys/socket.h>

// Adapted from libctru
static int _inetAtonDetail(int inBase, size_t *outNumBytes, const char *cp, struct in_addr *inp)
{
    int      base;
    uint32_t val;
    int      c;
    char     bytes[4];
    size_t   num_bytes = 0;

    c = *cp;
    for (;;)
    {
        if (!isdigit(c))
            return 0;

        val  = 0;
        base = inBase;
        if (!base)
        {
            base = 10;
            if (c == '0')
            {
                c = *++cp;
                if (c == 'x' || c == 'X')
                {
                    base = 16;
                    c    = *++cp;
                } else
                    base = 8;
            }
        }

        for (;;)
        {
            if (isdigit(c))
            {
                if (base == 8 && c >= '8')
                    return 0;
                val *= base;
                val += c - '0';
                c = *++cp;
            } else if (base == 16 && isxdigit(c))
            {
                val *= base;
                val += c + 10 - (islower(c) ? 'a' : 'A');
                c = *++cp;
            } else
                break;
        }

        if (c == '.')
        {
            if (num_bytes > 3)
                return 0;
            if (val > 0xFF)
                return 0;
            bytes[num_bytes++] = val;
            c                  = *++cp;
        } else
            break;
    }

    if (c != 0)
    {
        *outNumBytes = num_bytes;
        return 0;
    }

    switch (num_bytes)
    {
    case 0:
        break;

    case 1:
        if (val > 0xFFFFFF)
            return 0;
        val |= bytes[0] << 24;
        break;

    case 2:
        if (val > 0xFFFF)
            return 0;
        val |= bytes[0] << 24;
        val |= bytes[1] << 16;
        break;

    case 3:
        if (val > 0xFF)
            return 0;
        val |= bytes[0] << 24;
        val |= bytes[1] << 16;
        val |= bytes[2] << 8;
        break;
    }

    if (inp)
        inp->s_addr = htonl(val);

    *outNumBytes = num_bytes;

    return 1;
}

// Adapted from libctru
static const char *inet_ntop4(const void *src, char *dst, socklen_t size)
{
    const uint8_t *ip = src;

    char        *p;
    size_t       i;
    unsigned int n;

    if (size < INET_ADDRSTRLEN)
    {
        errno = ENOSPC;
        return NULL;
    }

    for (p = dst, i = 0; i < 4; ++i)
    {
        if (i > 0)
            *p++ = '.';

        n = ip[i];
        if (n >= 100)
        {
            *p++ = n / 100 + '0';
            n %= 100;
        }
        if (n >= 10 || ip[i] >= 100)
        {
            *p++ = n / 10 + '0';
            n %= 10;
        }
        *p++ = n + '0';
    }
    *p = 0;

    return dst;
}

static int inet_pton4(const char *src, void *dst)
{
    size_t numBytes;

    int ret = _inetAtonDetail(10, &numBytes, src, (struct in_addr *)dst);
    return (ret == 1 && numBytes == 3) ? 1 : 0;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    switch (af)
    {
    case AF_INET:
        return inet_ntop4(src, dst, size);
    default:
        errno = EAFNOSUPPORT;
        return NULL;
    }
}

int inet_pton(int af, const char *src, void *dst)
{
    switch (af)
    {
    case AF_INET:
        return inet_pton4(src, dst);
    default:
        errno = EAFNOSUPPORT;
        return -1;
    }
}

char *inet_ntoa(struct in_addr in)
{
    static char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in.s_addr, buffer, INET_ADDRSTRLEN);
    return buffer;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
    size_t numBytes;
    return _inetAtonDetail(0, &numBytes, cp, inp);
}

/*
 * Ascii internet address interpretation routine.
 * The value returned is in network order.
 */

/* inet_addr */
in_addr_t inet_addr(const char *cp)
{
    struct in_addr val;

    if (inet_aton(cp, &val))
    {
        return (val.s_addr);
    }
    return (INADDR_NONE);
}
