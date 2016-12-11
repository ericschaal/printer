#include <unistd.h>
#include <string.h>
#include <time.h>
#include "shared.h"

int errno;

int fd;
int source; // holds source id
int job_id; // holds job id
int duration; // once computed, holds duration of job
char buffer[1024]; // buffer for events messages
Shared* shared_mem; // pointer to the shared memory


// creates an event to be displayed by event program.
int new_event(char* message) {
	
	
	sem_wait(&shared_mem->event_empty); // wait if buffer is full
	sem_wait(&shared_mem->mutex_event); // wait to modify event buffer
	
	// insert element into queue
	if (shared_mem->rear_event == MAX_EVENTS -1) {
		shared_mem->rear_event = -1;
	}
	
	shared_mem->rear_event++;
	strcpy(shared_mem->events[shared_mem->rear_event], message);
	shared_mem->eventCount++;
	
	// unlock and notify there is a event in queue
	sem_post(&shared_mem->mutex_event);
	sem_post(&shared_mem->event_full);
	
	return 0;
}

// open shared memory.. or fail if isnt already created
int setup_shared_memory(){
    fd = shm_open(MY_SHM, O_RDWR, S_IRWXU);
    if(fd == -1){
        printf("shm_open() failed\n");
        exit(EXIT_FAILURE);
    }
}

// attach to sh mem
int attach_shared_memory(){
    shared_mem = (Shared*) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(EXIT_FAILURE);
    }
	
    return 0;
}

// cleanup 
void handler(int signo){
	// give back mutexes before exiting
    int temp;
    sem_getvalue(&shared_mem->mutex, &temp);
    if(temp != 1)
        sem_post(&shared_mem->mutex);
		
	sem_getvalue(&shared_mem->mutex_event, &temp);
	if(temp != 1)
        sem_post(&shared_mem->mutex);
    exit(EXIT_SUCCESS);
}

// add job to buffer
int add_new_job(Job job) {
	
	sem_wait(&shared_mem->empty);
	sem_wait(&shared_mem->mutex);
	
	// queue manipulation
	if (shared_mem->rear == MAX_SLOTS -1) {
		shared_mem->rear = -1;
	}
	
	job.id = shared_mem->jobCount; // id of the job (unique)
	shared_mem->rear++; // queue manipulation
	shared_mem->queue[shared_mem->rear] = job; // insert job
	
	// update counters
	shared_mem->itemCount++;
	shared_mem->jobCount++; 
	
	sem_post(&shared_mem->mutex);
	sem_post(&shared_mem->full);
	
	snprintf(buffer, sizeof(buffer), "Client[%d] Adding job[%d] to print queue. Estimated duration: %d s.\n", source, job.id ,job.duration);
	new_event(buffer);
	
	printf("Client[%d]: Adding job[%d] to print queue.\n", source, job.id);
	
	return 0;
}


int detach_from_shm() {
	munmap(shared_mem, sizeof(Shared));
}

int init(int source_id) {
	
	// id of client
	source = source_id;
	
	// duration job randomly generated
	srand(time(NULL));
	duration = rand() % 10 + 1; 
	
	if(signal(SIGINT, handler) == SIG_ERR)
        printf("Signal Handler Failure ..\n");
		
    setup_shared_memory();
    attach_shared_memory();
	
}

// creates a new job with computed duration and source given by user
Job new_job() {
	
	Job job;
	job.author_id = source;
	job.duration = duration;
	return job;
}


int main(int argc, char **argv)
{
	
	// needs 1 argument : source id
	if (argc != 2) {
		printf("Invalid usage, usage : client [sourceID].\n");
		exit(EXIT_FAILURE);
	}
	
	// string to int base 10.
	int src_id = strtol(argv[1], NULL, 10);
	
	// out of range
	if (errno == ERANGE) {
		printf("Error, out of range.");
		exit(EXIT_FAILURE);
	}
	
	
	init(src_id); // set states
	
	
	Job job = new_job(); // creates new job
	add_new_job(job); // add job to buffer
	
	detach_from_shm();
	
	return 0;
}
