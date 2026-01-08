#include "display.h"
#include "ui/ui_utils.h"
#include "ui/ui_warnings.h"

void tx_review_cleanup(void) {
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();
    ui_clear_warnings();
}
