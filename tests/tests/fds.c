#define FDS_IMPLEMENTATION
#include "fds.h"


typedef struct {
    bool success;      // Чи вдалося взагалі запустити процес (false, якщо файла не існує)
    int exit_code;     // Код завершення програми (0 = успіх)
    char *stdout_data; // Буфер стандартного виводу (завжди нуль-термінований)
    char *stderr_data; // Буфер виводу помилок (завжди нуль-термінований)
    size_t stdout_len; // Довжина виводу
    size_t stderr_len; // Довжина помилок
} fds_cmd_result_t;

// Звільняє пам'ять, виділену під результат команди
void fds_cmd_result_free(fds_cmd_result_t *res) {
    if (res->stdout_data) free(res->stdout_data);
    if (res->stderr_data) free(res->stderr_data);
    res->stdout_data = NULL;
    res->stderr_data = NULL;
    res->stderr_len = 0;
    res->stdout_len = 0;
    res->exit_code = -1;
}

static void fds_append_pipe_data(char **buffer, size_t *len, size_t *cap, const char *chunk, size_t chunk_size) {
    if (*len + chunk_size + 1 > *cap) {
        *cap = (*cap == 0) ? 1024 : (*cap * 2) + chunk_size;
        char *new_buf = (char *)realloc(*buffer, *cap);
        if (new_buf) {
            *buffer = new_buf;
        } else {
            return; // Обробка нестачі пам'яті (OOM)
        }
    }
    memcpy(*buffer + *len, chunk, chunk_size);
    *len += chunk_size;
    (*buffer)[*len] = '\0'; // Завжди тримаємо нуль-термінатор для printf
}
#ifdef _WIN32
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <poll.h>
    #include <fcntl.h>
#endif


// Виконує консольну команду і повертає її Exit Code.
// Якщо out_output != NULL, виділяє пам'ять і записує туди результат (stdout).
// Пам'ять потрібно буде звільнити через fds_free (або free).



// Асинхронний процес
typedef struct {
    bool valid;
#ifdef _WIN32
    void* hProcess;
    void* hThread;
    void* std_in;   // Вхід для батьківського процесу (щоб писати в STDIN дитини)
    void* std_out;  // Вихід для батьківського процесу (щоб читати STDOUT дитини)
    void* std_err;  // Вихід помилок
#else
    int pid;
    int std_in;
    int std_out;
    int std_err;
#endif
} fds_process_t;

// Запускає процес асинхронно.
// Якщо pipe_from != NULL, STDOUT першого процесу автоматично з'єднується зі STDIN нового (А | B)
fds_process_t fds_process_start(const char *cmd_utf8, fds_process_t *pipe_from);

// Читання / Запис. Повертають кількість байтів. 0 означає кінець (EOF).
size_t fds_process_read_stdout(fds_process_t *proc, void *buffer, size_t size);
size_t fds_process_read_stderr(fds_process_t *proc, void *buffer, size_t size);
size_t fds_process_write_stdin(fds_process_t *proc, const void *buffer, size_t size);

// Закриває STDIN. (Обов'язково, якщо ти пишеш у процес, який чекає кінця файлу, напр. grep)
void fds_process_close_stdin(fds_process_t *proc);

// Блокує потік, поки процес не завершиться, і звільняє всі ресурси
int fds_process_wait(fds_process_t *proc);

fds_process_t fds_process_start(const char *cmd_utf8, fds_process_t *pipe_from) {
    fds_process_t p;
    memset(&p, 0, sizeof(p)); // Гарантовано чистимо структуру, щоб уникнути будь-якого сміття

#ifdef _WIN32
    HANDLE in_rd = NULL, in_wr = NULL;
    HANDLE out_rd = NULL, out_wr = NULL;
    HANDLE err_rd = NULL, err_wr = NULL;
    
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    // Якщо ми робимо PIPE (A | B), беремо STDOUT від 'A' і віддаємо його в STDIN 'B'
    if (pipe_from && pipe_from->std_out) {
        in_rd = (HANDLE)pipe_from->std_out;
        // Дозволяємо дитині успадкувати цей Handle
        SetHandleInformation(in_rd, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        in_wr = NULL; // Батько більше не пише в цей STDIN, туди пише процес 'A'
        pipe_from->std_out = NULL; // Забираємо handle у 'A', щоб батько двічі не закрив
    } else {
        if (!CreatePipe(&in_rd, &in_wr, &sa, 0)) return p;
        SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0); // Батьківський запис не успадковується
    }

    if (!CreatePipe(&out_rd, &out_wr, &sa, 0)) goto win_cleanup;
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
    
    if (!CreatePipe(&err_rd, &err_wr, &sa, 0)) goto win_cleanup;
    SetHandleInformation(err_rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = in_rd;
    si.hStdOutput = out_wr;
    si.hStdError = err_wr;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    
    // ---------------------------------------------------------
    // БЕЗПЕЧНА конвертація UTF-8 у UTF-16
    // ---------------------------------------------------------
    int wcmd_len = MultiByteToWideChar(CP_UTF8, 0, cmd_utf8, -1, NULL, 0);
    if (wcmd_len == 0) goto win_cleanup;

    wchar_t *wcmd = (wchar_t *)malloc(wcmd_len * sizeof(wchar_t));
    if (!wcmd) goto win_cleanup;

    MultiByteToWideChar(CP_UTF8, 0, cmd_utf8, -1, wcmd, wcmd_len);

    // CreateProcessW має право модифікувати wcmd, тому передавати malloc буфер - безпечно
    if (!CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        free(wcmd);
        goto win_cleanup;
    }
    free(wcmd);

    // Батько закриває ті кінці труб, які належать дитині
    if (in_rd) CloseHandle(in_rd);
    CloseHandle(out_wr);
    CloseHandle(err_wr);

    p.valid = true;
    p.hProcess = pi.hProcess;
    p.hThread = pi.hThread;
    p.std_in = in_wr;
    p.std_out = out_rd;
    p.std_err = err_rd;

    return p;

win_cleanup:
    // Очистка при будь-якій помилці Win32 API, щоб не текли Handles
    if (in_rd) CloseHandle(in_rd);
    if (in_wr) CloseHandle(in_wr);
    if (out_rd) CloseHandle(out_rd);
    if (out_wr) CloseHandle(out_wr);
    if (err_rd) CloseHandle(err_rd);
    if (err_wr) CloseHandle(err_wr);
    return p;

#else
    // POSIX
    p.std_in = -1; p.std_out = -1; p.std_err = -1;
    int in_pipe[2] = {-1, -1}, out_pipe[2] = {-1, -1}, err_pipe[2] = {-1, -1};

    if (pipe_from && pipe_from->std_out != -1) {
        in_pipe[0] = pipe_from->std_out;
        in_pipe[1] = -1; 
        pipe_from->std_out = -1; 
    } else {
        if (pipe(in_pipe) < 0) return p;
    }
    
    if (pipe(out_pipe) < 0) goto posix_cleanup;
    if (pipe(err_pipe) < 0) goto posix_cleanup;

    pid_t pid = fork();
    if (pid < 0) goto posix_cleanup; // Помилка fork

    if (pid == 0) { // Child
        if (in_pipe[0] != -1)  { dup2(in_pipe[0], STDIN_FILENO); close(in_pipe[0]); }
        if (in_pipe[1] != -1)  close(in_pipe[1]);
        
        if (out_pipe[1] != -1) { dup2(out_pipe[1], STDOUT_FILENO); close(out_pipe[1]); }
        if (out_pipe[0] != -1) close(out_pipe[0]);
        
        if (err_pipe[1] != -1) { dup2(err_pipe[1], STDERR_FILENO); close(err_pipe[1]); }
        if (err_pipe[0] != -1) close(err_pipe[0]);

        execl("/bin/sh", "sh", "-c", cmd_utf8, (char *)NULL);
        exit(127);
    }

    // Parent
    if (in_pipe[0] != -1) close(in_pipe[0]);
    if (out_pipe[1] != -1) close(out_pipe[1]);
    if (err_pipe[1] != -1) close(err_pipe[1]);

    p.valid = true;
    p.pid = pid;
    p.std_in = in_pipe[1];
    p.std_out = out_pipe[0];
    p.std_err = err_pipe[0];
    
    return p;

posix_cleanup:
    // Очистка при будь-якій помилці POSIX, щоб не текли File Descriptors
    if (in_pipe[0] != -1) close(in_pipe[0]);
    if (in_pipe[1] != -1) close(in_pipe[1]);
    if (out_pipe[0] != -1) close(out_pipe[0]);
    if (out_pipe[1] != -1) close(out_pipe[1]);
    if (err_pipe[0] != -1) close(err_pipe[0]);
    if (err_pipe[1] != -1) close(err_pipe[1]);
    return p;
#endif
}


size_t fds_process_read_stdout(fds_process_t *proc, void *buffer, size_t size) {
    if (!proc || !proc->valid || !proc->std_out) return 0;
#ifdef _WIN32
    DWORD bytes_read = 0;
    if (ReadFile((HANDLE)proc->std_out, buffer, (DWORD)size, &bytes_read, NULL)) return bytes_read;
#else
    if (proc->std_out != -1) {
        ssize_t bytes = read(proc->std_out, buffer, size);
        if (bytes > 0) return (size_t)bytes;
    }
#endif
    return 0;
}

void fds_process_close_stdin(fds_process_t *proc) {
    if (!proc || !proc->valid) return;
#ifdef _WIN32
    if (proc->std_in) { CloseHandle((HANDLE)proc->std_in); proc->std_in = NULL; }
#else
    if (proc->std_in != -1) { close(proc->std_in); proc->std_in = -1; }
#endif
}

int fds_process_wait(fds_process_t *proc) {
    if (!proc || !proc->valid) return -1;
    int exit_code = -1;
#ifdef _WIN32
    WaitForSingleObject((HANDLE)proc->hProcess, INFINITE);
    DWORD dwExit = 0;
    GetExitCodeProcess((HANDLE)proc->hProcess, &dwExit);
    exit_code = (int)dwExit;

    CloseHandle((HANDLE)proc->hProcess);
    CloseHandle((HANDLE)proc->hThread);
    if (proc->std_in) {CloseHandle((HANDLE)proc->std_in);}
    if (proc->std_out) {CloseHandle((HANDLE)proc->std_out);}
    if (proc->std_err) {CloseHandle((HANDLE)proc->std_err);}
#else
    int status;
    waitpid(proc->pid, &status, 0);
    if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
    
    fds_process_close_stdin(proc);
    if (proc->std_out != -1) close(proc->std_out);
    if (proc->std_err != -1) close(proc->std_err);
#endif
    proc->valid = false;
    return exit_code;
}







int main() {
    printf("Тест конвеєра A | B (echo -> grep/find)...\n");

#ifdef _WIN32
    // Windows версія
    fds_process_t p1 = fds_process_start("g++ -h", NULL);
    fds_process_t p2 = fds_process_start("gcc -h", &p1); // &p1 з'єднує труби!
#else
    // POSIX версія
    fds_process_t p1 = fds_process_start("echo \"Hello World Pipeline Magic!\"", NULL);
    fds_process_t p2 = fds_process_start("grep \"Pipeline\"", &p1);
#endif

    // Читаємо тільки фінальний результат від Process 2
    char buf[128];
    size_t bytes;
    printf("\nФінальний результат:\n");
    while ((bytes = fds_process_read_stdout(&p2, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    while ((bytes = fds_process_read_stderr(&p1, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';
        printf("%s", buf);
    }

    // Чекаємо їх коректного завершення
    int code1 = fds_process_wait(&p1);
    int code2 = fds_process_wait(&p2);

    printf("\nКоди завершення: P1=%d, P2=%d\n", code1, code2);
    return 0;
}









// int main(int argc, char **argv) {
//     fds_cli_init(&argc, &argv);
//     StringArray cmd = {0};

//     sa_pushm(&cmd, "gcc", "-Wall", "-Wextra", "-pedantic");
//     sa_push(&cmd, "src/fds.c");
//     sa_pushm(&cmd, "-o", "build/fds.exe.new");

//     fds_rename("build/fds.exe","build/fds.exe.old");
    
//     // Команда echo працює і на Windows, і на POSIX
//     fds_cmd_result_t res = fds_cmd_run_ext(sa_join(&cmd, " "));

//     if(res.exit_code != 0 ) fds_rename("build/fds.exe.old","build/fds.exe");
//     if(res.exit_code == 0 ) fds_rename("build/fds.exe.new","build/fds.exe");
//     fds_file_delete("build/fds.exe.new");
//     fds_log(FINFO, "Stdout: %s", res.stdout_data);
//     fds_log(FINFO, "Stderr: %s", res.stderr_data);
//     fds_log(FINFO, "Exit code: %d", res.exit_code);
//     fds_log(FINFO, "fdfsf");
//     fds_log(FINFO, "second message");
//     fds_log(FINFO, "second message");



// }








// int main() {
//     char *output = NULL;
    
//     // Виконуємо команду компіляції і перехоплюємо stdout ТА stderr (2>&1)
//     int exit_code = fds_cmd_run("gcc --version 2>&1", &output);
    
//     if (exit_code == 0) {
//         fds_log(FINFO, "Успіх! Версія GCC:\n%s", output);
//     } else {
//         fds_log(FERROR, "Команда завершилася з помилкою %d:\n%s", exit_code, output);
//     }
    
//     // Не забуваємо очистити пам'ять
//     if (output) free(output);
    
//     return 0;
// }

// int main2(int argc, char **argv) {
    //     fds_cli_init(&argc, &argv);
    //     FlagSet *flag = flagset_new();
    
//     char *file_path = NULL;
//     flagset_string(flag, &file_path,"config", "config.ini", "File path");

//     flagset_parse(flag, argc, argv);

//     fds_log(FINFO, "loading config file %s", file_path);

//     IniConfig config = ini_parse(file_path);
//     int port = ini_get_int(&config, "database", "port", 69);

//     fds_log(FINFO, "Port :%d ", port);

//     flagset_free(flag);
//     ini_free(&config);
//     return 0;
// }


// int main()
// {
//     StringArray cmd= {0};
//     // sa_new(&cmd, 10);
//     sa_push(&cmd, "gcc");
//     sa_push(&cmd, "src\\fds.c");
//     sa_push(&cmd, "-o");
//     sa_push(&cmd, "build\\fds.exe");
//     //fds_log(FINFO, "%s", sa_join(&cmd, " "));

    
//     STARTUPINFO si;
//     PROCESS_INFORMATION pi;

//     memset(&si, 0, sizeof(si));
//     memset(&pi, 0, sizeof(pi));
//     si.cb = sizeof(si);
    
//     char *command = sa_join(&cmd, " ");

//     if (CreateProcessA(
//         NULL,           // Application name
//         command,        // Command line arguments
//         NULL,           // Process security attributes
//         NULL,           // Thread security attributes
//         FALSE,          // Inherit handles
//         0,              // Creation flags
//         NULL,           // Use parent's environment block
//         NULL,           // Use parent's starting directory 
//         &si,            // Pointer to STARTUPINFO structure
//         &pi             // Pointer to PROCESS_INFORMATION structure
//     )) {
//         // Wait until child process exits
//         WaitForSingleObject(pi.hProcess, INFINITE);

//         // Close process and thread handles
//         CloseHandle(pi.hProcess);
//         CloseHandle(pi.hThread);
//     } else {
//         printf("CreateProcess failed (%d).\n", GetLastError());
//     }


//     sa_free(&cmd);
//     return 0;
// }