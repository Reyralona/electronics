/* Checks every interface text against the rules in docs/TEXTOS.md, in both languages. */
#include "ui_text.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { MAX_SENTENCE_WORDS = 20, WORD_CAPACITY = 64, SPEC_CAPACITY = 256 };

static int failures;

static void fail(TextId id, UiLanguage language, const char *why)
{
    fprintf(stderr, "TXT_%s [%s]: %s\n    \"%s\"\n", ui_text_id_name(id), ui_text_language_code(language), why,
            tr_in(language, id));
    ++failures;
}

/* Printf conversions in order ("%d%.3gs" style), so both languages take the same arguments. */
static void specifiers(const char *text, char *out, size_t capacity)
{
    size_t used = 0;
    out[0] = 0;
    for (const char *p = text; *p; ++p)
    {
        if (*p != '%')
            continue;
        if (p[1] == '%')
        {
            ++p;
            continue;
        }
        const char *end = p + 1;
        while (*end && !isalpha((unsigned char)*end))
            ++end;
        while (*end == 'l' || *end == 'h' || *end == 'z')
            ++end;
        size_t length = (size_t)(end - p) + (*end ? 1 : 0);
        if (used + length + 1 >= capacity)
            return;
        memcpy(out + used, p, length);
        used += length;
        out[used] = 0;
        p = end;
    }
}

static int valid_utf8(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    while (*p)
    {
        int extra = *p < 0x80 ? 0 : (*p & 0xe0) == 0xc0 ? 1 : (*p & 0xf0) == 0xe0 ? 2 : (*p & 0xf8) == 0xf0 ? 3 : -1;
        if (extra < 0)
            return 0;
        ++p;
        for (int i = 0; i < extra; ++i, ++p)
            if ((*p & 0xc0) != 0x80)
                return 0;
    }
    return 1;
}

static int word_byte(unsigned char c)
{
    return isalpha(c) || c >= 0x80;
}

/* Calls visit(word) for every lowercase word; returns the longest sentence length in words. */
static int scan_words(const char *text, void (*visit)(const char *word, void *context), void *context)
{
    int longest = 0, sentence = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p)
    {
        if (!word_byte(*p))
        {
            if (*p == '.' || *p == '?' || *p == '!' || *p == ';' || *p == ':')
                sentence = 0;
            if (*p == 0xc2 && p[1] == 0xb7) /* " · " separates hints. */
                sentence = 0;
            ++p;
            continue;
        }
        char word[WORD_CAPACITY];
        size_t length = 0;
        while (word_byte(*p))
        {
            if (length + 1 < sizeof(word))
                word[length++] = (char)tolower(*p);
            ++p;
        }
        word[length] = 0;
        if (*p == 0xc2 && p[1] == 0xb7)
            sentence = 0;
        visit(word, context);
        if (++sentence > longest)
            longest = sentence;
    }
    return longest;
}

static const char *const forbidden[] = {
    "netlist", "snapshot", "runtime", "worker", "host", "buffer", "topologia", "topology", "transação",
    "transaction", "sucesso", "successfully", "please", "componente", "componentes", "component", "components",
    "joelho", "joelhos", "knee", "knees", "bloco", "blocos", "block", "blocks"};

/* Portuguese words written without their accents. */
static const char *const unaccented[] = {
    "nao", "possivel", "invalido", "invalida", "configuracao", "configuracoes", "funcao", "botao", "padrao",
    "padroes", "voce", "sera", "tambem", "conexao", "alimentacao", "simulacao", "calculo", "grafico", "tensao",
    "duracao", "posicao", "selecao", "opcao", "numero", "unico", "maximo", "minimo", "codigo", "memoria", "valido"};

typedef struct
{
    TextId id;
    UiLanguage language;
} WordCheck;

static int listed(const char *word, const char *const *list, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(word, list[i]))
            return 1;
    return 0;
}

static void check_word(const char *word, void *context)
{
    const WordCheck *check = context;
    char why[128];
    if (listed(word, forbidden, sizeof(forbidden) / sizeof(forbidden[0])))
    {
        snprintf(why, sizeof(why), "forbidden word \"%s\" (docs/TEXTOS.md)", word);
        fail(check->id, check->language, why);
    }
    if (check->language == UI_LANG_PT_BR && listed(word, unaccented, sizeof(unaccented) / sizeof(unaccented[0])))
    {
        snprintf(why, sizeof(why), "missing accents in \"%s\"", word);
        fail(check->id, check->language, why);
    }
}

static void check_text(TextId id, UiLanguage language)
{
    const char *text = tr_in(language, id);
    size_t length = strlen(text);
    if (!length)
    {
        fail(id, language, "empty");
        return;
    }
    if (!valid_utf8(text))
        fail(id, language, "invalid UTF-8");
    if (isspace((unsigned char)text[0]) || isspace((unsigned char)text[length - 1]))
        fail(id, language, "leading or trailing space");
    if (strstr(text, "  "))
        fail(id, language, "double space");
    WordCheck check = {id, language};
    if (scan_words(text, check_word, &check) > MAX_SENTENCE_WORDS)
        fail(id, language, "sentence longer than 20 words");
}

static void check_languages_match(TextId id)
{
    char first[SPEC_CAPACITY], other[SPEC_CAPACITY];
    specifiers(tr_in(UI_LANG_PT_BR, id), first, sizeof(first));
    for (int language = 1; language < UI_LANG_COUNT; ++language)
    {
        specifiers(tr_in((UiLanguage)language, id), other, sizeof(other));
        if (strcmp(first, other))
            fail(id, (UiLanguage)language, "printf specifiers differ from pt-BR");
    }
}

static void check_switching(void)
{
    assert(ui_text_language() == UI_LANG_PT_BR);
    assert(!strcmp(tr(TXT_SETTINGS_TITLE), "Configurações"));
    ui_text_set_language(UI_LANG_EN_US);
    assert(!strcmp(tr(TXT_SETTINGS_TITLE), "Settings"));
    ui_text_set_language((UiLanguage)UI_LANG_COUNT);
    assert(ui_text_language() == UI_LANG_EN_US);
    ui_text_set_language(UI_LANG_PT_BR);
    assert(ui_text_language_for_locale("pt") == UI_LANG_PT_BR);
    assert(ui_text_language_for_locale("en") == UI_LANG_EN_US);
    assert(ui_text_language_for_locale("de") == UI_LANG_EN_US);
    assert(ui_text_language_for_locale(NULL) == UI_LANG_EN_US);
    assert(!strcmp(tr((TextId)TXT_COUNT), ""));
}

int main(void)
{
    check_switching();
    for (int id = 0; id < TXT_COUNT; ++id)
    {
        for (int language = 0; language < UI_LANG_COUNT; ++language)
            check_text((TextId)id, (UiLanguage)language);
        check_languages_match((TextId)id);
    }
    if (failures)
    {
        fprintf(stderr, "%d interface text problem(s).\n", failures);
        return 1;
    }
    printf("Interface texts: %d ids in %d languages follow docs/TEXTOS.md.\n", TXT_COUNT, UI_LANG_COUNT);
    return 0;
}
