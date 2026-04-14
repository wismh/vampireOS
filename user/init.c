/* Reaper: fork/exec sh, wait, start it again. Boot user_run("init"). */
long fork(void);
long wait(long pid);
long exec(const char *path);
void exit(int code);

int main(void)
{
    long pid;

    for (;;) {
        pid = fork();
        if (pid == 0) {
            exec("sh");
            exit(1);
        }
        if (pid > 0) {
            (void)wait(0);
        }
    }
}
