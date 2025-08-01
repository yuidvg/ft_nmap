#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>

/* -------------------------------------------------------------
 * Minimal representation of a port scan result.
 * -----------------------------------------------------------*/

typedef enum
{
    PORT_STATE_OPEN,
    PORT_STATE_CLOSED,
    PORT_STATE_FILTERED
} port_state_t;

/* A tiny wrapper around the parameters we need for a SYN scan. */
typedef struct
{
    const char *const target_ip; /* dotted-quad IPv4 */
    const int         timeout_ms; /* fixed timeout per probe */
} syn_scan_config_t;

/* -------------------------------------------------------------
 * Perform *one* SYN probe against `port` on `config.target_ip`.
 * Returns the observed state of that port.
 * -----------------------------------------------------------*/
port_state_t syn_scan_port(const syn_scan_config_t config, const int port);

#endif /* SCANNER_H */
