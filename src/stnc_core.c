#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stnc_background_mining.h"
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
#define STNC_DERIVE_ADDRESS_REQUEST_ID UINT64_C(2)
#define STNC_BALANCE_REQUEST_ID UINT64_C(3)
#define STNC_CONTRACT_STATE_REQUEST_ID UINT64_C(4)
#define STNC_SUBMIT_TRANSACTION_REQUEST_ID UINT64_C(5)
#define STNC_PENDING_REQUEST_ID UINT64_C(6)
#define STNC_BLOCK_EVIDENCE_REQUEST_ID UINT64_C(7)
#define STNC_HISTORY_EVIDENCE_REQUEST_ID UINT64_C(8)
#define STNC_SUFFIX_EVIDENCE_REQUEST_ID UINT64_C(9)
#define STNC_BLOCK_HEIGHT_REQUEST_ID UINT64_C(10)
#define STNC_SUFFIX_STAGE_BEGIN_REQUEST_ID UINT64_C(11)
#define STNC_SUFFIX_STAGE_APPEND_REQUEST_ID UINT64_C(12)
#define STNC_SUFFIX_STAGE_COMMIT_REQUEST_ID UINT64_C(13)
#define STNC_SUFFIX_STAGE_ABORT_REQUEST_ID UINT64_C(14)
#define STNC_MINING_CONTEXT_REQUEST_ID UINT64_C(15)
#define STNC_MINING_TEMPLATE_REQUEST_ID UINT64_C(16)
#define STNC_CHECK_WORK_BASE_REQUEST_ID UINT64_C(17)
#define STNC_SUBMIT_WORK_REQUEST_ID UINT64_C(18)
#define STNC_CHAIN_REFRESH_INTERVAL_MS 10000u
#define STNC_RUNTIME_WAIT_MS 100u
#define STNC_RECONNECT_INTERVAL_MS 5000u
#define STNC_ROOT_PEER_RETRY_INTERVAL_MS 30000u
#define STNC_P2P_REFRESH_INTERVAL_MS 5000u
#define STNC_DIRECTORY_HOST "stn-chain.org"
#define STNC_DIRECTORY_PATH "/peers?format=json"
#define STNC_HISTORY_EVIDENCE_MAX_BLOCKS 4096u
/* Must never exceed Chain STNC v2 STN_RPC_MAX_PAYLOAD:
 * STN_RPC_MINING_SUBMISSION_PREFIX (137) + STN_BLOCK_MAX_SIZE. */
#define STNC_STNC_MAX_PAYLOAD (137u + STNC_STNC_BLOCK_MAX_SIZE)
#define STNC_HISTORY_EVIDENCE_MAX_BYTES STNC_STNC_MAX_PAYLOAD

static stnc_core_state core_state = STNC_CORE_STATE_UNINITIALIZED;
static stnc_network_connection chain_connection;
static stnc_network_connection p2p_connection;
static stnc_core_chain_state chain_state;
static uint32_t root_peer_capabilities;
static stnc_peer_candidates peer_candidates;
static stnc_peer_qualified_set qualified_peers;
static stnc_core_peer_status peer_status;

static int stnc_core_fetch_peer_block(
    uint32_t wanted,const uint8_t *advertised,uint8_t **owned,
    const uint8_t **block_bytes,size_t *block_length)
{
    uint8_t request[STNC_STNP_HEADER_SIZE+STNC_STNP_BLOCK_INDEX_SIZE];
    uint8_t header[STNC_STNP_HEADER_SIZE];
    size_t written,payload_length;
    uint32_t index;
    uint8_t *frame;

    if(owned==NULL||block_bytes==NULL||block_length==NULL)return 1;
    *owned=NULL;*block_bytes=NULL;*block_length=0u;
    if(stnc_stnp_encode_get_block(wanted,request,sizeof(request),&written)!=0||
       written!=sizeof(request)||stnc_network_send(&p2p_connection,request,written)!=0||
       stnc_network_receive(&p2p_connection,header,sizeof(header))!=0||
       stnc_stnp_decode_block_header(header,sizeof(header),&payload_length)!=0)return 1;
    frame=(uint8_t *)malloc(STNC_STNP_HEADER_SIZE+payload_length);if(frame==NULL)return 1;
    memcpy(frame,header,sizeof(header));
    if(stnc_network_receive(&p2p_connection,frame+STNC_STNP_HEADER_SIZE,payload_length)!=0||
       stnc_stnp_decode_block(frame,STNC_STNP_HEADER_SIZE+payload_length,&index,block_bytes,block_length)!=0||
       index!=wanted||*block_length<STNC_STNP_HEADER_WIRE_SIZE||
       (advertised!=NULL&&memcmp(*block_bytes,advertised,STNC_STNP_HEADER_WIRE_SIZE)!=0)){
        free(frame);*block_bytes=NULL;*block_length=0u;return 1;
    }
    *owned=frame;return 0;
}

static int stnc_core_chain_block(uint32_t index,uint8_t **owned,const uint8_t **block,size_t *length)
{
    uint8_t request[STNC_STNC_HEADER_SIZE+8u],header[STNC_STNC_HEADER_SIZE];
    stnc_stnc_message response;size_t written;uint8_t *payload;
    if(owned==NULL||block==NULL||length==NULL)return 1;
    *owned=NULL;*block=NULL;*length=0u;
    if(stnc_stnc_encode_block_height((uint64_t)index,STNC_BLOCK_HEIGHT_REQUEST_ID,
            request,sizeof(request),&written)!=0||
       stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_BLOCK_HEIGHT||
       response.request_id!=STNC_BLOCK_HEIGHT_REQUEST_ID||response.code!=STNC_STNC_OK||
       response.length<STNC_STNC_BLOCK_HEADER_SIZE||response.length>STNC_STNC_BLOCK_MAX_SIZE)return 1;
    payload=(uint8_t *)malloc(response.length);if(payload==NULL)return 1;
    if(stnc_network_receive(&chain_connection,payload,response.length)!=0){free(payload);return 1;}
    *owned=payload;*block=payload;*length=response.length;return 0;
}

static int stnc_core_common_prefix(uint32_t peer_count,uint32_t *prefix)
{
    uint32_t low=0u,high=peer_count<chain_state.block_count?peer_count:chain_state.block_count;
    if(prefix==NULL)return 1;
    while(low<high){
        uint32_t mid=low+(high-low+1u)/2u,index=mid-1u;
        uint8_t *po=NULL,*co=NULL;const uint8_t *pb=NULL,*cb=NULL;size_t pn=0u,cn=0u;int same;
        if(stnc_core_fetch_peer_block(index,NULL,&po,&pb,&pn)!=0||
           stnc_core_chain_block(index,&co,&cb,&cn)!=0){free(po);free(co);return 1;}
        same=pn==cn&&memcmp(pb,cb,pn)==0;free(po);free(co);
        if(same)low=mid;else high=mid-1u;
    }
    *prefix=low;return 0;
}

static int stnc_core_stage_control(uint16_t method,uint64_t request_id,stnc_stnc_message *response)
{
    uint8_t request[STNC_STNC_HEADER_SIZE],header[STNC_STNC_HEADER_SIZE];size_t written;
    if(response==NULL||stnc_stnc_encode_suffix_stage_control(method,request_id,
            request,sizeof(request),&written)!=0||
       stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),response)!=0||
       response->method!=method||response->request_id!=request_id)return 1;
    return 0;
}

static void stnc_core_stage_abort(void)
{
    stnc_stnc_message response;
    (void)stnc_core_stage_control(STNC_STNC_METHOD_SUFFIX_STAGE_ABORT,
        STNC_SUFFIX_STAGE_ABORT_REQUEST_ID,&response);
}

static stnc_core_evidence_result stnc_core_submit_peer_history_staged(
    uint32_t prefix,uint32_t suffix_count)
{
    uint8_t begin[STNC_STNC_HEADER_SIZE+8u],header[STNC_STNC_HEADER_SIZE];
    uint8_t *owned=NULL,*request=NULL,*payload=NULL;const uint8_t *block=NULL;
    const uint8_t *blocks[1];size_t lengths[1],block_length=0u,written,capacity;
    stnc_stnc_message response;uint32_t offset;uint64_t height;uint8_t tip[32],work[40];
    stnc_core_evidence_result rc=STNC_CORE_EVIDENCE_ERROR;

    if(stnc_stnc_encode_suffix_stage_begin(prefix,suffix_count,
            STNC_SUFFIX_STAGE_BEGIN_REQUEST_ID,begin,sizeof(begin),&written)!=0||
       stnc_network_send(&chain_connection,begin,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_SUFFIX_STAGE_BEGIN||
       response.request_id!=STNC_SUFFIX_STAGE_BEGIN_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length!=0u)return rc;

    for(offset=0u;offset<suffix_count;offset++){
        if(stnc_core_fetch_peer_block(prefix+offset,NULL,&owned,&block,&block_length)!=0)goto abort;
        blocks[0]=block;lengths[0]=block_length;
        if(block_length>SIZE_MAX-STNC_STNC_HEADER_SIZE-12u)goto abort;
        capacity=STNC_STNC_HEADER_SIZE+12u+block_length;
        request=(uint8_t *)malloc(capacity);if(request==NULL)goto abort;
        if(stnc_stnc_encode_suffix_stage_append(offset,blocks,lengths,1u,
                STNC_SUFFIX_STAGE_APPEND_REQUEST_ID,request,capacity,&written)!=0||
           stnc_network_send(&chain_connection,request,written)!=0||
           stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
           stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
           response.method!=STNC_STNC_METHOD_SUFFIX_STAGE_APPEND||
           response.request_id!=STNC_SUFFIX_STAGE_APPEND_REQUEST_ID||
           response.code!=STNC_STNC_OK||response.length!=0u)goto abort;
        free(request);request=NULL;free(owned);owned=NULL;block=NULL;
    }

    if(stnc_core_stage_control(STNC_STNC_METHOD_SUFFIX_STAGE_COMMIT,
            STNC_SUFFIX_STAGE_COMMIT_REQUEST_ID,&response)!=0)goto abort;
    if(response.code==STNC_STNC_CURRENT&&response.length==0u)return STNC_CORE_EVIDENCE_CURRENT;
    if(response.code!=STNC_STNC_OK||response.length!=STNC_STNC_BLOCK_ACCEPTED_SIZE)goto abort;
    payload=(uint8_t *)malloc(response.length);if(payload==NULL)goto abort;
    if(stnc_network_receive(&chain_connection,payload,response.length)!=0||
       stnc_stnc_decode_block_accepted(payload,response.length,tip,&height,work)!=0)goto abort;
    memcpy(chain_state.tip_id,tip,32u);chain_state.height=height;
    memcpy(chain_state.cumulative_work,work,40u);
    chain_state.block_count=height<UINT32_MAX?(uint32_t)(height+1u):UINT32_MAX;
    chain_state.available=1;rc=STNC_CORE_EVIDENCE_ADOPTED;
    free(payload);return rc;
abort:
    free(payload);free(request);free(owned);stnc_core_stage_abort();return rc;
}

static int stnc_core_submit_peer_history(uint32_t peer_block_count)
{
    uint8_t **owned=NULL;const uint8_t **blocks=NULL;size_t *lengths=NULL;
    size_t i,total=8u;uint32_t prefix=0u,suffix_count;int rc=1;
    if(peer_block_count==0u||stnc_core_common_prefix(peer_block_count,&prefix)!=0)return 1;
    if(prefix==0u||prefix>=peer_block_count){stnc_log_error("Selected Chain P2P history has no actionable divergent suffix.");return 1;}
    suffix_count=peer_block_count-prefix;
    if(suffix_count>STNC_HISTORY_EVIDENCE_MAX_BLOCKS){
        stnc_log_error("Selected Chain P2P divergent suffix exceeds the bounded recovery block limit.");return 1;
    }
    owned=(uint8_t **)calloc(suffix_count,sizeof(*owned));
    blocks=(const uint8_t **)calloc(suffix_count,sizeof(*blocks));
    lengths=(size_t *)calloc(suffix_count,sizeof(*lengths));
    if(owned==NULL||blocks==NULL||lengths==NULL)goto done;
    for(i=0u;i<(size_t)suffix_count;i++){
        uint32_t index=prefix+(uint32_t)i;
        if(stnc_core_fetch_peer_block(index,NULL,&owned[i],&blocks[i],&lengths[i])!=0)goto done;
        if(total>STNC_HISTORY_EVIDENCE_MAX_BYTES-4u||
           lengths[i]>STNC_HISTORY_EVIDENCE_MAX_BYTES-total-4u){
            stnc_core_evidence_result staged;
            for(;i<(size_t)suffix_count;i++)free(owned[i]);
            free(lengths);free(blocks);free(owned);
            stnc_log_info("Selected Chain P2P divergent suffix exceeds one STNC evidence frame; starting staged recovery.");
            staged=stnc_core_submit_peer_history_staged(prefix,suffix_count);
            if(staged==STNC_CORE_EVIDENCE_CURRENT){
                stnc_log_info("Chain retained current accepted state after staged peer suffix evidence.");return 0;
            }
            if(staged==STNC_CORE_EVIDENCE_ADOPTED){
                stnc_log_info("Chain accepted a preferred staged peer suffix.");return 0;
            }
            return 1;
        }
        total+=4u+lengths[i];
    }
    {
        stnc_core_evidence_result submit=stnc_core_submit_suffix_evidence(prefix,blocks,lengths,suffix_count);
        if(submit==STNC_CORE_EVIDENCE_CURRENT){
            stnc_log_info("Chain retained current accepted state after valid peer suffix evidence.");
            rc=0;goto done;
        }
        if(submit!=STNC_CORE_EVIDENCE_ADOPTED)goto done;
    }
    stnc_log_info("Chain accepted a preferred bounded peer suffix.");rc=0;
done:
    if(owned!=NULL)for(i=0u;i<(size_t)suffix_count;i++)free(owned[i]);
    free(lengths);free(blocks);free(owned);return rc;
}

static int stnc_core_probe_selected_peer_headers(
    const stnc_stnp_state *peer_state)
{
    uint8_t request[STNC_STNP_HEADER_SIZE+8u],response_header[STNC_STNP_HEADER_SIZE];
    uint8_t response_frame[STNC_STNP_HEADERS_FRAME_MAX];
    size_t written,payload_length;
    uint32_t start,count,offset;
    uint64_t first_index,available;
    char message[512];

    if(peer_state==NULL||!chain_state.available||!stnc_network_is_connected(&p2p_connection)){
        stnc_log_error("Selected Chain P2P synchronization prerequisites are unavailable.");return 1;
    }

    /* Equal height with a different tip is direct divergence evidence. Chain,
     * not Core, evaluates the complete peer history. */
    if(peer_state->block_count==chain_state.block_count&&
       memcmp(peer_state->tip_id,chain_state.tip_id,sizeof(chain_state.tip_id))!=0){
        stnc_log_info("Selected Chain P2P peer reports a competing equal-height history.");
        return stnc_core_submit_peer_history(peer_state->block_count);
    }

    while((uint64_t)peer_state->block_count>(uint64_t)chain_state.block_count){
        first_index=chain_state.block_count;
        available=(uint64_t)peer_state->block_count-first_index;
        count=available>STNC_STNP_HEADERS_MAX?STNC_STNP_HEADERS_MAX:(uint32_t)available;
        if(first_index>UINT32_MAX)return 1;
        start=(uint32_t)first_index;
        if(stnc_stnp_encode_get_headers(start,count,request,sizeof(request),&written)!=0||
           stnc_network_send(&p2p_connection,request,written)!=0||
           stnc_network_receive(&p2p_connection,response_header,sizeof(response_header))!=0||
           stnc_stnp_decode_headers_header(response_header,sizeof(response_header),&payload_length)!=0)return 1;
        memcpy(response_frame,response_header,sizeof(response_header));
        if(stnc_network_receive(&p2p_connection,response_frame+STNC_STNP_HEADER_SIZE,payload_length)!=0||
           stnc_stnp_decode_headers(response_frame,STNC_STNP_HEADER_SIZE+payload_length,&start,&count)!=0||
           (uint64_t)start!=first_index||count==0u||(uint64_t)count>available)return 1;

        for(offset=0u;offset<count;offset++){
            uint32_t wanted=start+offset;
            const uint8_t *advertised=response_frame+STNC_STNP_HEADER_SIZE+8u+
                ((size_t)offset*STNC_STNP_HEADER_WIRE_SIZE);
            uint8_t *owned=NULL;const uint8_t *block=NULL;size_t block_length=0u;

            if(stnc_core_fetch_peer_block(wanted,advertised,&owned,&block,&block_length)!=0){
                stnc_log_error("Selected Chain P2P BLOCK does not match advertised header evidence.");return 1;
            }
            if(stnc_core_submit_block_evidence(block,block_length)!=0){
                free(owned);
                /* A peer can be ahead yet diverged before our current tip.
                 * The failed linear extension is evidence of that possibility,
                 * never authority to choose the peer. Submit its complete
                 * history to Chain for independent fork evaluation. */
                stnc_log_info("Linear peer evidence did not extend accepted Chain state; evaluating complete peer history.");
                return stnc_core_submit_peer_history(peer_state->block_count);
            }
            free(owned);
            if(chain_state.block_count!=(uint32_t)(wanted+1u))return 1;
            if(snprintf(message,sizeof(message),"Chain synchronization accepted block %" PRIu32 " of %" PRIu32 ".",
                    wanted,peer_state->block_count-1u)<0)return 1;
            stnc_log_info(message);
        }
    }

    if(peer_state->block_count<chain_state.block_count){
        stnc_log_info("Selected Chain P2P peer is behind accepted Chain state.");
    }else if(memcmp(peer_state->tip_id,chain_state.tip_id,sizeof(chain_state.tip_id))!=0){
        /* The loop can make counts equal after a stale peer STATE snapshot.
         * A differing advertised tip still requires Chain-side evaluation. */
        stnc_log_info("Selected Chain P2P peer tip differs from accepted Chain state.");
        return stnc_core_submit_peer_history(peer_state->block_count);
    }else{
        stnc_log_info("Selected Chain P2P synchronization is current.");
    }
    return 0;
}
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
    memset(&peer_status,0,sizeof(peer_status));

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
        stnc_network_disconnect(&p2p_connection);memset(&peer_status,0,sizeof(peer_status));
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
        memset(&peer_status,0,sizeof(peer_status));
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
    memset(&peer_status,0,sizeof(peer_status));

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

    if (stnc_background_mining_init() != 0) {
        stnc_log_error("Background mining service initialization failed.");
        stnc_core_shutdown();
        core_state = STNC_CORE_STATE_UNINITIALIZED;
        return 1;
    }

    return 0;
}

int stnc_core_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
    char *address,
    size_t capacity
)
{
    uint8_t request[STNC_STNC_DERIVE_FRAME_MAX];
    uint8_t response_header[STNC_STNC_HEADER_SIZE];
    uint8_t response_payload[STNC_STNC_ADDRESS_MAX_SIZE];
    stnc_stnc_message response;
    size_t written;
    size_t expected_length;

    if (core_state == STNC_CORE_STATE_UNINITIALIZED ||
        core_state == STNC_CORE_STATE_STOPPED ||
        !stnc_network_is_connected(&chain_connection) ||
        address == NULL) {
        return 1;
    }

    if (stnc_stnc_encode_derive_address(
            type,
            source,
            source_length,
            STNC_DERIVE_ADDRESS_REQUEST_ID,
            request,
            sizeof(request),
            &written
        ) != 0 ||
        stnc_network_send(&chain_connection, request, written) != 0 ||
        stnc_network_receive(
            &chain_connection,
            response_header,
            sizeof(response_header)
        ) != 0 ||
        stnc_stnc_decode_header(
            response_header,
            sizeof(response_header),
            &response
        ) != 0 ||
        response.method != STNC_STNC_METHOD_DERIVE_ADDRESS ||
        response.request_id != STNC_DERIVE_ADDRESS_REQUEST_ID ||
        response.code != STNC_STNC_OK) {
        return 1;
    }

    expected_length = type == STNC_STNC_ADDRESS_IDENTITY
        ? STNC_STNC_ADDRESS_IDENTITY_SIZE
        : STNC_STNC_ADDRESS_TYPED_SIZE;

    if (response.length != expected_length ||
        stnc_network_receive(
            &chain_connection,
            response_payload,
            response.length
        ) != 0 ||
        stnc_stnc_decode_address(
            type,
            response_payload,
            response.length,
            address,
            capacity
        ) != 0) {
        return 1;
    }

    return 0;
}


static int stnc_core_address_query(uint16_t method, const char *address, uint64_t request_id,
    uint8_t *payload, size_t payload_capacity, size_t expected_length)
{
    uint8_t request[STNC_STNC_HEADER_SIZE + STNC_STNC_ADDRESS_TYPED_SIZE];
    uint8_t response_header[STNC_STNC_HEADER_SIZE];
    stnc_stnc_message response;
    size_t written;
    if (core_state == STNC_CORE_STATE_UNINITIALIZED || core_state == STNC_CORE_STATE_STOPPED ||
        !stnc_network_is_connected(&chain_connection) || address == NULL || payload == NULL ||
        payload_capacity < expected_length) return 1;
    if (stnc_stnc_encode_address_query(method,address,request_id,request,sizeof(request),&written) != 0 ||
        stnc_network_send(&chain_connection,request,written) != 0 ||
        stnc_network_receive(&chain_connection,response_header,sizeof(response_header)) != 0 ||
        stnc_stnc_decode_header(response_header,sizeof(response_header),&response) != 0 ||
        response.method != method || response.request_id != request_id || response.code != STNC_STNC_OK ||
        response.length != expected_length ||
        stnc_network_receive(&chain_connection,payload,expected_length) != 0) return 1;
    return 0;
}

int stnc_core_balance(const char *wallet, uint64_t *units)
{
    uint8_t payload[STNC_STNC_BALANCE_SIZE];
    if (units == NULL || stnc_core_address_query(STNC_STNC_METHOD_BALANCE,wallet,STNC_BALANCE_REQUEST_ID,
        payload,sizeof(payload),sizeof(payload)) != 0) return 1;
    return stnc_stnc_decode_balance(payload,sizeof(payload),units);
}

int stnc_core_contract_state(const char *contract, stnc_contract_state *state)
{
    uint8_t payload[STNC_STNC_CONTRACT_STATE_SIZE];
    if (state == NULL || stnc_core_address_query(STNC_STNC_METHOD_CONTRACT_STATE,contract,STNC_CONTRACT_STATE_REQUEST_ID,
        payload,sizeof(payload),sizeof(payload)) != 0) return 1;
    return stnc_stnc_decode_contract_state(payload,sizeof(payload),state);
}

int stnc_core_pending(stnc_pending_state *state)
{
    stnc_stnc_message request,response;
    uint8_t request_buffer[STNC_STNC_HEADER_SIZE],response_header[STNC_STNC_HEADER_SIZE],payload[STNC_STNC_PENDING_SIZE];
    size_t written;
    if(state==NULL||core_state==STNC_CORE_STATE_UNINITIALIZED||core_state==STNC_CORE_STATE_STOPPED||
       !stnc_network_is_connected(&chain_connection))return 1;
    memset(&request,0,sizeof(request));memset(&response,0,sizeof(response));
    request.kind=STNC_STNC_REQUEST;request.method=STNC_STNC_METHOD_PENDING;request.code=STNC_STNC_OK;request.request_id=STNC_PENDING_REQUEST_ID;
    if(stnc_stnc_encode(&request,request_buffer,sizeof(request_buffer),&written)!=0||
       stnc_network_send(&chain_connection,request_buffer,written)!=0||
       stnc_network_receive(&chain_connection,response_header,sizeof(response_header))!=0||
       stnc_stnc_decode_header(response_header,sizeof(response_header),&response)!=0||
       response.method!=STNC_STNC_METHOD_PENDING||response.request_id!=STNC_PENDING_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length!=sizeof(payload)||
       stnc_network_receive(&chain_connection,payload,sizeof(payload))!=0)return 1;
    return stnc_stnc_decode_pending(payload,sizeof(payload),state);
}

int stnc_core_mining_context(stnc_mining_context *context)
{
    uint8_t request[STNC_STNC_HEADER_SIZE],header[STNC_STNC_HEADER_SIZE];
    uint8_t payload[STNC_STNC_MINING_CONTEXT_SIZE];
    stnc_stnc_message response;size_t written;
    if(context==NULL||core_state==STNC_CORE_STATE_UNINITIALIZED||
       core_state==STNC_CORE_STATE_STOPPED||!stnc_network_is_connected(&chain_connection))return 1;
    if(stnc_stnc_encode_empty_request(STNC_STNC_METHOD_MINING_CONTEXT,
            STNC_MINING_CONTEXT_REQUEST_ID,request,sizeof(request),&written)!=0||
       stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_MINING_CONTEXT||
       response.request_id!=STNC_MINING_CONTEXT_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length!=sizeof(payload)||
       stnc_network_receive(&chain_connection,payload,sizeof(payload))!=0)return 1;
    return stnc_stnc_decode_mining_context(payload,sizeof(payload),context);
}

stnc_core_work_base_result stnc_core_check_work_base(const uint8_t tip_id[32],stnc_mining_context *context)
{
    uint8_t request[STNC_STNC_HEADER_SIZE+32u],header[STNC_STNC_HEADER_SIZE];
    uint8_t payload[STNC_STNC_MINING_CONTEXT_SIZE];stnc_stnc_message response;size_t written;
    if(tip_id==NULL||context==NULL||core_state==STNC_CORE_STATE_UNINITIALIZED||
       core_state==STNC_CORE_STATE_STOPPED||!stnc_network_is_connected(&chain_connection))return STNC_CORE_WORK_BASE_ERROR;
    memset(context,0,sizeof(*context));
    if(stnc_stnc_encode_check_work_base(tip_id,STNC_CHECK_WORK_BASE_REQUEST_ID,
            request,sizeof(request),&written)!=0||
       stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_CHECK_WORK_BASE||
       response.request_id!=STNC_CHECK_WORK_BASE_REQUEST_ID)return STNC_CORE_WORK_BASE_ERROR;
    if(response.code==STNC_STNC_STALE&&response.length==0u)return STNC_CORE_WORK_BASE_STALE;
    if(response.code!=STNC_STNC_OK||response.length!=sizeof(payload)||
       stnc_network_receive(&chain_connection,payload,sizeof(payload))!=0||
       stnc_stnc_decode_mining_context(payload,sizeof(payload),context)!=0)return STNC_CORE_WORK_BASE_ERROR;
    return STNC_CORE_WORK_BASE_CURRENT;
}
int stnc_core_mining_template(uint8_t **payload,size_t *payload_length,stnc_mining_template *work)
{
    uint8_t request[STNC_STNC_HEADER_SIZE],header[STNC_STNC_HEADER_SIZE],*owned;
    stnc_stnc_message response;size_t written;
    if(payload==NULL||payload_length==NULL||work==NULL)return 1;
    *payload=NULL;*payload_length=0u;memset(work,0,sizeof(*work));
    if(core_state==STNC_CORE_STATE_UNINITIALIZED||core_state==STNC_CORE_STATE_STOPPED||
       !stnc_network_is_connected(&chain_connection))return 1;
    if(stnc_stnc_encode_empty_request(STNC_STNC_METHOD_MINING_TEMPLATE,
            STNC_MINING_TEMPLATE_REQUEST_ID,request,sizeof(request),&written)!=0||
       stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_MINING_TEMPLATE||
       response.request_id!=STNC_MINING_TEMPLATE_REQUEST_ID||response.code!=STNC_STNC_OK||
       response.length<STNC_STNC_MINING_TEMPLATE_PREFIX_SIZE||
       response.length>STNC_STNC_MINING_TEMPLATE_PREFIX_SIZE+STNC_STNC_BLOCK_MAX_SIZE)return 1;
    owned=(uint8_t *)malloc(response.length);if(owned==NULL)return 1;
    if(stnc_network_receive(&chain_connection,owned,response.length)!=0||
       stnc_stnc_decode_mining_template(owned,response.length,work)!=0){free(owned);return 1;}
    *payload=owned;*payload_length=response.length;return 0;
}
void stnc_core_mining_template_release(uint8_t *payload){free(payload);}

int stnc_core_submit_work(
    const uint8_t parent_id[32],const uint8_t work_id[32],const char *miner_identity,
    const uint8_t *block,size_t block_length,uint8_t block_id[32],uint64_t *height,
    uint8_t cumulative_work[40])
{
    uint8_t *request=NULL,header[STNC_STNC_HEADER_SIZE],payload[STNC_STNC_BLOCK_ACCEPTED_SIZE];
    stnc_stnc_message response;size_t capacity,written;int rc=1;
    if(parent_id==NULL||work_id==NULL||miner_identity==NULL||block==NULL||
       block_id==NULL||height==NULL||cumulative_work==NULL||
       core_state==STNC_CORE_STATE_UNINITIALIZED||core_state==STNC_CORE_STATE_STOPPED||
       !stnc_network_is_connected(&chain_connection)||
       block_length>SIZE_MAX-STNC_STNC_HEADER_SIZE-STNC_STNC_MINING_SUBMISSION_PREFIX_SIZE)return 1;
    capacity=STNC_STNC_HEADER_SIZE+STNC_STNC_MINING_SUBMISSION_PREFIX_SIZE+block_length;
    request=(uint8_t *)malloc(capacity);if(request==NULL)return 1;
    if(stnc_stnc_encode_submit_work(parent_id,work_id,miner_identity,block,block_length,
            STNC_SUBMIT_WORK_REQUEST_ID,request,capacity,&written)!=0||
       stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_SUBMIT_WORK||
       response.request_id!=STNC_SUBMIT_WORK_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length!=sizeof(payload)||
       stnc_network_receive(&chain_connection,payload,sizeof(payload))!=0||
       stnc_stnc_decode_block_accepted(payload,sizeof(payload),block_id,height,cumulative_work)!=0)goto done;
    memcpy(chain_state.tip_id,block_id,32u);chain_state.height=*height;
    memcpy(chain_state.cumulative_work,cumulative_work,40u);
    chain_state.block_count=*height<UINT32_MAX?(uint32_t)(*height+1u):UINT32_MAX;
    chain_state.available=1;rc=0;
done:
    free(request);return rc;
}

int stnc_core_submit_block_evidence(const uint8_t *block,size_t block_length)
{
    uint8_t *request;
    uint8_t response_header[STNC_STNC_HEADER_SIZE];
    uint8_t payload[STNC_STNC_BLOCK_ACCEPTED_SIZE];
    uint8_t tip_id[32],work[40];
    uint64_t height;
    stnc_stnc_message response;
    size_t capacity,written;
    int rc=1;

    if(core_state==STNC_CORE_STATE_UNINITIALIZED||core_state==STNC_CORE_STATE_STOPPED||
       !stnc_network_is_connected(&chain_connection)||block==NULL||
       block_length<STNC_STNC_BLOCK_HEADER_SIZE||block_length>STNC_STNC_BLOCK_MAX_SIZE)return 1;

    capacity=STNC_STNC_HEADER_SIZE+block_length;
    request=(uint8_t *)malloc(capacity);
    if(request==NULL)return 1;

    if(stnc_stnc_encode_submit_block_evidence(block,block_length,STNC_BLOCK_EVIDENCE_REQUEST_ID,
            request,capacity,&written)==0 &&
       stnc_network_send(&chain_connection,request,written)==0 &&
       stnc_network_receive(&chain_connection,response_header,sizeof(response_header))==0 &&
       stnc_stnc_decode_header(response_header,sizeof(response_header),&response)==0 &&
       response.method==STNC_STNC_METHOD_SUBMIT_BLOCK_EVIDENCE &&
       response.request_id==STNC_BLOCK_EVIDENCE_REQUEST_ID &&
       response.code==STNC_STNC_OK &&
       response.length==sizeof(payload) &&
       stnc_network_receive(&chain_connection,payload,sizeof(payload))==0 &&
       stnc_stnc_decode_block_accepted(payload,sizeof(payload),tip_id,&height,work)==0){
        memcpy(chain_state.tip_id,tip_id,sizeof(tip_id));
        chain_state.height=height;
        memcpy(chain_state.cumulative_work,work,sizeof(work));
        chain_state.block_count=height<UINT32_MAX ? (uint32_t)(height+1u) : UINT32_MAX;
        chain_state.available=1;
        rc=0;
    }
    free(request);
    return rc;
}

int stnc_core_submit_history_evidence(
    const uint8_t *const *blocks,const size_t *block_lengths,size_t block_count)
{
    uint8_t *request,*payload=NULL,response_header[STNC_STNC_HEADER_SIZE];
    uint8_t tip_id[32],work[40];
    uint64_t height;
    stnc_stnc_message response;
    size_t capacity=STNC_STNC_HEADER_SIZE+4u,written,i;
    int rc=1;

    if(core_state==STNC_CORE_STATE_UNINITIALIZED||core_state==STNC_CORE_STATE_STOPPED||
       !stnc_network_is_connected(&chain_connection)||blocks==NULL||block_lengths==NULL||
       block_count==0u||block_count>UINT32_MAX)return 1;
    for(i=0u;i<block_count;i++){
        if(blocks[i]==NULL||block_lengths[i]<STNC_STNC_BLOCK_HEADER_SIZE||
           block_lengths[i]>STNC_STNC_BLOCK_MAX_SIZE||capacity>SIZE_MAX-4u||
           block_lengths[i]>SIZE_MAX-capacity-4u)return 1;
        capacity+=4u+block_lengths[i];
    }
    request=(uint8_t *)malloc(capacity);if(request==NULL)return 1;
    if(stnc_stnc_encode_submit_history_evidence(blocks,block_lengths,block_count,
            STNC_HISTORY_EVIDENCE_REQUEST_ID,request,capacity,&written)!=0)goto done;
    if(stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,response_header,sizeof(response_header))!=0||
       stnc_stnc_decode_header(response_header,sizeof(response_header),&response)!=0||
       response.method!=STNC_STNC_METHOD_SUBMIT_HISTORY_EVIDENCE||
       response.request_id!=STNC_HISTORY_EVIDENCE_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length!=STNC_STNC_BLOCK_ACCEPTED_SIZE)goto done;
    payload=(uint8_t *)malloc(response.length);if(payload==NULL)goto done;
    if(stnc_network_receive(&chain_connection,payload,response.length)!=0||
       stnc_stnc_decode_block_accepted(payload,response.length,tip_id,&height,work)!=0)goto done;
    memcpy(chain_state.tip_id,tip_id,sizeof(tip_id));chain_state.height=height;
    memcpy(chain_state.cumulative_work,work,sizeof(work));
    chain_state.block_count=height<UINT32_MAX?(uint32_t)(height+1u):UINT32_MAX;
    chain_state.available=1;rc=0;
done:
    free(payload);free(request);return rc;
}

stnc_core_evidence_result stnc_core_submit_suffix_evidence(
    uint32_t prefix_count,const uint8_t *const *blocks,const size_t *block_lengths,size_t block_count)
{
    uint8_t *request,*payload=NULL,header[STNC_STNC_HEADER_SIZE],tip[32],work[40];
    stnc_stnc_message response;uint64_t height;size_t capacity=STNC_STNC_HEADER_SIZE+8u,written,i;stnc_core_evidence_result rc=STNC_CORE_EVIDENCE_ERROR;
    if(prefix_count==0u||blocks==NULL||block_lengths==NULL||block_count==0u)return STNC_CORE_EVIDENCE_ERROR;
    for(i=0u;i<block_count;i++){if(blocks[i]==NULL||capacity>SIZE_MAX-4u||
        block_lengths[i]>SIZE_MAX-capacity-4u)return STNC_CORE_EVIDENCE_ERROR;capacity+=4u+block_lengths[i];}
    request=(uint8_t *)malloc(capacity);if(request==NULL)return STNC_CORE_EVIDENCE_ERROR;
    if(stnc_stnc_encode_submit_suffix_evidence(prefix_count,blocks,block_lengths,block_count,
            STNC_SUFFIX_EVIDENCE_REQUEST_ID,request,capacity,&written)!=0)goto done;
    if(stnc_network_send(&chain_connection,request,written)!=0||
       stnc_network_receive(&chain_connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_STNC_METHOD_SUBMIT_SUFFIX_EVIDENCE||
       response.request_id!=STNC_SUFFIX_EVIDENCE_REQUEST_ID)goto done;
    if(response.code==STNC_STNC_CURRENT&&response.length==0u){rc=STNC_CORE_EVIDENCE_CURRENT;goto done;}
    if(response.code!=STNC_STNC_OK||response.length!=STNC_STNC_BLOCK_ACCEPTED_SIZE)goto done;
    payload=(uint8_t *)malloc(response.length);if(payload==NULL)goto done;
    if(stnc_network_receive(&chain_connection,payload,response.length)!=0||
       stnc_stnc_decode_block_accepted(payload,response.length,tip,&height,work)!=0)goto done;
    memcpy(chain_state.tip_id,tip,32u);chain_state.height=height;memcpy(chain_state.cumulative_work,work,40u);
    chain_state.block_count=height<UINT32_MAX?(uint32_t)(height+1u):UINT32_MAX;chain_state.available=1;rc=STNC_CORE_EVIDENCE_ADOPTED;
done:
    free(payload);free(request);return rc;
}

int stnc_core_submit_transaction(const uint8_t *transaction,size_t transaction_length,stnc_submission_result *result)
{
    uint8_t *request;uint8_t response_header[STNC_STNC_HEADER_SIZE];uint8_t payload[STNC_STNC_SUBMISSION_RESPONSE_SIZE];
    stnc_stnc_message response;size_t capacity,written;int rc=1;
    if(core_state==STNC_CORE_STATE_UNINITIALIZED||core_state==STNC_CORE_STATE_STOPPED||
       !stnc_network_is_connected(&chain_connection)||transaction==NULL||result==NULL||
       transaction_length<12u||transaction_length>STNC_STNC_TRANSACTION_MAX)return 1;
    capacity=STNC_STNC_HEADER_SIZE+transaction_length;
    request=(uint8_t *)malloc(capacity);if(request==NULL)return 1;
    if(stnc_stnc_encode_submit_transaction(transaction,transaction_length,STNC_SUBMIT_TRANSACTION_REQUEST_ID,
            request,capacity,&written)==0 &&
       stnc_network_send(&chain_connection,request,written)==0 &&
       stnc_network_receive(&chain_connection,response_header,sizeof(response_header))==0 &&
       stnc_stnc_decode_header(response_header,sizeof(response_header),&response)==0 &&
       response.method==STNC_STNC_METHOD_SUBMIT_TRANSACTION &&
       response.request_id==STNC_SUBMIT_TRANSACTION_REQUEST_ID &&
       response.code==STNC_STNC_OK &&
       response.length==sizeof(payload) &&
       stnc_network_receive(&chain_connection,payload,sizeof(payload))==0 &&
       stnc_stnc_decode_submission(payload,sizeof(payload),result)==0)rc=0;
    free(request);return rc;
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
            stnc_background_mining_tick();
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
    stnc_background_mining_shutdown();

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
    memset(&peer_status,0,sizeof(peer_status));
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

const stnc_core_peer_status *stnc_core_get_peer_status(void)
{
    return &peer_status;
}

void stnc_core_get_runtime_status(stnc_core_runtime_status *status)
{
    if(status==NULL)return;
    memset(status,0,sizeof(*status));
    status->chain_connected=stnc_network_is_connected(&chain_connection);
    status->chain_state_available=chain_state.available;
    status->p2p_connected=stnc_network_is_connected(&p2p_connection);
    status->candidate_count=peer_candidates.count;
    status->qualified_count=qualified_peers.count;
    status->root_peer_capabilities=root_peer_capabilities;
}
