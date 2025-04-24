#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_PROCESSES 100
#define MAX_GANTT_CHART_SIZE 1000
#define MAX_FILENAME_LENGTH 256
#define DEFAULT_NICE_VALUE 0
#define MIN_NICE_VALUE -20
#define MAX_NICE_VALUE 19
#define DEFAULT_TIMESLICE 1
#define MIN_VRUNTIME_THRESHOLD 0.01

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
    int first_execution_time;
    int deadline;
    int criticality;
    int period;
    int nice;
    double vruntime;
    double weight;
    bool executed;
    bool completed;
} Process;

// CFS parameters
typedef struct
{
    double min_granularity;
    double latency;
    double target_latency;
    int total_weight;
} CFSParams;

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
    double fairness_index;
    int starvation_count;
    double load_balancing_efficiency;
} Metrics;

// Red-Black Tree Node for CFS
typedef struct RBNode
{
    Process *process;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;
    int color; // 0 for black, 1 for red
} RBNode;

// Global variables
Process processes[MAX_PROCESSES];
GanttChartItem gantt_chart[MAX_GANTT_CHART_SIZE];
int gantt_chart_size = 0;
Metrics metrics;
RBNode *root = NULL;

// Function prototypes
int readProcessesFromFile(Process *processes, const char *filename);
void writeDefaultInputFile(const char *filename);
void calculateWeight(Process *process);
RBNode *createNode(Process *process);
RBNode *insert(RBNode *root, Process *process);
Process *extractMinVruntime(RBNode **root);
void runCFS(Process *processes, int n, CFSParams *cfs);
void calculateMetrics(Process *processes, int n, int total_time);
void displayGanttChart();
void displayProcessDetails(Process *processes, int n);
void displayMetrics();
void addToGanttChart(int process_id, int start_time, int end_time);

// Insert a process into the RB tree (simplified for this implementation)
RBNode *insert(RBNode *root, Process *process)
{
    if (root == NULL)
    {
        return createNode(process);
    }

    // Lower vruntime goes to the left
    if (process->vruntime < root->process->vruntime)
    {
        root->left = insert(root->left, process);
        root->left->parent = root;
    }
    else
    {
        root->right = insert(root->right, process);
        root->right->parent = root;
    }

    return root;
}

// Create a new RB tree node
RBNode *createNode(Process *process)
{
    RBNode *node = (RBNode *)malloc(sizeof(RBNode));
    node->process = process;
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->color = 1; // New nodes are red
    return node;
}

// Extract the process with minimum vruntime (leftmost node)
Process *extractMinVruntime(RBNode **root)
{
    if (*root == NULL)
    {
        return NULL;
    }

    RBNode *current = *root;
    RBNode *parent = NULL;

    // Find the leftmost node
    while (current->left != NULL)
    {
        parent = current;
        current = current->left;
    }

    Process *process = current->process;

    // Handle removal and restructuring
    if (parent == NULL)
    {
        // Root node is being removed
        *root = current->right;
        if (*root != NULL)
        {
            (*root)->parent = NULL;
        }
    }
    else
    {
        parent->left = current->right;
        if (current->right != NULL)
        {
            current->right->parent = parent;
        }
    }

    free(current);
    return process;
}

// Calculate the weight based on nice value (similar to Linux CFS)
void calculateWeight(Process *process)
{
    // Map criticality to nice values
    process->nice = MAX_NICE_VALUE - (process->criticality * 3);
    if (process->nice < MIN_NICE_VALUE)
        process->nice = MIN_NICE_VALUE;
    if (process->nice > MAX_NICE_VALUE)
        process->nice = MAX_NICE_VALUE;

    // Weight calculation - approximation of Linux's formula
    process->weight = 1024.0 / (0.8 * process->nice + 1024);
}

// Function to read processes from a file
int readProcessesFromFile(Process *processes, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error opening file %s. Creating a default input file...\n", filename);
        writeDefaultInputFile(filename);

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
                   &processes[i].nice) != 7)
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
        processes[i].vruntime = 0;
        calculateWeight(&processes[i]);
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
    fprintf(file, "5 8 5 0 5 12 6\n");
    fprintf(file, "6 10 3 18 8 0 7\n");
    fprintf(file, "7 12 7 30 4 15 5\n");
    fprintf(file, "8 14 1 17 10 0 9\n");
    fprintf(file, "9 16 9 0 2 20 2\n");
    fprintf(file, "10 18 4 25 7 0 6\n");

    fclose(file);
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

// Main CFS algorithm
void runCFS(Process *processes, int n, CFSParams *cfs)
{
    int current_time = 0;
    int completed_processes = 0;
    int idle_time = 0;
    root = NULL;

    // Timeslice calculation based on weights is a key aspect of CFS
    double total_weight = 0;

    // Initial weight calculation
    for (int i = 0; i < n; i++)
    {
        total_weight += processes[i].weight;
    }

    cfs->total_weight = total_weight;

    // Continue until all processes are completed
    while (completed_processes < n)
    {
        // Check for newly arrived processes
        for (int i = 0; i < n; i++)
        {
            if (processes[i].arrival_time == current_time && !processes[i].completed)
            {
                // For newly arrived processes, set the vruntime
                // In real CFS, this is more complex, but we'll use a simplified approach
                if (!processes[i].executed)
                {
                    // Give new processes a small advantage
                    processes[i].vruntime = 0;
                }
                root = insert(root, &processes[i]);
            }
        }

        // If no process is ready, increment time and add idle to Gantt chart
        if (root == NULL)
        {
            current_time++;
            idle_time++;
            if (idle_time == 1)
            {
                addToGanttChart(-1, current_time - 1, current_time);
            }
            else
            {
                gantt_chart[gantt_chart_size - 1].end_time = current_time;
            }
            continue;
        }
        else
        {
            idle_time = 0;
        }

        // Get the process with the minimum vruntime
        Process *current_process = extractMinVruntime(&root);

        // Calculate dynamic timeslice based on process weight and target latency
        // In real CFS, this depends on many factors including load and sched_latency
        double active_processes = n - completed_processes;
        cfs->target_latency = fmax(cfs->min_granularity * active_processes, cfs->latency);

        // Calculate timeslice - simplified compared to real CFS
        double timeslice = (current_process->weight / cfs->total_weight) * cfs->target_latency;
        if (timeslice < 1)
            timeslice = 1;

        // Cap the timeslice to the remaining burst time
        int execution_time = (int)fmin(timeslice, current_process->remaining_burst);

        // If process is executing for the first time, record response time
        if (!current_process->executed)
        {
            current_process->first_execution_time = current_time;
            current_process->executed = true;
        }

        // Add to Gantt chart
        addToGanttChart(current_process->id, current_time, current_time + execution_time);

        // Update process information
        current_process->remaining_burst -= execution_time;

        // In CFS, vruntime increases based on actual runtime weighted by process weight
        // Lower weight (higher priority) processes accumulate vruntime more slowly
        current_process->vruntime += execution_time / current_process->weight;

        current_time += execution_time;

        // Check if process is completed
        if (current_process->remaining_burst <= 0)
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
            // Put the process back in the tree
            root = insert(root, current_process);
        }

        // Check for newly arrived processes during this time slice
        for (int i = 0; i < n; i++)
        {
            if (!processes[i].executed && !processes[i].completed &&
                processes[i].arrival_time > current_time - execution_time &&
                processes[i].arrival_time <= current_time)
            {

                // Set initial vruntime for newly arrived process
                // In real CFS, this would be the min_vruntime to avoid starvation
                processes[i].vruntime = 0;
                root = insert(root, &processes[i]);
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
    int starvation_threshold = 20;
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

    // Calculate throughput
    metrics.throughput = (double)n / total_time;

    // Calculate Jain's fairness index
    metrics.fairness_index = (sum * sum) / (n * sum_of_squares);

    // Record starvation count
    metrics.starvation_count = starved_count;

    // Calculate load balancing efficiency
    double mean_waiting_time = total_waiting_time / n;
    double variance = 0.0;

    for (int i = 0; i < n; i++)
    {
        variance += pow(processes[i].waiting_time - mean_waiting_time, 2);
    }

    double std_dev = sqrt(variance / n);
    double coefficient_of_variation = std_dev / mean_waiting_time;

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


// Display metrics
void displayProcessDetails(Process *processes, int n)
{
    printf("ProcessID,ArrivalTime,BurstTime,CompletionTime,TurnaroundTime,WaitingTime,ResponseTime,Deadline,Criticality,Period,Nice,Weight\n");
    for (int i = 0; i < n; i++)
    {
        printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f\n",
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
               processes[i].nice,
               processes[i].weight);
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
// Update the main function to accept command-line arguments
int main(int argc, char *argv[])
{
    int n;
    CFSParams cfs;
    char filename[MAX_FILENAME_LENGTH];

    // Initialize CFS parameters (approximating Linux defaults)
    cfs.min_granularity = 1.0; // Minimum timeslice (ms)
    cfs.latency = 20.0;        // Target latency (ms)
    cfs.target_latency = 20.0; // Initial target latency


    // Check if filename is provided as command-line argument
    if (argc > 1)
    {
        strncpy(filename, argv[1], MAX_FILENAME_LENGTH - 1);
        filename[MAX_FILENAME_LENGTH - 1] = '\0'; // Ensure null-terminated
    }
    else
    {
        // Use default filename if no argument is provided
        strcpy(filename, "input.txt");
        printf("No input file specified. Using default: input.txt\n");
    }

    // Read processes from file
    n = readProcessesFromFile(processes, filename);

    // Run the CFS algorithm
    runCFS(processes, n, &cfs);

    // Display results
    /*displayProcessDetails(processes, n);*/
    // displayGanttChart();
    displayMetrics();


    return 0;
}
