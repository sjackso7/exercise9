#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <string.h>

#define MAX_LENGTH 2000
#define DEF_MODE 0666

/* result stores either the max value or the sum of all values in the array */
/* choice indicates if function should find max (if 1) or sum (if 2) 
/* nums is the array of numbers the program should process */
static int result = 0, choice, nums[MAX_LENGTH + 1];
/* lock mechanism(?) */
static pthread_mutex_t mutex;

/* Thread function for adding values to matrix */
void *add_elements(void *params);

/* Thread function for summing or finding the max value */
void *action(void *params);

/* array_params struct will store the arguments for each thread */
typedef struct{
  int start_point, end_point, thread;
} array_params;

struct timeval tv_delta(struct timeval start, struct timeval end){
	struct timeval delta = end;

	delta.tv_sec -= start.tv_sec;
	delta.tv_usec -= start.tv_usec;
	if(delta.tv_usec < 0){
	  delta.tv_usec += 1000000;
	  delta.tv_sec--;
	}

	return delta;
}

int main(int argc, char *argv[]){
	/* i is counter variable, target is total number of elements in array,
	thread_count is number of threads, seg_leng is the number of elements
	a majority of threads will process, current_seg tracks whhere a segment
	should start */
	int i, target = atoi(argv[1]), thread_count = atoi(argv[2]),
	  seg_leng = target / thread_count, current_seg = 0; 
	pthread_t tids[MAX_LENGTH + 1];
	/* These structs will track time */
	struct rusage start_ru, end_ru;
	struct timeval start_wall, end_wall;
	struct timeval diff_ru_utime, diff_wall, diff_ru_stime;
	/* Array stores each threads' arguments, param will store a single 
	thread's arguments */
	array_params *segs[MAX_LENGTH + 1], *param;
	
	/* Get whether it should find max or sum */
	choice = atoi(argv[4]);

	/* Start timing */
	getrusage(RUSAGE_SELF, &start_ru);
    gettimeofday(&start_wall, NULL);

	/* Initiate anti-running case variable */
	pthread_mutex_init(&mutex, NULL);
	
	/* Create the arguments each thread will use */
	for(i = 0; i < thread_count; i++){
	  param = malloc(sizeof(array_params));

	  /* Create arguments for most threads */
	  if(i != (thread_count - 1)){
	    param->start_point = current_seg;
		param->end_point = current_seg + seg_leng - 1;
		param->thread = i;
	  }

	  /* Last thread will take care of the remainder of threads */
	  else{
	    param->start_point = current_seg;
		param->end_point = target - 1;
		param->thread = i;
	  }

	  /* Store arguments and move up */
	  segs[i] = param;

	  current_seg += seg_leng;
	}

	/* Seed the random number generator */
	srand(atoi(argv[3]));
	
	/* Create threads that will generate the random values for the array */
	for(i = 0; i < thread_count; i++){	  
	  pthread_create(&tids[i], NULL, add_elements, segs[i]);
	}

	/* Reap the threads */
	for(i = 0; i < atoi(argv[2]); i++){
	  pthread_join(tids[i], NULL);
	}

	/* Create threads that will perform function (max or sum) */
    for(i = 0; i < thread_count; i++){
	  pthread_create(&tids[i], NULL, action, segs[i]);
	}

	/* Reap the threads */
	for(i = 0; i < atoi(argv[2]); i++){
	  pthread_join(tids[i], NULL);
	}

	/* Free structs */
	for(i = 0; i < thread_count; i++){
	  free(segs[i]);
	}
	
	/* Get end time */
	gettimeofday(&end_wall, NULL);
	getrusage(RUSAGE_SELF, &end_ru);

	/* Computing difference */
	diff_ru_utime = tv_delta(start_ru.ru_utime, end_ru.ru_utime);
	diff_ru_stime = tv_delta(start_ru.ru_stime, end_ru.ru_stime);
	diff_wall = tv_delta(start_wall, end_wall);

	/* Print time */
	printf("User time: %ld.%06ld\n", diff_ru_utime.tv_sec,
		   diff_ru_utime.tv_usec);
	printf("System time: %ld.%06ld\n", diff_ru_stime.tv_sec,
		   diff_ru_stime.tv_usec);
	printf("Wall time: %ld.%06ld\n", diff_wall.tv_sec, diff_wall.tv_usec);

	/* Print results if user specifies */
	if(strcmp(argv[5], "Y") == 0){
	  /* Print the numbers the threads generated */
	  printf("\nArray Values: \n");

	  for(i = 0; i < atoi(argv[1]); i++){
		printf("%d\n", nums[i]);
	  }

	  printf("\n");
	  
	  /* Print max number */
	  if(choice == 1){
		printf("Max: %d\n", result);
	  }

	  /* Print sum */
	  else{
		printf("Sum: %d\n", result);
	  }
	}
	
	return 0;
}

/* add_elements is the function threads will use to add a random number 
to the array of numbers */
void *add_elements(void *params){
  /* stores the maxiumum number of values the array of numbers will store */
	array_params segs = *((array_params *)params);
	int *curr = &(segs.start_point), *end = &(segs.end_point);

	/* Store random value in thread if end of segment not reached */
	while(*curr <= *end){
	  int num = rand();
	  
	  pthread_mutex_lock(&mutex);
	  nums[*curr] = num;
	  pthread_mutex_unlock(&mutex);

	  (*curr)++;
	}

	return NULL;
}

/* Finds the maximum number in the array or the sum of
 all values in the array */
void *action(void *params){
    /* Tracks whether maxiumum or the sum should be found */
	array_params segs = *(array_params *)params;
    int *curr = &(segs.start_point), *end = &(segs.end_point);

	/* If end of segment not reached... */
   	while(*curr <= *end){

	  /* Check if max and if looking for max. If yes to both, store max */
	  if(choice == 1){
		pthread_mutex_lock(&mutex);
		if(nums[*curr] > result){
		  result = nums[*curr];
		}
		pthread_mutex_unlock(&mutex);
	  }

	  /* Calculate the sum if the sum should be found */
	  if(choice == 2){
		pthread_mutex_lock(&mutex);
		result = (result + nums[*curr]) % 1000000;
		pthread_mutex_unlock(&mutex);
	  }

	  /* Move to the next value in the array */
	  (*curr)++;
	}

	return NULL;
}
