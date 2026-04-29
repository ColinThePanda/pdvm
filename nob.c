#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

int main(int argc, char *argv[])
{
    GO_REBUILD_URSELF(argc, argv);

    bool run = false;
    flag_bool_var(&run, "run", false, "Run the program after compilation.");
    if (!flag_parse(argc, argv))
    {
        flag_print_error(stderr);
        return 1;
    }

    Cmd cmd = {};
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra");
    cmd_append(&cmd, "-opdvm", "pdvm.c");

    if (!cmd_run(&cmd))
        return 1;

    if (run)
    {
        cmd_append(&cmd, "./pdvm");
        for (int i = 2; i < argc; i++)
        {
            cmd_append(&cmd, argv[i]);
        }
        if (!cmd_run(&cmd))
            return 1;
    }
}