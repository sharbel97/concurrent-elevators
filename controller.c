#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "assert.h"
#include "controller.h"

struct psg {
    int id;
    int from_floor;
    int to_floor;
    int ele_number;
    enum { WAITING = 1,
           RIDING = 2,
           DONE = 3 } state;
    struct psg* next;
    pthread_cond_t cond;
} passengers[PASSENGERS];

struct ele {
    int current_floor;
    int direction;
    int occupancy;
    enum { ELEVATOR_ARRIVED = 1,
           ELEVATOR_OPEN = 2,
           ELEVATOR_CLOSED = 3 } state;
    struct psg* waiting;
    struct psg* riding;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} elevators[ELEVATORS];


// void add_to_list(struct psg** head, struct psg* passenger);
// void remove_from_list(struct psg** head, struct psg* passenger);
void add_to_list(struct psg** head, struct psg* passenger) {
    struct psg* p = *head;
    if (p == NULL)
        *head = passenger;
    else {
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = passenger;
    }
}

// Must hold lock, does not update occupancy.
void remove_from_list(struct psg** head, struct psg* passenger) {
    struct psg* p = *head;
    struct psg* prev = NULL;
    while (p != passenger) {
        prev = p;
        p = (p)->next;
        // don't crash because you got asked to remove someone that isn't on this
        // list
        assert(p != NULL);
    }
    // removing the head - must fix head pointer too
    if (prev == NULL)
        *head = p->next;
    else
        prev->next = p->next;
    passenger->next = NULL;
}


void scheduler_init() {
    for (int i = 0; i < ELEVATORS; i++) {
        elevators[i].current_floor=0;
        elevators[i].direction = -1;
        elevators[i].occupancy = 0;
        elevators[i].state = ELEVATOR_CLOSED;
        elevators[i].waiting = NULL;
        elevators[i].riding = NULL;
        pthread_cond_init(&elevators[i].cond, NULL);
        pthread_mutex_init(&elevators[i].lock, NULL);
    }

    for (int i = 0; i < PASSENGERS; i++) {
        passengers[i].id = i;
        int randomElevator = rand()%ELEVATORS;
        passengers[i].ele_number = randomElevator;
        passengers[i].from_floor = -1;
        passengers[i].to_floor = -1;
        passengers[i].state = WAITING;
        passengers[i].next = NULL;
        pthread_cond_init(&passengers[i].cond, NULL);
        add_to_list(&elevators[randomElevator].waiting, &passengers[i]);
    }
}


/*
passenger_request:
called whenever a passenger pushes a button in the elevator lobby.
call enter / exit to move passengers into / out of elevator
return only when the passenger is delivered to requested floor
*/
void passenger_request(int passenger, int from_floor, int to_floor,
        void (*enter)(int, int),
        void(*exit)(int, int))
{
    struct psg *p = &passengers[passenger];
    struct ele *e = &elevators[p->ele_number];

    pthread_mutex_lock(&e->lock);

    p->from_floor = from_floor;
    p->to_floor = to_floor;

    // wait for the elevator to arrive at our origin floor, then get in
    int waiting = 1;
    while(waiting)
    {
        pthread_cond_wait(&p->cond, &e->lock);
        if(e->current_floor == from_floor && e->state == ELEVATOR_OPEN && 
           e->occupancy == 0)
        {
            enter(passenger, p->ele_number);
            remove_from_list(&e->waiting, p);
            add_to_list(&e->riding, p);
            e->occupancy++;
            waiting=0;
            pthread_cond_signal(&e->cond);
        }
    }

    // wait for the elevator to arrive at our destination floor, then get out
    int riding=1;
    while(riding) 
    {
        pthread_cond_wait(&p->cond, &e->lock);
        if(e->current_floor == to_floor && e->state == ELEVATOR_OPEN) 
        {
            exit(passenger, p->ele_number);
            remove_from_list(&e->riding, p);
            e->occupancy--;
            riding=0;
            pthread_cond_signal(&e->cond);
        }

    }
    pthread_mutex_unlock(&e->lock);
}

/*
elevator_ready:
called whenever the doors are about to close.
call move_direction with direction -1 to descend, 1 to ascend.
must call door_open before letting passengers in, and door_close before moving the elevator
*/
void elevator_ready(int elevator, int at_floor,
        void(*move_direction)(int, int),
        void(*door_open)(int), void(*door_close)(int)) {

    struct ele *e = &elevators[elevator];

    pthread_mutex_lock(&e->lock);

    if (e->waiting == NULL && e->riding == NULL && at_floor == 0) {
        pthread_mutex_unlock(&e->lock);
        return;
    }

    if(e->state == ELEVATOR_ARRIVED) {
        door_open(elevator);
        e->state=ELEVATOR_OPEN;
    }
    else if(e->state == ELEVATOR_OPEN) {
        if (e->occupancy == 0) {
            // alert a waiting passenger if floors match
            struct psg* p = e->waiting;
            if (p != NULL && p->from_floor == at_floor) {
                pthread_cond_signal(&p->cond);
                pthread_cond_wait(&e->cond, &e->lock);
            }
        } else {
            // alert a riding passenger if floors match
            struct psg* p = e->riding;
            if (p != NULL && p->to_floor == at_floor) {
                pthread_cond_signal(&p->cond);
                pthread_cond_wait(&e->cond, &e->lock);
            }
        }
        door_close(elevator);
        e->state=ELEVATOR_CLOSED;
    }
    else {
        if (e->occupancy != 0) {
            int to_floor = e->riding->to_floor;
            e->direction = to_floor > at_floor ? 1 : -1;
        }
        else if (e->occupancy == 0 && e->waiting != NULL) {
            int to_floor = e->waiting->from_floor;
            e->direction = to_floor > at_floor ? 1 : -1;
        }

        if(at_floor == 0) e->direction = 1;
        else if (at_floor == FLOORS-1) e->direction = -1;
        
        move_direction(elevator,e->direction);
        e->current_floor =at_floor+e->direction;
        e->state=ELEVATOR_ARRIVED;
    }
    pthread_mutex_unlock(&e->lock);
}