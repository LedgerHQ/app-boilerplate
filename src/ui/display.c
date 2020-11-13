/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#pragma GCC diagnostic ignored "-Wformat-invalid-specifier"  // snprintf
#pragma GCC diagnostic ignored "-Wformat-extra-args"         // snprintf

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "display.h"
#include "action/validate.h"
#include "../context.h"
#include "../io.h"
#include "../sw.h"
#include "../common/bip32.h"

static strbuf_t g_buf, g_buf2;
static action_validate_cb g_validate_callback;

// Step with icon
UX_STEP_NOCB(ux_display_confirm_addr_step, pn, {&C_icon_eye, "Confirm Address"});
// Step to display title/text using buffer #1
UX_STEP_NOCB(ux_display_title_text_step,
             bnnn_paging,
             {
                 .title = g_buf.title,
                 .text = g_buf.text,
             });
// Step to display title/text using buffer #2
UX_STEP_NOCB(ux_display_title_text2_step,
             bnnn_paging,
             {
                 .title = g_buf2.title,
                 .text = g_buf2.text,
             });
// Step with approve button
UX_STEP_CB(ux_display_approve_step,
           pb,
           (*g_validate_callback)(true),
           {
               &C_icon_validate_14,
               "Approve",
           });
// Step with reject button
UX_STEP_CB(ux_display_reject_step,
           pb,
           (*g_validate_callback)(false),
           {
               &C_icon_crossmark,
               "Reject",
           });

// FLOW to display address UI:
// #1 screen: eye icon + "Confirm Address"
// #2 screen: display BIP32 Path
// #3 screen: display public key
// #4 screen: approve button
// #5 screen: reject button
UX_FLOW(ux_display_pubkey_flow,
        &ux_display_confirm_addr_step,
        &ux_display_title_text_step,
        &ux_display_title_text2_step,
        &ux_display_approve_step,
        &ux_display_reject_step);

void ui_display_public_key() {
    memset(&g_buf, 0, sizeof(g_buf));
    const char path[] = "Path";
    snprintf(g_buf.title, sizeof(g_buf.title), "%.*s", sizeof(path), path);
    bip32_path_to_str(pk_ctx.bip32_path, pk_ctx.bip32_path_len, g_buf.text, sizeof(g_buf.text));
    const char address[] = "Address";
    snprintf(g_buf2.title, sizeof(g_buf2.title), "%.*s", sizeof(address), address);
    snprintf(g_buf2.text,
             sizeof(g_buf2.text),
             "%.*H",
             sizeof(pk_ctx.public_key.W),
             pk_ctx.public_key.W);

    g_validate_callback = &ui_action_validate_pubkey;

    ux_flow_init(0, ux_display_pubkey_flow, NULL);
}

// Step to display amount
UX_STEP_NOCB(ux_display_review_step,
             pnn,
             {
                 &C_icon_eye,
                 "Review",
                 "Transaction",
             });

// FLOW to display amount info:
// #1 screen : eye icon + "Review Transaction"
// #2 screen : display amount
// #3 screen : approve button
// #4 scree : reject button
UX_FLOW(ux_display_amount_flow,
        &ux_display_review_step,
        &ux_display_title_text_step,
        &ux_display_approve_step,
        &ux_display_reject_step);

void ui_display_amount() {
    memset(&g_buf, 0, sizeof(g_buf));

    const char title[] = "Amount";
    snprintf(g_buf.title, sizeof(g_buf.title), "%.*s", sizeof(title), title);
    const char amount[] = "233.333 XYZ";
    snprintf(g_buf.text, sizeof(g_buf.text), "%.*s", sizeof(amount), amount);

    g_validate_callback = &ui_action_validate_amount;

    ux_flow_init(0, ux_display_amount_flow, NULL);
}
