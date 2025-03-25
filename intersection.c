/*
 * Intersection Part 1 - Basic Solution: Only one car in the intersection at a time.
 *
 * Ersoy Ata Baki (1971131)
 * STUDENT_NAME_2 (STUDENT_NR_2)
 * STUDENT_NAME_3 (STUDENT_NR_3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "arrivals.h"            
#include "intersection_time.h"      
#include "input.h"                


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
 * car_sem[][]
 *
 * A 2D array that defines a semaphore for each entry lane,
 *   which are used to signal the corresponding traffic light that a car has arrived
 * The two indices determine the entry lane: first index is Side, second index is Direction
 */
static sem_t car_sem[4][4];



// TODO: Global variables: mutexes, data structures, etc...

// Mutex to prevent interection between cars
pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER;

// Array to count the number of cars processed in each lane
static int processed_count[4][4] = { {0} };

// A struct to define traffic lights
typedef struct {
  int side;
  int direction;
} TrafficLightArgs;

// Global counters to determine when all arrivals have been processed
static int total_processed = 0;
int total_arrivals = sizeof(input_car_arrivals) / sizeof(Car_Arrival);
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * supply_cars()
 *
 * A function for supplying car arrivals to the intersection.
 * This thread reads from the fixed input array (from input.h) 
 * and posts on the corresponding semaphore.
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
    printf("Car Arrival: Side: %d, Direction: %d, Time: %d\n", arrival.side, arrival.direction, arrival.time); // Debugging line
    
    // wait until this arrival is supposed to arrive
    sleep(arrival.time - t);  
    t = arrival.time;
    
    // Store the arrival in the appropriate lane's array
    curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction]++;

    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&car_sem[arrival.side][arrival.direction]);
  }
  return (0);
}


/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
  
    // Define side and direction
    int side = ((TrafficLightArgs*)arg)->side;
    int direction = ((TrafficLightArgs*)arg)->direction;
    
    // Create an infinite loop to manage lights
    while (1)
    {
        // Wait for a car arrival in this lane
        sem_wait(&car_sem[side][direction]);

        // If no more cars are expected, stop
        pthread_mutex_lock(&count_mutex);
        if (total_processed >= total_arrivals)
        {
            pthread_mutex_unlock(&count_mutex);
            break;
        }
        pthread_mutex_unlock(&count_mutex);

        // Get the arrival of the next car and increment the processed count
        Car_Arrival arrival = curr_car_arrivals[side][direction][processed_count[side][direction]];
        processed_count[side][direction]++;
            
        

        // Lock the intersection so that only one car enters at a time
        pthread_mutex_lock(&intersection_mutex);
        printf("traffic light %d, %d turns green at time %d for car %d\n",
                side, direction, get_time_passed(), arrival.id);

        // Wait for the car to cross
        sleep(CROSS_TIME);  
        printf("traffic light %d, %d turns red at time %d\n",
                side, direction, get_time_passed());
        pthread_mutex_unlock(&intersection_mutex);

        
        // Update the global processed count
        pthread_mutex_lock(&count_mutex);
        total_processed++;
        pthread_mutex_unlock(&count_mutex);
        
        // If all arrivals have been processed, exit the loop
        pthread_mutex_lock(&count_mutex);
        if (total_processed >= total_arrivals)
        {
            pthread_mutex_unlock(&count_mutex);
            break;
        }
        pthread_mutex_unlock(&count_mutex);
    }
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
  
  // start the timer
  start_time(); 


  // TODO: create a thread per traffic light that executes manage_light
    // Create 9 traffic light threads and 1 u-turn light thread
    const int lights = 9;  
    pthread_t threads[lights + 1];  
    TrafficLightArgs trafficLights[lights];

    // Fill in the parameters for each traffic light.
    // Here, we assume side and direction are represented by numbers:
    // side: 1,2,3 and direction: 0,1,2 (for 9 lights).
    for (int i = 0; i < lights; i++) {
        trafficLights[i].side = (i / 3) + 1;       
        trafficLights[i].direction = i % 3;    

        if (pthread_create(&threads[i], NULL, manage_light, &trafficLights[i]) != 0) {
            perror("Failed to create traffic light thread");
            exit(EXIT_FAILURE);
        }
    }

    // Create the u-turn light thread
    trafficLights[lights].side = 2;
    trafficLights[lights].direction = 3;
    if (pthread_create(&threads[lights], NULL, manage_light, &trafficLights[lights]) != 0) {
        perror("Failed to create u-turn light thread");
        exit(EXIT_FAILURE);
    }

  // TODO: create a thread that executes supply_cars()
  pthread_t supplier;
  pthread_create(&supplier, NULL, supply_cars, NULL);

  // Wait for the supplier to finish
  pthread_join(supplier, NULL);

  // TODO: wait for all threads to finish
    for (int i = 0; i < lights + 1; i++) {
        pthread_join(threads[i], NULL);
    }

  // destroy semaphores
  for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
          sem_destroy(&car_sem[i][j]);
      }
  }
}
