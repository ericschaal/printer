#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "shared.h"

int fd;

char buffer[1024];
int jobs_printed;
int errno;
int id;

Shared* shared_mem;


// creates an event to be displayed by event program.
int new_event(char* message) {
	
	
	sem_wait(&shared_mem->event_empty);
	sem_wait(&shared_mem->mutex_event);
	
	// queue manipulation
	if (shared_mem->rear_event == MAX_EVENTS -1) {
		shared_mem->rear_event = -1;
	}
	shared_mem->rear_event++;
	strcpy(shared_mem->events[shared_mem->rear_event], message); // copy message at right location
	shared_mem->eventCount++;
	
	sem_post(&shared_mem->mutex_event);
	sem_post(&shared_mem->event_full);
	
	return 0;
}

// try to open shared memory
int setup_shared_memory(){
	
    fd = shm_open(MY_SHM, O_RDWR, S_IRWXU);
    if(fd == -1){
        printf("Failed to open shared memory, trying to create.\n");
		return -1;
    }
	return 0;
}
// create and open shared memory
int create_shared_memory(){
	
	
    fd = shm_open(MY_SHM, O_CREAT | O_RDWR, S_IRWXU);
    if(fd == -1){
        printf("Failed to create shared memory. Exiting.\n");
        exit(1);
    }
    ftruncate(fd, sizeof(Shared));
}


int attach_shared_memory(){
    shared_mem = (Shared*)  mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(1);
    }
    return 0;
}

int init_shared_memory() {

	for (int i = 0; i < MAX_PRINTERS; i++)
		shared_mem->printers[i].id=-1;
	
	// identification
	shared_mem->jobCount = 0;
	
	// queue utils & sem
	shared_mem->front = 0;
	shared_mem->itemCount = 0;
	shared_mem->rear = -1;
	
	// semaphores for job queue
    sem_init(&(shared_mem->mutex), 1, 1);
    sem_init(&(shared_mem->full), 1, 0);
	sem_init(&(shared_mem->empty), 1, MAX_SLOTS);
	
	// event utils & sem
	shared_mem->front_event = 0;
	shared_mem->eventCount = 0;
	shared_mem->rear_event = -1;
	
	// semaphores for event queue
	sem_init(&(shared_mem->mutex_event), 1, 1);
    sem_init(&(shared_mem->event_full), 1, 0);
	sem_init(&(shared_mem->event_empty), 1, MAX_EVENTS);

	
	return 0;
}

// gets printer an id. Tries to get id givent by user if one was given.
// or gives the first available id.
int get_printer_id(int desired_id) {
	
	int given_id;
	
	sem_wait(&shared_mem->mutex);
	
	// find the first available id, if no id available, fail.
	if (desired_id == -1) {
		for (int i = 0; i < MAX_PRINTERS; i++) {
		
			if (shared_mem->printers[i].id < 0) {
				given_id = i;
				shared_mem->printers[i].id = i;
				sem_post(&shared_mem->mutex);
				return given_id;
			}
		}
		
		sem_post(&shared_mem->mutex);
		return -1;
		
	}
	
	// id given by user already taken
	if (shared_mem->printers[desired_id].id > -1) {
		sem_post(&shared_mem->mutex);
		return -1;
	}
	
	// id is free.
	given_id = desired_id;
	shared_mem->printers[desired_id].id = desired_id;
	
	sem_post(&shared_mem->mutex);
	return given_id;
	
}

// arg checking and try to get an id.
int get_printer_info(int desired_id) {
	
	// user wants an invalid id. Let's try to give a valid id to printer.
	if ((desired_id < 0 ) || (desired_id >= MAX_PRINTERS)) {
		printf("Trying default id...\n");
		desired_id = -1;
	}
	
	// Cant get an id for printer.
	if ((id = get_printer_id(desired_id)) < 0) {
		printf("Error : Max connected printers reached or desired id unavailable.\n");
		exit(1);
	}
	
	return 0;
}


int setup(int desired_id) {
	
	
	if (setup_shared_memory() < 0) {
		// no shared memory exits try to create.
		create_shared_memory();
		attach_shared_memory();
		init_shared_memory();
	}
	else {
		attach_shared_memory();
	}
	
	get_printer_info(desired_id);
	
	// printing printer informations
	printf("\n ----- Printer Online -----\n\n");
	printf("-Printer id: %d \n", id);
	printf("-Max job buffer: %d \n", MAX_SLOTS);
	printf("-Max event buffer: %d \n", MAX_EVENTS);
	printf("-Max printers: %d \n\n", MAX_PRINTERS);
	printf("----- READY TO PRINT ----- \n\n");
	
	// new event
	snprintf(buffer, sizeof(buffer), "Printer[%d]: online.\n", id);
	new_event(buffer);
	
	return 0;
	
}


// remove job from buffer
Job remove_job() {
	
	// locking and underflow
	sem_wait(&shared_mem->full);
	sem_wait(&shared_mem->mutex);
	
	Job job = shared_mem->queue[shared_mem->front++]; // get the job
	
	// queue manipulation
	if (shared_mem->front == MAX_SLOTS) {
		shared_mem->front = 0;
	}
	shared_mem->itemCount--;
	
	// releasing and notify
	sem_post(&shared_mem->mutex);
	sem_post(&shared_mem->empty);
	
	return job;
}

int get_job_done(Job job) {
	
	// new event
	snprintf(buffer, sizeof(buffer), "Printer[%d]: Started printing job[%d] from client[%d]. Estimated duration : %d s.\n", id, job.id, job.author_id, job.duration);
	new_event(buffer);
	
	printf("Printer[%d]: Started printing job[%d] from client[%d]. Estimated duration : %d s.\n", id, job.id, job.author_id, job.duration);
	
	sleep(job.duration); // printing simulation
	
	printf("Printer[%d]: finished printing job[%d].\n", id, job.id);
	
	// new event
	snprintf(buffer, sizeof(buffer), "Printer[%d]: finished printing job[%d] from client[%d].\n", id, job.id, job.author_id);
	new_event(buffer);
	
	jobs_printed++; // tracking only
	
	return 0; 
}

void handler(int signo){
	
	// safe to block sigint, sigquit, sigterm
	// cleaning up
	sem_wait(&shared_mem->mutex);
	shared_mem->printers[id].id = -1;
	sem_post(&shared_mem->mutex);
	
	snprintf(buffer, sizeof(buffer), "Printer[%d]: offline. Total printed jobs: %d\n", id, jobs_printed);
	new_event(buffer);
	
	int temp;
    sem_getvalue(&shared_mem->mutex, &temp);
    if(temp != 1)
        sem_post(&shared_mem->mutex);
	
    exit(0);
}



int main(int argc, char **argv)
{
	int temp; // for holding sm value
	int sp_id = -1; //id, by default -1 -> no specific id wanted by user.
	
	
	// signal handling, keep track of connected printers.
	if(signal(SIGINT, handler) == SIG_ERR)
        printf("Signal Handler Failure ..\n");
	if(signal(SIGQUIT, handler) == SIG_ERR)
        printf("Signal Handler Failure ..\n");
	if(signal(SIGTERM, handler) == SIG_ERR)
        printf("Signal Handler Failure ..\n");
	
	if (argc == 2) { // user wants a specific id
	
		if ((strcmp(argv[1], "--unlink") == 0) || ((strcmp(argv[1], "-u") == 0))) {
			if (shm_unlink(MY_SHM) < 0) {
				printf("Failed to unlink. Exiting.\n");
				exit(EXIT_FAILURE);
			}
			printf("Unlinking.\n");
			exit(EXIT_SUCCESS);
		}
	
		sp_id = strtol(argv[1], NULL, 10);
		
		if (errno == ERANGE) {
			printf("Error, out of range.\n");

			exit(1);
		}
	}
	
	// printer setup
	setup(sp_id);
	
    
	while(1) {
		
		// nothing in the buffer
		sem_getvalue(&shared_mem->full, &temp);
		if (temp == 0)
			printf("Buffer empty. Waiting.\n");
			
		Job job = remove_job();
		get_job_done(job); // simulates printing
		
	}
	
	

}
