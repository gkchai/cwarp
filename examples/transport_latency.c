
/* Benchmarking the transport latency of 
 parallel read/write functions 
- Only reads/writes single buffer from/to each node 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "warp_functions.h"

#define MAX_LOOP 10010
#define CLOCKTYPE CLOCK_MONOTONIC_RAW

// calculate the time difference in milliseconds 
double diff(struct timespec end, struct timespec start){

	if (end.tv_nsec - start.tv_nsec  > 0){
		return (end.tv_sec - start.tv_sec)*1000 + (end.tv_nsec - start.tv_nsec)/1.0e6; 
	}else{
		return (end.tv_sec - start.tv_sec -1)*1000 + (1.0e9 + end.tv_nsec - start.tv_nsec)/1.0e6; 
	}
}

// returns average measured time for the passed fucntion 
double measureLatency(int numNodes, int num_samples, int* arr_node_sock, int* arr_node_id, int host_id, void (*func) (int, int, int*, int*,  int)){

	int count;
	double avgD, stdD, totD=0, totSqD=0;
	double loopD[MAX_LOOP];
	struct timespec tsi, tsf;

    for (count = 0; count < MAX_LOOP; count++){
    
    	clock_gettime(CLOCKTYPE, &tsi);		

    	func(numNodes, num_samples, arr_node_sock, arr_node_id, host_id);

	    clock_gettime(CLOCKTYPE, &tsf);			

	    loopD[count] = diff(tsf, tsi);
	    if (count > 9){
	    	totD = totD + loopD[count]; // we skip the measurements of the first 10 calls
	    }	
		if (count > 9){
	    	totSqD = totSqD + pow(loopD[count],2); // we skip the measurements of the first 10 calls
	    }
	    // printf("delay node%d [%d] : %f\n", arr_node_id[0], count, loopLatency[count]);
    }

    avgD = totD/(MAX_LOOP - 10); 
    stdD = sqrt((totSqD/(MAX_LOOP - 10)) - pow(avgD,2)); // std = avg(X^2) - (avg(X))^2

	printf("delay values of %d numNodes: avg=%f std=%f\n", arr_node_id[0], avgD, stdD);

    return avgD;
}




// parallel read
void multi_read(int numNodes, int num_samples, int* arr_node_sock, int* arr_node_id, int host_id){

	double complex* samples = malloc(num_samples*sizeof(double complex)); 
	#pragma omp parallel    
	{
		int niter;
		#pragma omp for private(niter)	
		for (niter = 0; niter < numNodes; niter++){

			readIQ(samples, 0, num_samples, arr_node_sock[niter], arr_node_id[niter], 1, host_id); // default: buffer id = RFA, sample offset = 0 
			// readIQ(samples, 0, num_samples, arr_node_sock[niter], arr_node_id[niter], 2, host_id); // Read RFB after RFA
		}
	}	
	free(samples);
}

// parallel write
void multi_write(int numNodes, int num_samples, int* arr_node_sock, int* arr_node_id, int host_id){

	double complex* samples = malloc(num_samples*sizeof(double complex)); // for now we dump zero samples 
	#pragma omp parallel    
	{
		int niter;
		#pragma omp for private(niter)	
		for (niter = 0; niter < numNodes; niter++){
			
			writeIQ(samples, 0, num_samples, arr_node_sock[niter], arr_node_id[niter], 1, host_id); // default: buffer id = RFA, sample offset = 0 
			// writeIQ(samples, 0, num_samples, arr_node_sock[niter], arr_node_id[niter], 2, host_id); // Read RFB after RFA
		}
	}

	free(samples);
}



int main(){

	int read_nodes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	int write_nodes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 , 11, 12, 13, 14, 15};
	int num_samples = 32767; // use 16383 for 16K buffer
	int host_id = 210; // 10.0.0.210 is the IP address of host
	int numNodeRange = 16;

	double readLatency[numNodeRange];
	double writeLatency[numNodeRange];

	int numNodes;
	FILE *fpr, *fpw;
	fpr = fopen("../traces/read_time_new.dat", "w"); 
	fpw = fopen("../traces/write_time_new.dat", "w"); 


	for (numNodes = 1; numNodes <= numNodeRange; numNodes++){

		int arr_node_sock[numNodes];

		// first initialize the sockets 
		nodes_initialize(arr_node_sock, numNodes);
		printf("-------nodes intialiazed------------\n");

		sendTrigger(); // send trigger before reading buffers
		
		readLatency[numNodes] = measureLatency(numNodes, num_samples, arr_node_sock, read_nodes, host_id, multi_read);

		printf(" Read latency [Nodes=%d] = %2.2f \n", numNodes, readLatency[numNodes]);

		// close all opened sockets
		nodes_disable(arr_node_sock, numNodes);
		printf("-------nodes closed------------\n");

		fprintf(fpr, "%d \t %2.4f\n", numNodes, readLatency[numNodes]);
	} 	

	fclose(fpr);

	for (numNodes = 1; numNodes <= numNodeRange; numNodes++){

		int arr_node_sock[numNodes];

		// first initialize the sockets 
		nodes_initialize(arr_node_sock, numNodes);
		printf("-------nodes intialiazed------------\n");
		
		writeLatency[numNodes] = measureLatency(numNodes, num_samples, arr_node_sock, read_nodes, host_id, multi_write);

		sendTrigger(); // send trigger after writing 

		printf(" Write latency [Nodes=%d] = %2.2f \n", numNodes, writeLatency[numNodes]);

		// close all opened sockets
		nodes_disable(arr_node_sock, numNodes);
		printf("-------nodes closed------------\n");

		fprintf(fpw, "%d \t %2.4f\n", numNodes, writeLatency[numNodes]);
	} 

	fclose(fpw);

	return 0;
}