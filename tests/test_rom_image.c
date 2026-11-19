#include "rom_image.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    RomImage image;
    int line = 0;
    assert(rom_image_parse(NULL, &image, &line));
    assert(image.data[0] == 0xFF && image.data[ROM_IMAGE_BYTES - 1] == 0xFF);

    /* Sequential bytes, an address jump, comments and commas. */
    assert(rom_image_parse("1E 2F, e0 ; program\n0d: 07 1c # data\n0E", &image, &line));
    assert(image.data[0] == 0x1E && image.data[1] == 0x2F && image.data[2] == 0xE0 && image.data[3] == 0xFF);
    assert(image.data[0x0D] == 0x07 && image.data[0x0E] == 0x1C && image.data[0x0F] == 0x0E);
    assert(rom_image_parse("7FF: 5A", &image, &line) && image.data[0x7FF] == 0x5A);

    /* Each error names its line and leaves the image untouched. */
    image.data[0] = 0x42;
    const char *invalid[] = {"00\n1G", "00\n\n100", "800: 00", "7FF: 00 01", "0A:: 00", "123456789", ":"};
    const int lines[] = {2, 3, 1, 1, 1, 1, 1};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        line = 0;
        assert(!rom_image_parse(invalid[i], &image, &line));
        assert(line == lines[i]);
        assert(image.data[0] == 0x42);
    }
    printf("EEPROM image: bytes, address jumps, comments and line-numbered errors passed.\n");
    return 0;
}
