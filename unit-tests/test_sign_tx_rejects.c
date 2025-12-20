#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <cmocka.h>

#include "handler/sign_tx.h"
#include "buffer.h"
#include "cardano_swo.h"
#include "globals.h"
#include "constants.h"
#include "securityPolicy/securityPolicy.h"
#include "transaction/tx_utils.h"
#include "transaction/tx_parse.h"
#include "hexUtils.h"
#include "utils/utils.h"

typedef enum {
    STATUS_TYPE_TRANSACTION_SIGNED = 0,
    STATUS_TYPE_TRANSACTION_REJECTED = 1,
} nbgl_reviewStatusType_t;

#define P1_TX_INIT 0x00
#define P1_TX_DATA_CHUNK 0x01
#define P1_TX_CHUNK_LAST 0x02

// ----------------------------------------------------------------------
// Simple mocks for IO and UI plumbing so we can drive the handler
// ----------------------------------------------------------------------

static uint16_t g_last_sw = 0;

int io_send_response_pointer(const uint8_t *buffer, size_t bufferLength, uint16_t sw) {
    (void) buffer;
    (void) bufferLength;
    g_last_sw = sw;
    return 0;
}

int io_send_sw(uint16_t sw) {
    g_last_sw = sw;
    return 0;
}

void nbgl_useCaseSpinner(const char *text) {
    (void) text;
}

void nbgl_useCaseStatus(const char *text, bool success, void (*callback)(void)) {
    (void) text;
    (void) success;
    if (callback != NULL) {
        callback();
    }
}

void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t reviewStatusType, void (*callback)(void)) {
    (void) reviewStatusType;
    if (callback != NULL) {
        callback();
    }
}

void ui_menu_main(void) {
    // no-op
}

int ui_display_transaction(void) {
    // reject tests never reach UI confirmation
    return 0;
}

int ui_display_witness(const bip44_path_t *path,
                       security_policy_t policy,
                       warning_bits_t warnings) {
    (void) path;
    (void) policy;
    (void) warnings;
    return 0;
}

bool app_mem_init(void) {
    return true;
}

void app_mem_deinit(void) {
    // no-op
}

void app_mem_dump_stats(void) {
    // no-op
}

void *app_mem_alloc_impl(size_t size, bool persistent, const char *file, int line) {
    (void) persistent;
    (void) file;
    (void) line;
    return malloc(size);
}

void app_mem_free_impl(void *ptr, const char *file, int line) {
    (void) file;
    (void) line;
    free(ptr);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_sw = 0;
}

typedef struct {
    const char *hex_payload;
    uint8_t p1;
    bool more;
} apdu_segment_t;

typedef struct {
    const char *name;
    const char *init_hex;
    const apdu_segment_t *chunks;
    size_t chunk_count;
    uint16_t expected_sw;
    bool expect_init_failure;
    const char *skip_reason;
} sign_tx_reject_fixture_t;

// ----------------------------------------------------------------------
// Fixture data (transaction init rejects first)
// ----------------------------------------------------------------------

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_REWARD_ADDRESS_KEY[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000025020E22058000073C800007178000000000000002000000000000"
    "00000000000A0000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_REWARD_ADDRESS_SCRIPT[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000002C020F55122A946B9AD3D2DDF029D3A828F0468AECE76895F15C9E"
    "FBD69B4277000000000000000A0000000000000000000000002A000000000000"
    "000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_NO_SPENDING_PATH_ORDINARY_TX_1[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000048020129FB5FD4AA8CADD6705ACC8263CEE0FC62EDCA5AC38DB593"
    "FEC2F9FD33122A946B9AD3D2DDF029D3A828F0468AECE76895F15C9EFBD69B42"
    "7700000000002DD2E80000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_NO_SPENDING_PATH_ORDINARY_TX_2[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000048020329FB5FD4AA8CADD6705ACC8263CEE0FC62EDCA5AC38DB593"
    "FEC2F9FD55122A946B9AD3D2DDF029D3A828F0468AECE76895F15C9EFBD69B42"
    "7700000000002DD2E80000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_POOL_OPERATOR_SPENDING_CHOICE_NOT_PATH[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000041020129FB5FD4AA8CADD6705ACC8263CEE0FC62EDCA5AC38DB593"
    "FEC2F9FD22058000073C80000717800001C80000000200000000000000000000"
    "000A0000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_MULTISIG_UNCONDITIONALLY[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003A0200058000073C80000717800000000000000000000000220580"
    "00073C8000071780000000000000020000000000000000006CA7930000000000"
    "000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_ADDRESS_POOL_OWNER_UNCONDITIONALLY[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003A0200058000073C80000717800000000000000000000000220580"
    "00073C8000071780000000000000020000000000000000006CA7930000000000"
    "000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_CERT_STAKING_SCRIPT_HASH_IN_ORDINARY_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000002C020F55122A946B9AD3D2DDF029D3A828F0468AECE76895F15C9E"
    "FBD69B4277000000000000000A0000000000000000000000002A000000000000"
    "000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_SCRIPT_HASH_AS_STAKE_CREDENTIAL_IN_ORDINARY_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A00000000000003E85529FB5FD4AA8C"
    "ADD6705ACC8263CEE0FC62EDCA5AC38DB593FEC2F9FD",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_NON_STAKING_PATH_AS_STAKE_CREDENTIAL_IN_ORDINARY_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A00000000000003E822058000073C80"
    "000717800000000000000000000000",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_STAKING_PATH_AS_STAKE_CREDENTIAL_IN_MULTISIG_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A00000000000003E822058000073C80"
    "000717800000000000000000000000",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_SINGLE_ACCOUNT_CHANGE_OUTPUT_AND_WITHDRAWAL_ACCOUNT_MISMATCH[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000004901003901EB0BAA5E570CFFBE2934DB29DF0B6A3D7C0430EE65D4"
    "C3A7AB2FEFB91BC428E4720702EBD5DAB4FB175324C192DC9BB76CC5DA956E3C"
    "8DFF00000000000000010100000000003A0200058000073C8000071780000000"
    "000000000000000022058000073C800007178000000000000002000000000000"
    "0000006CA7930000000000000000000000002A000000000000000A0000000000"
    "0003E822058000073C80000717800000010000000200000000",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_SINGLE_ACCOUNT_STAKE_DEREGISTRATION_CERTIFICATE_AND_WITHDRAWAL_ACCOUNT_MISMATCH[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000004901003901EB0BAA5E570CFFBE2934DB29DF0B6A3D7C0430EE65D4"
    "C3A7AB2FEFB91BC428E4720702EBD5DAB4FB175324C192DC9BB76CC5DA956E3C"
    "8DFF00000000000000010100000000000000000000002A000000000000000A00"
    "000000000003E822058000073C80000717800000010000000200000000",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_ORDINARY_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_MULTISIG_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_PLUTUS_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_CERT_POOL_RETIRE_NON_POOL_COLD_KEY_IN_ORDINARY_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000003B01002B82D818582183581C9E1C71DE652EC8B85FEC296F0685CA"
    "3988781C94A2E1A5D89D92F45FA0001A0D0C256100000000002DD2E800000000"
    "00000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_INVALID_CERT_POOL_REGISTRATION_WITH_NO_OWNERS[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000049010039017CB05FCE110FB999F01ABB4F62BC455E217D4A51FDE9"
    "09FA9AEA545443AC53C046CF6A42095E3C60310FA802771D0672F8FE2D186113"
    "8B0900000000000000010000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_POOL_ID_PATH_SENT_IN_FOR_POOL_REGISTRATION_OWNER_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000049010039017CB05FCE110FB999F01ABB4F62BC455E217D4A51FDE9"
    "09FA9AEA545443AC53C046CF6A42095E3C60310FA802771D0672F8FE2D186113"
    "8B0900000000000000010000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_POOL_ID_HASH_SENT_IN_FOR_POOL_REGISTRATION_OPERATOR_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000049010039017CB05FCE110FB999F01ABB4F62BC455E217D4A51FDE9"
    "09FA9AEA545443AC53C046CF6A42095E3C60310FA802771D0672F8FE2D186113"
    "8B0900000000000000010000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_POOL_OWNER_NON_STAKING_PATH_FOR_POOL_REGISTRATION_OWNER_TX[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000049010039017CB05FCE110FB999F01ABB4F62BC455E217D4A51FDE9"
    "09FA9AEA545443AC53C046CF6A42095E3C60310FA802771D0672F8FE2D186113"
    "8B0900000000000000010000000000000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_DATUM_HASH[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000069010039105E2F080EB93BAD86D401545E0CE5F2221096D6477E11"
    "E6643922FA8D2ED495234DC0D667C1316FF84E572310E265EDB31330448B36B7"
    "179E00000000006CA79301000001FFD4D009F554BA4FD8ED1F1D703244819861"
    "A9D34FD4753BCF3FF32F043CE18800000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_DATUM_INLINE[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000022B010039105E2F080EB93BAD86D401545E0CE5F2221096D6477E11"
    "E6643922FA8D2ED495234DC0D667C1316FF84E572310E265EDB31330448B36B7"
    "179E00000000006CA7930100000201E012B8240C5470B47C159597B6F71D78C7"
    "FC99D1D8D911CB19B8F50211938EF361A22D30CD8F6354EC50E99A7D3CF3E067"
    "97ED4AF3D358E01B2A957CAA4010DA328720B9FBE7A3A6D10209A13D2EB11933"
    "EB1BF2AB02713117E421B6DCC66297C41B95AD32D3457A0E6B44D8482385F311"
    "465964C3DAFF226ACFB7BBDA47011F1A",
        .p1 = 0x01,
        .more = true,
    },
    {
        .hex_payload =
    "6531DB30E5B5977143C48F8B8EB739487F87DC13896F58529CFB48E415FC6123"
    "E708CDC3CB15CC1900ECF88C5FC9FF66D8AD6DAE18C79E4A3C392A0DF4D16FFA"
    "3E370F4DAD8D8E9D171C5656BB317C78A2711057E7AE0BEB1DC66BA01AA69D0C"
    "0DB244E6742D7758CE8DA00DFED6225D4AED4B01C42A0352688ED5803F3FD648"
    "73F11355305D9DB309F4A2A6673CC408A06B8827A5EDEF7B0FD8742627FB8AA1"
    "02A084B7DB72FCB5C3D1BF437E2A936B738902A9C0258B462B9F2E9BEFD2C6BC"
    "FC036143BB34342B9124888A5B29FA5D60909C81319F034C11542B05CA3FF6C6"
    "4C7642FF1E2B25FB60DC9BB6F5C914DD",
        .p1 = 0x01,
        .more = true,
    },
    {
        .hex_payload =
    "4149F31896955D4D204D822DEDDC46F852115A479EDF7521CDF4CE5968058750"
    "11855158FD303C33A2A7916A9CB7ACAAF5AECA7E6EFB75960E9597CD845BD9A9"
    "3610BF1AB47AB0DE943E8A96E26A24C4996F7B07FAD437829FEE5BC349619260"
    "8D4C04AC642CDEC7BDBB8A948AD1D43400000000000000002A00000000000000"
    "0A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_REFERENCE_SCRIPT[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000005F010039105E2F080EB93BAD86D401545E0CE5F2221096D6477E11"
    "E6643922FA8D2ED495234DC0D667C1316FF84E572310E265EDB31330448B36B7"
    "179E00000000006CA79301000000020014DEADBEEFDEADBEEFDEADBEEFDEADBE"
    "EFDEADBEEF000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_DATUM_HASH[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "000000000069010039105E2F080EB93BAD86D401545E0CE5F2221096D6477E11"
    "E6643922FA8D2ED495234DC0D667C1316FF84E572310E265EDB31330448B36B7"
    "179E00000000006CA79301000001FFD4D009F554BA4FD8ED1F1D703244819861"
    "A9D34FD4753BCF3FF32F043CE18800000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_DATUM_INLINE[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000022B010039105E2F080EB93BAD86D401545E0CE5F2221096D6477E11"
    "E6643922FA8D2ED495234DC0D667C1316FF84E572310E265EDB31330448B36B7"
    "179E00000000006CA7930100000201E012B8240C5470B47C159597B6F71D78C7"
    "FC99D1D8D911CB19B8F50211938EF361A22D30CD8F6354EC50E99A7D3CF3E067"
    "97ED4AF3D358E01B2A957CAA4010DA328720B9FBE7A3A6D10209A13D2EB11933"
    "EB1BF2AB02713117E421B6DCC66297C41B95AD32D3457A0E6B44D8482385F311"
    "465964C3DAFF226ACFB7BBDA47011F1A",
        .p1 = 0x01,
        .more = true,
    },
    {
        .hex_payload =
    "6531DB30E5B5977143C48F8B8EB739487F87DC13896F58529CFB48E415FC6123"
    "E708CDC3CB15CC1900ECF88C5FC9FF66D8AD6DAE18C79E4A3C392A0DF4D16FFA"
    "3E370F4DAD8D8E9D171C5656BB317C78A2711057E7AE0BEB1DC66BA01AA69D0C"
    "0DB244E6742D7758CE8DA00DFED6225D4AED4B01C42A0352688ED5803F3FD648"
    "73F11355305D9DB309F4A2A6673CC408A06B8827A5EDEF7B0FD8742627FB8AA1"
    "02A084B7DB72FCB5C3D1BF437E2A936B738902A9C0258B462B9F2E9BEFD2C6BC"
    "FC036143BB34342B9124888A5B29FA5D60909C81319F034C11542B05CA3FF6C6"
    "4C7642FF1E2B25FB60DC9BB6F5C914DD",
        .p1 = 0x01,
        .more = true,
    },
    {
        .hex_payload =
    "4149F31896955D4D204D822DEDDC46F852115A479EDF7521CDF4CE5968058750"
    "11855158FD303C33A2A7916A9CB7ACAAF5AECA7E6EFB75960E9597CD845BD9A9"
    "3610BF1AB47AB0DE943E8A96E26A24C4996F7B07FAD437829FEE5BC349619260"
    "8D4C04AC642CDEC7BDBB8A948AD1D43400000000000000002A00000000000000"
    "0A",
        .p1 = 0x02,
        .more = false,
    },
};

static const apdu_segment_t SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_REFERENCE_SCRIPT[] = {
    {
        .hex_payload =
    "3B40265111D8BB3C3C608D95B3A0BF83461ACE32D79336579A1939B3AAD1C0B7"
    "00000000005F010039105E2F080EB93BAD86D401545E0CE5F2221096D6477E11"
    "E6643922FA8D2ED495234DC0D667C1316FF84E572310E265EDB31330448B36B7"
    "179E00000000006CA79301000000020014DEADBEEFDEADBEEFDEADBEEFDEADBE"
    "EFDEADBEEF000000000000002A000000000000000A",
        .p1 = 0x02,
        .more = false,
    },
};

static const sign_tx_reject_fixture_t SIGN_TX_REJECT_FIXTURES[] = {
    {
        .name = "[REJECT_INIT] Non-mainnet_protocol_magic",
        .init_hex =
        "0000000000000000012D964A0803000100010100000000010100000100000000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: enforce protocol magic validation in handler
        .skip_reason = "Protocol magic validation not enforced",
    },
    {
        .name = "[REJECT_INIT] Invalid_network_id",
        .init_hex =
        "0000000000000000102D964A0903000100010100000000010100000100000000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_INVALID_NETWORK_ID,
        .expect_init_failure = true,
        // TODO: enforce network ID validation in handler
        .skip_reason = "Network ID validation not enforced",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_too_few_certificates",
        .init_hex =
        "0000000000000000012D964A0905000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_too_few_certificates",
        .init_hex =
        "0000000000000000012D964A0904000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_too_many_certificates",
        .init_hex =
        "0000000000000000012D964A0905000100010200020000010100000100000000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_too_many_certificates",
        .init_hex =
        "0000000000000000012D964A0904000100010200020000010100000100000000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_too_many_withdrawals",
        .init_hex =
        "0000000000000000012D964A0905000100010200010001010100000100000000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_too_many_withdrawals",
        .init_hex =
        "0000000000000000012D964A0904000100010200010001010100000100000000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_mint_included",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100010100000000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_mint_included",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100010100000000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: pool registration signing mode not implemented yet
        .skip_reason = "Pool registration signing mode unsupported",
    },
    {
        .name = "[REJECT_INIT] Ordinary_tx_-_collateral_inputs_included",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100010000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral inputs not supported yet
        .skip_reason = "Collateral inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Multisig_tx_-_collateral_inputs_included",
        .init_hex =
        "0000000000000000012D964A0906000100010200000000010100000100010000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral inputs not supported yet
        .skip_reason = "Collateral inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_collateral_inputs_included",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100000100010000"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral inputs not supported yet
        .skip_reason = "Collateral inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_collateral_inputs_included",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100010000"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral inputs not supported yet
        .skip_reason = "Collateral inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_required_signers_included",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100000100000001"
        "0101010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: required signers not supported yet
        .skip_reason = "Required signers not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_required_signers_included",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000001"
        "0101010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: required signers not supported yet
        .skip_reason = "Required signers not supported",
    },
    {
        .name = "[REJECT_INIT] Ordinary_tx_-_collateral_output_included",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0102010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral outputs not supported yet
        .skip_reason = "Collateral outputs not supported",
    },
    {
        .name = "[REJECT_INIT] Multisig_tx_-_collateral_output_included",
        .init_hex =
        "0000000000000000012D964A0906000100010200000000010100000100000000"
        "0102010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral outputs not supported yet
        .skip_reason = "Collateral outputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_collateral_output_included",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100000100000000"
        "0102010000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral outputs not supported yet
        .skip_reason = "Collateral outputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_collateral_output_included",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000000"
        "0102010000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: collateral outputs not supported yet
        .skip_reason = "Collateral outputs not supported",
    },
    {
        .name = "[REJECT_INIT] Ordinary_tx_-_total_collateral_included",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0101020000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: total collateral not supported yet
        .skip_reason = "Total collateral not supported",
    },
    {
        .name = "[REJECT_INIT] Multisig_tx_-_total_collateral_included",
        .init_hex =
        "0000000000000000012D964A0906000100010200000000010100000100000000"
        "0101020000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: total collateral not supported yet
        .skip_reason = "Total collateral not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_total_collateral_included",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100000100000000"
        "0101020000000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: total collateral not supported yet
        .skip_reason = "Total collateral not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_total_collateral_included",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000000"
        "0101020000000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: total collateral not supported yet
        .skip_reason = "Total collateral not supported",
    },
    {
        .name = "[REJECT_INIT] Ordinary_tx_-_reference_inputs_included",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0101010001000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: reference inputs not supported yet
        .skip_reason = "Reference inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Multisig_tx_-_reference_inputs_included",
        .init_hex =
        "0000000000000000012D964A0906000100010200000000010100000100000000"
        "0101010001000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: reference inputs not supported yet
        .skip_reason = "Reference inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(operator)_-_reference_inputs_included",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100000100000000"
        "0101010001000001010001",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: reference inputs not supported yet
        .skip_reason = "Reference inputs not supported",
    },
    {
        .name = "[REJECT_INIT] Pool_registration_(owner)_-_reference_inputs_included",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000000"
        "0101010001000001010000",
        .chunks = NULL,
        .chunk_count = 0,
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = true,
        // TODO: reference inputs not supported yet
        .skip_reason = "Reference inputs not supported",
    },
    {
        .name = "[REJECT_ADDRESS] Reward_address_-_key",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_REWARD_ADDRESS_KEY,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_REWARD_ADDRESS_KEY),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_ADDRESS] Reward_address_-_script",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_REWARD_ADDRESS_SCRIPT,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_REWARD_ADDRESS_SCRIPT),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_ADDRESS] No_spending_path_-_Ordinary_Tx_1",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_NO_SPENDING_PATH_ORDINARY_TX_1,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_NO_SPENDING_PATH_ORDINARY_TX_1),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_ADDRESS] No_spending_path_-_Ordinary_Tx_2",
        .init_hex =
        "0000000000000000012D964A0903000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_NO_SPENDING_PATH_ORDINARY_TX_2,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_NO_SPENDING_PATH_ORDINARY_TX_2),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_ADDRESS] Pool_operator_-_spending_choice_not_path",
        .init_hex =
        "0000000000000000012D964A0905000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_POOL_OPERATOR_SPENDING_CHOICE_NOT_PATH,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_POOL_OPERATOR_SPENDING_CHOICE_NOT_PATH),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_ADDRESS] Multisig_-_unconditionally",
        .init_hex =
        "0000000000000000012D964A0906000100010200000000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_MULTISIG_UNCONDITIONALLY,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_MULTISIG_UNCONDITIONALLY),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_ADDRESS] Pool_owner_-_unconditionally",
        .init_hex =
        "0000000000000000012D964A0904000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_ADDRESS_POOL_OWNER_UNCONDITIONALLY,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_ADDRESS_POOL_OWNER_UNCONDITIONALLY),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Address parameter policy not implemented
        .skip_reason = "Address parameter policy not implemented",
    },
    {
        .name = "[REJECT_CERT_STAKING] Script_hash_in_Ordinary_Tx",
        .init_hex =
        "0000000000000000012D964A0903000100010200010000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_CERT_STAKING_SCRIPT_HASH_IN_ORDINARY_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_CERT_STAKING_SCRIPT_HASH_IN_ORDINARY_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Certificate staking policy not implemented
        .skip_reason = "Certificate staking policy not implemented",
    },
    {
        .name = "[REJECT_WITHDRAWAL] Script_hash_as_stake_credential_in_Ordinary_Tx",
        .init_hex =
        "0000000000000000012D964A0903000100010200000001010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_SCRIPT_HASH_AS_STAKE_CREDENTIAL_IN_ORDINARY_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_SCRIPT_HASH_AS_STAKE_CREDENTIAL_IN_ORDINARY_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Withdrawal policy not implemented
        .skip_reason = "Withdrawal policy not implemented",
    },
    {
        .name = "[REJECT_WITHDRAWAL] Non-staking_path_as_stake_credential_in_Ordinary_Tx",
        .init_hex =
        "0000000000000000012D964A0903000100010200000001010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_NON_STAKING_PATH_AS_STAKE_CREDENTIAL_IN_ORDINARY_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_NON_STAKING_PATH_AS_STAKE_CREDENTIAL_IN_ORDINARY_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Withdrawal policy not implemented
        .skip_reason = "Withdrawal policy not implemented",
    },
    {
        .name = "[REJECT_WITHDRAWAL] Staking_path_as_stake_credential_in_Multisig_Tx",
        .init_hex =
        "0000000000000000012D964A0906000100010200000001010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_STAKING_PATH_AS_STAKE_CREDENTIAL_IN_MULTISIG_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_WITHDRAWAL_STAKING_PATH_AS_STAKE_CREDENTIAL_IN_MULTISIG_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Withdrawal policy not implemented
        .skip_reason = "Withdrawal policy not implemented",
    },
    {
        .name = "[REJECT_SINGLE_ACCOUNT] Change_output_and_withdrawal_account_mismatch",
        .init_hex =
        "0000000000000000012D964A0903000100020200000001010100000100000000"
        "0101010000000001010002",
        .chunks = SIGN_TX_SEGMENTS_REJECT_SINGLE_ACCOUNT_CHANGE_OUTPUT_AND_WITHDRAWAL_ACCOUNT_MISMATCH,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_SINGLE_ACCOUNT_CHANGE_OUTPUT_AND_WITHDRAWAL_ACCOUNT_MISMATCH),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Single-account policy not implemented
        .skip_reason = "Single-account policy not implemented",
    },
    {
        .name = "[REJECT_SINGLE_ACCOUNT] Stake_deregistration_certificate_and_withdrawal_account_mismatch",
        .init_hex =
        "0000000000000000012D964A0903000100010200010001010100000100000000"
        "0101010000000001010003",
        .chunks = SIGN_TX_SEGMENTS_REJECT_SINGLE_ACCOUNT_STAKE_DEREGISTRATION_CERTIFICATE_AND_WITHDRAWAL_ACCOUNT_MISMATCH,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_SINGLE_ACCOUNT_STAKE_DEREGISTRATION_CERTIFICATE_AND_WITHDRAWAL_ACCOUNT_MISMATCH),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Single-account policy not implemented
        .skip_reason = "Single-account policy not implemented",
    },
    {
        .name = "[REJECT_CERT] Pool_registration_in_Ordinary_Tx",
        .init_hex =
        "0000000000000000012D964A0903000100010200010000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_ORDINARY_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_ORDINARY_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Certificate serialization not implemented
        .skip_reason = "Certificate serialization not implemented",
    },
    {
        .name = "[REJECT_CERT] Pool_registration_in_Multisig_Tx",
        .init_hex =
        "0000000000000000012D964A0906000100010200010000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_MULTISIG_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_MULTISIG_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Certificate serialization not implemented
        .skip_reason = "Certificate serialization not implemented",
    },
    {
        .name = "[REJECT_CERT] Pool_registration_in_Plutus_Tx",
        .init_hex =
        "0000000000000000012D964A0907000100010200010000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_PLUTUS_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_CERT_POOL_REGISTRATION_IN_PLUTUS_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Certificate serialization not implemented
        .skip_reason = "Certificate serialization not implemented",
    },
    {
        .name = "[REJECT_CERT_POOL_RETIRE] Non-pool_cold_key_in_Ordinary_Tx",
        .init_hex =
        "0000000000000000012D964A0903000100010200010000010100000100000000"
        "0101010000000001010001",
        .chunks = SIGN_TX_SEGMENTS_REJECT_CERT_POOL_RETIRE_NON_POOL_COLD_KEY_IN_ORDINARY_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_CERT_POOL_RETIRE_NON_POOL_COLD_KEY_IN_ORDINARY_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Certificate serialization not implemented
        .skip_reason = "Certificate serialization not implemented",
    },
    {
        .name = "[REJECT_INVALID_CERT] pool_registration_with_no_owners",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_INVALID_CERT_POOL_REGISTRATION_WITH_NO_OWNERS,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_INVALID_CERT_POOL_REGISTRATION_WITH_NO_OWNERS),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Certificate serialization not implemented
        .skip_reason = "Certificate serialization not implemented",
    },
    {
        .name = "[REJECT_POOL_ID] Path_sent_in_for_Pool_Registration_Owner_Tx",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_POOL_ID_PATH_SENT_IN_FOR_POOL_REGISTRATION_OWNER_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_POOL_ID_PATH_SENT_IN_FOR_POOL_REGISTRATION_OWNER_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Pool registration parsing not implemented
        .skip_reason = "Pool registration parsing not implemented",
    },
    {
        .name = "[REJECT_POOL_ID] Hash_sent_in_for_Pool_Registration_Operator_Tx",
        .init_hex =
        "0000000000000000012D964A0905000100010200010000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_POOL_ID_HASH_SENT_IN_FOR_POOL_REGISTRATION_OPERATOR_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_POOL_ID_HASH_SENT_IN_FOR_POOL_REGISTRATION_OPERATOR_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Pool registration parsing not implemented
        .skip_reason = "Pool registration parsing not implemented",
    },
    {
        .name = "[REJECT_POOL_OWNER] Non-staking_path_for_Pool_Registration_Owner_Tx",
        .init_hex =
        "0000000000000000012D964A0904000100010200010000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_POOL_OWNER_NON_STAKING_PATH_FOR_POOL_REGISTRATION_OWNER_TX,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_POOL_OWNER_NON_STAKING_PATH_FOR_POOL_REGISTRATION_OWNER_TX),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Pool registration parsing not implemented
        .skip_reason = "Pool registration parsing not implemented",
    },
    {
        .name = "[REJECT_OUTPUT] Pool_operator_-_datum_hash",
        .init_hex =
        "0000000000000000012D964A0905000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_DATUM_HASH,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_DATUM_HASH),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Output policy not implemented
        .skip_reason = "Output policy not implemented",
    },
    {
        .name = "[REJECT_OUTPUT] Pool_operator_-_datum_inline",
        .init_hex =
        "0000000000000000012D964A0905000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_DATUM_INLINE,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_DATUM_INLINE),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Output policy not implemented
        .skip_reason = "Output policy not implemented",
    },
    {
        .name = "[REJECT_OUTPUT] Pool_operator_-_reference_script",
        .init_hex =
        "0000000000000000012D964A0905000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_REFERENCE_SCRIPT,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OPERATOR_REFERENCE_SCRIPT),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Output policy not implemented
        .skip_reason = "Output policy not implemented",
    },
    {
        .name = "[REJECT_OUTPUT] Pool_owner_-_datum_hash",
        .init_hex =
        "0000000000000000012D964A0904000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_DATUM_HASH,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_DATUM_HASH),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Output policy not implemented
        .skip_reason = "Output policy not implemented",
    },
    {
        .name = "[REJECT_OUTPUT] Pool_owner_-_datum_inline",
        .init_hex =
        "0000000000000000012D964A0904000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_DATUM_INLINE,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_DATUM_INLINE),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Output policy not implemented
        .skip_reason = "Output policy not implemented",
    },
    {
        .name = "[REJECT_OUTPUT] Pool_owner_-_reference_script",
        .init_hex =
        "0000000000000000012D964A0904000100010200000000010100000100000000"
        "0101010000000001010000",
        .chunks = SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_REFERENCE_SCRIPT,
        .chunk_count = ARRAY_LEN(SIGN_TX_SEGMENTS_REJECT_OUTPUT_POOL_OWNER_REFERENCE_SCRIPT),
        .expected_sw = SWO_SECURITY_CONDITION_NOT_SATISFIED,
        .expect_init_failure = false,
        // TODO: Output policy not implemented
        .skip_reason = "Output policy not implemented",
    },
};

// ----------------------------------------------------------------------
// Fixture runner
// ----------------------------------------------------------------------

static void run_sign_tx_reject_fixture(const sign_tx_reject_fixture_t *fixture) {
    reset_context();
    assert_true(app_mem_init());

    uint8_t init_raw[512];
    size_t init_len = hex_to_bytes(fixture->init_hex, init_raw, sizeof(init_raw));
    buffer_t init_buf = {
        .ptr = init_raw,
        .size = init_len,
        .offset = 0,
    };

    g_last_sw = 0;
    int init_rc = handler_sign_tx(&init_buf, P1_TX_INIT, false);

    if (fixture->expect_init_failure) {
        assert_int_equal(init_rc, 0);
        assert_int_equal(g_last_sw, fixture->expected_sw);
        assert_int_equal(G_context.req_type, REQUEST_NONE);
        tx_context_cleanup();
        return;
    }

    assert_int_equal(init_rc, 0);
    assert_int_equal(g_last_sw, SWO_SUCCESS);
    assert_int_equal(G_context.req_type, REQUEST_SIGN_TRANSACTION);
    assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);

    bool failure_seen = false;
    for (size_t i = 0; i < fixture->chunk_count; i++) {
        const apdu_segment_t *segment = &fixture->chunks[i];
        uint8_t chunk_raw[512];
        size_t chunk_len = hex_to_bytes(segment->hex_payload, chunk_raw, sizeof(chunk_raw));
        buffer_t chunk_buf = {
            .ptr = chunk_raw,
            .size = chunk_len,
            .offset = 0,
        };
        g_last_sw = 0;
        int chunk_rc = handler_sign_tx(&chunk_buf, segment->p1, segment->more);
        if (g_last_sw != 0) {
            assert_int_equal(g_last_sw, fixture->expected_sw);
            assert_int_equal(chunk_rc, 0);
            failure_seen = true;
            break;
        }
        assert_int_equal(chunk_rc, SWO_SUCCESS);
    }

    assert_true(failure_seen);
    assert_int_equal(G_context.req_type, REQUEST_NONE);
    tx_context_cleanup();
}

static void test_sign_tx_reject_fixture(void **state) {
    const sign_tx_reject_fixture_t *fixture = (const sign_tx_reject_fixture_t *) *state;
    assert_non_null(fixture);
    if (fixture->skip_reason != NULL) {
        print_message("[SKIP] %s: %s\n", fixture->name, fixture->skip_reason);
        skip();
        return;
    }
    run_sign_tx_reject_fixture(fixture);
}

int main(void) {
    const size_t test_count = ARRAY_LEN(SIGN_TX_REJECT_FIXTURES);
    struct CMUnitTest tests[ARRAY_LEN(SIGN_TX_REJECT_FIXTURES)];
    size_t skipped_count = 0;

    for (size_t i = 0; i < test_count; i++) {
        tests[i] = (struct CMUnitTest) {
            .name = SIGN_TX_REJECT_FIXTURES[i].name,
            .test_func = test_sign_tx_reject_fixture,
            .initial_state = (void *) &SIGN_TX_REJECT_FIXTURES[i],
        };
        if (SIGN_TX_REJECT_FIXTURES[i].skip_reason != NULL) {
            skipped_count++;
        }
    }

    int result = cmocka_run_group_tests(tests, NULL, NULL);

    if (skipped_count > 0) {
        print_message("\n");
        print_message("================================================================================\n");
        print_message("WARNING: %zu/%zu TESTS WERE SKIPPED!\n", skipped_count, test_count);
        print_message("================================================================================\n");
        print_message("\nThese tests are not yet implemented. See skip_reason in test fixtures.\n");
        print_message("Test coverage is incomplete until all skipped tests are enabled.\n");
        print_message("================================================================================\n");
        print_message("\n");
        // Return non-zero to make test harness visible of skipped tests
        return 1;
    }

    return result;
}
