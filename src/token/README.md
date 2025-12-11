# Token Support in Boilerplate App

This directory contains the token infrastructure for the Boilerplate application, demonstrating three levels of token support.

## Token Support Levels

### 1. Hardcoded Tokens (Simple - Use Freely)

**Location:** `token_db.c` / `token_db.h`

**Use case:** You have a fixed set of tokens to support

**Implementation:**

- Add token metadata directly in `TOKENS` array
- No coordination with Ledger needed
- Works for both send and swap operations
- Recommended for most third-party apps

**Example:**

```c
{
    .address = "0123456789abcdef...",  // 32-byte hex token address
    .ticker = "USDC",
    .decimals = 12
}
```

**Limitations:**

- Requires app update to add new tokens
- For SWAP support, token must be added to CAL database (contact Ledger - quick process)

### 2. Dynamic Tokens (Complex - Requires Ledger Coordination)

**Location:** `dynamic_token_info.c` / `dynamic_token_info.h` / `handler/provide_token_info.c`

**Use case:** Tokens can be added without app updates via signed descriptors

**Requirements:**

- **Mandatory coordination with Ledger teams**
- PKI certificate management (Ledger-controlled)
- CAL descriptor signing infrastructure
- Security review and approval

**Implementation:**

- Device receives TLV-encoded signed descriptor via `PROVIDE_TOKEN_INFO` APDU
- Signature validated against PKI certificates
- Token metadata stored in RAM (overrides hardcoded DB)
- Persists across commands until app exit

**When to use:**

- Your chain has thousands of tokens
- Tokens are frequently added/updated
- You've coordinated with Ledger's security team

**Do NOT use if:**

- You have very few tokens (use hardcoded instead)
- You want a quick integration (use hardcoded instead)
- You haven't contacted Ledger yet (contact first!)

## Token Lookup Priority

```console
1. Dynamic token (CAL) - if provided and address matches
2. Hardcoded database - built-in tokens
3. Unknown - transaction fails (a different behavior could have been coded)
```

## Dynamic Token Slots

The Boilerplate application handles only one slot of dynamic token descriptor.
This is a design choice to keep the implementation minimalist and focused on the key parts (TLV, PKI, SDK API).

## Testing

- **Hardcoded tokens:** You simply need to craft and send token transaction relevant to your hardcoded database.
- **Dynamic tokens:** More complex to test, a fake CAL must be mocked and whitelisted on the device PKI
  by sending a test certificate. WILL NOT work on a real device, the test certificate will be rejected.
- **SWAP tokens:** Simply a token test done through the ExchangeTestRunner to emulate start from the Exchange application.

## Security Notes

- Always validate token addresses in your app
- Hardcoded tokens are reviewed in your app's code review
- Dynamic tokens require PKI signature validation (handled by SDK)
- The fake CAL mock cannot be onboarded on a real device because the associated certificate only has test permissions

## Questions?

- Hardcoded tokens: Fork and implement (no questions needed)
- Dynamic tokens: Contact Ledger developer support
- SWAP integration: Contact Ledger developer support
