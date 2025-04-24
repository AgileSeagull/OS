#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_PROCESSES 100
#define MAX_QUEUE_SIZE 100
#define MAX_GANTT_CHART_SIZE 1000
#define MAX_FILENAME_LENGTH 256

// Process structure
typedef struct
{
    int id;
    int arrival_time;
    int burst_time;
    int remaining_burst;
    int completion_time;
    int waiting_time;
    int turnaround_time;
    int response_time;
    int first_execution_time; // For response time calculation
    int deadline;             // For real-time processes
    int criticality;          // Higher for safety-critical tasks (1-10)
    int period;               // For periodic tasks
    int system_priority;      // Manual override or industry standard
    bool executed;            // Flag to check if process has started execution
    bool completed;           // Flag to check if process has completed
} Process;

// Dynamic Time Quantum structure
typedef struct
{
    double base;               // Base time quantum
    double current;            // Current time quantum after adjustment
    double load_factor;        // CPU load factor (0.0 to 1.0)
    double criticality_weight; // Weight for criticality (Wc)
    double deadline_weight;    // Weight for deadline (Wf)
    double aging_weight;       // Weight for aging (Wa)
    double priority_weight;    // Weight for system priority (Ws)
} DynamicQuantum;

// Ready Queue structure
typedef struct
{
    Process *processes[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} ReadyQueue;

// Gantt Chart structure
typedef struct
{
    int process_id;
    int start_time;
    int end_time;
} GanttChartItem;

// Benchmarking metrics
typedef struct
{
    double avg_turnaround_time;
    double avg_waiting_time;
    double avg_response_time;
    double throughput;
    double fairness_index; // Jain's fairness index
    int starvation_count;  // Number of starved processes
    double load_balancing_efficiency;
} Metrics;

// Global variables
Process processes[MAX_PROCESSES];
GanttChartItem gantt_chart[MAX_GANTT_CHART_SIZE];
int gantt_chart_size = 0;
Metrics metrics;

// Function prototypes
void initializeQueue(ReadyQueue *queue);
bool isQueueEmpty(ReadyQueue *queue);
bool isQueueFull(ReadyQueue *queue);
void enqueue(ReadyQueue *queue, Process *process);
Process *dequeue(ReadyQueue *queue);
void calculateDynamicPriority(Process *process, int current_time, DynamicQuantum *dtq);
double calculateAgingFactor(Process *process, int current_time);
void sortQueueByPriority(ReadyQueue *queue, int current_time, DynamicQuantum *dtq);
void runDPS_DTQ(Process *processes, int n, DynamicQuantum *dtq);
void calculateMetrics(Process *processes, int n, int total_time);
void displayGanttChart();
void displayProcessDetails(Process *processes, int n);
void displayMetrics();
void addToGanttChart(int process_id, int start_time, int end_time);
int readProcessesFromFile(Process *processes, const char *filename);
void writeDefaultInputFile(const char *filename);

// Initialize the ready queue
void initializeQueue(ReadyQueue *queue)
{
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
}

// Check if the queue is empty
bool isQueueEmpty(ReadyQueue *queue)
{
    return queue->size == 0;
}

// Check if the queue is full
bool isQueueFull(ReadyQueue *queue)
{
    return queue->size == MAX_QUEUE_SIZE;
}

// Add a process to the queue
void enqueue(ReadyQueue *queue, Process *process)
{
    if (isQueueFull(queue))
    {
        printf("Queue is full! Cannot add more processes.\n");
        return;
    }

    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->processes[queue->rear] = process;
    queue->size++;
}

// Remove a process from the queue
Process *dequeue(ReadyQueue *queue)
{
    if (isQueueEmpty(queue))
    {
        return NULL;
    }

    Process *process = queue->processes[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    return process;
}

// Calculate the aging factor for a process
double calculateAgingFactor(Process *process, int current_time)
{
    // Aging factor increases as the waiting time increases
    int waiting_time = current_time - process->arrival_time -
                       (process->burst_time - process->remaining_burst);

    // Normalize aging factor between 0 and 1, with a max of 10 time units for full effect
    double aging_factor = waiting_time > 0 ? (double)waiting_time / 10.0 : 0.0;
    if (aging_factor > 1.0)
        aging_factor = 1.0;

    return aging_factor;
}

// Calculate dynamic priority for a process
void calculateDynamicPriority(Process *process, int current_time, DynamicQuantum *dtq)
{
    double priority = 0.0;

    // Criticality component (higher criticality = higher priority)
    double criticality_component = process->criticality / 10.0; // Normalize between 0 and 1

    // Deadline component (closer to deadline = higher priority)
    double deadline_component = 0.0;
    if (process->deadline > 0)
    {
        int time_to_deadline = process->deadline - current_time;
        if (time_to_deadline <= 0)
        {
            deadline_component = 1.0; // Maximum priority if deadline passed or imminent
        }
        else
        {
            deadline_component = 1.0 / (1.0 + time_to_deadline); // Inverse relation to time left
        }
    }

    // Period component (shorter period = higher priority)
    double period_component = 0.0;
    if (process->period > 0)
    {
        period_component = 1.0 / process->period; // Inverse relation to period
    }

    // Aging component (longer wait = higher priority)
    double aging_component = calculateAgingFactor(process, current_time);

    // System priority component (higher system priority = higher priority)
    double system_priority_component = process->system_priority / 10.0; // Normalize between 0 and 1

    // Calculate final priority with weighted components
    priority = (dtq->criticality_weight * criticality_component) +
               (dtq->deadline_weight * deadline_component) +
               (dtq->aging_weight * aging_component) +
               (dtq->priority_weight * system_priority_component);

    // Adjust dynamic time quantum based on priority and load
    dtq->current = dtq->base * (1.0 + priority) * (1.0 - 0.5 * dtq->load_factor);

    // Store the calculated priority in the remaining_burst for comparison (for sorting)
    // This is just a hack for the demo - in a real implementation, we'd add a priority field
    process->system_priority = (int)(priority * 100); // Scale for easier comparison
}

// Sort the queue based on calculated priorities
void sortQueueByPriority(ReadyQueue *queue, int current_time, DynamicQuantum *dtq)
{
    // Calculate priorities for all processes in the queue
    for (int i = 0; i < queue->size; i++)
    {
        int idx = (queue->front + i) % MAX_QUEUE_SIZE;
        calculateDynamicPriority(queue->processes[idx], current_time, dtq);
    }

    // Simple bubble sort for demonstration (not efficient for large queues)
    for (int i = 0; i < queue->size - 1; i++)
    {
        for (int j = 0; j < queue->size - i - 1; j++)
        {
            int idx1 = (queue->front + j) % MAX_QUEUE_SIZE;
            int idx2 = (queue->front + j + 1) % MAX_QUEUE_SIZE;

            if (queue->processes[idx1]->system_priority < queue->processes[idx2]->system_priority)
            {
                // Swap processes
                Process *temp = queue->processes[idx1];
                queue->processes[idx1] = queue->processes[idx2];
                queue->processes[idx2] = temp;
            }
        }
    }
}

// Add an entry to the Gantt chart
void addToGanttChart(int process_id, int start_time, int end_time)
{
    if (gantt_chart_size < MAX_GANTT_CHART_SIZE)
    {
        gantt_chart[gantt_chart_size].process_id = process_id;
        gantt_chart[gantt_chart_size].start_time = start_time;
        gantt_chart[gantt_chart_size].end_time = end_time;
        gantt_chart_size++;
    }
    else
    {
        printf("Gantt chart is full!\n");
    }
}

// Function to read processes from a file
int readProcessesFromFile(Process *processes, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error opening file %s. Creating a default input file...\n", filename);
        writeDefaultInputFile(filename);

        // Try opening the file again
        file = fopen(filename, "r");
        if (file == NULL)
        {
            printf("Failed to create default input file. Exiting...\n");
            exit(1);
        }
        printf("Default input file created successfully.\n");
    }

    int n;
    if (fscanf(file, "%d", &n) != 1)
    {
        printf("Error reading number of processes from file.\n");
        fclose(file);
        exit(1);
    }

    if (n <= 0 || n > MAX_PROCESSES)
    {
        printf("Invalid number of processes: %d (must be between 1 and %d)\n", n, MAX_PROCESSES);
        fclose(file);
        exit(1);
    }

    // Read process data
    for (int i = 0; i < n; i++)
    {
        if (fscanf(file, "%d %d %d %d %d %d %d",
                   &processes[i].id,
                   &processes[i].arrival_time,
                   &processes[i].burst_time,
                   &processes[i].deadline,
                   &processes[i].criticality,
                   &processes[i].period,
                   &processes[i].system_priority) != 7)
        {
            printf("Error reading data for process %d\n", i + 1);
            fclose(file);
            exit(1);
        }

        // Initialize other fields
        processes[i].remaining_burst = processes[i].burst_time;
        processes[i].completion_time = 0;
        processes[i].waiting_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].response_time = 0;
        processes[i].first_execution_time = -1;
        processes[i].executed = false;
        processes[i].completed = false;
    }

    fclose(file);
    return n;
}

// Function to write a default input file if none exists
void writeDefaultInputFile(const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        printf("Error creating default input file.\n");
        return;
    }

    // Number of processes
    int n = 10;
    fprintf(file, "%d\n", n);

    // Sample process data: id, arrival_time, burst_time, deadline, criticality, period, system_priority
    fprintf(file, "1 0 8 20 7 0 5\n");
    fprintf(file, "2 2 4 15 9 0 8\n");
    fprintf(file, "3 4 2 10 6 10 3\n");
    fprintf(file, "4 6 6 25 3 0 4\n");
    fprintf(file, "5 8 5 0 5 12 6\n"); // No deadline
    fprintf(file, "6 10 3 18 8 0 7\n");
    fprintf(file, "7 12 7 30 4 15 5\n");
    fprintf(file, "8 14 1 17 10 0 9\n");
    fprintf(file, "9 16 9 0 2 20 2\n"); // No deadline
    fprintf(file, "10 18 4 25 7 0 6\n");

    fclose(file);
}

// Main DPS-DTQ algorithm
void runDPS_DTQ(Process *processes, int n, DynamicQuantum *dtq)
{
    ReadyQueue ready_queue;
    initializeQueue(&ready_queue);

    int current_time = 0;
    int completed_processes = 0;
    int idle_time = 0;

    // Continue until all processes are completed
    while (completed_processes < n)
    {
        // Check for newly arrived processes
        for (int i = 0; i < n; i++)
        {
            if (processes[i].arrival_time == current_time)
            {
                enqueue(&ready_queue, &processes[i]);
            }
        }

        // If ready queue is empty, increase time and continue
        if (isQueueEmpty(&ready_queue))
        {
            current_time++;
            idle_time++;
            // Add idle time to Gantt chart
            if (idle_time == 1)
            {
                addToGanttChart(-1, current_time - 1, current_time); // -1 represents idle
            }
            else
            {
                // Update the end time of the last idle entry
                gantt_chart[gantt_chart_size - 1].end_time = current_time;
            }
            continue;
        }
        else
        {
            idle_time = 0;
        }

        // Update CPU load factor based on queue size
        dtq->load_factor = (double)ready_queue.size / n;

        // Sort the ready queue based on the dynamic priority
        sortQueueByPriority(&ready_queue, current_time, dtq);

        // Get the highest priority process
        Process *current_process = dequeue(&ready_queue);

        // If process is executing for the first time, record response time
        if (!current_process->executed)
        {
            current_process->first_execution_time = current_time;
            current_process->executed = true;
        }

        // Calculate time quantum for this process
        calculateDynamicPriority(current_process, current_time, dtq);
        int time_quantum = (int)dtq->current;
        if (time_quantum < 1)
            time_quantum = 1; // Minimum time quantum

        // Determine how long the process will run
        int execution_time = (current_process->remaining_burst < time_quantum) ? current_process->remaining_burst : time_quantum;

        // Add to Gantt chart
        addToGanttChart(current_process->id, current_time, current_time + execution_time);

        // Update process information
        current_process->remaining_burst -= execution_time;
        current_time += execution_time;

        // Check if process is completed
        if (current_process->remaining_burst == 0)
        {
            current_process->completed = true;
            current_process->completion_time = current_time;
            current_process->turnaround_time = current_process->completion_time - current_process->arrival_time;
            current_process->waiting_time = current_process->turnaround_time - current_process->burst_time;
            current_process->response_time = current_process->first_execution_time - current_process->arrival_time;
            completed_processes++;
        }
        else
        {
            // Put the process back in the ready queue
            enqueue(&ready_queue, current_process);
        }

        // Check for newly arrived processes during this time slice
        for (int i = 0; i < n; i++)
        {
            if (!processes[i].executed &&
                processes[i].arrival_time > current_time - execution_time &&
                processes[i].arrival_time <= current_time)
            {
                enqueue(&ready_queue, &processes[i]);
            }
        }
    }

    // Calculate benchmarking metrics
    calculateMetrics(processes, n, current_time);
}

// Calculate various performance metrics
void calculateMetrics(Process *processes, int n, int total_time)
{
    double total_turnaround_time = 0.0;
    double total_waiting_time = 0.0;
    double total_response_time = 0.0;
    double sum_of_squares = 0.0;
    double sum = 0.0;
    int starvation_threshold = 20; // Define starvation as waiting > 20 time units
    int starved_count = 0;

    for (int i = 0; i < n; i++)
    {
        total_turnaround_time += processes[i].turnaround_time;
        total_waiting_time += processes[i].waiting_time;
        total_response_time += processes[i].response_time;

        // For Jain's fairness index
        sum += processes[i].turnaround_time;
        sum_of_squares += (double)processes[i].turnaround_time * processes[i].turnaround_time;

        // Check for starvation
        if (processes[i].waiting_time > starvation_threshold)
        {
            starved_count++;
        }
    }

    // Calculate average metrics
    metrics.avg_turnaround_time = total_turnaround_time / n;
    metrics.avg_waiting_time = total_waiting_time / n;
    metrics.avg_response_time = total_response_time / n;

    // Calculate throughput (processes per unit time)
    metrics.throughput = (double)n / total_time;

    // Calculate Jain's fairness index
    // This ranges from 1/n (worst case) to 1 (best case)
    metrics.fairness_index = (sum * sum) / (n * sum_of_squares);

    // Record starvation count
    metrics.starvation_count = starved_count;

    // Calculate load balancing efficiency
    // (ideally would measure across CPUs, but here we'll use a simple measure of variance in waiting times)
    double mean_waiting_time = total_waiting_time / n;
    double variance = 0.0;

    for (int i = 0; i < n; i++)
    {
        variance += pow(processes[i].waiting_time - mean_waiting_time, 2);
    }

    double std_dev = sqrt(variance / n);
    double coefficient_of_variation = std_dev / mean_waiting_time;

    // Lower coefficient of variation indicates better load balancing
    metrics.load_balancing_efficiency = 1.0 / (1.0 + coefficient_of_variation);
}

// Display Gantt chart
void displayGanttChart()
{
    printf("\n\nGantt Chart:\n");

    // Print top border
    printf(" ");
    for (int i = 0; i < gantt_chart_size; i++)
    {
        int duration = gantt_chart[i].end_time - gantt_chart[i].start_time;
        for (int j = 0; j < duration; j++)
        {
            printf("--");
        }
        printf(" ");
    }
    printf("\n|");

    // Print process IDs
    for (int i = 0; i < gantt_chart_size; i++)
    {
        int duration = gantt_chart[i].end_time - gantt_chart[i].start_time;
        for (int j = 0; j < duration; j++)
        {
            if (gantt_chart[i].process_id == -1)
            {
                printf("I "); // I for Idle
            }
            else
            {
                printf("P%d", gantt_chart[i].process_id);
            }
            if (j < duration - 1)
            {
                printf(" ");
            }
        }
        printf("|");
    }

    // Print bottom border
    printf("\n ");
    for (int i = 0; i < gantt_chart_size; i++)
    {
        int duration = gantt_chart[i].end_time - gantt_chart[i].start_time;
        for (int j = 0; j < duration; j++)
        {
            printf("--");
        }
        printf(" ");
    }

    // Print time markers
    printf("\n");
    for (int i = 0; i < gantt_chart_size; i++)
    {
        printf("%2d", gantt_chart[i].start_time);
        int duration = gantt_chart[i].end_time - gantt_chart[i].start_time;
        for (int j = 0; j < duration * 2 - 1; j++)
        {
            printf(" ");
        }
    }
    printf("%2d\n", gantt_chart[gantt_chart_size - 1].end_time);
}

// Display process details

void displayProcessDetails(Process *processes, int n)
{
    printf("ProcessID,ArrivalTime,BurstTime,CompletionTime,TurnaroundTime,WaitingTime,ResponseTime,Deadline,Criticality,Period,SystemPriority\n");

    for (int i = 0; i < n; i++)
    {
        printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
               processes[i].id,
               processes[i].arrival_time,
               processes[i].burst_time,
               processes[i].completion_time,
               processes[i].turnaround_time,
               processes[i].waiting_time,
               processes[i].response_time,
               processes[i].deadline,
               processes[i].criticality,
               processes[i].period,
               processes[i].system_priority);
    }
}

void displayMetrics()
{
    printf("Metric,Value\n");
    printf("Average Turnaround Time,%.2f\n", metrics.avg_turnaround_time);
    printf("Average Waiting Time,%.2f\n", metrics.avg_waiting_time);
    printf("Average Response Time,%.2f\n", metrics.avg_response_time);
    printf("Throughput,%.2f\n", metrics.throughput);
    printf("Fairness Index,%.2f\n", metrics.fairness_index);
    printf("Starvation Count,%d\n", metrics.starvation_count);
    printf("Load Balancing Efficiency,%.2f\n", metrics.load_balancing_efficiency);
}


// Main function
// Main function - modified to accept command line arguments
int main(int argc, char *argv[])
{
    int n;
    DynamicQuantum dtq;
    char filename[MAX_FILENAME_LENGTH];

    // Initialize dynamic time quantum parameters
    dtq.base = 4.0; // Base time quantum
    dtq.current = dtq.base;
    dtq.load_factor = 0.0;
    dtq.criticality_weight = 0.35;
    dtq.deadline_weight = 0.30;
    dtq.aging_weight = 0.25;
    dtq.priority_weight = 0.10;


    // Check if a filename was provided as a command line argument
    if (argc > 1)
    {
        strncpy(filename, argv[1], MAX_FILENAME_LENGTH - 1);
        filename[MAX_FILENAME_LENGTH - 1] = '\0'; // Ensure null termination
    }
    else
    {
        // Use default filename if no argument provided
        strcpy(filename, "input.txt");
        printf("No input file specified. Using default: %s\n", filename);
    }

    // Read processes from file
    n = readProcessesFromFile(processes, filename);

    // Run the DPS-DTQ algorithm
    runDPS_DTQ(processes, n, &dtq);

    // Display results
    /*displayProcessDetails(processes, n);*/
    // displayGanttChart();
    displayMetrics();


    return 0;
}
