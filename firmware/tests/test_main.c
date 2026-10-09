#include "testlib.h"

int t_fail, t_pass;
const char *t_name = "";

void suite_basic(void);
void suite_modbus(void);
void suite_display(void);
void suite_engine(void);
void suite_ui(void);

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : 0;
    if (!only || !strcmp(only, "basic"))   suite_basic();
    if (!only || !strcmp(only, "modbus"))  suite_modbus();
    if (!only || !strcmp(only, "display")) suite_display();
    if (!only || !strcmp(only, "engine"))  suite_engine();
    if (!only || !strcmp(only, "ui"))      suite_ui();
    printf("\n%d checks passed, %d failed\n", t_pass, t_fail);
    return t_fail ? 1 : 0;
}
