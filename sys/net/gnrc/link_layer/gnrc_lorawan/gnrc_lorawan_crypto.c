#include <stdio.h>
#include <string.h>

#include "hashes/cmac.h"
#include "crypto/ciphers.h"

#include "net/gnrc/lorawan/lorawan.h"
#include "byteorder.h"
#include "net/lorawan/hdr.h"

//TODO
static cmac_context_t CmacContext;
static uint8_t digest[16];
static cipher_t AesContext;

typedef struct  __attribute__((packed)) {
    uint8_t fb;
    uint32_t u8_pad;
    uint8_t dir;
    le_uint32_t dev_addr;
    le_uint32_t fcnt;
    uint8_t u32_pad;
    uint8_t len;
} lorawan_block_t;

uint32_t calculate_pkt_mic_2(lorawan_hdr_t *lw_hdr, uint8_t dir, gnrc_pktsnip_t *pkt, uint8_t *nwkskey)
{
    /* block */
    lorawan_block_t block; 

    block.fb = 0x49;
    block.u8_pad = 0;
    block.dir = dir & 0x1;

    memcpy(&block.dev_addr, &lw_hdr->addr, sizeof(lw_hdr->addr));
    memcpy(&block.fcnt, &lw_hdr->fcnt, sizeof(lw_hdr->fcnt));

    block.u32_pad = 0;

    /* TODO: length of packet snip */
    block.len = gnrc_pkt_len(pkt);

    cmac_init(&CmacContext, nwkskey, 16);
    cmac_update(&CmacContext, &block, sizeof(block) );
    while(pkt != NULL) {
        cmac_update(&CmacContext, pkt->data, pkt->size);
        pkt = pkt->next;
    }
    cmac_final(&CmacContext, digest);

    return ( uint32_t )( ( uint32_t )digest[3] << 24 | ( uint32_t )digest[2] << 16 | ( uint32_t )digest[1] << 8 | ( uint32_t )digest[0] );
}

uint32_t calculate_pkt_mic(uint8_t dir, uint8_t *dev_addr, uint16_t fcnt, gnrc_pktsnip_t *pkt, uint8_t *nwkskey)
{
    /* block */
    lorawan_block_t block; 

    block.fb = 0x49;
    block.u8_pad = 0;
    block.dir = dir & 0x1;

    memcpy(&block.dev_addr, dev_addr, 4);

    block.fcnt = byteorder_btoll(byteorder_htonl(fcnt));
    block.u32_pad = 0;

    /* TODO: Length of packet snip */
    block.len = gnrc_pkt_len(pkt);

    cmac_init(&CmacContext, nwkskey, 16);
    cmac_update(&CmacContext, &block, sizeof(block) );
    while(pkt != NULL) {
        cmac_update(&CmacContext, pkt->data, pkt->size);
        pkt = pkt->next;
    }
    cmac_final(&CmacContext, digest);

    return ( uint32_t )( ( uint32_t )digest[3] << 24 | ( uint32_t )digest[2] << 16 | ( uint32_t )digest[1] << 8 | ( uint32_t )digest[0] );
}

void encrypt_payload(uint8_t *payload, size_t size, uint8_t *dev_addr, uint16_t fcnt, uint8_t dir, uint8_t *appskey)
{
    uint8_t s_block[16];
    uint8_t a_block[16];

    memset(s_block, 0, sizeof(s_block));
    memset(a_block, 0, sizeof(a_block));

    lorawan_block_t *block = (lorawan_block_t*) a_block;

    cipher_init(&AesContext, CIPHER_AES_128, appskey, 16);

    block->fb = 0x01;

    block->u8_pad = 0;
    block->dir = dir & 0x1;

    memcpy(&block->dev_addr, dev_addr, 4);

    /* TODO: Frame Counter */
    block->fcnt = byteorder_btoll(byteorder_htonl(fcnt));
    block->u32_pad = 0;

    int blocks = ((size-1) >> 4) + 1;

    for (int j=1;j<=blocks;j++)
    {
        block->len=j;
        cipher_encrypt(&AesContext, a_block, s_block);

        for(unsigned i=0;i< (j == blocks ? size % 16 : 16);i++) {
            payload[i] = payload[i] ^ s_block[i];
        }

        payload += 16;
    }
}

uint32_t calculate_mic(uint8_t *buf, size_t size, uint8_t *appkey)
{
    cmac_init(&CmacContext, (const uint8_t*) appkey, 16);
    cmac_update(&CmacContext, buf, size);
    cmac_final(&CmacContext, digest);

    return ( uint32_t )( ( uint32_t )digest[3] << 24 | ( uint32_t )digest[2] << 16 | ( uint32_t )digest[1] << 8 | ( uint32_t )digest[0] );
}

void decrypt_join_accept(uint8_t *key, uint8_t *pkt, int has_clist, uint8_t *out)
{
    cipher_init(&AesContext, CIPHER_AES_128, key, 16);
    cipher_encrypt(&AesContext, pkt, out);

    if( has_clist )
    {
        cipher_encrypt(&AesContext, pkt+ 16, out + 16);
    }
}

void generate_session_keys(uint8_t *app_nonce, uint8_t *dev_nonce, uint8_t *appkey, uint8_t *nwkskey, uint8_t *appskey)
{
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));

    cipher_init(&AesContext, CIPHER_AES_128, appkey, 16);
    memcpy(buf + 1, app_nonce, 6);
    memcpy(buf + 7, dev_nonce, 2);

    /* Calculate Application Session Key */
    buf[0] = 0x01;
    cipher_encrypt(&AesContext, buf, nwkskey);

    /* Calculate Network Session Key */
    buf[0] = 0x02;
    cipher_encrypt(&AesContext, buf, appskey);
}

