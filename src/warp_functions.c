// headers file 
#include "warp_functions.h"
#include "warp_transport.h"
#include <string.h>

/*
Description: Initialization function to create the socket handles 
and set the buffer size

Arguments: 
	node_sock(int*)				- socket handle array
	numNodes (int)				- number of nodes
*/
void nodes_initialize(int* node_sock , int numNodes){

	if(!initialized){

	   // initalized is a static global variable 
	   // all sockets are memset'ed with this function call
       init_wl_mex_udp_transport();    
  	}	



	int num;
	for (num= 0; num < numNodes; num++){
		node_sock[num] = init_socket(); // socket handle for each node


		int REQUESTED_BUFF_SIZE = pow(2,24);

		// send buffer config
		set_send_buffer_size( node_sock[num], REQUESTED_BUFF_SIZE );
		int os_size = get_send_buffer_size(node_sock[num]); 

		if  (os_size < REQUESTED_BUFF_SIZE){
			 // printf("OS reduced send buff for sock %d to %d \n", node_sock[num], os_size);
		}

		// receive buffer config
		set_receive_buffer_size( node_sock[num], REQUESTED_BUFF_SIZE );
		os_size = get_receive_buffer_size( node_sock[num]); 

		if  (os_size < REQUESTED_BUFF_SIZE){
			 // printf("OS reduced send buff for sock %d to %d \n", node_sock[num], os_size);
		}
	}		
}

/*
Description: close the sockets opened for the nodes

Arguments: 
	node_sock(int*)				- socket handle array
	numNodes (int)				- number of nodes
*/
void nodes_disable(int* node_sock, int numNodes){

	int num; 
	for (num=0; num < numNodes; num++){
		close_socket(node_sock[num]);
	}
}


/*
 Description: send a broadcast trigger to all WARP nodes in the setup
*/
void sendTrigger(){

	assert(initialized ==1);
	
	int trig_sock = init_socket(); 

	get_send_buffer_size(trig_sock);
    get_receive_buffer_size(trig_sock);
	char trig_buffer[18] = {0, 0, 255, 255, 0, 202, 0, 0, 0, 4, 0, 13, 0, 0, 0, 0, 0, 1};

	// port 10000 is used for broadcast
	sendData(trig_sock, trig_buffer, sizeof(trig_buffer), "10.0.0.255", 10000);

	close_socket(trig_sock);
}


/*
 Description: read IQ samples from a given WARP node and store them in a given array 
 
 Arguments: 
	samples (double complex*) 		- pointer to sample array 
	start_sample (int)				- offset to the first sample to read
	num_samples (int)				- number of samples to read (between 1 and 2^15)
	node_sock (int)					- identifier of the node socket  
	node_id (int)					- identifier of the node  
	buffer_id (int)					- identifier of the buffer
	host_id (int)					- identifier of the host 
*/
void readIQ(double complex* samples, int start_sample, int num_samples, int node_sock, int node_id, int buffer_id, int host_id){

	assert(initialized==1);

	int node_port = 9000 + node_id; // source port at host for node	
	int max_length =  8928;//1438, 8938 1422, 8928; // number of bytes available for IQ samples after all headers
	int num_pkts = (int)(num_samples*4/max_length) + 1 ;
	
	char readIQ_buffer[42] =  {0, 0, 0, node_id, 0, host_id, 0, 1, 0, 28, 0, 10, 0, 0, 48, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	char base_ip_addr[20];
	char str[15];
	sprintf(str, "%d", node_id+1);

	strcpy(base_ip_addr, "10.0.0.");
	strcat(base_ip_addr, str);	  

	readSamples(samples, node_sock, readIQ_buffer , 42, base_ip_addr, node_port, num_samples, (uint32) buffer_id, start_sample, max_length, num_pkts);    
}

/*
 Description: write IQ samples to a given WARP node from a given array 
 
 Arguments: 
	samples (double complex*) 		- pointer to sample array 
	start_sample (int)				- offset to the first sample to write
	num_samples (int)				- number of samples to write (between 1 and 2^15)
	node_sock (int)					- identifier of the node socket  
	node_id (int)					- identifier of the node  
	buffer_id (int)					- identifier of the buffer
	host_id (int)					- identifier of the host 
*/
void writeIQ(double complex* samples, int start_sample, int num_samples, int node_sock, int node_id, int buffer_id, int host_id){

	// assert(initialized==1);

	int node_port = 9000 + node_id; // source port at host for node	
	int max_length =  8928;//1438, 8938 1422, 8928; // number of bytes available for IQ samples after all headers
	int num_pkts = (int)(num_samples*4/max_length) + 1;
	int max_samples = 2232; //366 2232	

	char writeIQ_buffer[22] =  {0, 0, 0, node_id, 0, host_id, 0, 1, 0, 8, 0, 9, 0, 0, 48, 0, 0, 7, 0, 0, 0, 0};

	uint16* sample_I_buffer = (uint16* ) malloc(num_samples*sizeof(uint16));
	uint16* sample_Q_buffer = (uint16* ) malloc(num_samples*sizeof(uint16));
	
	bzero(sample_I_buffer, num_samples*sizeof(uint16));
	bzero(sample_Q_buffer, num_samples*sizeof(uint16));

	int index;

	for (index = 0; index < num_samples; index++){	  
	  sample_I_buffer[index] = (uint16) pow(2,15)*creal(samples[index]); //UFix_16_15
	  sample_Q_buffer[index] = (uint16) pow(2,15)*cimag(samples[index]);
	}

	char base_ip_addr[20];
	char str[15];
	sprintf(str, "%d", node_id+1);

	strcpy(base_ip_addr, "10.0.0.");
	strcat(base_ip_addr, str);

	writeSamples(node_sock, writeIQ_buffer, 8962, (char*) base_ip_addr, node_port, num_samples, sample_I_buffer, sample_Q_buffer, (uint32) buffer_id, start_sample, num_pkts, max_samples, TRANSPORT_WARP_HW_v3);
}