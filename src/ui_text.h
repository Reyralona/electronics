#ifndef UI_TEXT_H
#define UI_TEXT_H
/* Every user-visible text, in all interface languages. Pure module: no SDL/OpenGL.
 * Texts live in src/text/<area>.def as UI_TEXT(ID, "pt-BR", "en-US"); style: docs/TEXTOS.md.
 * Languages are append-only: their index is saved in preferences. */
typedef enum
{
    UI_LANG_PT_BR,
    UI_LANG_EN_US,
    UI_LANG_COUNT
} UiLanguage;

typedef enum
{
#define UI_TEXT(id, pt, en) TXT_##id,
#include "text/core.def"
#include "text/catalog.def"
#include "text/mcu.def"
#include "text/render.def"
#include "text/app.def"
#include "text/engine.def"
#include "text/chips.def"
#undef UI_TEXT
    TXT_COUNT
} TextId;

/* Text in the current language; never NULL. */
const char *tr(TextId id);
/* Text in a given language (tests and language names). */
const char *tr_in(UiLanguage language, TextId id);
/* Symbolic name of a text ("LIBRARY_FULL"), for test messages. */
const char *ui_text_id_name(TextId id);
void ui_text_set_language(UiLanguage language);
UiLanguage ui_text_language(void);
/* "pt-BR" / "en-US". */
const char *ui_text_language_code(UiLanguage language);
/* Language for a locale's language code ("pt", "en"...): Portuguese for "pt", English otherwise. */
UiLanguage ui_text_language_for_locale(const char *language_code);
#endif
