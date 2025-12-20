#include "display.h"

extern void ui_clear_prepared_warning(void);
#include "ui/ui_utils.h"

void tx_review_cleanup(void) {
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();
    ui_clear_prepared_warning();
}
