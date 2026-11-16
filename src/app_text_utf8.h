#ifndef APP_TEXT_UTF8_H
#define APP_TEXT_UTF8_H
int app_text_utf8(float x, float baseline, const char *text, int maximum_width);
/* Rendered width in pixels at the current size (0 when unavailable); shares the glyph cache. */
int app_text_utf8_width(const char *text, int maximum_width);
void app_text_utf8_set_size(int pixels);
void app_text_utf8_clear(void);
#endif
