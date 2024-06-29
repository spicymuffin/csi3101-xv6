#include "types.h"
#include "user.h"
#include "stddef.h"
#include "syscall.h"
#include "fcntl.h"

#define ERR_NULL 0
#define ERR_SAME_TID_EXIST 1
#define ERR_DIFF_PID 2
#define ERR_DIFF_FILE_DESC 3
#define ERR_DIFF_HEAP_SPACE 4
#define ERR_RECLAIM_MEM_FAIL 5
#define ERR_NUM 6

int err_list[ERR_NUM] = {
    0,
};

int pid;
char *message = "test";

struct arg
{
    int id;
    int fd;
    char *msg;
};

void thread_test(void *a)
{
    printf(1, "%c", *(char *)a);
}

void thread_main(void *a)
{
    struct arg *arg = (struct arg *)a;

    if (getpid() != pid)
        err_list[ERR_DIFF_PID]++;

    arg->msg = malloc(100);
    strcpy(arg->msg, message);
    arg->msg[4] = '0' + arg->id;
    arg->msg[5] = '\0';

    arg->fd = open(arg->msg, O_CREATE | O_RDWR);
    if (arg->fd < 0)
        return;
    write(arg->fd, arg->msg, 5);

    return;
}

int main(int argc, char **argv)
{

    int i, j, n = 3;
    int score = 60;
    int *tids;
    struct arg *args;

    pid = getpid();

    if (argc > 1)
        n = atoi(argv[1]);

    tids = malloc(sizeof(*tids) * n);
    args = malloc(sizeof(*args) * n);

    printf(1, "\n!! TEST start !!\n\n");

    // create thread
    for (i = 0; i < n; i++)
    {
        args[i].id = i;
        tids[i] = thread_create(thread_main, &args[i]);
        for (j = 0; j < i; j++)
            if (tids[j] == tids[i])
                err_list[ERR_SAME_TID_EXIST]++;
        printf(1, "[thread parent(%d)] thread child #%d(%d) has been created\n", pid, i, tids[i], &args[i]);
    }

    // join thread
    for (i = 0; i < n; i++)
    {
        thread_join(tids[i]);
        printf(1, "[thread parent(%d)] thread child #%d(%d) has been joined\n", pid, i, tids[i]);
    }

    // check result
    char test_msg[6];
    char file_msg[6];
    strcpy(test_msg, message);
    for (i = 0; i < n; i++)
    {
        printf(1, "[thread child #%d] %s\n", i, args[i].msg);
        test_msg[4] = '0' + i;
        test_msg[5] = '\0';
        if (strcmp(args[i].msg, test_msg) != 0)
            err_list[ERR_DIFF_HEAP_SPACE]++;
        close(args[i].fd);
        open(args[i].msg, O_RDONLY);
        read(args[i].fd, file_msg, 6);
        if (strcmp(test_msg, file_msg) != 0)
            err_list[ERR_DIFF_FILE_DESC]++;
    }

    // check memory leak
    int free_priv = frees();
    for (i = 0; i < 100; i++)
    {
        char a = '.';
        int tid = thread_create(thread_test, &a);
        thread_join(tid);
    }
    if (free_priv != frees())
        err_list[ERR_RECLAIM_MEM_FAIL]++;

    // Grading
    int err_sum = 0;
    printf(1, "\n\n------------------- ERR LIST -------------------\n");
    for (i = 1; i < ERR_NUM; i++)
    {
        if (err_list[i] != 0)
        {
            err_sum++;
            switch (i)
            {
            case ERR_SAME_TID_EXIST:
                printf(1, "[ERR#%d] duplicate tids exist(%d): -5\n", i, err_list[i]);
                score -= 5;
                break;
            case ERR_DIFF_PID:
                printf(1, "[ERR#%d] wrong pid(%d): -5\n", i, err_list[i]);
                score -= 5;
                break;
            case ERR_DIFF_FILE_DESC:
                printf(1, "[ERR#%d] do not share file descriptor(%d): -10\n", i, err_list[i]);
                score -= 10;
                break;
            case ERR_DIFF_HEAP_SPACE:
                printf(1, "[ERR#%d] do do not share heap space(%d): -10\n", i, err_list[i]);
                score -= 10;
                break;
            case ERR_RECLAIM_MEM_FAIL:
                printf(1, "[ERR#%d] do not reclaim thread memory(%d): -10\n", i, err_list[i]);
                score -= 10;
                break;
            }
        }
    }
    if (err_sum == 0)
        printf(1, "(none)\n");

    printf(1, "\nthreadtest score=%d\n", score);
    printf(1, "\n!! TEST end !!\n\n");

    exit();
}
