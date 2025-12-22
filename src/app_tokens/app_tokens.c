#include "app_tokens.h"
#include "buffer_utils.h"
#include "textUtils.h"
#include "hash.h"
#include "addressUtils/bech32.h"
#include "os.h"

#define ASSET_FINGERPRINT_SIZE 20

void deriveAssetFingerprintBytes(const uint8_t* policyId,
                                 size_t policyIdSize,
                                 const uint8_t* assetName,
                                 size_t assetNameSize,
                                 uint8_t* fingerprintBuffer,
                                 size_t fingerprintBufferSize) {
    ASSERT(policyIdSize == MINTING_POLICY_ID_LENGTH);
    ASSERT(assetNameSize <= MAX_ASSET_NAME_LENGTH);
    ASSERT(fingerprintBufferSize >= ASSET_FINGERPRINT_SIZE);

    uint8_t hashInput[MINTING_POLICY_ID_LENGTH + MAX_ASSET_NAME_LENGTH] = {0};
    const size_t hashInputSize = policyIdSize + assetNameSize;
    {
        write_buffer_t buf = buffer_init(hashInput, SIZEOF(hashInput));

        // Buffer is sized correctly by design - failure is programming error
        bool success = buffer_write_bytes(&buf, policyId, policyIdSize);
        ASSERT(success);

        success = buffer_write_bytes(&buf, assetName, assetNameSize);
        ASSERT(success);

        ASSERT(buffer_written_size(&buf) == hashInputSize);
    }

    blake2b_160_hash(hashInput, hashInputSize, fingerprintBuffer, fingerprintBufferSize);
}

size_t deriveAssetFingerprintBech32(const uint8_t* policyId,
                                    size_t policyIdSize,
                                    const uint8_t* assetName,
                                    size_t assetNameSize,
                                    char* fingerprint,
                                    size_t fingerprintMaxSize) {
    ASSERT(policyIdSize == MINTING_POLICY_ID_LENGTH);
    ASSERT(assetNameSize <= MAX_ASSET_NAME_LENGTH);

    uint8_t fingerprintBuffer[ASSET_FINGERPRINT_SIZE];
    deriveAssetFingerprintBytes(policyId,
                                policyIdSize,
                                assetName,
                                assetNameSize,
                                fingerprintBuffer,
                                SIZEOF(fingerprintBuffer));

    bool success = format_bech32("asset",
                                 fingerprintBuffer,
                                 SIZEOF(fingerprintBuffer),
                                 fingerprint,
                                 fingerprintMaxSize);
    ASSERT(success);
    ASSERT(strlen(fingerprint) + 1 < fingerprintMaxSize);

    return strlen(fingerprint);
}

typedef struct {
    uint8_t fingerprint[ASSET_FINGERPRINT_SIZE];
    uint8_t decimals;
    const char* ticker;
} token_info_t;

const token_info_t tokenInfos[] = {
// a fixed list of most popular tokens
#include "../tokenRegistry/token_data.csource"
};

static const token_info_t* _getTokenInfo(const token_group_t* tokenGroup,
                                         const uint8_t* assetNameBytes,
                                         size_t assetNameSize) {
    ASSERT(assetNameSize <= MAX_ASSET_NAME_LENGTH);

    uint8_t fingerprintBuffer[ASSET_FINGERPRINT_SIZE];
    deriveAssetFingerprintBytes(tokenGroup->policyId,
                                SIZEOF(tokenGroup->policyId),
                                assetNameBytes,
                                assetNameSize,
                                fingerprintBuffer,
                                SIZEOF(fingerprintBuffer));

    for (size_t i = 0; i < ARRAY_LEN(tokenInfos); i++) {
        if (!memcmp(tokenInfos[i].fingerprint, fingerprintBuffer, ASSET_FINGERPRINT_SIZE)) {
            return &tokenInfos[i];
        }
    }

    return NULL;
}

bool str_formatTokenAmountOutput(const token_group_t* tokenGroup,
                                 const uint8_t* assetNameBytes,
                                 size_t assetNameSize,
                                 uint64_t amount,
                                 char* out,
                                 size_t outSize) {
    ASSERT(assetNameSize <= MAX_ASSET_NAME_LENGTH);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    const token_info_t* tokenInfo = _getTokenInfo(tokenGroup, assetNameBytes, assetNameSize);
    int decimals = (tokenInfo != NULL) ? tokenInfo->decimals : 0;
    TRACE("token decimal places = %u", decimals);
    bool formatted = str_formatDecimalAmount(amount, decimals, out, outSize);
    ASSERT(formatted);
    size_t length = strlen(out);

    const char* ticker = (tokenInfo != NULL) ? (const char*) PIC(tokenInfo->ticker) : "(unknown decimals)";
    TRACE("token ticker = %s", ticker);
    snprintf(out + length, outSize - length, " %s", ticker);
    length += 1 + strlen(ticker);

    ASSERT(length + 1 < outSize);
    ASSERT(length == strlen(out));

    return true;
}

bool str_formatTokenAmountMint(const token_group_t* tokenGroup,
                               const uint8_t* assetNameBytes,
                               size_t assetNameSize,
                               int64_t amount,
                               char* out,
                               size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(outSize >= 2);

    explicit_bzero(out, outSize);

    out[0] = (amount >= 0)
                 ? ' '
                 : '-';  // + sign instead of the space would be nice, but is unreadable on Nano S
    out[1] = '\0';

    bool formatted = str_formatTokenAmountOutput(tokenGroup,
                                                 assetNameBytes,
                                                 assetNameSize,
                                                 abs_int64(amount),
                                                 out + 1,
                                                 outSize - 1);
    ASSERT(formatted);

    size_t length = strlen(out);
    ASSERT(length + 1 < outSize);

    return true;
}
