#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "osPRNG.h"

#define MAX_PROCESSES 100
#define MAX_PRIORITY_LEVELS 8
#define MAX_QUEUE_SIZE 100
#define PROB_OF_IO_REQ 10
#define PROB_OF_IO_TERM 4

typedef struct
{
    int pid;
    int arrival_time;
    int burst_time;
    float estimated_burst_time;
    int remaining_time;
    int priority;
    int quantum;
    int state;
    int ready_time;
    int wait_time;
    int total_time;
} Process;

typedef struct
{
    Process *queue[MAX_QUEUE_SIZE];
    int rear;
} PriorityQueue;

typedef struct
{
    Process *processes[MAX_PROCESSES];
    int count;
} SJFQueue;

PriorityQueue priority_queues[MAX_PRIORITY_LEVELS];
SJFQueue sjf_queue;
Process processes[MAX_PROCESSES];
int process_count = 0;
int clock = 0;
int scheduling_algorithm = 0;
float aging_factor = 0.5;
bool verbose = false;
bool process_finished = false;
Process *current_process = NULL;

float last_burst_time = 5;
float last_estimated_burst_time = 5;

int finishedIO[MAX_PROCESSES];
int remaining = 0;

int remaining_quantum = 0;

int IOReq()
{
    if (osPRNG() % PROB_OF_IO_REQ == 0)
        return 1;
    else
        return 0;
}

int IOTerm()
{
    if (osPRNG() % PROB_OF_IO_TERM == 0)
        return 1;
    else
        return 0;
}

void termList(int pid)
{
    remaining = 1;
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (finishedIO[i] == -1)
        {
            finishedIO[i] = pid;
            break;
        };
    };
};

void printTerm()
{
    if (remaining == 1)
    {
        for (int i = 0; i < MAX_PROCESSES; i++)
        {
            if (finishedIO[i] != -1)
            {
                printf("%d:", finishedIO[i]);
            }
        };
    }
    else
    {
        printf("0:");
    }
};

void clearlist()
{
    remaining = 0;
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        finishedIO[i] = -1;
    };
};

int calc_q(int priority)
{
    return (1 << priority);
}

void initialize_PrioQ()
{
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        priority_queues[i].rear = -1;
    }
}

Process *removeFromQueu(PriorityQueue *pq)
{
    if (pq->rear == -1)
    {
        return NULL;
    }
    Process *process = pq->queue[0];
    process->state = 1;

    for (int i = 1; i <= pq->rear; i++)
    {
        pq->queue[i - 1] = pq->queue[i];
    }
    pq->rear--;

    remaining_quantum = process->quantum;
    return process;
}

void addToQueu(PriorityQueue *pq, Process *process) 
{
    if (pq->rear == MAX_QUEUE_SIZE - 1)
    {
        return; 
    }
    pq->rear++;
    pq->queue[pq->rear] = process;
    process->state = 0;
    process->quantum = calc_q(process->priority);
}

Process *select_FMP()
{
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        if (priority_queues[i].rear != -1)
        {
            return removeFromQueu(&priority_queues[i]);
        }
    }
    return NULL;
}

void init_SJF()
{
    sjf_queue.count = 0;
}

double next_estimated_time(float last_burst_time, float previous_estimated_time, float aging_factor)
{
    last_estimated_burst_time = (aging_factor * last_burst_time) + ((1 - aging_factor) * previous_estimated_time);
    return last_estimated_burst_time;
}

void add_SJF(Process *process)
{
    if (process->state == 3 || process->remaining_time <= 0)
    {
        return;
    }
    int i = sjf_queue.count - 1;
    process->state = 0;
    if (process->estimated_burst_time == 0)
    {
        process->estimated_burst_time = next_estimated_time(last_burst_time * 1.0, last_estimated_burst_time * 1.0, aging_factor);
    }
    while (i >= 0 && sjf_queue.processes[i]->estimated_burst_time > process->estimated_burst_time)
    {
        sjf_queue.processes[i + 1] = sjf_queue.processes[i];
        i--;
    }
    sjf_queue.processes[i + 1] = process;
    sjf_queue.processes[0]->state = 1;
    sjf_queue.count++;
}

Process *remove_SJF()
{
    if (sjf_queue.count == 0)
    {
        return NULL;
    }
    Process *process = sjf_queue.processes[0];
    for (int i = 1; i < sjf_queue.count; i++)
    {
        sjf_queue.processes[i - 1] = sjf_queue.processes[i];
    }
    process->state = 1;
    sjf_queue.count--;
    return process;
}

int get_Id()
{
    if (scheduling_algorithm == 1)
    {
        if (sjf_queue.processes[0] != NULL)
        {
            return sjf_queue.processes[0]->pid;
        }
        else
        {
            return -1;
        }
    }
    else if (scheduling_algorithm == 0)
    {
        for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
        {
            if (priority_queues[i].rear != -1)
            {
                return priority_queues[i].queue[0]->pid;
            }
        }
        return -1;
    }
    return -1;
}

Process *select_SJF()
{
    return remove_SJF();
}

int incoming_Time(const void *a, const void *b)
{
    Process *processA = (Process *)a;
    Process *processB = (Process *)b;
    return processA->arrival_time - processB->arrival_time;
}

void parse_Input(char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Erro ao abrir o arquivo de entrada");
        exit(EXIT_FAILURE);
    }

    char line[100];
    while (fgets(line, sizeof(line), file))
    {
        Process process;
        sscanf(line, "%d:%d:%d:%d", &process.pid, &process.arrival_time, &process.burst_time, &process.priority);
        process.remaining_time = process.burst_time;
        process.state = -1;
        process.ready_time = 0;
        process.wait_time = 0;
        process.total_time = 0;
        process.estimated_burst_time = 0;
        process.quantum = -1;
        processes[process_count++] = process;
    }

    fclose(file);

    qsort(processes, process_count, sizeof(Process), incoming_Time);
}

void print_Table()
{
    printf("\nProcesso | Tempo total     | Tempo total     | Tempo total\n");
    printf("         | em estado Ready | em estado Wait  | no processador\n\n");

    int total_execution_time = 0;
    int total_ready_wait_time = 0;
    int min_execution_time = processes[0].total_time;
    int max_execution_time = processes[0].total_time;

    for (int i = 0; i < process_count; i++)
    {
        printf("%-8d | %-15d | %-15d | %-15d\n", processes[i].pid, processes[i].ready_time,
               processes[i].wait_time, processes[i].total_time);

        total_execution_time += processes[i].total_time;
        total_ready_wait_time += processes[i].ready_time + processes[i].wait_time;

        if (processes[i].total_time < min_execution_time)
        {
            min_execution_time = processes[i].total_time;
        }

        if (processes[i].total_time > max_execution_time)
        {
            max_execution_time = processes[i].total_time;
        }
    }

    float avg_execution_time = (float)total_execution_time / process_count;
    float avg_ready_wait_time = (float)total_ready_wait_time / process_count;
    printf("\nTempo total de simulacao: %d\n", clock);
    printf("Numero de processos: %d\n", process_count);
    printf("Menor tempo de execucao: %d\n", min_execution_time);
    printf("Maior tempo de execucao: %d\n", max_execution_time);
    printf("Tempo medio de execucao: %.2f\n", avg_execution_time);
    printf("Tempo medio em Pronto/Espera: %.2f\n", avg_ready_wait_time);
}

void print_process_status1(int remaining_quantum, int remaining_time, bool requiresIO) {
    if (remaining_quantum == 0 && remaining_time > 0 && !requiresIO) {
        printf("preempted\n");
    } else if (requiresIO) {
        printf("idle\n");
    } else if (remaining_time > 0) {
        printf("running\n");
    } else {
        printf("end\n");
    }
}

void print_process_status2(int nextPID, Process *current_process, bool requiresIO) {
    if (nextPID != current_process->pid && current_process->remaining_time > 0 && !requiresIO) {
        printf("preempted\n");
    } else if (requiresIO) {
        printf("idle\n");
    } else if (current_process->remaining_time > 0) {
        printf("running\n");
    } else {
        printf("end\n");
    }
}

void parse_command_line_args(int argc, char **argv)
{
    char *input_filename = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-F") == 0)
        {
            scheduling_algorithm = 0;
        }
        else if (strcmp(argv[i], "-S") == 0)
        {
            scheduling_algorithm = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-' && strlen(argv[i + 1]) < 4)
            {
                aging_factor = atof(argv[++i]);
            }
        }
        else if (strcmp(argv[i], "-v") == 0)
        {
            verbose = true;
        }
        else
        {
            input_filename = argv[i];
        }
    }

    if (input_filename == NULL)
    {
        fprintf(stderr, "Uso: %s [-F | -S [a]] [-v] <inputfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

void handle_arriving_processes()
{
    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].arrival_time == clock && processes[i].state == -1)
        {
            processes[i].state = 0;
            if (scheduling_algorithm == 0)
            {
                addToQueu(&priority_queues[processes[i].priority], &processes[i]);
            }
            else
            {
                add_SJF(&processes[i]);
            }
        }
    }
}

void handle_io_requests()
{
    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].state == 2 && IOTerm())
        {
            processes[i].state = 0;
            termList(processes[i].pid);

            if (scheduling_algorithm == 0)
            {
                addToQueu(&priority_queues[0], &processes[i]);
            }
            else
            {
                add_SJF(&processes[i]);
            }
        }
    }
}

void manage_current_process()
{
    int requiresIO = 0;
    if (scheduling_algorithm == 0 && remaining_quantum == 0)
    {
        current_process = select_FMP();
    }
    else if (scheduling_algorithm == 1)
    {
        current_process = select_SJF();
    }

    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].state == 2)
        {
            processes[i].wait_time++;
        }
        if (processes[i].state == 0)
        {
            processes[i].ready_time++;
        }
    }

    if (current_process != NULL)
    {
        current_process->remaining_time--;
        current_process->total_time++;
        if (scheduling_algorithm == 0)
        {
            remaining_quantum--;
        }

        if (current_process->remaining_time == 0)
        {
            current_process->state = 3;
            remaining_quantum = 0;
            last_burst_time = current_process->burst_time * 1.0;
        }
        else if (IOReq() && current_process->state == 1)
        {
            requiresIO = 1;
            current_process->state = 2;
            remaining_quantum = 0;
        }
        else
        {
            if (scheduling_algorithm == 0 && current_process->state != 3 && current_process->state != 2 && remaining_quantum == 0)
            {
                if (current_process->priority != MAX_PRIORITY_LEVELS - 1)
                {
                    current_process->priority++;
                }
                addToQueu(&priority_queues[current_process->priority], current_process);
            }
            else if (scheduling_algorithm == 1 && current_process->state != 3 && current_process->state != 2)
            {
                add_SJF(current_process);
            }
        }
    }
}

int all_processes_finished()
{
    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].state != 3)
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    parse_command_line_args(argc, argv);
    parse_Input("data.txt");
    initialize_PrioQ();
    init_SJF();

    while (1)
    {
        int requiresIO = 0;
        handle_arriving_processes();
        handle_io_requests();
        manage_current_process();

        if (all_processes_finished())
        {
            break;
        }

        if (verbose)
        {
            if (scheduling_algorithm == 0)
            {
                if (current_process != NULL)
                {
                    printf("%d:%d:%d:%d:", clock, current_process->pid, current_process->remaining_time,
                           requiresIO);
                    printTerm();
                    print_process_status1(remaining_quantum, current_process->remaining_time, requiresIO);
                }
                else if (current_process == NULL)
                {
                    printf("%d:-1:-1:%d:", clock, requiresIO);
                    printTerm();
                    printf("-1\n");
                }
            }
            else if (scheduling_algorithm == 1)
            {
                int nextPID = get_Id();
                if (current_process != NULL && nextPID != -1)
                {
                    printf("%d:%d:%d:%d:", clock, current_process->pid, current_process->remaining_time,
                           requiresIO);
                    printTerm();
                    print_process_status2(nextPID, current_process, requiresIO);
                }
                else if (current_process == NULL)
                {
                    printf("%d:-1:-1:%d:", clock, requiresIO);
                    printTerm();
                    printf("-1\n");
                }
            }
        }
        clearlist();
        clock++;
    }
    print_Table();

    return 0;
}