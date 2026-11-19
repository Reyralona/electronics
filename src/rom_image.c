#include "rom_image.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define ROM_TOKEN_CAPACITY 8

static int hex_value(const char *token, long maximum, long *value)
{
    char *end = NULL;
    if (!*token)
        return 0;
    for (const char *c = token; *c; ++c)
        if (!isxdigit((unsigned char)*c))
            return 0;
    *value = strtol(token, &end, 16);
    return end && !*end && *value >= 0 && *value <= maximum;
}

static int separator(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
}

/* Applies one token: "1A:" sets the address, "3F" stores a byte and advances it. */
static int apply_token(const char *token, RomImage *image, long *address)
{
    size_t length = strlen(token);
    long value;
    if (token[length - 1] == ':')
    {
        char digits[ROM_TOKEN_CAPACITY];
        memcpy(digits, token, length - 1);
        digits[length - 1] = 0;
        if (!hex_value(digits, ROM_IMAGE_BYTES - 1, &value))
            return 0;
        *address = value;
        return 1;
    }
    if (length > 2 || !hex_value(token, 0xFF, &value) || *address >= ROM_IMAGE_BYTES)
        return 0;
    image->data[(*address)++] = (unsigned char)value;
    return 1;
}

int rom_image_parse(const char *text, RomImage *image, int *error_line)
{
    RomImage parsed;
    long address = 0;
    int line = 1;
    memset(parsed.data, ROM_IMAGE_ERASED, sizeof(parsed.data));
    for (const char *c = text; c && *c;)
    {
        if (*c == ';' || *c == '#')
        {
            while (*c && *c != '\n')
                ++c;
            continue;
        }
        if (separator(*c))
        {
            line += *c == '\n';
            ++c;
            continue;
        }
        char token[ROM_TOKEN_CAPACITY];
        size_t length = 0;
        while (*c && !separator(*c) && *c != ';' && *c != '#')
        {
            if (length + 1 >= sizeof(token))
                length = sizeof(token); /* too long for any valid token: reported below */
            else
                token[length++] = *c;
            ++c;
        }
        if (length >= sizeof(token))
            length = 0;
        token[length] = 0;
        if (!length || !apply_token(token, &parsed, &address))
        {
            if (error_line)
                *error_line = line;
            return 0;
        }
    }
    if (image)
        *image = parsed;
    return 1;
}
