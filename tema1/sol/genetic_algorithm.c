#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "genetic_algorithm.h"

#define minim(a, b) (a<b ? a:b);

struct arg_struct {
    int thread_id;
    int object_count;
	individual* current_generation;
	individual* next_generation;

	const sack_object *objects;
	int generations_count;
	int sack_capacity;
	int P;
	pthread_barrier_t *barrier;
}args;


int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int argc, char *argv[], int *P)
{
	FILE *fp;

	if (argc < 4) {
		fprintf(stderr, "Usage:\n\t./tema1_par in_file generations_count number_of_threads\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

	//numar thread-uri
	*P = (int) atoi(argv[3]);
	if(P <= 0) {
		fprintf(stderr, "Numarul thread-urilor este 0 sau mai mic decat 0\n");
		return 0;
	}

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int object_count, int sack_capacity)
{
	int weight;
	int profit;

	for (int i = 0; i < object_count; ++i) {
		weight = 0;
		profit = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
			}
		}

		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	//int i;
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		//int first_count = 0, second_count = 0;

		/*for (i = 0; i < first->chromosome_length && i < second->chromosome_length; ++i) {
			first_count += first->chromosomes[i];
			second_count += second->chromosomes[i];
		}*/

		res = first->count - second->count; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}



void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}


void *thread_function(void *arguments)
{
	
	struct arg_struct *args = (struct arg_struct *)arguments;
	int ok = args->thread_id;
	int cursor;
	int count;
	individual *tmp = NULL;

	pthread_barrier_t *barrier = args->barrier;
	
	int start = args->thread_id * (double) args->object_count/args->P;
	int end = minim(args->object_count, (args->thread_id + 1)* (double) args->object_count/args->P);

	for (int i = start; i < end; ++i) {
		
		//printf("Index = %d \n", i);
		args->current_generation[i].fitness = 0;
		args->current_generation[i].chromosomes = (int*) calloc(args->object_count, sizeof(int));
		args->current_generation[i].chromosomes[i] = 1;
		args->current_generation[i].index = i;
		args->current_generation[i].chromosome_length = args->object_count;
		
		args->next_generation[i].fitness = 0;
		args->next_generation[i].chromosomes = (int*) calloc(args->object_count, sizeof(int));
		args->next_generation[i].index = i;
		args->next_generation[i].chromosome_length = args->object_count;
	}
	pthread_barrier_wait(barrier);

	for (int k = 0; k < args->generations_count; ++k) {
		cursor = 0;

		// compute fitness and sort by it
		compute_fitness_function(args->objects, args->current_generation, args->object_count, args->sack_capacity);
		
		


		//for ul din cmp
		
			int s, e;
			s = args->thread_id * (double) args->object_count/args->P;
			e = minim(count, (args->thread_id + 1)* (double) args->object_count/args->P);
		for(int j = s; j < (int) e; j++){
			args->current_generation[j].count = 0;
			for (int i = 0; i < (int) args->current_generation[j].chromosome_length; ++i) {
			args->current_generation[j].count += args->current_generation[j].chromosomes[i];
			
		}
		}
		
		if(ok == 0){
		qsort(args->current_generation, args->object_count, sizeof(individual), cmpfunc);
		
		}
		pthread_barrier_wait(barrier);


		// keep first 30% children (elite children selection)
		count = args->object_count * 3 / 10;

		int start1 = args->thread_id * (double) count/args->P;
		int end1 = minim(count, (args->thread_id + 1)* (double) count/args->P);

		for (int i = start1; i < end1; ++i) {
			copy_individual(args->current_generation + i, args->next_generation + i);
		}
		pthread_barrier_wait(barrier);

		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		count = args->object_count * 2 / 10;

		int start2 = args->thread_id * (double) count/args->P;
		int end2 = minim(count, (args->thread_id + 1)* (double) count/args->P);

		for (int i = start2; i < end2; ++i) {
			copy_individual(args->current_generation + i, args->next_generation + cursor + i);
			mutate_bit_string_1(args->next_generation + cursor + i, k);
		}
		pthread_barrier_wait(barrier);

		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		count = args->object_count * 2 / 10;


		int start3 = args->thread_id * (double) count/args->P;
		int end3 = minim(count, (args->thread_id + 1)* (double) count/args->P);
		for (int i = start3; i < end3; ++i) {
			copy_individual(args->current_generation + i + count, args->next_generation + cursor + i);
			mutate_bit_string_2(args->next_generation + cursor + i, k);
		}
		pthread_barrier_wait(barrier);
		cursor += count;

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = args->object_count * 3 / 10;

		if (count % 2 == 1) {
			copy_individual(args->current_generation + args->object_count - 1, args->next_generation + cursor + count - 1);
			count--;
		}


		int start4 = args->thread_id * (double) count/args->P;
		int end4 = minim(count, (args->thread_id + 1)* (double) count/args->P);

		if(start4 % 2 == 1){
			start4 += 1;
		}

		for (int i = start4; i < end4; i += 2) {
			crossover(args->current_generation + i, args->next_generation + cursor + i, k);
		}

		
		pthread_barrier_wait(barrier);

		// switch to new generation
		tmp = args->current_generation;
		args->current_generation = args->next_generation;
		args->next_generation = tmp;


		int start5 = args->thread_id * (double) args->object_count/args->P;
		int end5 = minim(args->object_count, (args->thread_id + 1)* (double) args->object_count/args->P);

		for (int i = start5; i < end5; ++i) {
			args->current_generation[i].index = i;
		}
		pthread_barrier_wait(barrier);

		if(ok == 0){
			if (k % 5 == 0) {
			print_best_fitness(args->current_generation);
			}
		}
		pthread_barrier_wait(barrier);

	}
	
	pthread_barrier_wait(barrier);


	pthread_exit(NULL);
}


void run_genetic_algorithm(const sack_object *objects, int object_count, int generations_count, int sack_capacity, int P)
{
	
	//int count, cursor;
	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));
	//individual *tmp = NULL;

	int i;
	pthread_t *tid =(pthread_t*) malloc(P*sizeof(pthread_t));
	int *thread_id =(int*) malloc(P*sizeof(int));
	pthread_barrier_t barrier;
	pthread_barrier_init(&barrier, NULL, P);
	struct arg_struct* args = (struct arg_struct*) malloc(P*sizeof(struct arg_struct));

	for (i = 0; i < P; i++) {
		thread_id[i] = i;
    	args[i].thread_id = thread_id[i];
		args[i].object_count = object_count;
		args[i].current_generation = current_generation;
		args[i].next_generation = next_generation;
		args[i].generations_count = generations_count;
		args[i].objects = objects;
		args[i].sack_capacity = sack_capacity;
		args[i].P = P;
		args[i].barrier = &barrier;
		pthread_create(&tid[i], NULL, thread_function, (void *)&args[i]);
	}


	// set initial generation (composed of object_count individuals with a single item in the sack)
	/*for (int i = 0; i < object_count; ++i) {
		current_generation[i].fitness = 0;
		current_generation[i].chromosomes = (int*) calloc(object_count, sizeof(int));
		current_generation[i].chromosomes[i] = 1;
		current_generation[i].index = i;
		current_generation[i].chromosome_length = object_count;

		next_generation[i].fitness = 0;
		next_generation[i].chromosomes = (int*) calloc(object_count, sizeof(int));
		next_generation[i].index = i;
		next_generation[i].chromosome_length = object_count;
	}*/

	

	// iterate for each generation
	/*for (int k = 0; k < generations_count; ++k) {
		cursor = 0;

		// compute fitness and sort by it
		compute_fitness_function(objects, args->current_generation, args->object_count, sack_capacity);
		qsort(args->current_generation, args->object_count, sizeof(individual), cmpfunc);

		// keep first 30% children (elite children selection)
		count = args->object_count * 3 / 10;

		args1->count = count;
		for (int i = 0; i < count; ++i) {
			copy_individual(args->current_generation + i, args->next_generation + i);
		}

		for (i = 0; i < P; i++) {
		thread_id[i] = i;
		args1->thread_id = i;
		args1->current_generation = args->current_generation + i;
		args1->next_generation = args->next_generation + i;
		pthread_create(&tid[i], NULL, thread_function1, (void *)&args1[i]);
		}

		for (i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
		}

		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		count = object_count * 2 / 10;
		for (int i = 0; i < count; ++i) {
			copy_individual(args->current_generation + i, args->next_generation + cursor + i);
			mutate_bit_string_1(args->next_generation + cursor + i, k);
		}
		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		count = args->object_count * 2 / 10;
		for (int i = 0; i < count; ++i) {
			copy_individual(args->current_generation + i + count, args->next_generation + cursor + i);
			mutate_bit_string_2(args->next_generation + cursor + i, k);
		}
		cursor += count;

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = args->object_count * 3 / 10;

		if (count % 2 == 1) {
			copy_individual(args->current_generation + args->object_count - 1, args->next_generation + cursor + count - 1);
			count--;
		}

		for (int i = 0; i < count; i += 2) {
			crossover(args->current_generation + i, args->next_generation + cursor + i, k);
		}

		// switch to new generation
		tmp = args->current_generation;
		args->current_generation = args->next_generation;
		args->next_generation = tmp;

		for (int i = 0; i < args->object_count; ++i) {
			args->current_generation[i].index = i;
		}

		if (k % 5 == 0) {
			print_best_fitness(args->current_generation);
		}
	}*/

	for (i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
		}

	compute_fitness_function(objects, args->current_generation, args->object_count, sack_capacity);

	/*for(int j = 0; j < (int) args->object_count; j++){
			for (int i = 0; i < (int) args->current_generation[j].chromosome_length; ++i) {
			args->current_generation[j].count += args->current_generation[j].chromosomes[i];
			
		}
	}*/
	qsort(args->current_generation, args->object_count, sizeof(individual), cmpfunc);
	
	print_best_fitness(args->current_generation);

	
	pthread_barrier_destroy(&barrier);
	// free resources for old generation
	free_generation(args->current_generation);
	free_generation(args->next_generation);

	// free resources
	free(args->current_generation);
	free(args->next_generation);
}

