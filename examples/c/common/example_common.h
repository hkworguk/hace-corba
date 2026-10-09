/*
 * Shared helpers for the examples/c client/server pairs: parsing an
 * optional IOR string argument (falling back to a sensible localhost
 * default), and announcing a server's own IOR on startup. Not part of
 * the runtime -- this is example/demo code, so unlike runtime/ it uses
 * ordinary libc (printf/fprintf) freely; the DAL C "no stdio" posture
 * applies to the certified runtime, not to host-side example programs.
 *
 * This is the one pattern genuinely repeated across all four examples,
 * not a framework -- each example's actual CORBA logic (the servant,
 * the request/reply encoding) stays in that example's own server.c/
 * client.c.
 */
#ifndef HACE_EXAMPLE_COMMON_H
#define HACE_EXAMPLE_COMMON_H

#include "hace/corba_ior.h"
#include "hace/hal.h"

#include <stdio.h>
#include <string.h>

/* 127.0.0.1 -- every example defaults to localhost; pass a real IOR
 * string (as printed by a server) to run client/server on separate
 * hosts instead. */
#define HACE_EXAMPLE_LOOPBACK_IPV4 (((uint32_t)127U << 24) | 1U)

static inline void hace_example_default_ior(hace_corba_ior_t *ior, uint16_t port,
                                             const uint8_t *object_key,
                                             size_t object_key_length)
{
    ior->host_ipv4 = HACE_EXAMPLE_LOOPBACK_IPV4;
    ior->port = port;
    (void)memcpy(ior->object_key, object_key, object_key_length);
    ior->object_key_length = object_key_length;
}

static inline void hace_example_announce_server(const char *role,
                                                 const hace_corba_ior_t *ior)
{
    char str[HACE_CORBA_MAX_IOR_STRING];

    if (hace_corba_object_to_string(ior, str, sizeof(str)) == HACE_CORBA_OK) {
        (void)printf("%s listening as %s\n", role, str);
        (void)fflush(stdout);
    }
}

/* Opens a UDP socket on port, builds the corresponding default IOR,
 * and announces it. Returns 1 on success, 0 on failure (with a message
 * already printed to stderr). */
static inline int hace_example_server_start(hace_hal_socket_handle_t *out_socket,
                                             hace_corba_ior_t *out_ior, const char *role,
                                             uint16_t port, const uint8_t *object_key,
                                             size_t object_key_length)
{
    if (hace_hal_socket_open(out_socket, port) != HACE_HAL_OK) {
        (void)fprintf(stderr, "%s: could not open UDP socket on port %u\n", role,
                       (unsigned)port);
        return 0;
    }
    hace_example_default_ior(out_ior, port, object_key, object_key_length);
    hace_example_announce_server(role, out_ior);
    return 1;
}

/* Parses argv[arg_index] as an IOR string (as printed by
 * hace_example_announce_server) if present, else falls back to the
 * localhost default for default_port/default_object_key. Always
 * succeeds in producing *some* IOR in *out_ior; returns 0 only if an
 * explicitly-given argument was malformed (a warning is printed either
 * way that argument is used). */
static inline int hace_example_parse_ior_arg(int argc, char **argv, int arg_index,
                                              uint16_t default_port,
                                              const uint8_t *default_object_key,
                                              size_t default_object_key_length,
                                              hace_corba_ior_t *out_ior)
{
    if ((argc > arg_index) && (argv[arg_index] != NULL)) {
        if (hace_corba_string_to_object(argv[arg_index], out_ior) == HACE_CORBA_OK) {
            return 1;
        }
        (void)fprintf(stderr,
                       "warning: could not parse IOR argument '%s', using the "
                       "localhost default instead\n",
                       argv[arg_index]);
    }
    hace_example_default_ior(out_ior, default_port, default_object_key,
                              default_object_key_length);
    return 1;
}

#endif /* HACE_EXAMPLE_COMMON_H */
