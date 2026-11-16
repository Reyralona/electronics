#ifndef CODE_TEXT_H
#define CODE_TEXT_H
#include <stddef.h>
/* Text-buffer operations for the microcontroller code editor. Pure module: no SDL/OpenGL.
 * Positions are byte offsets into a NUL-terminated UTF-8 buffer; columns count code points. */
#define CODE_INDENT 4
#define CODE_COMPLETION_MAX 8

/* Returns 0 (buffer unchanged) when the insert does not fit in capacity. */
int code_insert(char *text, size_t capacity, int *cursor, const char *insert);
void code_backspace(char *text, int *cursor);
void code_delete(char *text, int cursor);
/* Enter: keeps the current indentation, one level deeper after '{'. */
int code_newline(char *text, size_t capacity, int *cursor);
/* '}' typed on a blank line moves one level left before it is inserted. */
int code_close_brace(char *text, size_t capacity, int *cursor);

int code_line_start(const char *text, int position);
int code_line_end(const char *text, int position);
int code_line_index(const char *text, int position);
int code_line_count(const char *text);
int code_line_offset(const char *text, int line);
int code_column(const char *text, int position);
/* Offset of a column on a line, clamped to the line end. */
int code_position(const char *text, int line, int column);
int code_next(const char *text, int position);
int code_previous(const char *text, int position);

/* Completion: the identifier (with an optional "object." prefix) that ends at the cursor. */
int code_word_start(const char *text, int cursor);
/* Fills matches with indices into mcu_completion(); returns how many (0 when nothing is typed). */
int code_completions(const char *text, int cursor, int *matches, int capacity);
/* Replaces the typed word with the completion; functions get "()" with the cursor inside. */
int code_accept(char *text, size_t capacity, int *cursor, int completion);
#endif
