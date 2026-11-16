#include "ui_text.h"
#include <string.h>

static const char *const texts[UI_LANG_COUNT][TXT_COUNT] = {
    {
#define UI_TEXT(id, pt, en) pt,
#include "text/core.def"
#include "text/catalog.def"
#include "text/mcu.def"
#include "text/render.def"
#include "text/app.def"
#include "text/engine.def"
#undef UI_TEXT
    },
    {
#define UI_TEXT(id, pt, en) en,
#include "text/core.def"
#include "text/catalog.def"
#include "text/mcu.def"
#include "text/render.def"
#include "text/app.def"
#include "text/engine.def"
#undef UI_TEXT
    },
};

static const char *const names[TXT_COUNT] = {
#define UI_TEXT(id, pt, en) #id,
#include "text/core.def"
#include "text/catalog.def"
#include "text/mcu.def"
#include "text/render.def"
#include "text/app.def"
#include "text/engine.def"
#undef UI_TEXT
};

static const char *const codes[UI_LANG_COUNT] = {"pt-BR", "en-US"};
static UiLanguage current = UI_LANG_PT_BR;

static int valid_language(UiLanguage language)
{
    return language >= 0 && language < UI_LANG_COUNT;
}

const char *tr_in(UiLanguage language, TextId id)
{
    if (!valid_language(language) || id < 0 || id >= TXT_COUNT)
        return "";
    return texts[language][id];
}

const char *tr(TextId id)
{
    return tr_in(current, id);
}

const char *ui_text_id_name(TextId id)
{
    return id >= 0 && id < TXT_COUNT ? names[id] : "";
}

void ui_text_set_language(UiLanguage language)
{
    if (valid_language(language))
        current = language;
}

UiLanguage ui_text_language(void)
{
    return current;
}

const char *ui_text_language_code(UiLanguage language)
{
    return valid_language(language) ? codes[language] : "";
}

UiLanguage ui_text_language_for_locale(const char *language_code)
{
    return language_code && !strncmp(language_code, "pt", 2) ? UI_LANG_PT_BR : UI_LANG_EN_US;
}
