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

// Global simulation start time (make sure it's accessible; you may need to declare it extern in intersection_time.h)
extern struct timespec begin_time;

static Car_Arrival curr_car_arrivals[4][4][20];
static sem_t car_sem[4][4];
pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER;
static int processed_count[4][4] = { {0} };

typedef struct {
  int side;
  int direction;
} TrafficLightArgs;

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * supply_cars()
 *
 * (No changes here.)
 */
static void* supply_cars()
{
  int t = 0;
  int num_curr_arrivals[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
  for (int i = 0; i < sizeof(input_car_arrivals) / sizeof(Car_Arrival); i++)
  {
    Car_Arrival arrival = input_car_arrivals[i];
    // printf("Car Arrival: Side: %d, Direction: %d, Time: %d\n",
    //       arrival.side, arrival.direction, arrival.time);
    sleep(arrival.time - t);
    t = arrival.time;
    curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction]++;
    sem_post(&car_sem[arrival.side][arrival.direction]);
  }
  return (0);
}

/*
 * manage_light()
 *
 * Modified to use an absolute deadline equal to begin_time + END_TIME.
 */
static void* manage_light(void* arg)
{
    int side = ((TrafficLightArgs*)arg)->side;
    int direction = ((TrafficLightArgs*)arg)->direction;
    
    // Compute the absolute deadline: simulation start time + END_TIME.
    struct timespec deadline = begin_time;
    deadline.tv_sec += END_TIME;
    // (We assume tv_nsec stays the same; if needed, adjust for carryover.)
    
    while (1)
    {
        int sem_result = sem_timedwait(&car_sem[side][direction], &deadline);
        if (sem_result == -1 && errno == ETIMEDOUT) {
            // printf("Time %d: Traffic light %d,%d: Simulation ended (timeout). Stopping.\n",
            //       get_time_passed(), side, direction);
            break;
        }
        
        // Process the next arrival.
        Car_Arrival arrival = curr_car_arrivals[side][direction][processed_count[side][direction]];
        processed_count[side][direction]++;
            
        pthread_mutex_lock(&intersection_mutex);
        printf("traffic light %d %d turns green at time %d for car %d\n",
                side, direction, get_time_passed(), arrival.id);
        sleep(CROSS_TIME);
        printf("traffic light %d %d turns red at time %d\n",
                side, direction, get_time_passed());
        pthread_mutex_unlock(&intersection_mutex);
        
        pthread_mutex_lock(&count_mutex);
        pthread_mutex_unlock(&count_mutex);
    }
    return (0);
}

int main(int argc, char * argv[])
{
  // Initialize semaphores for all lanes (4 sides, 4 directions)
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {  
      sem_init(&car_sem[i][j], 0, 0);
    }
  }
  
  start_time(); // This sets begin_time
  
  // (Optional: initialize end_sem if you want to use it elsewhere)
  
  // Create traffic light threads.
  const int lights = 9;  // 9 regular traffic light threads
  pthread_t threads[lights + 1];  // +1 for u-turn
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
  
  // Create u-turn light thread (e.g., side 2, direction 3).
  TrafficLightArgs uTurnLight;
  uTurnLight.side = 2;
  uTurnLight.direction = 3;
  if (pthread_create(&threads[lights], NULL, manage_light, &uTurnLight) != 0) {
      perror("Failed to create u-turn light thread");
      exit(EXIT_FAILURE);
  }
  
  // Create the supply_cars thread.
  pthread_t supplier;
  pthread_create(&supplier, NULL, supply_cars, NULL);
  
  pthread_join(supplier, NULL);
  
  // Wait for all traffic light threads to finish.
  for (int i = 0; i < lights + 1; i++) {
      pthread_join(threads[i], NULL);
  }
  
  // Clean up semaphores.
  for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
          sem_destroy(&car_sem[i][j]);
      }
  }
  
  return 0;
}
