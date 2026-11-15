#include "workbench_group_name.h"
#include <string.h>

int bench_group_name_valid(const char *name)
{
    if (!name)
        return 0;
    size_t length = 0;
    while (length < BENCH_GROUP_NAME_CAPACITY && name[length])
        ++length;
    if (!length || length == BENCH_GROUP_NAME_CAPACITY)
        return 0;
    int visible = 0;
    for (size_t i = 0; i < length;)
    {
        unsigned char c = (unsigned char)name[i++];
        unsigned value;
        int more;
        if (c < 0x80)
        {
            if (c < 32 || c == 127)
                return 0;
            visible |= c != ' ';
            continue;
        }
        if (c >= 0xc2 && c <= 0xdf)
        {
            value = c & 31;
            more = 1;
        }
        else if (c >= 0xe0 && c <= 0xef)
        {
            value = c & 15;
            more = 2;
        }
        else if (c >= 0xf0 && c <= 0xf4)
        {
            value = c & 7;
            more = 3;
        }
        else
            return 0;
        int count = more;
        while (more--)
        {
            if (i >= length || ((unsigned char)name[i] & 0xc0) != 0x80)
                return 0;
            value = (value << 6) | ((unsigned char)name[i++] & 63);
        }
        if ((count == 2 && value < 0x800) || (count == 3 && value < 0x10000) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff) || value < 0xa0)
            return 0;
        visible = 1;
    }
    return visible;
}

int bench_group_set_name(Workbench *bench, int group, const char *name)
{
    if (!bench || group < 1 || group > WORKBENCH_MAX_COMPONENTS || !bench->groups[group - 1].active ||
        !bench_group_name_valid(name))
        return 0;
    if (strcmp(bench->groups[group - 1].name, name))
    {
        memcpy(bench->groups[group - 1].name, name, strlen(name) + 1);
        ++bench->revision;
    }
    return 1;
}
