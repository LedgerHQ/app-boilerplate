#pragma once

#include "nbgl_use_case.h"

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;

void ui_all_cleanup(void);

bool ui_pairs_init(uint8_t nbPairs);
void ui_pairs_cleanup(void);
