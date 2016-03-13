#define __ROS_LONG64__

#define STANDALONE
#include <wine/test.h>

extern void func_CMyComputer(void);
extern void func_CShellDesktop(void);
extern void func_menu(void);

const struct test winetest_testlist[] =
{
    { "CMyComputer", func_CMyComputer },
    { "CShellDesktop", func_CShellDesktop },
    { "menu", func_menu },
    { 0, 0 }
};
