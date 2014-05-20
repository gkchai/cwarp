// Header file to define the basic functions
#include <complex.h>


/*
Description: Initialization function to create the socket handles 
and set the buffer size

Arguments: 
	node_sock(int*)				- socket handle array
	numNodes (int)				- number of nodes
*/
void nodes_initialize(int* node_sock, int numNodes);

/*
Description: close the sockets opened for the nodes

Arguments: 
	node_sock(int*)				- socket handle array
	numNodes (int)				- number of nodes
*/
void nodes_disable(int* node_sock, int numNodes); 

/*
 Description: send a broadcast trigger to all WARP nodes in the setup
*/
void sendTrigger();


/*
 Description: read IQ samples from a given WARP node and store them in a given array 
 
 Arguments: 
	samples (double complex*) 		- pointer to sample array 
	start_sample (int)				- offset to the first sample to read
	num_samples (int)				- number of samples to read (between 1 and 2^15)
	node_id (int)					- identifier of the node  
	node_sock (int)					- identifier of the node socket  
	buffer_id (int)					- identifier of the buffer
	host_id (int)					- identifier of the host 
*/
void readIQ(double complex* samples, int start_sample, int num_samples, int node_sock, int node_id, int buffer_id, int host_id);

/*
 Description: write IQ samples to a given WARP node from a given array 
 
 Arguments: 
	samples (double complex*) 		- pointer to sample array 
	start_sample (int)				- offset to the first sample to write
	num_samples (int)				- number of samples to write (between 1 and 2^15)
	node_id (int)					- identifier of the node  
	node_sock (int)					- identifier of the node socket  
	buffer_id (int)					- identifier of the buffer
	host_id (int)					- identifier of the host 

 Requirements:
 	samples must be in [0, 1]  
*/
void writeIQ(double complex* samples, int start_sample, int num_samples, int node_sock, int node_id, int buffer_id, int host_id);


