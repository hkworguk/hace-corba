/*
 * observer_attach subject: activates a Subject object (object key
 * "subj") whose attach(obs) operation registers a caller-supplied
 * Observer object reference (CDR-encoded IOR, since there's no idlc
 * to marshal an object reference parameter automatically). Every
 * ~1s of wall-clock time, pushes one simulated event to every
 * attached observer via a oneway Observer.update() call -- the server
 * acting as a client on the very same socket it serves attach() on.
 * No BiDirIIOP-style negotiation needed (see
 * docs/architecture/adr-0002-giop-framing.md /
 * adr-0003-uiop-transport.md): just two ordinary object references,
 * each side's POA handling whichever direction a call comes from.
 * See ../server.idl for the hand-implemented interfaces.
 */
#include "../../common/c/example_common.h"
#include "hace/corba_poa.h"

#include <stdio.h>
#include <string.h>

#define SUBJECT_PORT 59131U
#define NOTIFY_COUNT 3U
#define NOTIFY_INTERVAL_MS 1000U
#define MAX_ATTACHED 4U

static const uint8_t SUBJECT_OBJECT_KEY[] = {(uint8_t)'s', (uint8_t)'u', (uint8_t)'b',
                                              (uint8_t)'j'};

typedef struct {
    hace_corba_ior_t observers[MAX_ATTACHED];
    int in_use[MAX_ATTACHED];
    size_t count;
} observer_registry_t;

static uint64_t time_to_ms(const hace_hal_time_t *t)
{
    return ((uint64_t)t->seconds * 1000ULL) + ((uint64_t)t->nanoseconds / 1000000ULL);
}

static void attach_servant(void *context, const char *operation,
                            hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                            hace_corba_system_exception_t *out_exception)
{
    observer_registry_t *registry = (observer_registry_t *)context;
    hace_corba_ior_t observer_ior;
    size_t slot;
    size_t i;

    (void)reply;

    if (strcmp(operation, "attach") != 0) {
        out_exception->exception = HACE_CORBA_EXC_BAD_OPERATION;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }
    if (hace_corba_ior_decode(args, &observer_ior) != HACE_CORBA_OK) {
        out_exception->exception = HACE_CORBA_EXC_MARSHAL;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    slot = MAX_ATTACHED;
    for (i = 0U; i < MAX_ATTACHED; i++) {
        if (!registry->in_use[i]) {
            slot = i;
            break;
        }
    }
    if (slot == MAX_ATTACHED) {
        out_exception->exception = HACE_CORBA_EXC_NO_MEMORY;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    registry->observers[slot] = observer_ior;
    registry->in_use[slot] = 1;
    registry->count++;
    (void)printf("subject: observer attached (slot %zu), total=%zu\n", slot,
                  registry->count);
    (void)fflush(stdout);
}

static int build_update_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                                  const char *event, const uint8_t *object_key,
                                  size_t object_key_length)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = 0; /* oneway */
    (void)memcpy(header.object_key, object_key, object_key_length);
    header.object_key_length = object_key_length;
    (void)strncpy(header.operation, "update", sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_cdr_encode_string(buf, event, 128U) != HACE_CDR_OK) {
        return 0;
    }
    if (hace_giop_finish_message(buf, HACE_GIOP_MSG_REQUEST, 0) != HACE_GIOP_OK) {
        return 0;
    }
    return 1;
}

static void notify_all(const observer_registry_t *registry,
                        const hace_uiop_transport_t *transport,
                        uint32_t *next_request_id, const char *event)
{
    size_t i;

    for (i = 0U; i < MAX_ATTACHED; i++) {
        if (registry->in_use[i]) {
            uint8_t storage[256];
            hace_cdr_buffer_t buf;
            uint32_t id = (*next_request_id)++;

            if ((hace_cdr_buffer_init(&buf, storage, sizeof(storage),
                                       HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK) &&
                build_update_request(&buf, id, event, registry->observers[i].object_key,
                                      registry->observers[i].object_key_length) &&
                (hace_uiop_send_message(transport, &buf, HACE_GIOP_MSG_REQUEST, id,
                                         registry->observers[i].host_ipv4,
                                         registry->observers[i].port,
                                         HACE_UIOP_MAX_DATAGRAM_SIZE) == HACE_UIOP_OK)) {
                (void)printf("subject: notified observer %zu: %s\n", i, event);
            } else {
                (void)fprintf(stderr, "subject: failed to notify observer %zu\n", i);
            }
        }
    }
    (void)fflush(stdout);
}

int main(void)
{
    static const char *const events[] = {"temperature=21C", "temperature=23C",
                                           "temperature=19C"};
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_ior_t ior;
    hace_corba_poa_t poa;
    hace_uiop_transport_t transport;
    observer_registry_t registry;
    hace_hal_time_t start_time;
    uint64_t start_ms;
    uint32_t next_request_id = 1000U; /* a range distinct from clients' own ids */
    size_t notifications_sent = 0U;

    (void)memset(&registry, 0, sizeof(registry));

    if (!hace_example_server_start(&server_socket, &ior, "observer_attach subject",
                                    SUBJECT_PORT, SUBJECT_OBJECT_KEY,
                                    sizeof(SUBJECT_OBJECT_KEY))) {
        return 1;
    }

    if ((hace_corba_poa_init(&poa) != HACE_CORBA_OK) ||
        (hace_corba_poa_activate_object(&poa, SUBJECT_OBJECT_KEY,
                                         sizeof(SUBJECT_OBJECT_KEY), attach_servant,
                                         &registry) != HACE_CORBA_OK)) {
        (void)fprintf(stderr, "subject: poa setup failed\n");
        return 1;
    }

    transport = hace_uiop_transport_for_socket(&server_socket);

    if (hace_hal_time_now(&start_time) != HACE_HAL_OK) {
        (void)fprintf(stderr, "subject: could not read the clock\n");
        (void)hace_hal_socket_close(server_socket);
        return 1;
    }
    start_ms = time_to_ms(&start_time);

    while (notifications_sent < NOTIFY_COUNT) {
        hace_hal_time_t now_time;
        hace_uiop_status_t status = hace_corba_poa_handle_one_request(
            &poa, &transport, 200U, HACE_UIOP_MAX_DATAGRAM_SIZE);

        if ((status != HACE_UIOP_OK) && (status != HACE_UIOP_ERR_TIMEOUT)) {
            (void)fprintf(stderr, "subject: dispatch error (%d)\n", (int)status);
            break;
        }

        if (hace_hal_time_now(&now_time) != HACE_HAL_OK) {
            break;
        }
        if ((time_to_ms(&now_time) - start_ms) >=
            ((uint64_t)(notifications_sent + 1U) * (uint64_t)NOTIFY_INTERVAL_MS)) {
            notify_all(&registry, &transport, &next_request_id,
                       events[notifications_sent]);
            notifications_sent++;
        }
    }

    (void)hace_hal_socket_close(server_socket);
    return 0;
}
