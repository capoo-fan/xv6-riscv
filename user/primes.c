#include "../kernel/types.h"
#include "../user/user.h"

void filter(int read_fd)
{
    int prime;
    // 如果读不到数据，说明管道已经关闭，直接退出
    if (read(read_fd, &prime, sizeof(int))==0)
    {
        close(read_fd);
        exit(0);
    }

    printf("prime %d\n", prime);
    // 递归调用，处理下一个 prime
    int next_pipe[2];
    pipe(next_pipe);
    int pid = fork();
    if (pid == 0)
    {
        close(next_pipe[1]); // 子进程关闭写端
        filter(next_pipe[0]);
        exit(0);
    }
    else
    {
        close(next_pipe[0]); // 父进程关闭读端
        int num;
        while (read(read_fd, &num, sizeof(int))) // 从上一个进程传来的管道继续获取数据
        {
            if (num % prime != 0)
            {
                write(next_pipe[1], &num, sizeof(int));
            }
        }
        close(read_fd);
        close(next_pipe[1]);
        wait(0);
        exit(0);
    }
}
int main(int argc, char *argv[])
{
    int p[2]; // 第一个进程将 2-35 一次送入管道当中
    pipe(p);
    int pid = fork();

    // 子进程
    if (pid == 0)
    {
        close(p[1]); // 子进程只需要读
        filter(p[0]);
        exit(0);
    }
    else
    {
        close(p[0]);
        for (int i = 2; i <= 35; i++)
        {
            write(p[1], &i, sizeof(int));
        }
        close(p[1]);  // 如果不关闭，子进程会一直阻塞在 read(),不会返回0
        wait(0); // 父进程等待子进程结束
    }
    exit(0);
}