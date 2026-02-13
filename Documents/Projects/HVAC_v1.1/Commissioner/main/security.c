#include "mbedtls/md.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static const char *TAG = "SECURITY";

// Verify command signature: "CMD_BODY|SIGNATURE_HEX"
// Example: "add * SECRET123|a1b2c3..."
bool verify_command_signature(char *input_buffer, char **cmd_part)
{
    // 1. Split Command and Signature
    char *separator = strrchr(input_buffer, '|');
    if (!separator) {
        ESP_LOGW(TAG, "Command rejected: No signature found");
        return false;
    }
    *separator = '\0'; // Split string
    char *received_sig_hex = separator + 1;
    *cmd_part = input_buffer;

    // 2. Calculate Expected HMAC-SHA256
    uint8_t hmac_output[32];
    const char *key = SECURE_HMAC_KEY;
    
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, strlen(key));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)input_buffer, strlen(input_buffer));
    mbedtls_md_hmac_finish(&ctx, hmac_output);
    mbedtls_md_free(&ctx);

    // 3. Convert calculated HMAC to Hex String
    char expected_sig_hex[65];
    for(int i=0; i<32; i++) {
        sprintf(expected_sig_hex + (i*2), "%02x", hmac_output[i]);
    }
    expected_sig_hex[64] = '\0';

    // 4. Compare (Constant time comparison is better, but strcmp is okay for now)
    if (strcmp(received_sig_hex, expected_sig_hex) == 0) {
        return true;
    } else {
        ESP_LOGE(TAG, "Signature Mismatch! Exp: %s, Got: %s", expected_sig_hex, received_sig_hex);
        return false;
    }
}
