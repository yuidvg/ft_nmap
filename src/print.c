#include "internal.h"

const char *scan_type2str(const scan_type_t scan_type)
{
    switch (scan_type)
    {
    case SCAN_TYPE_SYN:
        return "SYN";
    case SCAN_TYPE_NULL:
        return "NULL";
    case SCAN_TYPE_FIN:
        return "FIN";
    case SCAN_TYPE_XMAS:
        return "XMAS";
    case SCAN_TYPE_ACK:
        return "ACK";
    case SCAN_TYPE_UDP:
        return "UDP";
    case SCAN_TYPE_ALL:
        return "ALL";
    default:
        return "UNKNOWN";
    }
}
