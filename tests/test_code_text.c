#include "code_text.h"
#include "mcu_lang.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int named(const char *name)
{
    for (int i = 0; i < mcu_completion_count(); ++i)
        if (!strcmp(mcu_completion(i)->name, name))
            return i;
    return -1;
}

static void editing(void)
{
    char text[64] = "void loop() {";
    int cursor = (int)strlen(text);
    assert(code_newline(text, sizeof(text), &cursor) && !strcmp(text, "void loop() {\n    "));
    assert(code_close_brace(text, sizeof(text), &cursor) && !strcmp(text, "void loop() {\n}"));
    cursor = 13;
    assert(code_newline(text, sizeof(text), &cursor));
    assert(code_insert(text, sizeof(text), &cursor, "x"));
    assert(!strcmp(text, "void loop() {\n    x\n}") && code_line_count(text) == 3);
    assert(code_line_index(text, cursor) == 1 && code_column(text, cursor) == 5);
    assert(code_position(text, 2, 9) == (int)strlen(text) && code_position(text, 0, 4) == 4);
    code_backspace(text, &cursor);
    code_backspace(text, &cursor);
    assert(!strcmp(text, "void loop() {\n\n}") && cursor == 14);
    char small[8] = "abc";
    cursor = 3;
    assert(!code_insert(small, sizeof(small), &cursor, "defghij") && !strcmp(small, "abc") && cursor == 3);
    char accent[16] = "a\xc3\xa7z";
    cursor = 3;
    assert(code_column(accent, cursor) == 2);
    code_backspace(accent, &cursor);
    assert(!strcmp(accent, "az") && cursor == 1);
}

static void completion(void)
{
    int matches[CODE_COMPLETION_MAX];
    char text[128] = "  digitalW";
    int cursor = (int)strlen(text);
    int count = code_completions(text, cursor, matches, CODE_COMPLETION_MAX);
    assert(count == 1 && matches[0] == named("digitalWrite"));
    assert(code_accept(text, sizeof(text), &cursor, matches[0]));
    assert(!strcmp(text, "  digitalWrite()") && text[cursor] == ')');
    strcpy(text, "Serial.pr");
    cursor = (int)strlen(text);
    assert(code_completions(text, cursor, matches, CODE_COMPLETION_MAX) == 2);
    strcpy(text, "tela.setC");
    cursor = (int)strlen(text);
    assert(code_completions(text, cursor, matches, CODE_COMPLETION_MAX) == 1);
    assert(code_accept(text, sizeof(text), &cursor, matches[0]) && !strcmp(text, "tela.setCursor()"));
    strcpy(text, "lcd.clear");
    cursor = (int)strlen(text);
    assert(code_completions(text, cursor, matches, CODE_COMPLETION_MAX) == 0);
    strcpy(text, "millis");
    cursor = 3;
    assert(code_completions(text, cursor, matches, CODE_COMPLETION_MAX) == 1);
    assert(code_accept(text, sizeof(text), &cursor, matches[0]) && !strcmp(text, "millis()lis"));
    strcpy(text, "x = 12");
    cursor = (int)strlen(text);
    assert(code_completions(text, cursor, matches, CODE_COMPLETION_MAX) == 0);
}

int main(void)
{
    editing();
    completion();
    puts("Code editor text: indentation, UTF-8 erase, capacity and completion passed.");
    return 0;
}
