#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "stnc_config.h"
#include "stnc_directory.h"
#include "stnc_http.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_network.h"
#include "stnc_platform.h"
#include "stnc_peers.h"
#include "stnc_peer_select.h"
#include "stnc_stnc.h"
#include "stnc_stnp.h"

#define STNC_INFO_REQUEST_ID UINT64_C(1)
#define STNC_CHAIN_REFRESH_INTERVAL_MS 10000u
#define STNC_RUNTIME_WAIT_MS 100u
#define STNC_RECONNECT_INTERVAL_MS 5000u
#define STNC_ROOT_PEER_RETRY_INTERVAL_MS 30000u
#define STNC_P2P_REFRESH_INTERVAL_MS 5000u
#define STNC_DIRECTORY_HOST "stn-chain.org"
#define STNC_DIRECTORY_PATH "/peers?format=json"

static stnc_core_state core_state = STNC_CORE_STATE_UNINITIALIZED;
static stnc_network_connection chain_connection;
static stnc_network_connection p2p_connection;
static stnc_core_chain_state chain_state;
static uint32_t root_peer_capabilities;
static stnc_peer_candidates peer_candidates;
static stnc_peer_qualified_set qualified_peers;

static int stnc_core_probe_selected_peer_headers(const stnc_stnp_state *peer_state);
static int stnc_core_select_peer(void);

static void stnc_core_clear_chain_state(void)
{
    memset(&chain_state, 0, sizeof(chain_state));
}

static void stnc_core_store_chain_info(const stnc_chain_info *info)
{
    if (info == NULL) {
        return;
    }

    stnc_core_clear_chain_state();

    memcpy(chain_state.network_id, info->network_id, sizeof(chain_state.network_id));
    memcpy(chain_state.genesis_id, info->genesis_id, sizeof(chain_state.genesis_id));
    chain_state.height = info->height;
    memcpy(chain_state.tip_id, info->tip_id, sizeof(chain_state.tip_id));
    memcpy(chain_state.cumulative_work, info->cumulative_work, sizeof(chain_state.cumulative_work));
    memcpy(chain_state.current_target, info->current_target, sizeof(chain_state.current_target));
    chain_state.protocol_revision = info->protocol_revision;
    chain_state.block_count = info->block_count;
    chain_state.available = 1;
}

static int stnc_core_request_chain_info(int log_request)
{
    stnc_stnc_message request;
    stnc_stnc_message response;
    stnc_chain_info info;
    unsigned char request_buffer[STNC_STNC_HEADER_SIZE];
    unsigned char response_header[STNC_STNC_HEADER_SIZE];
    unsigned char response_payload[STNC_STNC_INFO_SIZE];
    size_t request_length;
    char message[512];

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    memset(&info, 0, sizeof(info));

    request.kind = STNC_STNC_REQUEST;
    request.method = STNC_STNC_METHOD_INFO;
    request.code = STNC_STNC_OK;
    request.request_id = STNC_INFO_REQUEST_ID;

    if (stnc_stnc_encode(&request, request_buffer, sizeof(request_buffer), &request_length) != 0) {
        stnc_log_error("STNC INFO request encoding failed.");
        return 1;
    }

    if (request_length != STNC_STNC_HEADER_SIZE) {
        stnc_log_error("STNC INFO request length is invalid.");
        return 1;
    }

    if (log_request) {
        stnc_log_info("Sending STNC INFO request.");
    }

    if (stnc_network_send(&chain_connection, request_buffer, request_length) != 0) {
        stnc_log_error("STNC INFO request transmission failed.");
        return 1;
    }

    if (stnc_network_receive(&chain_connection, response_header, sizeof(response_header)) != 0) {
        stnc_log_error("STNC INFO response header receive failed.");
        return 1;
    }

    if (stnc_stnc_decode_header(response_header, sizeof(response_header), &response) != 0) {
        stnc_log_error("STNC INFO response header is invalid.");
        return 1;
    }

    if (response.kind != STNC_STNC_RESPONSE) {
        stnc_log_error("STNC INFO response kind is invalid.");
        return 1;
    }

    if (response.method != STNC_STNC_METHOD_INFO) {
        stnc_log_error("STNC INFO response method does not match request.");
        return 1;
    }

    if (response.request_id != STNC_INFO_REQUEST_ID) {
        stnc_log_error("STNC INFO response request ID does not match request.");
        return 1;
    }

    if (response.code != STNC_STNC_OK) {
        if (snprintf(message, sizeof(message), "STNC INFO request rejected: code=%u",
                (unsigned int)response.code) < 0) {
            return 1;
        }
        stnc_log_error(message);
        return 1;
    }

    if (response.length != STNC_STNC_INFO_SIZE) {
        if (snprintf(message, sizeof(message), "STNC INFO response payload length is invalid: %zu",
                response.length) < 0) {
            return 1;
        }
        stnc_log_error(message);
        return 1;
    }

    if (stnc_network_receive(&chain_connection, response_payload, sizeof(response_payload)) != 0) {
        stnc_log_error("STNC INFO response payload receive failed.");
        return 1;
    }

    if (stnc_stnc_decode_info(response_payload, sizeof(response_payload), &info) != 0) {
        stnc_log_error("STNC INFO response payload is invalid.");
        return 1;
    }

    stnc_core_store_chain_info(&info);

    if (log_request) {
        if (snprintf(message, sizeof(message),
                "STNC v2 confirmed: height=%" PRIu64 " blocks=%" PRIu32 " protocol=%" PRIu32,
                chain_state.height, chain_state.block_count, chain_state.protocol_revision) < 0) {
            stnc_core_clear_chain_state();
            return 1;
        }
        stnc_log_info(message);
    }

    return 0;
}

static int stnc_core_connect_configured_peer(int log_qualification)
{
    const stnc_config *config;

    config = stnc_config_get();

    if (config == NULL) {
        stnc_log_error("STNC Core configuration is unavailable.");
        return 1;
    }

    if (stnc_network_is_connected(&chain_connection)) {
        stnc_network_disconnect(&chain_connection);
    }

    if (stnc_network_connect(&chain_connection, config->peer, config->port) != 0) {
        return 1;
    }

    if (stnc_core_request_chain_info(log_qualification) != 0) {
        stnc_core_clear_chain_state();
        stnc_network_disconnect(&chain_connection);
        return 1;
    }

    return 0;
}



static int stnc_core_discover_directory_peers(void)
{
    char response[STNC_DIRECTORY_RESPONSE_MAX + 1u];
    size_t response_length;
    size_t previous_count;
    char message[512];

    memset(response, 0, sizeof(response));
    response_length = 0;
    previous_count = peer_candidates.count;

    if (stnc_http_get_https(
            STNC_DIRECTORY_HOST,
            STNC_DIRECTORY_PATH,
            response,
            sizeof(response),
            &response_length
        ) != 0) {
        stnc_log_error("ChAoS MVC peer directory request failed.");
        return 1;
    }

    if (stnc_directory_decode(
            response,
            response_length,
            &peer_candidates
        ) != 0) {
        stnc_log_error("ChAoS MVC peer directory response is invalid.");
        return 1;
    }

    if (snprintf(
            message,
            sizeof(message),
            "ChAoS MVC peer directory admitted %zu candidate(s); Core set contains %zu candidate(s).",
            peer_candidates.count - previous_count,
            peer_candidates.count
        ) < 0) {
        return 1;
    }

    stnc_log_info(message);
    return 0;
}


static int stnc_core_qualify_candidate(
    const stnc_peer_candidate *candidate,
    stnc_peer_qualified *qualified
)
{
    stnc_network_connection connection;
    stnc_stnp_hello hello;
    uint8_t request[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    uint8_t response[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    size_t written;
    uint64_t started;
    uint64_t finished;
    char message[512];

    if (candidate == NULL || qualified == NULL || !chain_state.available) {
        stnc_log_error("Chain P2P candidate qualification prerequisites are unavailable.");
        return 1;
    }

    memset(&connection, 0, sizeof(connection));
    memset(&hello, 0, sizeof(hello));
    memset(qualified, 0, sizeof(*qualified));

    if (snprintf(
            message,
            sizeof(message),
            "Qualifying Chain P2P candidate: %s:%u",
            candidate->host,
            (unsigned int)candidate->port
        ) < 0) {
        return 1;
    }
    stnc_log_info(message);

    started = stnc_platform_monotonic_ms();

    if (stnc_network_connect(
            &connection,
            candidate->host,
            candidate->port
        ) != 0) {
        stnc_log_error("Chain P2P candidate CONNECT failed.");
        return 1;
    }
    stnc_log_info("Chain P2P candidate CONNECT passed.");

    if (stnc_stnp_encode_hello(
            chain_state.network_id,
            chain_state.genesis_id,
            1u,
            request,
            sizeof(request),
            &written
        ) != 0 ||
        written != sizeof(request)) {
        stnc_log_error("Chain P2P candidate HELLO ENCODE failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate HELLO ENCODE passed: 80 bytes.");

    if (stnc_network_send(&connection, request, written) != 0) {
        stnc_log_error("Chain P2P candidate HELLO SEND failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate HELLO SEND passed: 80 bytes.");

    if (stnc_network_receive(
            &connection,
            response,
            sizeof(response)
        ) != 0) {
        stnc_log_error("Chain P2P candidate HELLO RECEIVE failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate HELLO RECEIVE passed: 80 bytes.");

    if (stnc_stnp_decode_hello(
            response,
            sizeof(response),
            &hello
        ) != 0) {
        stnc_log_error("Chain P2P candidate HELLO DECODE failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate HELLO DECODE passed.");

    if (memcmp(hello.network_id, chain_state.network_id, 32) != 0) {
        stnc_log_error("Chain P2P candidate NETWORK ID validation failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate NETWORK ID validation passed.");

    if (memcmp(hello.genesis_id, chain_state.genesis_id, 32) != 0) {
        stnc_log_error("Chain P2P candidate GENESIS ID validation failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate GENESIS ID validation passed.");

    if (hello.capabilities != 1u && hello.capabilities != 3u) {
        stnc_log_error("Chain P2P candidate CAPABILITIES validation failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P candidate CAPABILITIES validation passed.");

    finished = stnc_platform_monotonic_ms();
    stnc_network_disconnect(&connection);

    qualified->candidate = *candidate;
    qualified->capabilities = hello.capabilities;
    qualified->latency_ms = finished >= started
        ? finished - started
        : 0u;

    return 0;
}

static int stnc_core_open_selected_peer(
    const stnc_peer_qualified *selected
)
{
    stnc_stnp_hello hello;
    stnc_stnp_state state;
    uint8_t hello_request[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    uint8_t hello_response[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    uint8_t state_request[STNC_STNP_HEADER_SIZE];
    uint8_t state_response[STNC_STNP_HEADER_SIZE + STNC_STNP_STATE_SIZE];
    size_t written;
    char message[512];
    int same_state;

    if (selected == NULL || !chain_state.available) {
        stnc_log_error("Selected Chain P2P peer session prerequisites are unavailable.");
        return 1;
    }

    memset(&hello, 0, sizeof(hello));
    memset(&state, 0, sizeof(state));

    if (stnc_network_is_connected(&p2p_connection)) {
        stnc_network_disconnect(&p2p_connection);
    }

    if (snprintf(
            message,
            sizeof(message),
            "Opening selected Chain P2P session: %s:%u",
            selected->candidate.host,
            (unsigned int)selected->candidate.port
        ) < 0) {
        return 1;
    }
    stnc_log_info(message);

    if (stnc_network_connect(
            &p2p_connection,
            selected->candidate.host,
            selected->candidate.port
        ) != 0) {
        stnc_log_error("Selected Chain P2P peer CONNECT failed.");
        return 1;
    }
    stnc_log_info("Selected Chain P2P peer CONNECT passed.");

    if (stnc_stnp_encode_hello(
            chain_state.network_id,
            chain_state.genesis_id,
            1u,
            hello_request,
            sizeof(hello_request),
            &written
        ) != 0 ||
        written != sizeof(hello_request)) {
        stnc_log_error("Selected Chain P2P peer HELLO ENCODE failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (stnc_network_send(&p2p_connection, hello_request, written) != 0) {
        stnc_log_error("Selected Chain P2P peer HELLO SEND failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (stnc_network_receive(
            &p2p_connection,
            hello_response,
            sizeof(hello_response)
        ) != 0) {
        stnc_log_error("Selected Chain P2P peer HELLO RECEIVE failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (stnc_stnp_decode_hello(
            hello_response,
            sizeof(hello_response),
            &hello
        ) != 0 ||
        memcmp(hello.network_id, chain_state.network_id, 32) != 0 ||
        memcmp(hello.genesis_id, chain_state.genesis_id, 32) != 0 ||
        (hello.capabilities != 1u && hello.capabilities != 3u)) {
        stnc_log_error("Selected Chain P2P peer HELLO validation failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }
    stnc_log_info("Selected Chain P2P peer HELLO validation passed.");

    if (stnc_stnp_encode_state(
            state_request,
            sizeof(state_request),
            &written
        ) != 0 ||
        written != sizeof(state_request)) {
        stnc_log_error("Selected Chain P2P peer STATE ENCODE failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (stnc_network_send(&p2p_connection, state_request, written) != 0) {
        stnc_log_error("Selected Chain P2P peer STATE SEND failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }
    stnc_log_info("Selected Chain P2P peer STATE request sent.");

    if (stnc_network_receive(
            &p2p_connection,
            state_response,
            sizeof(state_response)
        ) != 0) {
        stnc_log_error("Selected Chain P2P peer STATE RECEIVE failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (stnc_stnp_decode_state(
            state_response,
            sizeof(state_response),
            &state
        ) != 0) {
        stnc_log_error("Selected Chain P2P peer STATE validation failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (snprintf(
            message,
            sizeof(message),
            "Selected Chain P2P peer STATE validated: height=%" PRIu64 " blocks=%" PRIu32,
            state.height,
            state.block_count
        ) < 0) {
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }
    stnc_log_info(message);

    same_state =
        state.height == chain_state.height &&
        state.block_count == chain_state.block_count &&
        memcmp(state.tip_id, chain_state.tip_id, 32) == 0 &&
        memcmp(state.cumulative_work, chain_state.cumulative_work, 40) == 0;

    if (same_state) {
        stnc_log_info("Selected Chain P2P peer STATE matches current qualified STNC Chain view.");
    } else {
        if (snprintf(
                message,
                sizeof(message),
                "Selected Chain P2P peer STATE differs from current STNC Chain view: peer_height=%" PRIu64 " peer_blocks=%" PRIu32 " local_height=%" PRIu64 " local_blocks=%" PRIu32,
                state.height,
                state.block_count,
                chain_state.height,
                chain_state.block_count
            ) < 0) {
            stnc_network_disconnect(&p2p_connection);
            return 1;
        }
        stnc_log_info(message);
    }

    stnc_log_info("Selected Chain P2P operational session established.");

    if (stnc_core_probe_selected_peer_headers(&state) != 0) {
        stnc_log_error("Selected Chain P2P forward header evidence probe failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    return 0;
}

static int stnc_core_probe_selected_peer_headers(
    const stnc_stnp_state *peer_state
)
{
    uint8_t request[STNC_STNP_HEADER_SIZE + 8u];
    uint8_t response_header[STNC_STNP_HEADER_SIZE];
    uint8_t response_frame[STNC_STNP_HEADERS_FRAME_MAX];
    size_t written;
    size_t payload_length;
    uint32_t start;
    uint32_t count;
    uint64_t first_index;
    uint64_t available;
    char message[512];

    if (peer_state == NULL ||
        !chain_state.available ||
        !stnc_network_is_connected(&p2p_connection)) {
        stnc_log_error("Selected Chain P2P header evidence prerequisites are unavailable.");
        return 1;
    }

    first_index = chain_state.block_count;
    if ((uint64_t)peer_state->block_count <= first_index) {
        stnc_log_info("Selected Chain P2P peer has no forward header evidence beyond the current STNC Chain view.");
        return 0;
    }

    available = (uint64_t)peer_state->block_count - first_index;
    count = available > STNC_STNP_HEADERS_MAX
        ? STNC_STNP_HEADERS_MAX
        : (uint32_t)available;

    if (first_index > UINT32_MAX) {
        stnc_log_error("Selected Chain P2P header evidence start exceeds STNP index range.");
        return 1;
    }
    start = (uint32_t)first_index;

    if (stnc_stnp_encode_get_headers(
            start,
            count,
            request,
            sizeof(request),
            &written
        ) != 0 ||
        written != sizeof(request)) {
        stnc_log_error("Selected Chain P2P peer GET_HEADERS ENCODE failed.");
        return 1;
    }

    if (stnc_network_send(&p2p_connection, request, written) != 0) {
        stnc_log_error("Selected Chain P2P peer GET_HEADERS SEND failed.");
        return 1;
    }

    if (snprintf(
            message,
            sizeof(message),
            "Selected Chain P2P peer GET_HEADERS sent: start=%" PRIu32 " count=%" PRIu32,
            start,
            count
        ) < 0) {
        return 1;
    }
    stnc_log_info(message);

    if (stnc_network_receive(
            &p2p_connection,
            response_header,
            sizeof(response_header)
        ) != 0 ||
        stnc_stnp_decode_headers_header(
            response_header,
            sizeof(response_header),
            &payload_length
        ) != 0) {
        stnc_log_error("Selected Chain P2P peer HEADERS header is unavailable or invalid.");
        return 1;
    }

    memcpy(response_frame, response_header, sizeof(response_header));

    if (stnc_network_receive(
            &p2p_connection,
            response_frame + STNC_STNP_HEADER_SIZE,
            payload_length
        ) != 0 ||
        stnc_stnp_decode_headers(
            response_frame,
            STNC_STNP_HEADER_SIZE + payload_length,
            &start,
            &count
        ) != 0) {
        stnc_log_error("Selected Chain P2P peer HEADERS payload is unavailable or invalid.");
        return 1;
    }

    if ((uint64_t)start != first_index ||
        count == 0u ||
        (uint64_t)count > available) {
        stnc_log_error("Selected Chain P2P peer HEADERS range does not match the requested evidence.");
        return 1;
    }

    if (snprintf(
            message,
            sizeof(message),
            "Selected Chain P2P peer HEADERS evidence received: start=%" PRIu32 " count=%" PRIu32,
            start,
            count
        ) < 0) {
        return 1;
    }
    stnc_log_info(message);
    stnc_log_info("Selected Chain P2P header evidence remains unaccepted pending full block retrieval and Chain validation.");
    return 0;
}

static int stnc_core_select_peer(void)
{
    const stnc_peer_qualified *selected;
    size_t index;
    char message[512];

    stnc_peer_select_clear(&qualified_peers);

    for (index = 0; index < peer_candidates.count; ++index) {
        stnc_peer_qualified qualified;

        if (stnc_core_qualify_candidate(
                &peer_candidates.entries[index],
                &qualified
            ) != 0) {
            if (snprintf(
                    message,
                    sizeof(message),
                    "Chain P2P candidate rejected: %s:%u",
                    peer_candidates.entries[index].host,
                    (unsigned int)peer_candidates.entries[index].port
                ) < 0) {
                return 1;
            }

            stnc_log_info(message);
            continue;
        }

        if (stnc_peer_select_add(&qualified_peers, &qualified) != 0) {
            return 1;
        }

        if (snprintf(
                message,
                sizeof(message),
                "Chain P2P candidate qualified: %s:%u latency=%" PRIu64 "ms capabilities=%" PRIu32,
                qualified.candidate.host,
                (unsigned int)qualified.candidate.port,
                qualified.latency_ms,
                qualified.capabilities
            ) < 0) {
            return 1;
        }

        stnc_log_info(message);
    }

    selected = stnc_peer_select_best(&qualified_peers);

    if (selected == NULL) {
        stnc_log_info("No discovered Chain P2P candidate qualified for selection.");
        return 0;
    }

    if (snprintf(
            message,
            sizeof(message),
            "Selected Chain P2P peer: %s:%u latency=%" PRIu64 "ms",
            selected->candidate.host,
            (unsigned int)selected->candidate.port,
            selected->latency_ms
        ) < 0) {
        return 1;
    }

    stnc_log_info(message);

    if (stnc_core_open_selected_peer(selected) != 0) {
        stnc_log_error("Selected Chain P2P peer operational session failed.");
        return 1;
    }

    return 0;
}

static int stnc_core_qualify_root_peer(void)
{
    const stnc_config *config;
    stnc_network_connection connection;
    stnc_stnp_hello hello;
    stnc_stnp_peers peers;
    uint8_t hello_request[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    uint8_t hello_response[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    uint8_t discovery_request[STNC_STNP_HEADER_SIZE];
    uint8_t discovery_header[STNC_STNP_HEADER_SIZE];
    uint8_t discovery_frame[STNC_STNP_PEERS_FRAME_MAX];
    size_t written;
    size_t payload_length;
    size_t index;
    char message[512];

    config = stnc_config_get();

    if (config == NULL || !chain_state.available) {
        stnc_log_error("Chain P2P root peer qualification prerequisites are unavailable.");
        return 1;
    }

    memset(&connection, 0, sizeof(connection));
    memset(&hello, 0, sizeof(hello));
    memset(&peers, 0, sizeof(peers));

    if (snprintf(message, sizeof(message),
            "Qualifying Chain P2P root peer: %s:%u",
            config->root_peer,
            (unsigned int)config->root_peer_port) < 0) {
        return 1;
    }
    stnc_log_info(message);

    if (stnc_network_connect(
            &connection,
            config->root_peer,
            config->root_peer_port
        ) != 0) {
        stnc_log_error("Chain P2P root peer CONNECT failed.");
        return 1;
    }
    stnc_log_info("Chain P2P root peer CONNECT passed.");

    if (stnc_stnp_encode_hello(
            chain_state.network_id,
            chain_state.genesis_id,
            1u,
            hello_request,
            sizeof(hello_request),
            &written
        ) != 0 ||
        written != sizeof(hello_request)) {
        stnc_log_error("Chain P2P root peer HELLO ENCODE failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer HELLO ENCODE passed: 80 bytes.");

    if (stnc_network_send(&connection, hello_request, written) != 0) {
        stnc_log_error("Chain P2P root peer HELLO SEND failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer HELLO SEND passed: 80 bytes.");

    if (stnc_network_receive(
            &connection,
            hello_response,
            sizeof(hello_response)
        ) != 0) {
        stnc_log_error("Chain P2P root peer HELLO RECEIVE failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer HELLO RECEIVE passed: 80 bytes.");

    if (stnc_stnp_decode_hello(
            hello_response,
            sizeof(hello_response),
            &hello
        ) != 0) {
        stnc_log_error("Chain P2P root peer HELLO DECODE failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer HELLO DECODE passed.");

    if (memcmp(hello.network_id, chain_state.network_id, 32) != 0) {
        stnc_log_error("Chain P2P root peer NETWORK ID validation failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer NETWORK ID validation passed.");

    if (memcmp(hello.genesis_id, chain_state.genesis_id, 32) != 0) {
        stnc_log_error("Chain P2P root peer GENESIS ID validation failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer GENESIS ID validation passed.");

    if (hello.capabilities != 1u && hello.capabilities != 3u) {
        stnc_log_error("Chain P2P root peer CAPABILITIES validation failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }
    stnc_log_info("Chain P2P root peer CAPABILITIES validation passed.");

    root_peer_capabilities = hello.capabilities;

    if ((root_peer_capabilities & 2u) != 0u) {
        if (stnc_stnp_encode_get_peers(
                discovery_request,
                sizeof(discovery_request),
                &written
            ) != 0 ||
            written != sizeof(discovery_request)) {
            stnc_log_error("Chain P2P root peer GET_PEERS ENCODE failed.");
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }

        if (stnc_network_send(
                &connection,
                discovery_request,
                written
            ) != 0) {
            stnc_log_error("Chain P2P root peer GET_PEERS SEND failed.");
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }

        if (stnc_network_receive(
                &connection,
                discovery_header,
                sizeof(discovery_header)
            ) != 0 ||
            stnc_stnp_decode_peers_header(
                discovery_header,
                sizeof(discovery_header),
                &payload_length
            ) != 0) {
            stnc_log_error("Chain P2P root peer PEERS header is unavailable or invalid.");
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }

        memcpy(discovery_frame, discovery_header, sizeof(discovery_header));

        if (stnc_network_receive(
                &connection,
                discovery_frame + STNC_STNP_HEADER_SIZE,
                payload_length
            ) != 0 ||
            stnc_stnp_decode_peers(
                discovery_frame,
                STNC_STNP_HEADER_SIZE + payload_length,
                &peers
            ) != 0) {
            stnc_log_error("Chain P2P root peer PEERS payload is unavailable or invalid.");
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }

        if (snprintf(message, sizeof(message),
                "Chain P2P root peer discovery received %zu candidate(s).",
                peers.count) < 0) {
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }
        stnc_log_info(message);

        if (stnc_peers_add_stnp(&peer_candidates, &peers) != 0) {
            stnc_log_error("Chain P2P root peer discovery candidate admission failed.");
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }

        if (snprintf(message, sizeof(message),
                "Core peer candidate set contains %zu candidate(s).",
                peer_candidates.count) < 0) {
            root_peer_capabilities = 0;
            stnc_network_disconnect(&connection);
            return 1;
        }
        stnc_log_info(message);

        for (index = 0; index < peers.count; ++index) {
            if (snprintf(message, sizeof(message),
                    "Discovered Chain P2P candidate: %u.%u.%u.%u:%u",
                    (unsigned int)peers.entries[index].address[0],
                    (unsigned int)peers.entries[index].address[1],
                    (unsigned int)peers.entries[index].address[2],
                    (unsigned int)peers.entries[index].address[3],
                    (unsigned int)peers.entries[index].port) < 0) {
                root_peer_capabilities = 0;
                stnc_network_disconnect(&connection);
                return 1;
            }
            stnc_log_info(message);
        }
    } else {
        stnc_log_info("Chain P2P root peer does not advertise peer discovery.");
    }

    stnc_network_disconnect(&connection);

    if (snprintf(message, sizeof(message),
            "Chain P2P root peer qualified: STNP v2 capabilities=%" PRIu32,
            root_peer_capabilities) < 0) {
        root_peer_capabilities = 0;
        return 1;
    }

    stnc_log_info(message);
    return 0;
}

static int stnc_core_refresh_selected_peer(void)
{
    stnc_stnp_state state;
    uint8_t request[STNC_STNP_HEADER_SIZE];
    uint8_t response[STNC_STNP_HEADER_SIZE + STNC_STNP_STATE_SIZE];
    size_t written;
    char message[512];

    if (!stnc_network_is_connected(&p2p_connection)) {
        return 1;
    }

    memset(&state, 0, sizeof(state));

    if (stnc_stnp_encode_state(
            request,
            sizeof(request),
            &written
        ) != 0 ||
        written != sizeof(request) ||
        stnc_network_send(&p2p_connection, request, written) != 0 ||
        stnc_network_receive(&p2p_connection, response, sizeof(response)) != 0 ||
        stnc_stnp_decode_state(response, sizeof(response), &state) != 0) {
        stnc_log_error("Selected Chain P2P peer runtime STATE refresh failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    if (snprintf(
            message,
            sizeof(message),
            "Selected Chain P2P peer runtime STATE: height=%" PRIu64 " blocks=%" PRIu32,
            state.height,
            state.block_count
        ) < 0) {
        return 1;
    }
    stnc_log_info(message);

    if (stnc_core_probe_selected_peer_headers(&state) != 0) {
        stnc_log_error("Selected Chain P2P peer runtime evidence probe failed.");
        stnc_network_disconnect(&p2p_connection);
        return 1;
    }

    return 0;
}

static int stnc_core_refresh_chain_state(void)
{
    uint64_t previous_height;
    uint32_t previous_block_count;
    char message[512];

    previous_height = chain_state.height;
    previous_block_count = chain_state.block_count;

    if (stnc_core_request_chain_info(0) != 0) {
        stnc_core_clear_chain_state();

        if (stnc_network_is_connected(&chain_connection)) {
            stnc_network_disconnect(&chain_connection);
        }

        stnc_log_error("Chain connection lost.");
        return 1;
    }

    if (chain_state.height != previous_height ||
        chain_state.block_count != previous_block_count) {
        if (snprintf(message, sizeof(message),
                "Chain state updated: height=%" PRIu64 " blocks=%" PRIu32,
                chain_state.height, chain_state.block_count) < 0) {
            return 1;
        }
        stnc_log_info(message);
    }

    return 0;
}

int stnc_core_init(void)
{
    const stnc_config *config;
    char message[512];

    if (core_state != STNC_CORE_STATE_UNINITIALIZED) {
        return 1;
    }

    stnc_core_clear_chain_state();
    stnc_peers_clear(&peer_candidates);
    stnc_peer_select_clear(&qualified_peers);

    if (stnc_platform_init() != 0) {
        return 1;
    }

    if (stnc_log_init() != 0) {
        stnc_platform_shutdown();
        return 1;
    }

    if (stnc_config_init() != 0) {
        stnc_log_error("STNC Core configuration initialization failed.");
        stnc_log_shutdown();
        stnc_platform_shutdown();
        return 1;
    }

    config = stnc_config_get();

    if (config == NULL) {
        stnc_log_error("STNC Core configuration is unavailable.");
        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();
        return 1;
    }

    if (snprintf(message, sizeof(message), "Configuration loaded: peer=%s port=%u",
            config->peer, (unsigned int)config->port) < 0) {
        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();
        return 1;
    }

    if (stnc_network_init() != 0) {
        stnc_log_error("STNC Core network initialization failed.");
        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();
        return 1;
    }

    if (stnc_platform_install_stop_handler() != 0) {
        stnc_log_error("STNC Core stop handler initialization failed.");
        stnc_network_shutdown();
        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();
        return 1;
    }

    core_state = STNC_CORE_STATE_INITIALIZED;
    stnc_log_info("STNC Core initialized.");
    stnc_log_info(message);
    stnc_log_info("Connecting to configured Chain peer.");

    if (stnc_core_connect_configured_peer(1) != 0) {
        stnc_log_error("Configured peer failed STNC v2 qualification.");
        stnc_network_shutdown();
        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();
        core_state = STNC_CORE_STATE_UNINITIALIZED;
        return 1;
    }

    stnc_log_info("Chain connection established.");
    stnc_log_info("Configured Chain peer qualified.");

    if (stnc_core_qualify_root_peer() != 0) {
        root_peer_capabilities = 0;
        stnc_log_error("Configured Chain P2P root peer is currently unavailable.");
        stnc_log_info("STNC Core will continue using the qualified Chain RPC connection.");
    }

    if (stnc_core_discover_directory_peers() != 0) {
        stnc_log_info("STNC Core will continue without the public peer directory.");
    }

    if (stnc_core_select_peer() != 0) {
        stnc_log_error("Automatic Chain P2P peer selection failed.");
    }

    return 0;
}

int stnc_core_run(void)
{
    unsigned int refresh_elapsed;
    unsigned int reconnect_elapsed;
    unsigned int root_peer_retry_elapsed;
    unsigned int p2p_refresh_elapsed;

    if (core_state != STNC_CORE_STATE_INITIALIZED) {
        return 1;
    }

    core_state = STNC_CORE_STATE_RUNNING;
    refresh_elapsed = 0;
    reconnect_elapsed = 0;
    root_peer_retry_elapsed = 0;
    p2p_refresh_elapsed = 0;
    stnc_log_info("STNC Core running.");

    while (core_state == STNC_CORE_STATE_RUNNING) {
        stnc_platform_wait(STNC_RUNTIME_WAIT_MS);

        if (stnc_network_is_connected(&chain_connection)) {
            refresh_elapsed += STNC_RUNTIME_WAIT_MS;
            reconnect_elapsed = 0;

            if (root_peer_capabilities == 0) {
                root_peer_retry_elapsed += STNC_RUNTIME_WAIT_MS;

                if (root_peer_retry_elapsed >= STNC_ROOT_PEER_RETRY_INTERVAL_MS) {
                    root_peer_retry_elapsed = 0;
                    stnc_log_info("Retrying Chain P2P root peer qualification.");

                    if (stnc_core_qualify_root_peer() != 0) {
                        stnc_log_error("Chain P2P root peer remains unavailable.");
                    }
                }
            } else {
                root_peer_retry_elapsed = 0;
            }

            if (stnc_network_is_connected(&p2p_connection)) {
                p2p_refresh_elapsed += STNC_RUNTIME_WAIT_MS;
                if (p2p_refresh_elapsed >= STNC_P2P_REFRESH_INTERVAL_MS) {
                    p2p_refresh_elapsed = 0;
                    if (stnc_core_refresh_selected_peer() != 0) {
                        stnc_log_info("Selected Chain P2P peer refresh failed; reselection scheduled.");
                    }
                }
            } else {
                p2p_refresh_elapsed += STNC_RUNTIME_WAIT_MS;
                if (p2p_refresh_elapsed >= STNC_RECONNECT_INTERVAL_MS) {
                    p2p_refresh_elapsed = 0;
                    stnc_log_info("Reselecting Chain P2P peer.");
                    if (stnc_core_select_peer() != 0) {
                        stnc_log_error("Chain P2P peer reselection failed.");
                    }
                }
            }

            if (refresh_elapsed >= STNC_CHAIN_REFRESH_INTERVAL_MS) {
                refresh_elapsed = 0;
                if (stnc_core_refresh_chain_state() != 0) {
                    reconnect_elapsed = 0;
                    stnc_log_info("Reconnect scheduled.");
                }
            }
        } else {
            refresh_elapsed = 0;
            reconnect_elapsed += STNC_RUNTIME_WAIT_MS;

            if (reconnect_elapsed >= STNC_RECONNECT_INTERVAL_MS) {
                reconnect_elapsed = 0;
                stnc_log_info("Reconnecting to configured Chain peer.");

                if (stnc_core_connect_configured_peer(0) == 0) {
                    stnc_log_info("Chain connection restored.");
                } else {
                    stnc_log_error("Chain reconnect failed.");
                }
            }
        }
    }

    return 0;
}

void stnc_core_request_stop(void)
{
    if (core_state != STNC_CORE_STATE_RUNNING) {
        return;
    }

    core_state = STNC_CORE_STATE_STOPPING;
    stnc_log_info("STNC Core stop requested.");
}

void stnc_core_shutdown(void)
{
    if (core_state == STNC_CORE_STATE_UNINITIALIZED ||
        core_state == STNC_CORE_STATE_STOPPED) {
        return;
    }

    stnc_log_info("STNC Core shutting down.");

    if (stnc_network_is_connected(&p2p_connection)) {
        stnc_network_disconnect(&p2p_connection);
        stnc_log_info("Chain P2P connection closed.");
    }

    if (stnc_network_is_connected(&chain_connection)) {
        stnc_network_disconnect(&chain_connection);
        stnc_log_info("Chain connection closed.");
    }

    root_peer_capabilities = 0;
    stnc_peers_clear(&peer_candidates);
    stnc_peer_select_clear(&qualified_peers);
    stnc_core_clear_chain_state();
    stnc_network_shutdown();
    stnc_config_shutdown();
    stnc_log_shutdown();
    stnc_platform_shutdown();
    core_state = STNC_CORE_STATE_STOPPED;
}

stnc_core_state stnc_core_get_state(void)
{
    return core_state;
}

const stnc_core_chain_state *stnc_core_get_chain_state(void)
{
    return &chain_state;
}
