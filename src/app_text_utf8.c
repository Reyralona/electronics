#include <windows.h>
#include <SDL3/SDL_opengl.h>
#include "app_text_utf8.h"
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Eviction limits cached GPU resources, never the set of strings that can be displayed. */
#define TEXT_CACHE_ENTRIES 256
typedef struct
{
    char *key;
    int maximum_width, width, height, ascent, texture_width, texture_height, monospace;
    GLuint texture;
    uint64_t used;
} TextImage;
static TextImage cache[TEXT_CACHE_ENTRIES];
static uint64_t sequence;
static int font_pixels = 15;
static int monospace; /* Code editor face (Consolas) instead of the interface face. */

static void release_image(TextImage *image)
{
    if (image->texture)
        glDeleteTextures(1, &image->texture);
    free(image->key);
    memset(image, 0, sizeof(*image));
}

void app_text_utf8_clear(void)
{
    for (int i = 0; i < TEXT_CACHE_ENTRIES; ++i)
        release_image(&cache[i]);
}

void app_text_utf8_set_size(int pixels)
{
    if (pixels >= 12 && pixels <= 20 && pixels != font_pixels)
    {
        app_text_utf8_clear();
        font_pixels = pixels;
    }
}

static int power_of_two(int value)
{
    int result = 1;
    while (result < value)
    {
        if (result > INT_MAX / 2)
            return 0;
        result *= 2;
    }
    return result;
}

static WCHAR *wide_text(const char *text)
{
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!size || size > INT_MAX - 3)
        return NULL;
    WCHAR *wide = malloc((size_t)(size + 3) * sizeof(*wide));
    if (!wide)
        return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, size))
    {
        free(wide);
        return NULL;
    }
    return wide;
}

static int fit_text(HDC dc, WCHAR *text, int maximum_width, SIZE *size)
{
    int length = lstrlenW(text);
    if (!GetTextExtentPoint32W(dc, text, length, size))
        return 0;
    if (size->cx > maximum_width)
    {
        SIZE dots;
        int fit = 0;
        if (!GetTextExtentPoint32W(dc, L"...", 3, &dots) || maximum_width <= dots.cx ||
            !GetTextExtentExPointW(dc, text, length, maximum_width - dots.cx, &fit, NULL, size))
            return 0;
        if (fit > 0 && text[fit - 1] >= 0xd800 && text[fit - 1] <= 0xdbff)
            --fit;
        memcpy(text + fit, L"...", 4 * sizeof(WCHAR));
        return GetTextExtentPoint32W(dc, text, fit + 3, size);
    }
    return 1;
}

static int upload_text(TextImage *image, HDC dc, const WCHAR *text, const SIZE *size)
{
    TEXTMETRICW metrics;
    if (!GetTextMetricsW(dc, &metrics))
        return 0;
    image->width = size->cx;
    image->height = size->cy;
    image->ascent = metrics.tmAscent;
    image->texture_width = power_of_two(size->cx + 2);
    image->texture_height = power_of_two(size->cy + 2);
    GLint maximum;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (!image->texture_width || !image->texture_height || image->texture_width > maximum ||
        image->texture_height > maximum)
        return 0;
    BITMAPINFO bitmap = {0};
    bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
    bitmap.bmiHeader.biWidth = image->texture_width;
    bitmap.bmiHeader.biHeight = -image->texture_height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    void *pixels = NULL;
    HBITMAP surface = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!surface)
        return 0;
    HGDIOBJ previous = SelectObject(dc, surface);
    size_t count = (size_t)image->texture_width * image->texture_height;
    unsigned char *alpha = calloc(count, 1);
    int ok = 0;
    if (alpha && previous && previous != HGDI_ERROR)
    {
        memset(pixels, 0, count * 4);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, RGB(0, 0, 0));
        if (TextOutW(dc, 0, 0, text, lstrlenW(text)) && GdiFlush())
        {
            for (size_t i = 0; i < count; ++i)
                alpha[i] = ((unsigned char *)pixels)[i * 4];
            glGenTextures(1, &image->texture);
            glPushAttrib(GL_TEXTURE_BIT);
            glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
            glBindTexture(GL_TEXTURE_2D, image->texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, image->texture_width, image->texture_height, 0, GL_ALPHA,
                         GL_UNSIGNED_BYTE, alpha);
            ok = image->texture != 0 && glGetError() == GL_NO_ERROR;
            glPopClientAttrib();
            glPopAttrib();
        }
    }
    free(alpha);
    if (previous && previous != HGDI_ERROR)
        SelectObject(dc, previous);
    DeleteObject(surface);
    return ok;
}

static int create_image(TextImage *image, const char *text, int maximum_width)
{
    WCHAR *wide = wide_text(text);
    HDC dc = CreateCompatibleDC(NULL);
    HFONT font = CreateFontW(-font_pixels, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY,
                             monospace ? FIXED_PITCH : DEFAULT_PITCH, monospace ? L"Consolas" : L"Segoe UI");
    int ok = 0;
    if (wide && dc && font)
    {
        HGDIOBJ previous = SelectObject(dc, font);
        SIZE size;
        if (previous && previous != HGDI_ERROR && fit_text(dc, wide, maximum_width, &size))
            ok = upload_text(image, dc, wide, &size);
        if (previous && previous != HGDI_ERROR)
            SelectObject(dc, previous);
    }
    if (font)
        DeleteObject(font);
    if (dc)
        DeleteDC(dc);
    free(wide);
    if (ok)
    {
        image->key = malloc(strlen(text) + 1);
        if (image->key)
            memcpy(image->key, text, strlen(text) + 1);
        ok = image->key != NULL;
    }
    if (!ok)
        release_image(image);
    return ok;
}

static TextImage *cached_image(const char *text, int maximum_width)
{
    if (!text || !text[0] || maximum_width < 1)
        return NULL;
    TextImage *image = NULL, *oldest = &cache[0];
    for (int i = 0; i < TEXT_CACHE_ENTRIES; ++i)
    {
        if (cache[i].key && cache[i].maximum_width == maximum_width && cache[i].monospace == monospace &&
            !strcmp(cache[i].key, text))
            image = &cache[i];
        if (cache[i].used < oldest->used)
            oldest = &cache[i];
    }
    if (!image)
    {
        image = oldest;
        release_image(image);
        if (!create_image(image, text, maximum_width))
            return NULL;
        image->maximum_width = maximum_width;
        image->monospace = monospace;
    }
    image->used = ++sequence;
    return image;
}

int app_text_utf8_width(const char *text, int maximum_width)
{
    TextImage *image = cached_image(text, maximum_width);
    return image ? image->width : 0;
}

int app_text_utf8(float x, float baseline, const char *text, int maximum_width)
{
    TextImage *image = cached_image(text, maximum_width);
    if (!image)
        return 0;
    float top = baseline - image->ascent;
    float u = (float)image->width / image->texture_width, v = (float)image->height / image->texture_height;
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, image->texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, top);
    glTexCoord2f(u, 0);
    glVertex2f(x + image->width, top);
    glTexCoord2f(u, v);
    glVertex2f(x + image->width, top + image->height);
    glTexCoord2f(0, v);
    glVertex2f(x, top + image->height);
    glEnd();
    glPopAttrib();
    return 1;
}

void app_text_utf8_set_monospace(int enabled)
{
    monospace = enabled != 0;
}
