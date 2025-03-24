/*
 * Intersection Part 1 - Basic Solution: Only one car in the intersection at a time.
 *
 * STUDENT_NAME_1 (STUDENT_NR_1)
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

#include "arrivals.h"             // Car arrival definitions
#include "intersection_time.h"      // Timing functions
#include "input.h"                // Input parameters and arrival array

// Global mutex to ensure only one car is in the intersection (Part 1)
pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER;

// 3D array to store car arrivals
static Car_Arrival curr_car_arrivals[4][4][20];

// Global array to track the number of processed cars per lane
static int processed_count[4][4] = { {0} };

// 2D array of semaphores for each lane (side, direction)
static sem_t car_sem[4][4];

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
  int num_curr_arrivals[4][4] = { {0} };

  for (int i = 0; i < sizeof(input_car_arrivals) / sizeof(Car_Arrival); i++)
  {
    Car_Arrival arrival = input_car_arrivals[i];
    printf("Car Arrival: Side: %d, Direction: %d, Time: %d\n", arrival.side, arrival.direction, arrival.time); // Debugging line
    
    sleep(arrival.time - t);  // wait until the car's arrival time
    t = arrival.time;
    
    // Store the arrival in the appropriate lane's array
    curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction]++;
    
    // Signal the traffic light thread that a car has arrived
    sem_post(&car_sem[arrival.side][arrival.direction]);
  }
  return NULL;
}


/*
 * manage_light()
 *
 * Implements the behavior of a traffic light for a specific lane.
 * Each thread waits for a car, then, using the global intersection_mutex,
 * allows one car to cross the intersection.
 *
 * The argument is expected to be a pointer to an array of two ints:
 * [side, direction].
 */
static void* manage_light(void* arg)
{
    int side = ((TrafficLightArgs*)arg)->side;
    int direction = ((TrafficLightArgs*)arg)->direction;
    
    while (1)
    {
        // Wait for a car arrival in this lane
        sem_wait(&car_sem[side][direction]);

        // If no more cars are expected (this example uses total count), break out.
        pthread_mutex_lock(&count_mutex);
        if (total_processed >= total_arrivals)
        {
            pthread_mutex_unlock(&count_mutex);
            break;
        }
        pthread_mutex_unlock(&count_mutex);
        
        // Special case for U-turn (direction == 3)
        if (direction == 3) {
            printf("Time %d: U-Turn Light (Side %d) GREEN: Car %d entering intersection.\n", get_time_passed(), side, curr_car_arrivals[side][direction][processed_count[side][direction]].id);
            sleep(CROSS_TIME);  // Simulate the time it takes for the car to cross
            printf("Time %d: U-Turn Light (Side %d) RED: Car %d leaving intersection.\n", get_time_passed(), side, curr_car_arrivals[side][direction][processed_count[side][direction]].id);
        } else {
            // Retrieve the next arrival from the lane
            Car_Arrival arrival = curr_car_arrivals[side][direction][processed_count[side][direction]];
            processed_count[side][direction]++;
            
            // Lock the intersection so that only one car enters at a time
            pthread_mutex_lock(&intersection_mutex);
            printf("Time %d: Light (Side %d, Dir %d) GREEN: Car %d entering intersection.\n",
                   get_time_passed(), side, direction, arrival.id);
            sleep(CROSS_TIME);  // Simulate the time it takes for the car to cross
            printf("Time %d: Light (Side %d, Dir %d) RED: Car %d leaving intersection.\n",
                   get_time_passed(), side, direction, arrival.id);
            pthread_mutex_unlock(&intersection_mutex);
        }
        
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
    return NULL;
}


int main(int argc, char * argv[])
{
  // Initialize semaphores for the 10 relevant lanes (3 directions per side and 1 U-turn)
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {  // 3 directions per side (no U-turn)
      sem_init(&car_sem[i][j], 0, 0);
    }
  }
  
  start_time(); // Start the simulation timer

  // Create one traffic light thread per lane (10 total: 3 directions per side)
  // TODO: create a thread per traffic light that executes manage_light
    const int lights = 9;  // 9 threads from the array
    pthread_t threads[lights + 1];  // +1 for the u-turn light

    // Create an array of TrafficLightArgs for the 9 traffic lights.
    TrafficLightArgs trafficLights[lights];

    // Fill in the parameters for each traffic light.
    // Here, we assume side and direction are represented by numbers:
    // side: 0,1,2 and direction: 0,1,2 (for 9 lights).
    for (int i = 0; i < lights; i++) {
        trafficLights[i].side = i / 3;       // yields 0,0,0,1,1,1,2,2,2
        trafficLights[i].direction = i % 3;    // yields 0,1,2,0,1,2,0,1,2

        if (pthread_create(&threads[i], NULL, manage_light, &trafficLights[i]) != 0) {
            perror("Failed to create traffic light thread");
            exit(EXIT_FAILURE);
        }
    }

    TrafficLightArgs uTurnLight;
    uTurnLight.side = 3;         // For example, assign side 3 to u-turn.
    uTurnLight.direction = 3;   // -1 indicates u-turn.
    if (pthread_create(&threads[lights], NULL, manage_light, &uTurnLight) != 0) {
        perror("Failed to create u-turn light thread");
        exit(EXIT_FAILURE);
    }
  // Create the supply_cars thread to provide arrivals
  pthread_t supplier;
  pthread_create(&supplier, NULL, supply_cars, NULL);

  // Wait for the supplier to finish
  pthread_join(supplier, NULL);

// TODO: wait for all threads to finish
    for (int i = 0; i < lights + 1; i++) {
        pthread_join(threads[i], NULL);
    }

  // Clean up: destroy all semaphores for the 10 lanes
  for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
          sem_destroy(&car_sem[i][j]);
      }
  }

  return 0;
}
