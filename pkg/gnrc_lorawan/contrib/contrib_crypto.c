#include "gnrc_lorawan/lorawan.h"
#include "hashes/cmac.h"
#include "net/loramac.h"
#include "crypto/ciphers.h"

static cmac_context_t CmacContext;
static cipher_t AesContext;

void gnrc_lorawan_cmac_init(gnrc_lorawan_t *mac, const void *key)
{
    (void) mac;
    cmac_init(&CmacContext, key, LORAMAC_APPKEY_LEN);
}

void gnrc_lorawan_cmac_update(gnrc_lorawan_t *mac, const void *buf, size_t len)
{
    (void) mac;
    cmac_update(&CmacContext, buf, len);
}

void gnrc_lorawan_cmac_finish(gnrc_lorawan_t *mac, void *out)
{
    (void) mac;
    cmac_final(&CmacContext, out);
}

void gnrc_lorawan_aes128_init(gnrc_lorawan_t *mac, const void *key)
{
    (void) mac;
    cipher_init(&AesContext, CIPHER_AES_128, key, LORAMAC_APPKEY_LEN);
}

void gnrc_lorawan_aes128_encrypt(gnrc_lorawan_t *mac, const void *in, void *out)
{
    (void) mac;
    cipher_encrypt(&AesContext, in, out);
}
