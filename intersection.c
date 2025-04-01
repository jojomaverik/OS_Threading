/*
 * Operating Systems (2INC0) Practical Assignment
 * Threading
 *
 * Intersection Part 2
 * 
 * Ersoy Baki (1971131)
 * Arda Bizel (1937901)
 * Isik Ali Akpinar (1959425)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "arrivals.h"            
#include "intersection_time.h"      
#include "input.h"                

//  TODO: Global variables: mutexes, data structures, etc...
extern struct timespec begin_time;

// Initialize semaphores for all lanes and mutexes
static sem_t car_sem[4][4];
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
static int processed_count[4][4] = { {0} };

// Array of zones of intersections
#define NUM_ZONES 7
pthread_mutex_t zone_mutex[NUM_ZONES];

// Define traffic light as a struct
typedef struct {
  int side;
  int direction;
} TrafficLightArgs;

/* 
 * curr_car_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Car_Arrival curr_car_arrivals[4][4][20];



/*
 * supply_cars()
 *
 * A function for supplying car arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_cars()
{
  int t = 0;
  int num_curr_arrivals[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    // for every arrival in the list
  for (int i = 0; i < sizeof(input_car_arrivals) / sizeof(Car_Arrival); i++)
  {
    // get the next arrival in the list
    Car_Arrival arrival = input_car_arrivals[i];
    // wait until this arrival is supposed to arrive
    sleep(arrival.time - t);
    t = arrival.time;
    // store the new arrival in curr_arrivals
    curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction] += 1;
    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&car_sem[arrival.side][arrival.direction]);
  }
  return (0);
}

// An array of intersection zones for each traffic light and path
static int path_zones[4][4][5] = {
    // NORTH - side 0
    {
        // directions, left, straight, right, u-turn in order, -1 if no intersection
        {-1, -1, -1, -1, -1 },
        {-1, -1, -1, -1, -1 },
        {-1, -1, -1, -1, -1 },
        {-1, -1, -1, -1, -1 }  
    },
    // EAST - side 1
    {
        { 1, 2, 7, -1, -1 },
        { 3, 4, -1, -1, -1 },
        { 5, -1, -1, -1, -1 },
        { -1, -1, -1, -1, -1 }
    },
    // SOUTH - side 2
    {
        { 1, 4, -1, -1, -1 },
        { 2, 3, 5, -1, -1 },
        { 6, -1, -1, -1, -1 },
        { 7, -1, -1, -1, -1 }  
    },
    // WEST - side 3
    {
        { 4, 3, 5, -1, -1 },
        { 1, 2, 6, -1, -1 },
        { 7, -1, -1, -1, -1 },
        { -1, -1, -1, -1, -1 }
    }
};


/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
    int side = ((TrafficLightArgs*)arg)->side;
    int direction = ((TrafficLightArgs*)arg)->direction;
    
    // Calculate the deadline for the traffic light
    struct timespec deadline = begin_time;
    deadline.tv_sec += END_TIME;

    
    while (1)
    {
        // Check for end time
        int sem_result = sem_timedwait(&car_sem[side][direction], &deadline);
        if (sem_result == -1 && errno == ETIMEDOUT) {
            break;
        }
        
        // Process the next arrival.
        Car_Arrival arrival = curr_car_arrivals[side][direction][processed_count[side][direction]];
        processed_count[side][direction]++;

        // Get the intersection zone list 
        int *zones_for_path = path_zones[side][direction];
        
        // Lock all zones according to zone list
        for (int i = 0; zones_for_path[i] != -1; i++) {
            // Subtract 1 since zones are 1-indexed.
            pthread_mutex_lock(&zone_mutex[zones_for_path[i] - 1]);
        }
            
        printf("traffic light %d %d turns green at time %d for car %d\n",
                side, direction, get_time_passed(), arrival.id);
        sleep(CROSS_TIME);
        printf("traffic light %d %d turns red at time %d\n",
                side, direction, get_time_passed());
        
        // Unlock the zones 
        for (int i = 0; zones_for_path[i] != -1; i++) {
            pthread_mutex_unlock(&zone_mutex[zones_for_path[i] - 1]);
        }
    }

    // TODO:
    // while not all arrivals have been handled, repeatedly:
    //  - wait for an arrival using the semaphore for this traffic light
    //  - lock the right mutex(es)
    //  - make the traffic light turn green
    //  - sleep for CROSS_TIME seconds
    //  - make the traffic light turn red
    //  - unlock the right mutex(es)
    return (0);
}

int main(int argc, char * argv[])
{
  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {  
      sem_init(&car_sem[i][j], 0, 0);
    }
  }

  // initalizing zone mutexes
  for (int i = 0; i < NUM_ZONES; i++) {
    pthread_mutex_init(&zone_mutex[i], NULL);
}

    // start the timer
  start_time();
  
  // TODO: create a thread per traffic light that executes manage_light
  const int lights = 9;  
  pthread_t threads[lights + 1];  
  TrafficLightArgs trafficLights[lights];
  
  // Create threads for 9 lights: sides 1,2,3 and directions 0,1,2.
  for (int i = 0; i < lights; i++) {
      trafficLights[i].side = (i / 3) + 1;
      trafficLights[i].direction = i % 3;
      if (pthread_create(&threads[i], NULL, manage_light, &trafficLights[i]) != 0) {
          perror("Failed to create traffic light thread");
          exit(EXIT_FAILURE);
      }
  }
  
  // Create u-turn light thread
  TrafficLightArgs uTurnLight;
  uTurnLight.side = 2;
  uTurnLight.direction = 3;
  if (pthread_create(&threads[lights], NULL, manage_light, &uTurnLight) != 0) {
      perror("Failed to create u-turn light thread");
      exit(EXIT_FAILURE);
  }
  
  // TODO: create a thread that executes supply_cars()
  pthread_t supplier;
  pthread_create(&supplier, NULL, supply_cars, NULL);
  
  pthread_join(supplier, NULL);
  
    // TODO: wait for all threads to finish
  for (int i = 0; i < lights + 1; i++) {
      pthread_join(threads[i], NULL);
  }

  // destroy intersection threads
    for (int i = 0; i < NUM_ZONES; i++) {
        pthread_mutex_destroy(&zone_mutex[i]);
    }
  
  // destroy semaphores
  for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
          sem_destroy(&car_sem[i][j]);
      }
  }
  
  return 0;
}
