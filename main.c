#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <semaphore.h>
#include <time.h>
#define N 1098
#define M 8

// sem_t *sem;
typedef struct
{
    double shared_array[N * N];
    double shared_result;
    sem_t *sem;
    int last_index;
} shared_data_t;

int main(int argc, char *argv[])
{

    if (argc != 1)
    {
        shm_unlink("/bolts");
        return EXIT_SUCCESS;
    }

    int fd = shm_open("/my_shared_memory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        fprintf(stderr, "Open failed:%s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    ftruncate(fd, sizeof(double) * N * N + sizeof(double));
    shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0.0);
    // double *shared_array = mmap((&shared_result)+8, sizeof(double) * N * N, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0.0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // Initialize shared_array with random values between 1 and N

    for (int i = 0; i < N * N; i++)
    {
        shared_data->shared_array[i] = (double)rand() / (double)(RAND_MAX / N);
    }
    // Initialize the semaphore with value 1
    shared_data->sem = sem_open("/my_semaphore", O_CREAT, 0666, 1);

    // Compute the sum of shared_array serially and print the result
    double serial_result = 0;
    clock_t start = clock();
    for (int i = 0; i < N * N; i++)
    {
        serial_result += shared_data->shared_array[i];
    }
    clock_t end = clock();

    double serial_time = (double)(end - start) / CLOCKS_PER_SEC;

    // Create M processes to compute the sum in parallel
    shared_data->shared_result = 0;

    start = clock();
    for (int i = 0; i < M; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // This is the child process
            // Compute the sum of ((N*N)/M) items in shared_array
            // This is the child process
            int start = i * (N * N / M);
            int end = (i + 1) * (N * N / M);
            if (i == M - 1)
                shared_data->last_index = end;
            printf("Child %d: start = %d, end = %d\n", i, start, end);
            double sum = 0;
            for (int j = start; j < end; j++)
            {
                sum += shared_data->shared_array[j];
            }
            // sem_post(sem);
            printf("Child %d: local sum = %f\n", i, sum);
            // Use the semaphore to synchronize access to shared_result
            sem_wait(shared_data->sem);
            shared_data->shared_result += sum;
            sem_post(shared_data->sem);

            exit(0);
        }
    }

    // Wait for all child processes to finish
    for (int p = 0; p < N; p++)
    {
        wait(NULL);
    }
    printf("last_index: %d\n", shared_data->last_index);
    double sum = 0;
    for (int j = shared_data->last_index; j < N * N; j++)
    {
        sum += shared_data->shared_array[j];
    }
    sem_wait(shared_data->sem);
    shared_data->shared_result += sum;
    sem_post(shared_data->sem);
    end = clock();
    double parallel_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Serial expected result: %f\n", serial_result);
    printf("Serial time: %f milliseconds\n", serial_time * 1000);
    printf("Parallel computed result: %f\n", shared_data->shared_result);

    printf("Parallel time: %f milliseconds\n", parallel_time * 1000);

    munmap(shared_data, sizeof(shared_data_t));

    return 0;
}
