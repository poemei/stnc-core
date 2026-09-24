#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_network.h"
#include "stnc_platform.h"
#include "stnc_stnc.h"
#include "stnc_stnp.h"

#define STNC_INFO_REQUEST_ID UINT64_C(1)
#define STNC_CHAIN_REFRESH_INTERVAL_MS 10000u
#define STNC_RUNTIME_WAIT_MS 100u
#define STNC_RECONNECT_INTERVAL_MS 5000u

static stnc_core_state core_state = STNC_CORE_STATE_UNINITIALIZED;
static stnc_network_connection chain_connection;
static stnc_core_chain_state chain_state;
static uint32_t root_peer_capabilities;

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


static int stnc_core_qualify_root_peer(void)
{
    const stnc_config *config;
    stnc_network_connection connection;
    stnc_stnp_hello hello;
    uint8_t request[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    uint8_t response[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    size_t written;
    char message[512];

    config = stnc_config_get();

    if (config == NULL || !chain_state.available) {
        return 1;
    }

    memset(&connection, 0, sizeof(connection));
    memset(&hello, 0, sizeof(hello));

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
        stnc_log_error("Chain P2P root peer connection failed.");
        return 1;
    }

    if (stnc_stnp_encode_hello(
            chain_state.network_id,
            chain_state.genesis_id,
            1u,
            request,
            sizeof(request),
            &written
        ) != 0 ||
        stnc_network_send(&connection, request, written) != 0 ||
        stnc_network_receive(&connection, response, sizeof(response)) != 0) {
        stnc_log_error("Chain P2P root peer HELLO exchange failed.");
        stnc_network_disconnect(&connection);
        return 1;
    }

    stnc_network_disconnect(&connection);

    if (stnc_stnp_decode_hello(response, sizeof(response), &hello) != 0) {
        stnc_log_error("Chain P2P root peer returned an invalid STNP HELLO.");
        return 1;
    }

    if (memcmp(hello.network_id, chain_state.network_id, 32) != 0 ||
        memcmp(hello.genesis_id, chain_state.genesis_id, 32) != 0) {
        stnc_log_error("Chain P2P root peer belongs to a different network or genesis.");
        return 1;
    }

    if (hello.capabilities != 1u && hello.capabilities != 3u) {
        stnc_log_error("Chain P2P root peer capabilities are incompatible.");
        return 1;
    }

    root_peer_capabilities = hello.capabilities;

    if (snprintf(message, sizeof(message),
            "Chain P2P root peer qualified: STNP v2 capabilities=%" PRIu32,
            root_peer_capabilities) < 0) {
        root_peer_capabilities = 0;
        return 1;
    }

    stnc_log_info(message);
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

    return 0;
}

int stnc_core_run(void)
{
    unsigned int refresh_elapsed;
    unsigned int reconnect_elapsed;

    if (core_state != STNC_CORE_STATE_INITIALIZED) {
        return 1;
    }

    core_state = STNC_CORE_STATE_RUNNING;
    refresh_elapsed = 0;
    reconnect_elapsed = 0;
    stnc_log_info("STNC Core running.");

    while (core_state == STNC_CORE_STATE_RUNNING) {
        stnc_platform_wait(STNC_RUNTIME_WAIT_MS);

        if (stnc_network_is_connected(&chain_connection)) {
            refresh_elapsed += STNC_RUNTIME_WAIT_MS;
            reconnect_elapsed = 0;

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

    if (stnc_network_is_connected(&chain_connection)) {
        stnc_network_disconnect(&chain_connection);
        stnc_log_info("Chain connection closed.");
    }

    root_peer_capabilities = 0;
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
