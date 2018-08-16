#include <stdio.h>
#include <string.h>

#include "hashes/cmac.h"
#include "crypto/ciphers.h"

#include "net/gnrc/lorawan/lorawan.h"

#define PKT_WRITE_BYTE(CURSOR, BYTE) *(CURSOR++) = BYTE

#define PKT_WRITE(CURSOR, SRC, LEN) do {\
    memcpy(CURSOR, SRC, LEN); \
    CURSOR += LEN; \
} while (0);

//TODO
static cmac_context_t CmacContext;
static uint8_t digest[16];
static cipher_t AesContext;


uint32_t calculate_pkt_mic(uint8_t dir, uint8_t *dev_addr, uint16_t fcnt, uint8_t* msg, size_t size, uint8_t *nwkskey)
{
    /* Block */
    uint8_t block[16];
    uint8_t *p = block;
    memset(block, 0, 16);
    
    PKT_WRITE_BYTE(p, 0x49);
    /* pad */
    p += 4;

    PKT_WRITE_BYTE(p, dir & 0x1);
    PKT_WRITE(p, dev_addr, 4);

    PKT_WRITE_BYTE(p, fcnt & 0xFF);
    PKT_WRITE_BYTE(p, (fcnt >> 8) & 0xFF);
    PKT_WRITE_BYTE(p, 0);
    PKT_WRITE_BYTE(p, 0);

    /* More pad */
    PKT_WRITE_BYTE(p, 0);

    PKT_WRITE_BYTE(p, size);

    cmac_init(&CmacContext, nwkskey, 16);
    cmac_update(&CmacContext, block, sizeof(block) );
    cmac_update(&CmacContext, msg, size);
    cmac_final(&CmacContext, digest);

    return ( uint32_t )( ( uint32_t )digest[3] << 24 | ( uint32_t )digest[2] << 16 | ( uint32_t )digest[1] << 8 | ( uint32_t )digest[0] );
}

void encrypt_payload(uint8_t *payload, size_t size, uint8_t *dev_addr, uint16_t fcnt, uint8_t dir, uint8_t *appskey, uint8_t *out)
{
    uint8_t s_block[16];
    uint8_t a_block[16];

    memset(s_block, 0, sizeof(s_block));
    memset(a_block, 0, sizeof(a_block));

    uint8_t *p = a_block;

    PKT_WRITE_BYTE(p, 0x01);

    /* pad */
    p += 4;

    PKT_WRITE_BYTE(p, dir & 0x1);
    PKT_WRITE(p, dev_addr, 4);

    /* TODO: Frame Counter */
    PKT_WRITE_BYTE(p, fcnt & 0xFF);
    PKT_WRITE_BYTE(p, (fcnt >> 8) & 0xFF);
    PKT_WRITE_BYTE(p, 0);
    PKT_WRITE_BYTE(p, 0);

    /* More pad */
    PKT_WRITE_BYTE(p, 0);

    /* TODO: */
    PKT_WRITE_BYTE(p, 1);

    //uint8_t blocks = (size >> 8) + 1;
    /* TODO: APPKEY HARDCODED! */
    cipher_init(&AesContext, CIPHER_AES_128, appskey, 16);
    cipher_encrypt(&AesContext, a_block, s_block);

    for(unsigned i=0;i<size;i++) {
        out[i] = payload[i] ^ s_block[i];
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

