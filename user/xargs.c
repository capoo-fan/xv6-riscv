#include "../kernel/param.h"
#include "../kernel/types.h"
#include "../user/user.h"

void run(char *buf, int argc, char *argv[])
{
    char *new_argv[MAXARG];

    int n = 0; // 先录入 1 ~ argc-1 的 参数
    for (int i = 1; i < argc; i++)
        new_argv[n++] = argv[i];

    char *p = buf;

    while (*p != '\0')
    {
        while (*p == ' ')
        {
            p++;
        }

        // 已经到字符串末尾
        if (*p == '\0')
        {
            break;
        }

        // 当前参数从这里开始
        new_argv[n++] = p;

        // 找这个参数的末尾
        while (*p != ' ' && *p != '\0')
        {
            p++;
        }

        // 如果碰到空格，把空格变成 '\0'
        if (*p == ' ')
        {
            *p = '\0';
            p++;
        }
    }
    new_argv[n] = 0;

    int pid = fork();

    if (pid == 0)
    {
        exec(new_argv[0], new_argv);
        exit(0);
    }
    wait(0);
}
int main(int argc, char *argv[])
{
    char buf[512];
    int pos = 0;
    char c;

    // 一次读一个字符
    while (read(0, &c, 1) > 0)
    {
        // 每次读到换行符就处理一行
        if (c == '\n')
        {

            // 一整行结束
            buf[pos] = '\0';

            if (pos > 0)
            {
                run(buf, argc, argv);
            }
            pos = 0;
        }
        else
        {
            buf[pos++] = c;
        }
    }
    if (pos > 0)
    {
        buf[pos] = '\0';
        run(buf, argc, argv);
    }
    exit(0);
}