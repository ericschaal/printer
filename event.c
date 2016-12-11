#include <unistd.h>
#include <string.h>
#include "shared.h"

int fd;
Shared* shared_mem;

int setup_shared_memory(){
    fd = shm_open(MY_SHM, O_RDWR, S_IRWXU);
    if(fd == -1){
        printf("shm_open() failed. Run printer server before running event manager.\n");
        exit(EXIT_FAILURE);
    }
}

int attach_shared_memory(){
    shared_mem = (Shared*) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(EXIT_FAILURE);
    }
	
    return 0;
}

void handler(int signo){
	// give back mutexes before exiting
    int temp;
    sem_getvalue(&shared_mem->mutex_event, &temp);
    if(temp != 1)
        sem_post(&shared_mem->mutex_event);
    exit(EXIT_SUCCESS);
}

char* remove_event() { // remove event from queue
	
	char* message = shared_mem->events[shared_mem->front_event++];
	
	if (shared_mem->front_event == MAX_EVENTS) {
		shared_mem->front_event = 0;
	}
	shared_mem->eventCount--;
	
	return message;
}

int main(int argc, char **argv)
{
	setup_shared_memory();
    attach_shared_memory();
	
	// trying to catch signal for cleaning up before exiting
	if(signal(SIGINT, handler) == SIG_ERR)
        printf("Signal Handler Failure ..\n");
		
		
    
	while(1) {
	
	// remove event from queue and print its message
	sem_wait(&shared_mem->event_full);
	sem_wait(&shared_mem->mutex_event);
	char* message = remove_event();
	sem_post(&shared_mem->mutex_event);
	sem_post(&shared_mem->event_empty);
	printf(message);
	
	}
	
	return 0;
}
