#include <stdio.h>
#include <string.h>

#include "stnc_stnc.h"

static int check_type(uint16_t type, const char *prefix, size_t address_length)
{
    uint8_t frame[STNC_STNC_DERIVE_FRAME_MAX];
    uint8_t address[STNC_STNC_ADDRESS_MAX_SIZE];
    char decoded[STNC_STNC_ADDRESS_MAX_SIZE + 1u];
    const char source[] = "canonical-source";
    size_t written;
    size_t index;

    if (stnc_stnc_encode_derive_address(
            type,
            (const uint8_t *)source,
            strlen(source),
            UINT64_C(7),
            frame,
            sizeof(frame),
            &written
        ) != 0 ||
        written != STNC_STNC_HEADER_SIZE + STNC_STNC_DERIVE_PREFIX_SIZE + strlen(source) ||
        memcmp(frame, "STNC", 4) != 0 ||
        frame[8] != 0u ||
        frame[9] != STNC_STNC_METHOD_DERIVE_ADDRESS ||
        frame[24] != 0u ||
        frame[25] != (uint8_t)type) {
        return 1;
    }

    memset(address, 'a', address_length);
    memcpy(address, prefix, strlen(prefix));
    for (index = strlen(prefix); index < address_length; index++) {
        address[index] = (uint8_t)"0123456789abcdef"[index & 15u];
    }

    if (stnc_stnc_decode_address(
            type,
            address,
            address_length,
            decoded,
            sizeof(decoded)
        ) != 0 ||
        strlen(decoded) != address_length ||
        memcmp(decoded, prefix, strlen(prefix)) != 0) {
        return 1;
    }

    return 0;
}

static int check_queries(void)
{
    uint8_t frame[STNC_STNC_HEADER_SIZE + STNC_STNC_ADDRESS_TYPED_SIZE];
    uint8_t balance[STNC_STNC_BALANCE_SIZE] = {0,0,0,0,0,0,0,42};
    uint8_t contract[STNC_STNC_CONTRACT_STATE_SIZE] = {0,2,0,4,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,9,0,3,0,0,0,12};
    const char wallet[] = "stnw0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const char contract_address[] = "stnc0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    stnc_contract_state state;
    uint64_t units;
    size_t written;
    if (stnc_stnc_encode_address_query(STNC_STNC_METHOD_BALANCE,wallet,UINT64_C(8),frame,sizeof(frame),&written)!=0 ||
        written!=sizeof(frame) || frame[9]!=STNC_STNC_METHOD_BALANCE ||
        memcmp(frame+STNC_STNC_HEADER_SIZE,wallet,STNC_STNC_ADDRESS_TYPED_SIZE)!=0 ||
        stnc_stnc_encode_address_query(STNC_STNC_METHOD_CONTRACT_STATE,contract_address,UINT64_C(9),frame,sizeof(frame),&written)!=0 ||
        frame[9]!=STNC_STNC_METHOD_CONTRACT_STATE ||
        stnc_stnc_encode_address_query(STNC_STNC_METHOD_BALANCE,contract_address,UINT64_C(1),frame,sizeof(frame),&written)==0 ||
        stnc_stnc_encode_address_query(STNC_STNC_METHOD_CONTRACT_STATE,wallet,UINT64_C(1),frame,sizeof(frame),&written)==0) return 1;
    if (stnc_stnc_decode_balance(balance,sizeof(balance),&units)!=0 || units!=UINT64_C(42) ||
        stnc_stnc_decode_balance(balance,sizeof(balance)-1u,&units)==0) return 1;
    if (stnc_stnc_decode_contract_state(contract,sizeof(contract),&state)!=0 ||
        state.state!=2u || state.type!=4u || state.sequence!=UINT64_C(7) || state.created_at!=UINT64_C(9) ||
        state.participant_count!=3u || state.terms_length!=12u) return 1;
    contract[1]=10u;
    if (stnc_stnc_decode_contract_state(contract,sizeof(contract),&state)==0) return 1;
    return 0;
}

static int check_pending(void)
{
    uint8_t payload[STNC_STNC_PENDING_SIZE]={0,0,0,2,0,0,0,64,0,0,0,214,0,16,0,0};
    stnc_pending_state state;
    if(stnc_stnc_decode_pending(payload,sizeof(payload),&state)!=0||
       state.count!=2u||state.max_entries!=64u||state.bytes!=214u||state.max_bytes!=1048576u)return 1;
    payload[3]=65u;if(stnc_stnc_decode_pending(payload,sizeof(payload),&state)==0)return 1;
    return 0;
}

static int check_submission(void)
{
    uint8_t tx[214]={0};uint8_t frame[STNC_STNC_HEADER_SIZE+sizeof(tx)];uint8_t response[STNC_STNC_SUBMISSION_RESPONSE_SIZE]={0};
    stnc_submission_result result;size_t written;size_t i;
    memcpy(tx,"STNT",4);tx[5]=1;tx[7]=9;tx[11]=202;
    if(stnc_stnc_encode_submit_transaction(tx,sizeof(tx),UINT64_C(10),frame,sizeof(frame),&written)!=0||
       written!=sizeof(frame)||frame[8]!=0x10u||frame[9]!=0x05u||
       memcmp(frame+STNC_STNC_HEADER_SIZE,tx,sizeof(tx))!=0)return 1;
    response[1]=1;response[3]=STNC_STNC_SUBMISSION_ADMITTED;for(i=0;i<32u;i++)response[4+i]=(uint8_t)i;
    if(stnc_stnc_decode_submission(response,sizeof(response),&result)!=0||result.result!=STNC_STNC_SUBMISSION_ADMITTED||
       !result.has_transaction_id||memcmp(result.transaction_id,response+4,32u)!=0)return 1;
    memset(response,0,sizeof(response));response[1]=1;response[3]=STNC_STNC_SUBMISSION_UNAUTHORIZED;
    if(stnc_stnc_decode_submission(response,sizeof(response),&result)!=0||result.has_transaction_id)return 1;
    response[4]=1;if(stnc_stnc_decode_submission(response,sizeof(response),&result)==0)return 1;
    return 0;
}

static int check_block_evidence(void)
{
    uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE]={0};
    uint8_t frame[STNC_STNC_HEADER_SIZE+STNC_STNC_BLOCK_HEADER_SIZE];
    uint8_t accepted[STNC_STNC_BLOCK_ACCEPTED_SIZE]={0};
    uint8_t tip[32],work[40];
    uint64_t height=0;
    size_t written=0,i;
    block[0]=0u;block[1]=3u;
    for(i=0;i<32u;i++)accepted[i]=(uint8_t)i;
    accepted[39]=42u;
    for(i=0;i<40u;i++)accepted[40u+i]=(uint8_t)(0x80u+i);
    if(stnc_stnc_encode_submit_block_evidence(block,sizeof(block),UINT64_C(11),
            frame,sizeof(frame),&written)!=0||written!=sizeof(frame)||
       frame[8]!=0x10u||frame[9]!=0x06u||
       memcmp(frame+STNC_STNC_HEADER_SIZE,block,sizeof(block))!=0)return 1;
    if(stnc_stnc_decode_block_accepted(accepted,sizeof(accepted),tip,&height,work)!=0||
       height!=UINT64_C(42)||memcmp(tip,accepted,32u)!=0||memcmp(work,accepted+40u,40u)!=0)return 1;
    if(stnc_stnc_encode_submit_block_evidence(block,STNC_STNC_BLOCK_HEADER_SIZE-1u,UINT64_C(11),
            frame,sizeof(frame),&written)==0||written!=0u)return 1;
    return 0;
}

int main(void)
{
    uint8_t frame[STNC_STNC_DERIVE_FRAME_MAX];
    size_t written = 99u;

    if (check_type(STNC_STNC_ADDRESS_IDENTITY, "stn0_", STNC_STNC_ADDRESS_IDENTITY_SIZE) != 0 ||
        check_type(STNC_STNC_ADDRESS_CONTRACT, "stnc0_", STNC_STNC_ADDRESS_TYPED_SIZE) != 0 ||
        check_type(STNC_STNC_ADDRESS_WALLET, "stnw0_", STNC_STNC_ADDRESS_TYPED_SIZE) != 0) {
        return 1;
    }

    if (stnc_stnc_encode_derive_address(
            99u,
            (const uint8_t *)"x",
            1u,
            UINT64_C(1),
            frame,
            sizeof(frame),
            &written
        ) == 0 ||
        written != 0u) {
        return 1;
    }

    if (check_queries() != 0 || check_pending() != 0 || check_submission() != 0 || check_block_evidence() != 0) {
        return 1;
    }

    printf("STNC codec tests passed.\n");
    return 0;
}
