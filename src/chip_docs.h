#ifndef CHIP_DOCS_H
#define CHIP_DOCS_H
#include "workbench.h"
#include "ui_text.h"
/* Reference sheet of an IC for the info (i) window: what it does and what each pin does, from its
 * datasheet. Texts live in src/text/chips.def. Pure data: no SDL/OpenGL. */
#define CHIP_DOC_MAX_PINS 28

typedef struct
{
    PartType type;
    TextId summary;                   /* function and key behaviour, a few short sentences */
    TextId pins[CHIP_DOC_MAX_PINS];   /* datasheet pin n is pins[n - 1] */
} ChipDoc;

/* NULL when the part has no sheet. */
const ChipDoc *chip_doc(PartType type);
#endif
