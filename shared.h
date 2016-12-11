#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// from `man shm_open`
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>           
#include <signal.h>
#include <semaphore.h>
#include <errno.h>

#define MY_SHM "/MYPRINTERICSCHAALXX" // key for shared mem
#define MAX_SLOTS 10 // max slots in buffer
#define MAX_EVENTS 256 // max events in event queue
#define MAX_PRINTERS 10 // max connected printers
#define MAX_BUFFER_SIZE 512 // buffer size for event messages

typedef struct _Job { // simple job struct
	
	int id;
	int author_id;
	int duration;
	
} Job;

typedef struct _Printer {
	int id;
} Printer;


typedef struct _Shared { 
	
	// job queue semaphores
	sem_t mutex;
	sem_t empty;
	sem_t full;
	
	// event management
	sem_t mutex_event;
	sem_t event_full;
	sem_t event_empty;
	
	// events utils
	int front_event;
	int rear_event;
	int eventCount;
	

	// queue utils 
	int front;
	int rear;
	int itemCount;
	
	// total job count
	int jobCount;
	
	// queue for events
	char events[MAX_EVENTS][MAX_BUFFER_SIZE];
	
	// queue for jobs
	Job queue[MAX_SLOTS];
	
	// keeping track of connected printers (not used like a queue)
	Printer printers[MAX_PRINTERS];
	
} Shared;
