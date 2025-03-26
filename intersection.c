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

static Car_Arrival curr_car_arrivals[4][4][20];
static sem_t car_sem[4][4];
pthread_mutex_t intersection_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t end_sem;
static int processed_count[4][4] = { {0} };

typedef struct {
  int side;
  int direction;
} TrafficLightArgs;

static int total_processed = 0;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* supply_cars() {
  int t = 0;
  int num_curr_arrivals[4][4] = {{0}};
  for (int i = 0; i < sizeof(input_car_arrivals) / sizeof(Car_Arrival); i++) {
    Car_Arrival arrival = input_car_arrivals[i];
    sleep(arrival.time - t);  
    t = arrival.time;
    curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction]++;
    sem_post(&car_sem[arrival.side][arrival.direction]);
  }
  return (0);
}

static void* manage_light(void* arg) {
    int side = ((TrafficLightArgs*)arg)->side;
    int direction = ((TrafficLightArgs*)arg)->direction;
    
    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct timespec timeout;
        timeout.tv_sec = ts.tv_sec + END_TIME;
        timeout.tv_nsec = ts.tv_nsec;

        int sem_result = sem_timedwait(&car_sem[side][direction], &timeout);

        if (sem_result == -1 && errno == ETIMEDOUT) {
            pthread_mutex_lock(&count_mutex);
            if (ts.tv_sec >= END_TIME) {
                pthread_mutex_unlock(&count_mutex);
                break;
            }
            pthread_mutex_unlock(&count_mutex);
            continue;
        }

        Car_Arrival arrival = curr_car_arrivals[side][direction][processed_count[side][direction]];
        processed_count[side][direction]++;
        pthread_mutex_lock(&intersection_mutex);
        printf("traffic light %d, %d turns green at time %d for car %d\n", side, direction, get_time_passed(), arrival.id);
        sleep(CROSS_TIME);  
        printf("traffic light %d, %d turns red at time %d\n", side, direction, get_time_passed());
        pthread_mutex_unlock(&intersection_mutex);
        
        pthread_mutex_lock(&count_mutex);
        total_processed++;
        pthread_mutex_unlock(&count_mutex);
    }
    return (0);
}

int main(int argc, char * argv[]) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {  
      sem_init(&car_sem[i][j], 0, 0);
    }
  }
  
  start_time();

  const int lights = 9;  
  pthread_t threads[lights + 1];  
  TrafficLightArgs trafficLights[lights];

  for (int i = 0; i < lights; i++) {
      trafficLights[i].side = (i / 3) + 1;       
      trafficLights[i].direction = i % 3;    
      if (pthread_create(&threads[i], NULL, manage_light, &trafficLights[i]) != 0) {
          perror("Failed to create traffic light thread");
          exit(EXIT_FAILURE);
      }
  }

  trafficLights[lights].side = 2;
  trafficLights[lights].direction = 3;
  if (pthread_create(&threads[lights], NULL, manage_light, &trafficLights[lights]) != 0) {
      perror("Failed to create u-turn light thread");
      exit(EXIT_FAILURE);
  }

  pthread_t supplier;
  pthread_create(&supplier, NULL, supply_cars, NULL);
  pthread_join(supplier, NULL);

  for (int i = 0; i < lights + 1; i++) {
      pthread_join(threads[i], NULL);
  }

  for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
          sem_destroy(&car_sem[i][j]);
      }
  }
}