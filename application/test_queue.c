#include <raplayer.h>
#include <stdio.h>

int main() {
    ra_queue_t queue;
    init_queue(&queue, 4);

    puts("== QUEUE TEST ==");
    ra_task_t *task01 = malloc(sizeof(ra_task_t));
    task01->data = malloc(100);
    strcpy(task01->data, "data01");
    task01->data_len = strlen(task01->data);
    printf("put task01 - %s, %s\n", (char *) task01->data, enqueue_task(&queue, task01) ? "OK" : "FAIL");
    printf("retrieve task01 - %s\n\n", (char *) retrieve_task(&queue, 0, true)->data);

    ra_task_t *task02 = malloc(sizeof(ra_task_t));;
    task02->data = malloc(100);
    strcpy(task02->data, "data02");
    task02->data_len = strlen(task02->data);
    printf("put task02 - %s, %s\n", (char *) task02->data, enqueue_task(&queue, task02) ? "OK" : "FAIL");
    printf("retrieve task01 - %s\n", (char *) retrieve_task(&queue, 0, true)->data);
    printf("retrieve task02 - %s\n\n", (char *) retrieve_task(&queue, 1, true)->data);

    ra_task_t *task03 = malloc(sizeof(ra_task_t));;
    task03->data = malloc(100);
    strcpy(task03->data, "data03");
    task03->data_len = strlen(task03->data);
    printf("put task03 - %s, %s\n", (char *) task03->data, enqueue_task(&queue, task03) ? "OK" : "FAIL");
    printf("retrieve task01 - %s\n", (char *) retrieve_task(&queue, 0, false)->data);
    printf("retrieve task02 - %s\n", (char *) retrieve_task(&queue, 1, false)->data);
    printf("retrieve task03 - %s\n\n", (char *) retrieve_task(&queue, 2, false)->data);

    printf("retrieve last task - %s\n", (char *) retrieve_task(&queue, -1, false)->data);
    printf("dequeue task - %s\n", (char *)dequeue_task(&queue)->data);
    printf("retrieve last task - %s\n", (char *) retrieve_task(&queue, -1, false)->data);

    ra_task_t *task04 = malloc(sizeof(ra_task_t));
    task04->data = malloc(100);
    strcpy(task04->data, "data04");
    task04->data_len = strlen(task04->data);
    printf("put task04 - %s, %s\n\n", (char *) task04->data, enqueue_task(&queue, task04) ? "OK" : "FAIL");
    retrieve_task(&queue, -1, true);

    printf("dequeue remaining task..\n");
    for(int i = 0; i < 3; i++) {
        ra_task_t *task = dequeue_task(&queue);
        printf("get %s\n", (char *) task->data);
    }

    for(int i = 0; i < 100; i++) {
        char* buf = malloc(100);
        sprintf(buf, "task%2d", i);
        ra_task_t *task = create_task(100);
        strcpy(task->data, buf);
        enqueue_task_with_removal(&queue, task);

        for(int j = 0; j < 4; j++)
        {
            ra_task_t *r_task = retrieve_task(&queue, j, false);
            if(r_task != NULL)
                printf("[%d] - %d: get %s\n", i, j, (char *) r_task->data);
        }
        printf("\n");
    }
    return 0;
}