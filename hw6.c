#include "hw6.h"
#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include<pthread.h>

pthread_barrier_t barrier;
pthread_cond_t cond_var_passenger[PASSENGERS];
pthread_cond_t cond_var_elevator[ELEVATORS];
pthread_mutex_t locks[ELEVATORS];

struct psg {
    int id;
    int from_floor;
    int to_floor;
    int elenumber;
    enum { WAITING = 1,
           RIDING = 2,
           DONE = 3 } state;
    struct psg* next;
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

    // setup elevators
    for (int i = 0; i < ELEVATORS; i++) {
        elevators[i].current_floor=0;
        elevators[i].direction = -1;
        elevators[i].occupancy = 0;
        elevators[i].state = ELEVATOR_ARRIVED;
        elevators[i].waiting = NULL;
        elevators[i].riding = NULL;
        pthread_cond_init(&cond_var_elevator[i], NULL);
        pthread_mutex_init(&locks[i], NULL);
    }

    // initialize passengers, assign to elevators randomly
    for (int i = 0; i < PASSENGERS; i++) {
        passengers[i].id = i;
        int randomElevator = rand()%ELEVATORS;
        passengers[i].elenumber = randomElevator;
        add_to_list(&(elevators[randomElevator].waiting), &passengers[i]);
        pthread_cond_init(&cond_var_passenger[i], NULL);
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
    int curElevator = passengers[passenger].elenumber;

    int waiting=1;
    pthread_mutex_lock(&locks[passengers[passenger].elenumber]);
    while(waiting) {

        while (elevators[curElevator].current_floor != from_floor
              || elevators[curElevator].state != ELEVATOR_OPEN 
              || elevators[curElevator].occupancy>=MAX_CAPACITY
              || passengers[passenger].elenumber != curElevator) {

            pthread_cond_wait(&cond_var_passenger[passenger], &locks[curElevator]);
        } 
        enter(passenger, passengers[passenger].elenumber);
        remove_from_list(&elevators[curElevator].waiting, &passengers[passenger]);
        add_to_list(&elevators[curElevator].riding, &passengers[passenger]);
        elevators[curElevator].occupancy++;
        break;
    }

    passengers[passenger].state = RIDING; // set passenger to riding
    pthread_cond_signal(&cond_var_elevator[curElevator]);

    int riding=1;
    while(riding) {
        
        while(elevators[curElevator].current_floor != to_floor 
             || elevators[curElevator].state != ELEVATOR_OPEN) {
            pthread_cond_wait(&cond_var_passenger[passenger], &locks[curElevator]);
        }
        
        exit(passenger, passengers[passenger].elenumber);
        remove_from_list(&elevators[curElevator].riding, &passengers[passenger]);
        elevators[curElevator].occupancy--;
        break;
    }

    pthread_cond_signal(&cond_var_elevator[curElevator]);
    pthread_mutex_unlock(&locks[curElevator]);
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

    // if (elevator != 0) return;
    pthread_mutex_lock(&locks[elevator]);

    struct psg* passengerWaiting = elevators[elevator].waiting;
    struct psg* passengerRiding = elevators[elevator].riding;

    // go thru every passenger waiting and open/close door for them
    while (passengerWaiting != NULL) {

        if (passengerWaiting->from_floor == at_floor 
            && elevators[elevator].occupancy<MAX_CAPACITY
            && elevators[elevator].state == ELEVATOR_ARRIVED
            && elevators[elevator].current_floor == passengerWaiting->from_floor
            && passengerWaiting->elenumber == elevator) {
            
            door_open(elevator);
            elevators[elevator].state=ELEVATOR_OPEN;

            pthread_cond_signal(&cond_var_passenger[passengerWaiting->id]);
            while (passengerWaiting->state == WAITING) {
                pthread_cond_wait(&cond_var_passenger[passengerWaiting->id], &locks[elevator]);
            }
        }
        
        passengerWaiting = passengerWaiting->next;
    }

    while (passengerRiding!= NULL) {
        if (passengerRiding->to_floor == at_floor) {
            door_open(elevator);
            elevators[elevator].state = ELEVATOR_OPEN;

            pthread_cond_signal(&cond_var_passenger[passengerRiding->id]);
            while(passengerRiding->state!=RIDING) {
                pthread_cond_wait(&cond_var_passenger[passengerWaiting->id], &locks[elevator]);
            }
        }
        passengerRiding = passengerRiding->next;
    }

    if(elevators[elevator].state == ELEVATOR_OPEN) {
        door_close(elevator);
        elevators[elevator].state=ELEVATOR_CLOSED;
    } 

    if(at_floor==0 || at_floor==FLOORS-1) elevators[elevator].direction*=-1;

    move_direction(elevator,elevators[elevator].direction);
    elevators[elevator].current_floor=at_floor+elevators[elevator].direction;
    // elevators[elevator].state=ELEVATOR_ARRIVED;

    pthread_mutex_unlock(&locks[elevator]);
    if (elevators[elevator].waiting == NULL && elevators[elevator].riding == NULL) {
        pthread_barrier_wait(&barrier);
    }
}


// set flag
// pthread_cond_signal(&cond_var);
// pthread_mutex_unlock(&lock);

// while(some flag) {
//     pthread_cond_wait(&cond_var, &lock) // wait for passenger to exit? elevator to open?
// }


/*
elevator                            passenger

1. waiting           
signal passenger                    passenger_cond_var
wait for elevator sign var

reached the floor
signal passenger                    wait for passenger cond_Var

2. riding

*/


/*
passenger_request: 
called whenever a passenger pushes a button in the elevator lobby.
call enter / exit to move passengers into / out of elevator
return only when the passenger is delivered to requested floor

void passenger_request(int passenger, int from_floor, int to_floor, 
        void (*enter)(int, int), 
        void(*exit)(int, int))
{
    int curElevator = passengers[passenger].elenumber;
    // wait for the elevator to arrive at our origin floor, then get in
    int waiting=1;
    pthread_mutex_lock(&locks[passengers[passenger].elenumber]);
    while(waiting) {

        while(elevators[curElevator].current_floor != from_floor 
           && elevators[curElevator].state != ELEVATOR_OPEN 
           && elevators[curElevator].occupancy>=MAX_CAPACITY) {

            pthread_cond_wait(&cond_var_passenger[passenger], &locks[curElevator]);
        }
            enter(passenger, passengers[passenger].elenumber);
            remove_from_list(&elevators[curElevator].waiting, &passengers[passenger]); // remove them from waiting
            add_to_list(&elevators[curElevator].riding, &passengers[passenger]); // add passenger to riding list
            elevators[curElevator].occupancy++;
            waiting=0;

            passengers[passenger].state = RIDING; // set passenger to riding
            pthread_cond_signal(&cond_var_elevator[curElevator]);
    }

    // wait for the elevator at our destination floor, then get out
    int riding=1;
    while(riding) {
        
        if(elevators[curElevator].current_floor == to_floor && 
            elevators[curElevator].state == ELEVATOR_OPEN) {
            
            exit(passenger, passengers[passenger].elenumber);
            remove_from_list(&elevators[curElevator].riding, &passengers[passenger]); // remove passengr from riding list
            elevators[curElevator].occupancy--;
            riding=0;
        }
        // do we wait for passenger to exit?
        pthread_cond_wait(&cond_var_elevator[curElevator], &locks[curElevator]);
    }
    // remove passenger from riding
    remove_from_list(&elevators[curElevator].riding, &passengers[passenger]);

    pthread_cond_signal(&cond_var_elevator[curElevator]);
    pthread_mutex_unlock(&locks[curElevator]);
}


elevator_ready: 
called whenever the doors are about to close.
call move_direction with direction -1 to descend, 1 to ascend.
must call door_open before letting passengers in, and door_close before moving the elevator

void elevator_ready(int elevator, int at_floor, 
        void(*move_direction)(int, int), 
        void(*door_open)(int), void(*door_close)(int)) {

    // if (elevator != 0) return;
    pthread_mutex_lock(&locks[elevator]);

    struct psg* passengerWaiting = elevators[elevator].waiting;
    struct psg* passengerRiding = elevators[elevator].riding;


    while (passengerWaiting != NULL) {
        if (passengerWaiting->from_floor == at_floor 
            && passengerWaiting->state == WAITING 
            && elevators[elevator].occupancy<MAX_CAPACITY) {
            
            if(elevators[elevator].state == ELEVATOR_ARRIVED) {
                door_open(elevator);
              ,  elevators[elevator].state=ELEVATOR_OPEN;
            }

            pthread_cond_signal(&cond_var_passenger[passengerWaiting->id]);
            while (passengerWaiting->state == WAITING) {
                pthread_cond_wait(&cond_var_elevator[elevator], &locks[elevator]);
            }
        }
        
        passengerWaiting = passengerWaiting->next;
    }

    while (passengerRiding!= NULL) {
        if (passengerRiding->to_floor == at_floor
            && passengerRiding->state == RIDING) {
            
            if (elevators[elevator].state == ELEVATOR_CLOSED) {
                door_close(elevator);
                elevators[elevator].state = ELEVATOR_OPEN;
            }
        }
        pthread_cond_signal(&cond_var_passenger[passengerRiding->id]);
        while(passengerRiding->state==RIDING) {
            pthread_cond_wait(&cond_var_elevator[elevator], &locks[elevator]);
        }
        passengerRiding = passengerRiding->next;
    }

    if(elevators[elevator].state == ELEVATOR_OPEN) {
        door_close(elevator);
        elevators[elevator].state=ELEVATOR_CLOSED;
    } 
    if(at_floor==0 || at_floor==FLOORS-1) elevators[elevator].direction*=-1;

    move_direction(elevator,elevators[elevator].direction);
    elevators[elevator].current_floor=at_floor+elevators[elevator].direction;
    // elevators[elevator].state=ELEVATOR_ARRIVED;

    pthread_mutex_unlock(&locks[elevator]);
    if (elevators[elevator].waiting == NULL && elevators[elevator].riding == NULL) {
        pthread_barrier_wait(&barrier);
    }
}
*/