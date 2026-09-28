#include <string.h>

#include "stnc_config.h"
#include "stnc_network.h"
#include "stnc_stnc.h"
#include "stnc_transaction_status.h"

#define STNC_TRANSACTION_STATUS_METHOD 13u
#define STNC_TRANSACTION_STATUS_NOT_FOUND 6u
#define STNC_TRANSACTION_STATUS_REQUEST_ID UINT64_C(19)

static uint32_t read_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0u;
    unsigned int i;
    for (i = 0u; i < 8u; ++i) value = (value << 8) | (uint64_t)bytes[i];
    return value;
}

stnc_transaction_acceptance stnc_transaction_status_query(
    const uint8_t transaction_id[STNC_TRANSACTION_STATUS_ID_SIZE],
    stnc_transaction_status *status)
{
    const stnc_config *config;
    stnc_network_connection connection;
    stnc_stnc_message request_message;
    stnc_stnc_message response;
    uint8_t request[STNC_STNC_HEADER_SIZE + STNC_TRANSACTION_STATUS_ID_SIZE];
    uint8_t header[STNC_STNC_HEADER_SIZE];
    uint8_t payload[STNC_TRANSACTION_STATUS_PAYLOAD_SIZE];
    size_t written;
    stnc_transaction_acceptance result = STNC_TRANSACTION_ACCEPTANCE_ERROR;

    if (transaction_id == NULL || status == NULL) return STNC_TRANSACTION_ACCEPTANCE_ERROR;
    memset(status, 0, sizeof(*status));
    config = stnc_config_get();
    if (config == NULL) return STNC_TRANSACTION_ACCEPTANCE_ERROR;

    memset(&connection, 0, sizeof(connection));
    memset(&request_message, 0, sizeof(request_message));
    request_message.kind = STNC_STNC_REQUEST;
    request_message.method = STNC_TRANSACTION_STATUS_METHOD;
    request_message.code = STNC_STNC_OK;
    request_message.request_id = STNC_TRANSACTION_STATUS_REQUEST_ID;
    request_message.payload = transaction_id;
    request_message.length = STNC_TRANSACTION_STATUS_ID_SIZE;

    if (stnc_stnc_encode(&request_message, request, sizeof(request), &written) != 0 ||
        stnc_network_connect(&connection, config->peer, config->port) != 0) {
        return STNC_TRANSACTION_ACCEPTANCE_ERROR;
    }
    if (stnc_network_send(&connection, request, written) != 0 ||
        stnc_network_receive(&connection, header, sizeof(header)) != 0 ||
        stnc_stnc_decode_header(header, sizeof(header), &response) != 0 ||
        response.method != STNC_TRANSACTION_STATUS_METHOD ||
        response.request_id != STNC_TRANSACTION_STATUS_REQUEST_ID) goto done;

    if (response.code == STNC_TRANSACTION_STATUS_NOT_FOUND && response.length == 0u) {
        result = STNC_TRANSACTION_ACCEPTANCE_PENDING;
        goto done;
    }
    if (response.code != STNC_STNC_OK || response.length != sizeof(payload) ||
        stnc_network_receive(&connection, payload, sizeof(payload)) != 0) goto done;

    status->height = read_u64(payload);
    memcpy(status->block_id, payload + 8u, 32u);
    status->transaction_position = read_u32(payload + 40u);
    result = STNC_TRANSACTION_ACCEPTANCE_ACCEPTED;

done:
    stnc_network_disconnect(&connection);
    return result;
}
