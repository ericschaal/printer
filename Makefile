LIBS := -lrt -lpthread

all: printer client event

event: event.c
	gcc $^ -o $@ -g $(LIBS)

printer: server.c
	gcc $^ -o $@ $(LIBS)

client: client.c
	gcc $^ -o $@ $(LIBS)

clean:
	-rm printer client event
