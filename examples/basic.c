// example file

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "warp_functions.h"

#define WRITE_TO_FILE 1

int main(){

	int num_samples = 32767;
	int start_sample = 0;
	int numNodes = 2; 
	int node_id_read = 1; // rxnode
	int node_id_write = 0; // txnode
	int node_sock[2];
	int buffer_id[4] = {1, 2, 4, 8}; // 1 for RFA, 2 for RFB, 4 for RFC, 8 for RFC  
	int host_id = 210;

	nodes_initialize(node_sock, numNodes);

	sendTrigger();

	double complex* read_samples =  (double complex*) malloc(num_samples*sizeof(double complex)); 
	bzero(read_samples, num_samples*sizeof(double complex));

	readIQ(read_samples, start_sample, num_samples, node_sock[0], node_id_read, buffer_id[0], host_id);
	printf("Done reading\n");

	double complex* write_samples = malloc(num_samples*sizeof(double complex));
	//bzero(write_samples, num_samples*sizeof(double complex));

	// copy read array into write
	write_samples = read_samples; 

	writeIQ(write_samples, start_sample, num_samples, node_sock[1], node_id_write, buffer_id[0], host_id);
	printf("Done writing\n");

	nodes_disable(node_sock, numNodes);

	if (WRITE_TO_FILE == 1){

		FILE *fp;

		char filestr[50];
		sprintf(filestr, "../traces/RxSamples_NodeID=%d_Num=%d_BuffID=%d.dat", node_id_read, num_samples, buffer_id[0]);
		fp = fopen(filestr, "w");

		int count; 
		for (count = 0; count < 1000; count++){

			fprintf(fp, "%f %f\n",  creal(read_samples[count]), cimag(read_samples[count]) );
			//printf("%d : %f %f\n", count,  creal(read_samples[count]), cimag(read_samples[count]));

		}
		fclose(fp);

	}

	printf("here !!\n");

	free(read_samples);
	//free(write_samples);

	return 0;
}
