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

#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "../globals.h"
#include "processing.h"

// Screen: Processing...
UX_STEP_NOCB(ux_processing_flow_1_step, pb, {&C_icon_processing, "Processing"});

// FLOW when processing APDUs
UX_FLOW(ux_processing_flow, &ux_processing_flow_1_step);

void ui_processing() {
    ux_flow_init(0, ux_processing_flow, NULL);
}
