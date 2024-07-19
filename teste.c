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

typedef struct Process {
    int pid;
    int arrival_time;
    int burst_time;
    int estimated_burst_time;
    int remaining_time;
    int priority;
    int state; // 0: Ready, 1: Running, 2: Waiting, 3: Finished
    int ready_time;
    int wait_time;
    int total_time;
    bool io_request;
    bool io_complete;
    struct Process *next; // Pointer to the next process in the queue
} Process;

typedef struct {
    Process *queue[MAX_QUEUE_SIZE];
    int front;
    int rear;
} Escalonador;

typedef struct {
    Process *processes[MAX_PROCESSES];
    int count;
} Queue;

Escalonador priority_queues[MAX_PRIORITY_LEVELS];
Queue sjf_queue;
Process processes[MAX_PROCESSES];
int process_count = 0;
int clock = 0;
int scheduling_algorithm = 0;
float aging_factor = 0.5;
bool verbose = false;

int last_burst_time = 5;
int last_estimated_burst_time = 5;

int IOReq() {
    if(osPRNG() % PROB_OF_IO_REQ == 0)
        return 1;
    else
        return 0;
}

int IOTerm() { 
    if(osPRNG() % PROB_OF_IO_TERM == 0)
        return 1;
    else
        return 0;
}

void initializePriorityQueues() {
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        priority_queues[i].front = -1;
        priority_queues[i].rear = -1;
    }
}

void addToPriorityQueue(Escalonador *pq, Process *process) { 
    if (pq->rear == MAX_QUEUE_SIZE - 1) return; // Fila cheia
    if (pq->front == -1) pq->front = 0;
    pq->queue[++pq->rear] = process;
}

Process* removeFromPriorityQueue(Escalonador *pq) {
    if (pq->front == -1 || pq->front > pq->rear) return NULL; // Fila vazia
    Process *process = pq->queue[pq->front++];
    if (pq->front > pq->rear) pq->front = pq->rear = -1;
    return process;
}

Process* selectProcessFMP() { 
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {  // Percorre as filas de prioridade
        if (priority_queues[i].front != -1) { // Fila não vazia
            return removeFromPriorityQueue(&priority_queues[i]); // Retorna o primeiro processo da fila
        }
    }
    return NULL; // Nenhum processo disponível
}

void initializeSJFQueue() {
    sjf_queue.count = 0;
}

double estimateNextBurstTime(double last_burst_time, double previous_estimated_time, float aging_factor) {
    last_estimated_burst_time = (aging_factor * last_burst_time) + ((1 - aging_factor) * previous_estimated_time);
    return last_estimated_burst_time;
}

void addToSJFQueue(Process *process) { 
    if (process->state == 3) return; // Já terminado

    int i = sjf_queue.count - 1;
    if (process->estimated_burst_time == 0) {
        process->estimated_burst_time = estimateNextBurstTime(last_burst_time, last_estimated_burst_time, aging_factor);
    }
    while (i >= 0 && sjf_queue.processes[i]->estimated_burst_time > process->estimated_burst_time) {
        sjf_queue.processes[i + 1] = sjf_queue.processes[i];
        i--;
    }
    sjf_queue.processes[i + 1] = process;
    sjf_queue.count++;
}

Process* removeFromSJFQueue() {
    if (sjf_queue.count == 0) return NULL;
    Process *process = sjf_queue.processes[0];
    for (int i = 1; i < sjf_queue.count; i++) {
        sjf_queue.processes[i - 1] = sjf_queue.processes[i];
    }
    sjf_queue.count--;
    return process;
}

Process* selectProcessSJF() {
    return removeFromSJFQueue();
}

void updateProcessStatistics(Process *process) {
    if (process->state == 0) { // Ready
        process->ready_time++;
    } else if (process->state == 2) { // Waiting
        process->wait_time++;
    } else if (process->state == 1) { // Running
        process->total_time++;
    }
}

void verboseLog(int clock_tick, Process *process, const char *state) {
    if (verbose) {
        fprintf(stderr, "%d:%d:%d:%d:%s:%s\n", clock_tick, process ? process->pid : 0, process ? process->remaining_time : 0, process->io_request,process->io_complete ? "0" : "null", state);
    }
}

void parseProcessInput(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo de entrada");
        exit(EXIT_FAILURE);
    }

    char line[100];
    while (fgets(line, sizeof(line), file)) {
        Process *process = malloc(sizeof(Process));
        if (sscanf(line, "%d:%d:%d:%d", &process->pid, &process->arrival_time, &process->burst_time, &process->priority) == 4) {
            process->remaining_time = process->burst_time;
            process->state = -1; 
            process->ready_time = 0;
            process->wait_time = 0;
            process->total_time = 0;
            process->estimated_burst_time = 0;
            process->next = NULL;
            processes[process_count++] = *process;
            free(process); // Não precisamos mais do malloc já que o process está sendo copiado para o array
        }
    }

    fclose(file);
}

void printProcessStatistics() {
    int min_time = __INT_MAX__;
    int max_time = 0;
    int total_exec_time = 0;
    int total_ready_time = 0;
    int total_wait_time = 0;

    printf("=========+=================+=================+=================\n");
    printf("Processo | Tempo total     | Tempo total     | Tempo total\n");
    printf("         | em estado Ready | em estado Waiting | no sistema\n");
    printf("=========+=================+=================+=================\n");

    for (int i = 0; i < process_count; i++) {
        Process *p = &processes[i];
        int total_time = p->total_time;
        int ready_time = p->ready_time;
        int wait_time = p->wait_time;
        
        // Usando largura fixa para cada coluna
        printf("%-9d| %-16d| %-16d| %-15d\n", p->pid, ready_time, wait_time, total_time);

        // Calcula os tempos mínimos e máximos
        if (total_time < min_time) min_time = total_time;
        if (total_time > max_time) max_time = total_time;

        // Acumula os tempos totais
        total_exec_time += total_time;
        total_ready_time += ready_time;
        total_wait_time += wait_time;
    }

    printf("=========+=================+=================+=================\n");
    printf("Tempo total de simulacao.: %d\n", clock);
    printf("Numero de processos......: %d\n", process_count);
    printf("Menor tempo de execucao..: %d\n", min_time);
    printf("Maior tempo de execucao..: %d\n", max_time);
    printf("Tempo medio de execucao..: %.2f\n", (float)total_exec_time / process_count);
    printf("Tempo medio em Ready/Wait: %.2f\n", (float)(total_ready_time + total_wait_time) / process_count);
}


void handleFMP() { // CONSERTAR LOOP INFINITO, NAO TÁ ENTRANDO NO IF
    // Adiciona processos que chegam no tempo atual
    int count = 0;
    int count2 = 0;
    for (int i = 0; i < process_count; i++) {
        count++;
        if (processes[i].arrival_time == clock && processes[i].state == -1) {
            processes[i].state = 0; // Ready
            updateProcessStatistics(&processes[i]);
            addToPriorityQueue(&priority_queues[processes[i].priority], &processes[i]);
        }
    }
    printf("check : %d\n", count);
    // Recoloca processos que estavam esperando e tiveram término de I/O
    for (int i = 0; i < process_count; i++) { 
        count2 ++;
        if (processes[i].state == 2 && IOTerm()) {
            processes[i].state = 0; // Ready
            updateProcessStatistics(&processes[i]);
            addToPriorityQueue(&priority_queues[processes[i].priority], &processes[i]);
        }
    }
    printf("check3 : %d\n", count2);
    
// \/\/\/\/  CONSERTAR LOOP INFINITO, NAO TÁ ENTRANDO NO IF  \/\/\/\/

    // Seleciona e executa o processo
    Process *current_process = selectProcessFMP();
    if (current_process != NULL) {  
        current_process->state = 1; // Running
        current_process->remaining_time--;
        int io_req = IOReq();
        if (verbose) {
            verboseLog(clock, current_process, "Running");
        }

        if (current_process->remaining_time == 0) {             // Processo terminou
            updateProcessStatistics(current_process);
            current_process->state = 3; // Finished
            last_burst_time = current_process->burst_time;      // Atualiza o tempo de burst
            if (verbose) {
            verboseLog(clock, current_process, "end");  // verbose término do processo
            }

        } else if (io_req && current_process->state == 1) {     // Processo precisa de I/O
            updateProcessStatistics(current_process);           // Atualiza as estatísticas do processo
            current_process->state = 2; // Waiting
            if (verbose) {
                verboseLog(clock, current_process, "Waiting");
            }

        } else {    // Reajusta a prioridade e coloca o processo de volta na fila
            if (current_process->remaining_time > 0) {
                int new_priority = current_process->priority + 1;   // Aumenta a prioridade
                if (new_priority >= MAX_PRIORITY_LEVELS) {          // Verifica se a prioridade é maior que o máximo
                    new_priority = MAX_PRIORITY_LEVELS - 1;         // Ajusta a prioridade para o máximo
                }

                current_process->priority = new_priority;           // Atualiza a prioridade do processo
                addToPriorityQueue(&priority_queues[current_process->priority], current_process);    // Adiciona o processo na fila
            }
        }
    }
}

void handleSJF() {
    // Adiciona processos que chegam no tempo atual
    for (int i = 0; i < process_count; i++) {
        if (processes[i].arrival_time == clock && processes[i].state == -1) {
            processes[i].state = 0; // Ready
            updateProcessStatistics(&processes[i]);
            addToSJFQueue(&processes[i]);
        }
    }

    // Recoloca processos que estavam esperando e tiveram término de I/O
    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == 2 && IOTerm()) {
            processes[i].state = 0; // Ready
            updateProcessStatistics(&processes[i]);
            addToSJFQueue(&processes[i]);
        }
    }

    // Seleciona e executa o processo
    Process *current_process = selectProcessSJF();
    if (current_process != NULL) {
        current_process->state = 1; // Running
        current_process->remaining_time--;
        int io_req = IOReq();
        if (verbose) {
            verboseLog(clock, current_process, "Running");
        }
        if (current_process->remaining_time == 0) {
            updateProcessStatistics(current_process);
            current_process->state = 3; // Finished
            if (verbose) {
                verboseLog(clock, current_process, "end");
            }
            last_burst_time = current_process->burst_time;
        } else if (io_req && current_process->state == 1) {
            updateProcessStatistics(current_process);
            current_process->state = 2; // Waiting
            if (verbose) {
                verboseLog(clock, current_process, "Waiting");
            }
        } else {
            addToSJFQueue(current_process); // Recoloca o processo na fila SJF
        }
    }
}

int main(int argc, char **argv) {
    char *input_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-F") == 0) {   // Verifica se o argumento é -F
            scheduling_algorithm = 0;
        } 
        else if (strcmp(argv[i], "-S") == 0) {  
            scheduling_algorithm = 1;          
            if (i + 1 < argc && argv[i + 1][0] != '-' && strlen(argv[i + 1]) < 4) { // Verifica se o próximo argumento não é uma flag e tem menos de 4 caracteres
                aging_factor = atof(argv[++i]);                                     // Converte o argumento para float
            }
            if (aging_factor < 0 || aging_factor > 1) {                            // Verifica se o fator de envelhecimento está entre 0 e 1
                fprintf(stderr, "O fator de envelhecimento deve estar entre 0 e 1\n");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;                         // Ativa o modo verbose
        } else {
            input_filename = argv[i];               // Define o nome do arquivo de entrada
        }
    }

    if (input_filename == NULL) {
        fprintf(stderr, "Uso: %s [-F | -S [a]] [-v] <inputfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    parseProcessInput(input_filename);
    initializePriorityQueues();
    initializeSJFQueue();

    while (1) {
        if (scheduling_algorithm == 0) {
            handleFMP();
        } else {
            handleSJF();
        }

        // Atualiza estatísticas dos processos
        for (int i = 0; i < process_count; i++) {
            if (processes[i].state != 3 && clock >= processes[i].arrival_time) {
                updateProcessStatistics(&processes[i]);
            }
        }

        // Verifica se todos os processos foram concluídos
        int all_finished = 1;
        for (int i = 0; i < process_count; i++) {
            if (processes[i].state != 3) {
                all_finished = 0;
                break;
            }
        }
        if (all_finished) break;

        clock++;
    }

    printProcessStatistics();
    printf("Aging Factor: %.2f\n", aging_factor);
    return 0;
}

       
