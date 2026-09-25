#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "stnc_stnc.h"

void stnc_stnc_write_u16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8) & 0xffu);
    buffer[1] = (uint8_t)(value & 0xffu);
}

void stnc_stnc_write_u32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)((value >> 24) & 0xffu);
    buffer[1] = (uint8_t)((value >> 16) & 0xffu);
    buffer[2] = (uint8_t)((value >> 8) & 0xffu);
    buffer[3] = (uint8_t)(value & 0xffu);
}

static void stnc_write_u64(uint8_t *buffer, uint64_t value)
{
    unsigned int index;
    for (index = 0; index < 8; index++) {
        buffer[index] = (uint8_t)((value >> ((7u - index) * 8u)) & UINT64_C(0xff));
    }
}

static uint16_t stnc_read_u16(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
}

static uint32_t stnc_read_u32(const uint8_t *buffer)
{
    return ((uint32_t)buffer[0] << 24) |
           ((uint32_t)buffer[1] << 16) |
           ((uint32_t)buffer[2] << 8) |
           (uint32_t)buffer[3];
}

static uint64_t stnc_read_u64(const uint8_t *buffer)
{
    uint64_t value = 0;
    unsigned int index;
    for (index = 0; index < 8; index++) {
        value <<= 8;
        value |= (uint64_t)buffer[index];
    }
    return value;
}

static int stnc_address_type_valid(uint16_t type)
{
    return type == STNC_STNC_ADDRESS_IDENTITY ||
           type == STNC_STNC_ADDRESS_CONTRACT ||
           type == STNC_STNC_ADDRESS_WALLET;
}

int stnc_stnc_encode(
    const stnc_stnc_message *message,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    if (written != NULL) {
        *written = 0;
    }

    if (message == NULL ||
        buffer == NULL ||
        written == NULL ||
        message->kind != STNC_STNC_REQUEST ||
        message->code != STNC_STNC_OK ||
        message->length > UINT32_MAX ||
        (message->length != 0 && message->payload == NULL) ||
        capacity < STNC_STNC_HEADER_SIZE ||
        message->length > capacity - STNC_STNC_HEADER_SIZE) {
        return 1;
    }

    memcpy(buffer, "STNC", 4);
    stnc_stnc_write_u16(buffer + 4, STNC_STNC_VERSION);
    stnc_stnc_write_u16(buffer + 6, message->kind);
    stnc_stnc_write_u16(buffer + 8, message->method);
    stnc_stnc_write_u16(buffer + 10, message->code);
    stnc_write_u64(buffer + 12, message->request_id);
    stnc_stnc_write_u32(buffer + 20, (uint32_t)message->length);
    if (message->length != 0) {
        memcpy(buffer + STNC_STNC_HEADER_SIZE, message->payload, message->length);
    }
    *written = STNC_STNC_HEADER_SIZE + message->length;
    return 0;
}

int stnc_stnc_encode_empty_request(
    uint16_t method,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;
    if(written!=NULL)*written=0;
    if(method!=STNC_STNC_METHOD_MINING_CONTEXT&&method!=STNC_STNC_METHOD_MINING_TEMPLATE)return 1;
    memset(&message,0,sizeof(message));message.kind=STNC_STNC_REQUEST;message.method=method;
    message.code=STNC_STNC_OK;message.request_id=request_id;
    return stnc_stnc_encode(&message,buffer,capacity,written);
}

int stnc_stnc_decode_mining_template(
    const uint8_t *payload,size_t length,stnc_mining_template *work)
{
    stnc_mining_template decoded;uint32_t block_length;
    if(payload==NULL||work==NULL||length<STNC_STNC_MINING_TEMPLATE_PREFIX_SIZE)return 1;
    block_length=stnc_read_u32(payload+64u);
    if(block_length<STNC_STNC_BLOCK_HEADER_SIZE||block_length>STNC_STNC_BLOCK_MAX_SIZE||
       (size_t)block_length!=length-STNC_STNC_MINING_TEMPLATE_PREFIX_SIZE)return 1;
    memset(&decoded,0,sizeof(decoded));memcpy(decoded.parent_id,payload,32u);
    memcpy(decoded.work_id,payload+32u,32u);decoded.block=payload+STNC_STNC_MINING_TEMPLATE_PREFIX_SIZE;
    decoded.block_length=(size_t)block_length;*work=decoded;return 0;
}

int stnc_stnc_decode_mining_context(
    const uint8_t *payload,size_t length,stnc_mining_context *context)
{
    stnc_mining_context decoded;
    if(payload==NULL||context==NULL||length!=STNC_STNC_MINING_CONTEXT_SIZE)return 1;
    memset(&decoded,0,sizeof(decoded));memcpy(decoded.tip_id,payload,32u);
    memcpy(decoded.target,payload+32u,32u);decoded.height=stnc_read_u64(payload+64u);
    decoded.template_available=stnc_read_u32(payload+72u);
    if(decoded.template_available>1u)return 1;
    *context=decoded;return 0;
}

int stnc_stnc_encode_block_height(
    uint64_t height,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written)
{
    uint8_t payload[8];
    stnc_stnc_message message;
    stnc_write_u64(payload,height);
    memset(&message,0,sizeof(message));
    message.kind=STNC_STNC_REQUEST;message.method=STNC_STNC_METHOD_BLOCK_HEIGHT;
    message.code=STNC_STNC_OK;message.request_id=request_id;
    message.payload=payload;message.length=sizeof(payload);
    return stnc_stnc_encode(&message,buffer,capacity,written);
}

int stnc_stnc_encode_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    uint8_t payload[STNC_STNC_DERIVE_PAYLOAD_MAX];
    stnc_stnc_message message;

    if (written != NULL) {
        *written = 0;
    }

    if (!stnc_address_type_valid(type) ||
        source == NULL ||
        source_length == 0 ||
        source_length > STNC_STNC_DERIVE_SOURCE_MAX) {
        return 1;
    }

    stnc_stnc_write_u16(payload, type);
    stnc_stnc_write_u32(payload + 2, (uint32_t)source_length);
    memcpy(payload + STNC_STNC_DERIVE_PREFIX_SIZE, source, source_length);

    memset(&message, 0, sizeof(message));
    message.kind = STNC_STNC_REQUEST;
    message.method = STNC_STNC_METHOD_DERIVE_ADDRESS;
    message.code = STNC_STNC_OK;
    message.request_id = request_id;
    message.payload = payload;
    message.length = STNC_STNC_DERIVE_PREFIX_SIZE + source_length;

    return stnc_stnc_encode(&message, buffer, capacity, written);
}

int stnc_stnc_encode_address_query(
    uint16_t method, const char *address, uint64_t request_id,
    uint8_t *buffer, size_t capacity, size_t *written)
{
    stnc_stnc_message message;
    const char *prefix;
    if (written != NULL) *written = 0;
    if (address == NULL ||
        (method != STNC_STNC_METHOD_BALANCE && method != STNC_STNC_METHOD_CONTRACT_STATE) ||
        strlen(address) != STNC_STNC_ADDRESS_TYPED_SIZE) return 1;
    prefix = method == STNC_STNC_METHOD_BALANCE ? "stnw0_" : "stnc0_";
    if (memcmp(address, prefix, 6u) != 0) return 1;
    memset(&message, 0, sizeof(message));
    message.kind = STNC_STNC_REQUEST; message.method = method; message.code = STNC_STNC_OK;
    message.request_id = request_id; message.payload = (const uint8_t *)address;
    message.length = STNC_STNC_ADDRESS_TYPED_SIZE;
    return stnc_stnc_encode(&message, buffer, capacity, written);
}

int stnc_stnc_decode_header(
    const uint8_t *buffer,
    size_t length,
    stnc_stnc_message *message
)
{
    if (buffer == NULL || message == NULL || length != STNC_STNC_HEADER_SIZE) {
        return 1;
    }
    if (memcmp(buffer, "STNC", 4) != 0 ||
        stnc_read_u16(buffer + 4) != STNC_STNC_VERSION) {
        return 1;
    }

    message->kind = stnc_read_u16(buffer + 6);
    message->method = stnc_read_u16(buffer + 8);
    message->code = stnc_read_u16(buffer + 10);
    message->request_id = stnc_read_u64(buffer + 12);
    message->length = (size_t)stnc_read_u32(buffer + 20);
    message->payload = NULL;

    return message->kind == STNC_STNC_RESPONSE ? 0 : 1;
}

int stnc_stnc_decode_info(
    const uint8_t *payload,
    size_t length,
    stnc_chain_info *info
)
{
    if (payload == NULL || info == NULL || length != STNC_STNC_INFO_SIZE) {
        return 1;
    }

    memset(info, 0, sizeof(*info));
    memcpy(info->network_id, payload, 32);
    memcpy(info->genesis_id, payload + 32, 32);
    info->height = stnc_read_u64(payload + 64);
    memcpy(info->tip_id, payload + 72, 32);
    memcpy(info->cumulative_work, payload + 104, 40);
    memcpy(info->current_target, payload + 144, 32);
    info->protocol_revision = stnc_read_u32(payload + 176);
    info->block_count = stnc_read_u32(payload + 180);
    return 0;
}

int stnc_stnc_decode_address(
    uint16_t type,
    const uint8_t *payload,
    size_t length,
    char *address,
    size_t capacity
)
{
    size_t expected;
    const char *prefix;
    size_t prefix_length;

    if (!stnc_address_type_valid(type) || payload == NULL || address == NULL) {
        return 1;
    }

    if (type == STNC_STNC_ADDRESS_IDENTITY) {
        expected = STNC_STNC_ADDRESS_IDENTITY_SIZE;
        prefix = "stn0_";
        prefix_length = 5u;
    } else if (type == STNC_STNC_ADDRESS_CONTRACT) {
        expected = STNC_STNC_ADDRESS_TYPED_SIZE;
        prefix = "stnc0_";
        prefix_length = 6u;
    } else {
        expected = STNC_STNC_ADDRESS_TYPED_SIZE;
        prefix = "stnw0_";
        prefix_length = 6u;
    }

    if (length != expected ||
        capacity <= length ||
        memcmp(payload, prefix, prefix_length) != 0) {
        return 1;
    }

    memcpy(address, payload, length);
    address[length] = '\0';
    return 0;
}

int stnc_stnc_decode_balance(const uint8_t *payload, size_t length, uint64_t *units)
{
    if (payload == NULL || units == NULL || length != STNC_STNC_BALANCE_SIZE) return 1;
    *units = stnc_read_u64(payload);
    return 0;
}

int stnc_stnc_decode_contract_state(const uint8_t *payload, size_t length, stnc_contract_state *state)
{
    stnc_contract_state decoded;
    if (payload == NULL || state == NULL || length != STNC_STNC_CONTRACT_STATE_SIZE) return 1;
    memset(&decoded, 0, sizeof(decoded));
    decoded.state = stnc_read_u16(payload);
    decoded.type = stnc_read_u16(payload + 2);
    decoded.sequence = stnc_read_u64(payload + 4);
    decoded.created_at = stnc_read_u64(payload + 12);
    decoded.participant_count = stnc_read_u16(payload + 20);
    decoded.terms_length = stnc_read_u32(payload + 22);
    if (decoded.state < STNC_STNC_CONTRACT_STATE_DRAFT || decoded.state > STNC_STNC_CONTRACT_STATE_CLOSED ||
        decoded.type < STNC_STNC_CONTRACT_TYPE_GENERIC || decoded.type > STNC_STNC_CONTRACT_TYPE_SERVICE_AGREEMENT ||
        decoded.participant_count > STNC_STNC_CONTRACT_MAX_PARTICIPANTS ||
        decoded.terms_length > STNC_STNC_CONTRACT_MAX_TERMS) return 1;
    *state = decoded;
    return 0;
}

int stnc_stnc_encode_submit_transaction(
    const uint8_t *transaction,size_t transaction_length,uint64_t request_id,
    uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;
    if(written!=NULL)*written=0;
    if(transaction==NULL||transaction_length<12u||transaction_length>STNC_STNC_TRANSACTION_MAX)return 1;
    memset(&message,0,sizeof(message));
    message.kind=STNC_STNC_REQUEST;
    message.method=STNC_STNC_METHOD_SUBMIT_TRANSACTION;
    message.code=STNC_STNC_OK;
    message.request_id=request_id;
    message.payload=transaction;
    message.length=transaction_length;
    return stnc_stnc_encode(&message,buffer,capacity,written);
}

int stnc_stnc_encode_submit_block_evidence(
    const uint8_t *block,size_t block_length,uint64_t request_id,
    uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;
    if(written!=NULL)*written=0;
    if(block==NULL||block_length<STNC_STNC_BLOCK_HEADER_SIZE||
       block_length>STNC_STNC_BLOCK_MAX_SIZE)return 1;
    memset(&message,0,sizeof(message));
    message.kind=STNC_STNC_REQUEST;
    message.method=STNC_STNC_METHOD_SUBMIT_BLOCK_EVIDENCE;
    message.code=STNC_STNC_OK;
    message.request_id=request_id;
    message.payload=block;
    message.length=block_length;
    return stnc_stnc_encode(&message,buffer,capacity,written);
}

int stnc_stnc_encode_submit_history_evidence(
    const uint8_t *const *blocks,const size_t *block_lengths,size_t block_count,
    uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;
    uint8_t *payload;
    size_t payload_length=4u,at=4u,i;
    int rc;
    if(written!=NULL)*written=0;
    if(blocks==NULL||block_lengths==NULL||block_count==0u||block_count>UINT32_MAX)return 1;
    for(i=0u;i<block_count;i++){
        if(blocks[i]==NULL||block_lengths[i]<STNC_STNC_BLOCK_HEADER_SIZE||
           block_lengths[i]>STNC_STNC_BLOCK_MAX_SIZE||
           payload_length>UINT32_MAX-4u||block_lengths[i]>UINT32_MAX-payload_length-4u)return 1;
        payload_length+=4u+block_lengths[i];
    }
    if(capacity<STNC_STNC_HEADER_SIZE||payload_length>capacity-STNC_STNC_HEADER_SIZE)return 1;
    payload=(uint8_t *)malloc(payload_length);
    if(payload==NULL)return 1;
    stnc_stnc_write_u32(payload,(uint32_t)block_count);
    for(i=0u;i<block_count;i++){
        stnc_stnc_write_u32(payload+at,(uint32_t)block_lengths[i]);at+=4u;
        memcpy(payload+at,blocks[i],block_lengths[i]);at+=block_lengths[i];
    }
    memset(&message,0,sizeof(message));message.kind=STNC_STNC_REQUEST;
    message.method=STNC_STNC_METHOD_SUBMIT_HISTORY_EVIDENCE;message.code=STNC_STNC_OK;
    message.request_id=request_id;message.payload=payload;message.length=payload_length;
    rc=stnc_stnc_encode(&message,buffer,capacity,written);
    free(payload);return rc;
}

int stnc_stnc_encode_submit_suffix_evidence(
    uint32_t prefix_count,const uint8_t *const *blocks,const size_t *block_lengths,
    size_t block_count,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;
    uint8_t *payload;
    size_t payload_length=8u,at=8u,i;
    int rc;
    if(written!=NULL)*written=0;
    if(prefix_count==0u||blocks==NULL||block_lengths==NULL||
       block_count==0u||block_count>UINT32_MAX)return 1;
    for(i=0u;i<block_count;i++){
        if(blocks[i]==NULL||block_lengths[i]<STNC_STNC_BLOCK_HEADER_SIZE||
           block_lengths[i]>STNC_STNC_BLOCK_MAX_SIZE||
           payload_length>UINT32_MAX-4u||block_lengths[i]>UINT32_MAX-payload_length-4u)return 1;
        payload_length+=4u+block_lengths[i];
    }
    if(capacity<STNC_STNC_HEADER_SIZE||payload_length>capacity-STNC_STNC_HEADER_SIZE)return 1;
    payload=(uint8_t *)malloc(payload_length);if(payload==NULL)return 1;
    stnc_stnc_write_u32(payload,prefix_count);
    stnc_stnc_write_u32(payload+4u,(uint32_t)block_count);
    for(i=0u;i<block_count;i++){
        stnc_stnc_write_u32(payload+at,(uint32_t)block_lengths[i]);at+=4u;
        memcpy(payload+at,blocks[i],block_lengths[i]);at+=block_lengths[i];
    }
    memset(&message,0,sizeof(message));message.kind=STNC_STNC_REQUEST;
    message.method=STNC_STNC_METHOD_SUBMIT_SUFFIX_EVIDENCE;message.code=STNC_STNC_OK;
    message.request_id=request_id;message.payload=payload;message.length=payload_length;
    rc=stnc_stnc_encode(&message,buffer,capacity,written);free(payload);return rc;
}

int stnc_stnc_encode_suffix_stage_begin(
    uint32_t prefix_count,uint32_t suffix_count,uint64_t request_id,
    uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;uint8_t payload[8u];
    if(written!=NULL)*written=0;
    if(prefix_count==0u||suffix_count==0u)return 1;
    stnc_stnc_write_u32(payload,prefix_count);stnc_stnc_write_u32(payload+4u,suffix_count);
    memset(&message,0,sizeof(message));message.kind=STNC_STNC_REQUEST;
    message.method=STNC_STNC_METHOD_SUFFIX_STAGE_BEGIN;message.request_id=request_id;
    message.payload=payload;message.length=sizeof(payload);
    return stnc_stnc_encode(&message,buffer,capacity,written);
}

int stnc_stnc_encode_suffix_stage_append(
    uint32_t start,const uint8_t *const *blocks,const size_t *block_lengths,
    size_t block_count,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;uint8_t *payload;size_t n=8u,at=8u,i;int rc;
    if(written!=NULL)*written=0;
    if(blocks==NULL||block_lengths==NULL||block_count==0u||block_count>UINT32_MAX)return 1;
    for(i=0u;i<block_count;i++){
        if(blocks[i]==NULL||block_lengths[i]<STNC_STNC_BLOCK_HEADER_SIZE||
           block_lengths[i]>STNC_STNC_BLOCK_MAX_SIZE||n>UINT32_MAX-4u||
           block_lengths[i]>UINT32_MAX-n-4u)return 1;
        n+=4u+block_lengths[i];
    }
    if(capacity<STNC_STNC_HEADER_SIZE||n>capacity-STNC_STNC_HEADER_SIZE)return 1;
    payload=(uint8_t *)malloc(n);if(payload==NULL)return 1;
    stnc_stnc_write_u32(payload,start);stnc_stnc_write_u32(payload+4u,(uint32_t)block_count);
    for(i=0u;i<block_count;i++){
        stnc_stnc_write_u32(payload+at,(uint32_t)block_lengths[i]);at+=4u;
        memcpy(payload+at,blocks[i],block_lengths[i]);at+=block_lengths[i];
    }
    memset(&message,0,sizeof(message));message.kind=STNC_STNC_REQUEST;
    message.method=STNC_STNC_METHOD_SUFFIX_STAGE_APPEND;message.request_id=request_id;
    message.payload=payload;message.length=n;
    rc=stnc_stnc_encode(&message,buffer,capacity,written);free(payload);return rc;
}

int stnc_stnc_encode_suffix_stage_control(
    uint16_t method,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written)
{
    stnc_stnc_message message;
    if(written!=NULL)*written=0;
    if(method!=STNC_STNC_METHOD_SUFFIX_STAGE_COMMIT&&
       method!=STNC_STNC_METHOD_SUFFIX_STAGE_ABORT)return 1;
    memset(&message,0,sizeof(message));message.kind=STNC_STNC_REQUEST;
    message.method=method;message.request_id=request_id;
    return stnc_stnc_encode(&message,buffer,capacity,written);
}

int stnc_stnc_decode_block_accepted(
    const uint8_t *payload,size_t length,uint8_t tip_id[32],
    uint64_t *height,uint8_t cumulative_work[40])
{
    if(payload==NULL||tip_id==NULL||height==NULL||cumulative_work==NULL||
       length!=STNC_STNC_BLOCK_ACCEPTED_SIZE)return 1;
    memcpy(tip_id,payload,32u);
    *height=stnc_read_u64(payload+32u);
    memcpy(cumulative_work,payload+40u,40u);
    return 0;
}

int stnc_stnc_decode_pending(const uint8_t *payload,size_t length,stnc_pending_state *state)
{
    stnc_pending_state decoded;
    if(payload==NULL||state==NULL||length!=STNC_STNC_PENDING_SIZE)return 1;
    decoded.count=stnc_read_u32(payload);
    decoded.max_entries=stnc_read_u32(payload+4);
    decoded.bytes=stnc_read_u32(payload+8);
    decoded.max_bytes=stnc_read_u32(payload+12);
    if(decoded.max_entries==0u||decoded.max_bytes==0u||
       decoded.count>decoded.max_entries||decoded.bytes>decoded.max_bytes)return 1;
    *state=decoded;
    return 0;
}

int stnc_stnc_decode_submission(const uint8_t *payload,size_t length,stnc_submission_result *result)
{
    stnc_submission_result decoded;
    uint16_t version;
    if(payload==NULL||result==NULL||length!=STNC_STNC_SUBMISSION_RESPONSE_SIZE)return 1;
    version=stnc_read_u16(payload);
    memset(&decoded,0,sizeof(decoded));
    decoded.result=stnc_read_u16(payload+2);
    if(version!=1u||decoded.result>STNC_STNC_SUBMISSION_INTERNAL)return 1;
    if(decoded.result==STNC_STNC_SUBMISSION_ADMITTED||decoded.result==STNC_STNC_SUBMISSION_DUPLICATE){
        memcpy(decoded.transaction_id,payload+4,32u);
        decoded.has_transaction_id=1;
    }else{
        size_t i;
        for(i=4u;i<STNC_STNC_SUBMISSION_RESPONSE_SIZE;i++)if(payload[i]!=0u)return 1;
    }
    *result=decoded;
    return 0;
}
