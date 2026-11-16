#include "code_text.h"
#include "mcu_lang.h"
#include <ctype.h>
#include <string.h>

static int continuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

int code_next(const char *text, int position)
{
    if (!text[position])
        return position;
    ++position;
    while (continuation((unsigned char)text[position]))
        ++position;
    return position;
}

int code_previous(const char *text, int position)
{
    if (position <= 0)
        return 0;
    --position;
    while (position > 0 && continuation((unsigned char)text[position]))
        --position;
    return position;
}

int code_insert(char *text, size_t capacity, int *cursor, const char *insert)
{
    size_t length = strlen(text), added = strlen(insert);
    if (*cursor < 0 || (size_t)*cursor > length || length + added + 1 > capacity)
        return 0;
    memmove(text + *cursor + added, text + *cursor, length - (size_t)*cursor + 1);
    memcpy(text + *cursor, insert, added);
    *cursor += (int)added;
    return 1;
}

static void erase_range(char *text, int from, int to)
{
    memmove(text + from, text + to, strlen(text + to) + 1);
}

void code_backspace(char *text, int *cursor)
{
    if (*cursor <= 0)
        return;
    int start = code_line_start(text, *cursor), column_bytes = *cursor - start;
    int blank = column_bytes > 0 && (int)strspn(text + start, " ") >= column_bytes;
    int from = code_previous(text, *cursor);
    if (blank) /* inside leading spaces: remove back to the previous indent stop */
        from = *cursor - ((column_bytes - 1) % CODE_INDENT + 1);
    erase_range(text, from, *cursor);
    *cursor = from;
}

void code_delete(char *text, int cursor)
{
    if (text[cursor])
        erase_range(text, cursor, code_next(text, cursor));
}

int code_line_start(const char *text, int position)
{
    while (position > 0 && text[position - 1] != '\n')
        --position;
    return position;
}

int code_line_end(const char *text, int position)
{
    while (text[position] && text[position] != '\n')
        ++position;
    return position;
}

int code_line_index(const char *text, int position)
{
    int line = 0;
    for (int i = 0; i < position && text[i]; ++i)
        line += text[i] == '\n';
    return line;
}

int code_line_count(const char *text)
{
    return code_line_index(text, (int)strlen(text)) + 1;
}

int code_line_offset(const char *text, int line)
{
    int position = 0;
    while (line > 0 && text[position])
    {
        if (text[position] == '\n')
            --line;
        ++position;
    }
    return position;
}

int code_column(const char *text, int position)
{
    int column = 0;
    for (int i = code_line_start(text, position); i < position; i = code_next(text, i))
        ++column;
    return column;
}

int code_position(const char *text, int line, int column)
{
    int position = code_line_offset(text, line < 0 ? 0 : line);
    while (column-- > 0 && text[position] && text[position] != '\n')
        position = code_next(text, position);
    return position;
}

static int line_indent(const char *text, int position)
{
    int start = code_line_start(text, position), indent = 0;
    while (text[start + indent] == ' ')
        ++indent;
    return indent;
}

int code_newline(char *text, size_t capacity, int *cursor)
{
    char insert[2 + 64] = "\n";
    int indent = line_indent(text, *cursor), before = *cursor;
    while (before > code_line_start(text, *cursor) && text[before - 1] == ' ')
        --before;
    if (before > 0 && text[before - 1] == '{')
        indent += CODE_INDENT;
    if (indent > 60)
        indent = 60;
    memset(insert + 1, ' ', (size_t)indent);
    insert[indent + 1] = 0;
    return code_insert(text, capacity, cursor, insert);
}

int code_close_brace(char *text, size_t capacity, int *cursor)
{
    int start = code_line_start(text, *cursor), column_bytes = *cursor - start;
    if (column_bytes >= CODE_INDENT && (int)strspn(text + start, " ") >= column_bytes)
    {
        erase_range(text, *cursor - CODE_INDENT, *cursor);
        *cursor -= CODE_INDENT;
    }
    return code_insert(text, capacity, cursor, "}");
}

static int word_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '.';
}

int code_word_start(const char *text, int cursor)
{
    while (cursor > 0 && word_char(text[cursor - 1]))
        --cursor;
    return cursor;
}

static int prefix_matches(const char *candidate, const char *typed, size_t length)
{
    for (size_t i = 0; i < length; ++i)
        if (!candidate[i] || tolower((unsigned char)candidate[i]) != tolower((unsigned char)typed[i]))
            return 0;
    return 1;
}

/* "tela.pr" offers lcd.print too: any object other than Serial is taken as an LCD. */
static int member_matches(const char *name, const char *typed, size_t length)
{
    const char *typed_dot = memchr(typed, '.', length), *name_dot = strchr(name, '.');
    if (!typed_dot)
        return length && prefix_matches(name, typed, length) && strlen(name) != length;
    if (!name_dot)
        return 0;
    size_t object = (size_t)(typed_dot - typed), member = length - object - 1;
    int same_object = (size_t)(name_dot - name) == object && !strncmp(name, typed, object);
    int lcd_alias = !strncmp(name, "lcd.", 4) && strncmp(typed, "Serial.", 7);
    return (same_object || lcd_alias) && prefix_matches(name_dot + 1, typed_dot + 1, member) &&
           strlen(name_dot + 1) != member;
}

int code_completions(const char *text, int cursor, int *matches, int capacity)
{
    int start = code_word_start(text, cursor), count = 0;
    size_t length = (size_t)(cursor - start);
    if (!length || isdigit((unsigned char)text[start]))
        return 0;
    for (int i = 0; i < mcu_completion_count() && count < capacity; ++i)
        if (member_matches(mcu_completion(i)->name, text + start, length))
            matches[count++] = i;
    return count;
}

/* Builtins written as name(...) get the parentheses; keywords and declarations do not. */
static const char *call_suffix(const McuCompletion *completion)
{
    size_t length = strlen(completion->name);
    if (strncmp(completion->signature, completion->name, length) || completion->signature[length] != '(')
        return NULL;
    return completion->signature[length + 1] == ')' ? "()" : "(";
}

int code_accept(char *text, size_t capacity, int *cursor, int completion)
{
    const McuCompletion *chosen = mcu_completion(completion);
    if (!chosen)
        return 0;
    int start = code_word_start(text, *cursor);
    const char *typed_dot = memchr(text + start, '.', (size_t)(*cursor - start));
    const char *insert = chosen->name, *name_dot = strchr(chosen->name, '.');
    if (typed_dot && name_dot)
    {
        start = (int)(typed_dot - text) + 1;
        insert = name_dot + 1;
    }
    size_t length = strlen(text), removed = (size_t)(*cursor - start);
    if (length - removed + strlen(insert) + 3 > capacity)
        return 0;
    erase_range(text, start, *cursor);
    *cursor = start;
    code_insert(text, capacity, cursor, insert);
    const char *suffix = call_suffix(chosen);
    if (suffix && text[*cursor] != '(')
    {
        code_insert(text, capacity, cursor, "()");
        if (suffix[1] != ')')
            --*cursor;
    }
    return 1;
}
