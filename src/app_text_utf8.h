#ifndef APP_TEXT_UTF8_H
#define APP_TEXT_UTF8_H
int app_text_utf8(float x, float baseline, const char *text, int maximum_width);
/* Rendered width in pixels at the current size (0 when unavailable); shares the glyph cache. */
int app_text_utf8_width(const char *text, int maximum_width);
void app_text_utf8_set_size(int pixels);
void app_text_utf8_clear(void);
/* Device pixels per canvas unit for the following draws; a change clears the cache. */
void app_text_utf8_set_scale(float scale);
/* Selects the fixed-pitch code face for following draws; callers restore 0 afterwards. */
void app_text_utf8_set_monospace(int enabled);
#endif
