#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>

// Change boltuns number here
#define NUM_BOLTUNS 5
// Change waiting time for call in seconds here
#define MAX_WAIT_TIME 5

typedef struct
{
    int phone_numbers[NUM_BOLTUNS];
    int phone_statuses[NUM_BOLTUNS];
    sem_t sem_lock;
} div_data;

void make_call(int id, div_data *data);
void wait_for_call(int id, div_data *data);
void switch_state(int id, div_data *data);
int get_random_phone_number(int id, div_data *data);
int get_random_wait_time();

// Service function for destroying semaphores in all cases
void clean_up(int fd, div_data *data);

int main()
{
    // Checking if in all cases clean up of memory and sems will be done
    atexit(clean_up);

    int fd = shm_open("/memory", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(div_data));
    div_data *data = mmap(NULL, sizeof(div_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // Initialize shared data
    for (int i = 0; i < NUM_BOLTUNS; i++)
    {
        data->phone_numbers[i] = i;
        data->phone_statuses[i] = 0;
    }
    sem_init(&data->sem_lock, 1, 1);

    // Start child processes
    pid_t pids[NUM_BOLTUNS];
    for (int i = 0; i < NUM_BOLTUNS; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            srand(time(NULL) + i); // Initialize random seed for each child process
            while (1)
            {
                if (data->phone_statuses[i] == 0)
                {
                    make_call(i, data);
                }
                else
                {
                    wait_for_call(i, data);
                }
                int wait_time = get_random_wait_time();
                usleep(wait_time * 1000000);
            }
            exit(0);
        }
    }

    // Wait for child processes to finish
    for (int i = 0; i < NUM_BOLTUNS; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    munmap(data, sizeof(div_data));
    close(fd);
    shm_unlink("/memory");
    sem_destroy(&data->sem_lock);
    sem_unlink("/sem_lock");

    return 0;
}

void make_call(int id, div_data *data)
{
    int phone_number = get_random_phone_number(id, data);
    sem_wait(&data->sem_lock);
    if (data->phone_statuses[phone_number] == 0)
    {
        data->phone_statuses[phone_number] = id + 1;
        printf("Boltun %d is calling Boltun %d\n", id + 1, phone_number + 1);
    }
    sem_post(&data->sem_lock);
    switch_state(id, data);
}

void wait_for_call(int id, div_data *data)
{
    sem_wait(&data->sem_lock);
    int phone_number = -1;
    for (int i = 0; i < NUM_BOLTUNS; i++)
    {
        if (data->phone_statuses[i] == id + 1)
        {
            phone_number = i;
            break;
        }
    }
    sem_post(&data->sem_lock);
    if (phone_number != -1)
    {
        printf("Boltun %d is talking with Boltun %d\n", id + 1, phone_number + 1);
        int wait_time = get_random_wait_time();
        usleep(wait_time * 1000000);
        switch_state(id, data);
    }
}

void switch_state(int id, div_data *data)
{
    sem_wait(&data->sem_lock);
    data->phone_statuses[id] = 0;
    sem_post(&data->sem_lock);
}

int get_random_phone_number(int id, div_data *data)
{
    int phone_number;
    do
    {
        phone_number = rand() % NUM_BOLTUNS;
    } while (phone_number == id || data->phone_statuses[phone_number] != 0);
    return phone_number;
}

int get_random_wait_time()
{
    return rand() % MAX_WAIT_TIME + 1;
}

void clean_up(int fd, div_data *data)
{
    printf("Clean up function is called\n");
    munmap(data, sizeof(div_data));
    close(fd);
    shm_unlink("/memory");
    sem_destroy(&data->sem_lock);
    sem_unlink("/sem_lock");
}