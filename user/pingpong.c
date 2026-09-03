#include "../kernel/types.h"
#include "../user/user.h"

int main(int argc, char *argv[])
{
    // partent -> child
    int parent[2];
    pipe(parent);
    // child -> parent
    int child[2];
    pipe(child);

    char byte = 'x';
    // 先 pipe 再 fork
    // fork() 会让子进程继承父进程当前打开的文件描述符
    int pid = fork();

    // pid == 0 表示子进程
    if (pid == 0)
    {
        read(parent[0], &byte, 1); // 子进程读到一个字节
        printf("%d: received ping\n", getpid());
        write(child[1], &byte, 1);
        close(parent[1]);
        close(child[0]);
        exit(0);
    }
    else
    {
        write(parent[1], &byte, 1); // 父进程发送一个字节
        read(child[0], &byte, 1);
        printf("%d: received pong\n", getpid());
        close(parent[0]);
        close(child[1]);
        wait(0);
        exit(0);
    }
}