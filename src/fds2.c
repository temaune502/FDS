#define FDS_IMPLEMENTATION
#include "fds.h"






int main(int argc, char **argv)
{
    fds_cli_init(&argc, &argv);

    fds_cmd_result result = fds_cmd_run_ext("cmd /c dir /r /s *.h");
    fds_log(FINFO, " Exit code: %d\n error data: %s\n  error data len: %d\n  stdout data: %s\n stdout data len: %d  seccess : %d", 
        result.exit_code,
        result.stderr_data,
        result.stderr_len,
        result.stdout_data,
        result.stdout_len,
        result.success);
}