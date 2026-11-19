#ifndef ROM_IMAGE_H
#define ROM_IMAGE_H
/* Contents of a parallel EEPROM (AT28C16, 2K x 8) written as text in Part.code. Pure: no SDL/OpenGL.
 *
 * Text: hexadecimal bytes separated by spaces, commas or new lines, stored from address 0 up.
 * "1A:" moves the next byte to address 0x1A. ';' or '#' starts a comment until the end of the line.
 * Addresses never written read FF, like an erased chip. */
#define ROM_IMAGE_BYTES 2048
#define ROM_IMAGE_ERASED 0xFF

typedef struct
{
    unsigned char data[ROM_IMAGE_BYTES];
} RomImage;

/* NULL text is an erased chip. On failure returns 0 and, when error_line is not NULL, the 1-based line. */
int rom_image_parse(const char *text, RomImage *image, int *error_line);
#endif
