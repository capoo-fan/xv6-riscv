#include "../kernel/types.h"
#include "../kernel/fs.h"
#include "../kernel/stat.h"
#include "../user/user.h"

char *strcat(char *dest, const char *src)
{
    char *ptr = dest + strlen(dest);
    while (*src != '\0')
    {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

void find(char *root, char *file)
{
    // root 表示要搜索的根目录 file 表示要搜索的文件
    int fd;
    char buf[512];
    char *p;

    struct dirent de; // 目录中有什么名字
    struct stat st;   // 名字对应的文件

    if ((fd = open(root, 0)) < 0)
    {
        return; //
    }

    fstat(fd, &st);

    switch (st.type)
    {
    case T_FILE: {
        break;
    }
    case T_DIR: {
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            strcpy(buf, root);
            p = buf + strlen(buf);
            *p = '/';
            p++;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if (strcmp(p, file) == 0)
                printf("%s\n", buf);

            find(buf, file);
        }
        break;
    }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    char *file = argv[2];

    find(".", file);
    exit(0);
}