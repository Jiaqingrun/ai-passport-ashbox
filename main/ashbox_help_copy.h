#pragma once

/** Help list (怎么玩). Count matches the compiled string table. */
#define ASHBOX_HELP_COUNT 94
/** Visible wrapped rows in the help viewport (taller rows; must fit LIST_LAYER_H=228). */
#define ASHBOX_HELP_VIEW 3
/** Help row card height (px); fits ~4 wrapped lines of ashbox_font_16. */
#define ASHBOX_HELP_ROW_H 68
/** Vertical pitch between help rows (px). 8 + VIEW*STEP <= 228. */
#define ASHBOX_HELP_ROW_STEP 70
/** Label width inside a help row (px). */
#define ASHBOX_HELP_LABEL_W 196

/** Player-facing help line i (0 .. ASHBOX_HELP_COUNT-1); empty if out of range. */
const char *ashbox_help_line(unsigned i);
