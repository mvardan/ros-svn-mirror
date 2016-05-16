#define __ROS_LONG64__

#define STANDALONE
#include <apitest.h>

extern void func_ACListISF(void);
extern void func_SHExplorerParseCmdLine(void);

const struct test winetest_testlist[] =
{
    { "ACListISF", func_ACListISF },
    { "SHExplorerParseCmdLine", func_SHExplorerParseCmdLine },

    { 0, 0 }
};

