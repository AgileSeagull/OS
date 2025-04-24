#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Define the process structure
typedef struct
{
    int pid;
    int arrival_time;
    int burst_time;
    int deadline;
    int criticality;
    int period;
    int nice;
    int remaining_time;
    int completed;
    int start_time;      // When process starts execution for the first time
    int completion_time; // When process completes execution
    int in_ready_queue;  // Flag to track if process is in ready queue
} Process;

// Define the ready queue
typedef struct
{
    Process *processes;
    int size;
    int capacity;
} ReadyQueue;

// Function to create a new ready queue
ReadyQueue *createReadyQueue(int capacity)
{
    ReadyQueue *queue = (ReadyQueue *)malloc(sizeof(ReadyQueue));
    queue->processes = (Process *)malloc(sizeof(Process) * capacity);
    queue->size = 0;
    queue->capacity = capacity;
    return queue;
}

// Function to add a process to the ready queue
void addToReadyQueue(ReadyQueue *queue, Process process)
{
    if (queue->size < queue->capacity)
    {
        queue->processes[queue->size] = process;
        queue->size++;
    }
    else
    {
        printf("Ready queue is full\n");
    }
}

// Function to remove a process from the ready queue
Process removeFromReadyQueue(ReadyQueue *queue, int index)
{
    Process process = queue->processes[index];

    // Shift all processes after index one position to the left
    for (int i = index; i < queue->size - 1; i++)
    {
        queue->processes[i] = queue->processes[i + 1];
    }

    queue->size--;
    return process;
}

// Comparison function for sorting processes by remaining time (SRPT)
int compareRemainingTime(const void *a, const void *b)
{
    Process *p1 = (Process *)a;
    Process *p2 = (Process *)b;
    return p1->remaining_time - p2->remaining_time;
}

// Function to calculate median of an array
float median(int arr[], int n)
{
    // Create a copy of the array to avoid modifying the original
    int *temp = (int *)malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++)
    {
        temp[i] = arr[i];
    }

    // Sort the array
    for (int i = 0; i < n; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (temp[i] > temp[j])
            {
                int t = temp[i];
                temp[i] = temp[j];
                temp[j] = t;
            }
        }
    }

    float med;
    if (n % 2 == 0)
    {
        med = (temp[n / 2] + temp[n / 2 - 1]) / 2.0;
    }
    else
    {
        med = temp[n / 2];
    }

    free(temp);
    return med;
}

// Function to calculate mean of an array
float mean(int arr[], int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++)
    {
        sum += arr[i];
    }
    return (float)sum / n;
}

// Function to calculate fairness index using Jain's fairness formula
float calculateFairnessIndex(Process processes[], int n)
{
    float sum_squared = 0;
    float squared_sum = 0;

    for (int i = 0; i < n; i++)
    {
        int waiting_time = processes[i].completion_time - processes[i].arrival_time - processes[i].burst_time;
        float normalized_wait = (float)(waiting_time + 1) / (processes[i].burst_time + 1); // +1 to avoid divide by zero

        sum_squared += normalized_wait;
        squared_sum += normalized_wait * normalized_wait;
    }

    sum_squared = sum_squared * sum_squared;
    return sum_squared / (n * squared_sum);
}

// Function to count starved processes (those that miss their deadlines)
int calculateStarvationCount(Process processes[], int n)
{
    int count = 0;
    for (int i = 0; i < n; i++)
    {
        if (processes[i].completion_time > processes[i].deadline + processes[i].arrival_time)
        {
            count++;
        }
    }
    return count;
}

// Function to calculate load balancing efficiency
float calculateLoadBalancingEfficiency(Process processes[], int n, int total_time)
{
    int total_busy_time = 0;
    for (int i = 0; i < n; i++)
    {
        total_busy_time += processes[i].burst_time;
    }

    return (float)total_busy_time / total_time;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Open the input file
    FILE *file = fopen(argv[1], "r");
    if (file == NULL)
    {
        printf("Error opening file: %s\n", argv[1]);
        return 1;
    }

    // Read the number of processes
    int n;
    if (fscanf(file, "%d", &n) != 1)
    {
        printf("Error reading number of processes\n");
        fclose(file);
        return 1;
    }

    // Allocate memory for processes
    Process *processes = (Process *)malloc(sizeof(Process) * n);

    // Read process information from file
    for (int i = 0; i < n; i++)
    {
        if (fscanf(file, "%d %d %d %d %d %d %d",
                   &processes[i].pid,
                   &processes[i].arrival_time,
                   &processes[i].burst_time,
                   &processes[i].deadline,
                   &processes[i].criticality,
                   &processes[i].period,
                   &processes[i].nice) != 7)
        {
            printf("Error reading process information\n");
            fclose(file);
            free(processes);
            return 1;
        }

        processes[i].remaining_time = processes[i].burst_time;
        processes[i].completed = 0;
        processes[i].start_time = -1; // -1 indicates not started yet
        processes[i].completion_time = 0;
        processes[i].in_ready_queue = 0;
    }

    fclose(file);

    // Create ready queue
    ReadyQueue *ready_queue = createReadyQueue(n);

    // Initialize simulation variables
    int current_time = 0;
    int completed_processes = 0;

    // Simulation loop
    while (completed_processes < n)
    {
        // Step 1: Add arrived processes to the ReadyQueue
        for (int i = 0; i < n; i++)
        {
            if (processes[i].arrival_time <= current_time &&
                !processes[i].in_ready_queue &&
                !processes[i].completed)
            {
                addToReadyQueue(ready_queue, processes[i]);
                processes[i].in_ready_queue = 1;

                // Update the original array
                Process *proc = &processes[i];
                proc->in_ready_queue = 1;
            }
        }

        // Step 2: If ReadyQueue is not empty, schedule processes
        if (ready_queue->size > 0)
        {
            // Sort ready queue by remaining time (SRPT)
            qsort(ready_queue->processes, ready_queue->size, sizeof(Process), compareRemainingTime);

            // Calculate time quantum based on mean and median of burst times
            int bt_list[ready_queue->size];
            for (int i = 0; i < ready_queue->size; i++)
            {
                bt_list[i] = ready_queue->processes[i].remaining_time;
            }

            float mean_bt = mean(bt_list, ready_queue->size);
            float median_bt = median(bt_list, ready_queue->size);
            int time_quantum = (int)((mean_bt + median_bt) / 2);

            // Ensure time quantum is at least 1
            if (time_quantum < 1)
                time_quantum = 1;

            // Step 3: Execute first process in ReadyQueue
            Process current_process = removeFromReadyQueue(ready_queue, 0);

            // Find the process in the original array
            int idx = -1;
            for (int i = 0; i < n; i++)
            {
                if (processes[i].pid == current_process.pid)
                {
                    idx = i;
                    break;
                }
            }

            if (idx == -1)
            {
                printf("Error: Process not found in original array\n");
                continue;
            }

            // Record start time if this is the first time the process runs
            if (processes[idx].start_time == -1)
            {
                processes[idx].start_time = current_time;
            }

            if (current_process.remaining_time <= time_quantum)
            {
                // Process completes execution
                current_time += current_process.remaining_time;
                processes[idx].remaining_time = 0;
                processes[idx].completed = 1;
                processes[idx].completion_time = current_time;
                processes[idx].in_ready_queue = 0;
                completed_processes++;
            }
            else
            {
                // Process is preempted
                current_time += time_quantum;
                processes[idx].remaining_time -= time_quantum;

                // Add process back to ready queue
                processes[idx].in_ready_queue = 0; // Reset flag before adding back
                addToReadyQueue(ready_queue, processes[idx]);
                processes[idx].in_ready_queue = 1;
            }
        }
        else
        {
            // No process in queue, increment time
            current_time++;
        }
    }

    // Calculate metrics
    float total_turnaround_time = 0;
    float total_waiting_time = 0;
    float total_response_time = 0;

    for (int i = 0; i < n; i++)
    {
        int turnaround_time = processes[i].completion_time - processes[i].arrival_time;
        int waiting_time = turnaround_time - processes[i].burst_time;
        int response_time = processes[i].start_time - processes[i].arrival_time;

        total_turnaround_time += turnaround_time;
        total_waiting_time += waiting_time;
        total_response_time += response_time;
    }

    float avg_turnaround_time = total_turnaround_time / n;
    float avg_waiting_time = total_waiting_time / n;
    float avg_response_time = total_response_time / n;
    float throughput = (float)n / processes[n - 1].completion_time;
    float fairness_index = calculateFairnessIndex(processes, n);
    int starvation_count = calculateStarvationCount(processes, n);
    float load_balancing_efficiency = calculateLoadBalancingEfficiency(processes, n, current_time);

    // Write output to CSV file
    printf("Metric,Value\n");
    printf("Average Turnaround Time,%.2f\n", avg_turnaround_time);
    printf("Average Waiting Time,%.2f\n", avg_waiting_time);
    printf("Average Response Time,%.2f\n", avg_response_time);
    printf("Throughput,%.2f\n", throughput);
    printf("Fairness Index,%.2f\n", fairness_index);
    printf("Starvation Count,%d\n", starvation_count);
    printf("Load Balancing Efficiency,%.2f\n", load_balancing_efficiency);


    free(processes);
    free(ready_queue->processes);
    free(ready_queue);

    return 0;
}

