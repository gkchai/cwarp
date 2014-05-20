// include the header 
#include "warp_transport.h"
#include "omp.h"


/*********************** Global Variable Definitions *************************/

int       initialized    = 0;   // Global variable to initialize the driver
static int       tx_buffer_size = 0;   // Global variable for TX buffer size
static int       rx_buffer_size = 0;   // Global variable for RX buffer size
wl_trans_socket  sockets[TRANSPORT_MAX_SOCKETS];  // Global structure of socket connections




/*****************************************************************************/
/**
*  Function:  init_wl_mex_udp_transport
* 
*  Initialize the driver
*
******************************************************************************/

void init_wl_mex_udp_transport( void ) {
    int i;

    // Print initalization information
    // printf("Loaded wl_mex_udp_transport version %s \n", WL_MEX_UDP_TRANSPORT_VERSION );

    // Initialize Socket datastructure
    for ( i = 0; i < TRANSPORT_MAX_SOCKETS; i++ ) {
        memset( &sockets[i], 0, sizeof(wl_trans_socket) );
        
        sockets[i].handle  = INVALID_SOCKET;
        sockets[i].status  = TRANSPORT_SOCKET_FREE;
        sockets[i].timeout = 0;
        sockets[i].packet  = NULL;
    }

#ifdef WIN32
    // Load the Winsock 2.0 DLL
    if ( WSAStartup(MAKEWORD(2, 0), &wsaData) != 0 ) {
        die_with_error("WSAStartup() failed");
    }
#endif
        
    // Set cleanup function to remove persistent data    
   cleanup();

    // Set initialized flag to '1' so this is not run again
    initialized = 1;
}


/*****************************************************************************/
/**
*  Function:  init_socket
*
*  Initializes a socket and returns the index in to the sockets array
*
******************************************************************************/

int init_socket( void ) {
    int i;
    
    // Allocate a socket in the datastructure
    for ( i = 0; i < TRANSPORT_MAX_SOCKETS; i++ ) {
        if ( sockets[i].status == TRANSPORT_SOCKET_FREE ) {  break; }    
    }

    // Check to see if we cannot allocate a socket
    if ( i == TRANSPORT_MAX_SOCKETS) {
        die_with_error("Error:  Cannot allocate a socket");
    }
        
    // Create a best-effort datagram socket using UDP
    if ( ( sockets[i].handle = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) < 0) {
        die_with_error("socket() failed");
    }

    // Update the status field of the socket
    sockets[i].status = TRANSPORT_SOCKET_IN_USE;
    
    // Set the reuse_address and broadcast flags for all sockets
    set_reuse_address( i, 1 );
    set_broadcast( i, 1 );
    
    // Listen on the socket; Make sure we have a non-blocking socket
    listen( sockets[i].handle, TRANSPORT_NUM_PENDING );
    non_blocking_socket( sockets[i].handle );

    return i;    
}


/*****************************************************************************/
/**
*  Function:  set_so_timeout
*
*  Sets the Socket Timeout to the value (in ms)
*
******************************************************************************/
void set_so_timeout( int index, int value ) {

    sockets[index].timeout = value;
}


/*****************************************************************************/
/**
*  Function:  set_reuse_address
*
*  Sets the Reuse Address option on the socket
*
******************************************************************************/
void set_reuse_address( int index, int value ) {
    int optval;

    if ( value ) {
        optval = 1;
        setsockopt( sockets[index].handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval) );
    } else {
        optval = 0;
        setsockopt( sockets[index].handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval) );
    }    
}

/*****************************************************************************/
/**
*  Function:  set_broadcast
*
*  Sets the Broadcast option on the socket
*
******************************************************************************/
void set_broadcast( int index, int value ) {
    int optval;

    if ( value ) {
        optval = 1;
        setsockopt( sockets[index].handle, SOL_SOCKET, SO_BROADCAST, (const char *)&optval, sizeof(optval) );
    } else {
        optval = 0;
        setsockopt( sockets[index].handle, SOL_SOCKET, SO_BROADCAST, (const char *)&optval, sizeof(optval) );
    }    
}


/*****************************************************************************/
/**
*  Function:  set_send_buffer_size
*
*  Sets the send buffer size on the socket
*
******************************************************************************/
void set_send_buffer_size( int index, int size ) {
    int optval = size;

    // Set the global variable to what we requested the transmit buffer size to be
    tx_buffer_size = size;

    setsockopt( sockets[index].handle, SOL_SOCKET, SO_SNDBUF, (const char *)&optval, sizeof(optval) );
}


/*****************************************************************************/
/**
*  Function:  get_send_buffer_size
*
*  Gets the send buffer size on the socket
*
******************************************************************************/
int get_send_buffer_size( int index ) {
    int optval = 0;
    int optlen = sizeof(int);
    int retval = 0;
    
    if ( (retval = getsockopt( sockets[index].handle, SOL_SOCKET, SO_SNDBUF, (char *)&optval, (socklen_t *)&optlen )) != 0 ) {
        die_with_error("Error:  Could not get socket option - send buffer size"); 
    }
    
#ifdef _DEBUG_    
    printf("Send Buffer Size:  %d \n", optval );
#endif
    
    // Set the global variable to what the OS reports that it is
    tx_buffer_size = optval;

    return optval;
}


/*****************************************************************************/
/**
*  Function:  set_receive_buffer_size
*
*  Sets the receive buffer size on the socket
*
******************************************************************************/
void set_receive_buffer_size( int index, int size ) {
    int optval = size;

    // Set the global variable to what we requested the receive buffer size to be
    rx_buffer_size = size;

    setsockopt( sockets[index].handle, SOL_SOCKET, SO_RCVBUF, (const char *)&optval, sizeof(optval) );
}


/*****************************************************************************/
/**
*  Function:  get_receive_buffer_size
*
*  Gets the receive buffer size on the socket
*
******************************************************************************/
int get_receive_buffer_size( int index ) {
    int optval = 0;
    int optlen = sizeof(int);
    int retval = 0;
    
    if ( (retval = getsockopt( sockets[index].handle, SOL_SOCKET, SO_RCVBUF, (char *)&optval, (socklen_t *)&optlen )) != 0 ) {
        die_with_error("Error:  Could not get socket option - send buffer size"); 
    }
    
#ifdef _DEBUG_    
    printf("Rcvd Buffer Size:  %d \n", optval );
#endif
    
    // Set the global variable to what the OS reports that it is
    rx_buffer_size = optval;

    return optval;
}


/*****************************************************************************/
/**
*  Function:  close_socket
*
*  Closes the socket based on the index
*
******************************************************************************/
void close_socket( int index ) {

#ifdef _DEBUG_
    printf("Close Socket: %d\n", index);
#endif    

    if ( sockets[index].handle != INVALID_SOCKET ) {
        close( sockets[index].handle );
        
        if ( sockets[index].packet != NULL ) {
            free( sockets[index].packet );
        }
    } else {
        printf( "WARNING:  Connection %d already closed.\n", index );
    }

    sockets[index].handle  = INVALID_SOCKET;
    sockets[index].status  = TRANSPORT_SOCKET_FREE;
    sockets[index].timeout = 0;
    sockets[index].packet  = NULL;
}


/*****************************************************************************/
/**
*  Function:  send_socket
*
*  Sends the buffer to the IP address / Port that is passed in
*
******************************************************************************/
int send_socket( int index, char *buffer, int length, char *ip_addr, int port ) {

    struct sockaddr_in socket_addr;  // Socket address
    int                length_sent;
    int                size;

    // Construct the address structure
    memset( &socket_addr, 0, sizeof(socket_addr) );        // Zero out structure 
    socket_addr.sin_family      = AF_INET;                 // Internet address family
    socket_addr.sin_addr.s_addr = inet_addr(ip_addr);      // IP address 
    socket_addr.sin_port        = htons(port);             // Port 

    // If we are sending a large amount of data, we need to make sure the entire 
    // buffer has been sent.
    
    length_sent = 0;
    size        = 0xFFFF;
    
    if ( sockets[index].status != TRANSPORT_SOCKET_IN_USE ) {
        return length_sent;
    }

    while ( length_sent < length ) {
    
        // If we did not send more than MIN_SEND_SIZE, then wait a bit
        if ( size < TRANSPORT_MIN_SEND_SIZE ) {
            usleep( TRANSPORT_SLEEP_TIME );
        }

        // Send as much data as possible to the address
        size = sendto( sockets[index].handle, &buffer[length_sent], (length - length_sent), 0, 
                      (struct sockaddr *) &socket_addr, sizeof(socket_addr) );

        // Check the return value    
        if ( size == SOCKET_ERROR )  {
            if ( get_last_error != EWOULDBLOCK ) {
                die_with_error("Error:  Socket Error.");
            } else {
                // If the socket is not ready, then we did not send any bytes
                length_sent += 0;
            }
        } else {
            // Update how many bytes we sent
            length_sent += size;        
        }

        // TODO:  IMPLEMENT A TIMEOUT SO WE DONT GET STUCK HERE FOREVER
        //        FOR WARPLab 7.3.0, this is not implemented and has not 
        //        been an issue during testing.
    }
    
    return length_sent;
}


/*****************************************************************************/
/**
*  Function:  receive_socket
*
*  Reads data from the socket; will return 0 if no data is available
*
******************************************************************************/
int receive_socket( int index, int length, char * buffer ) {

    wl_trans_data_pkt  *pkt;           
    int                 size;
    int                 socket_addr_size = sizeof(struct sockaddr_in);
    
    // Allocate a packet in memory if necessary
    if ( sockets[index].packet == NULL ) {
        sockets[index].packet = (wl_trans_data_pkt *) malloc( sizeof(wl_trans_data_pkt) );
        
	  // only required is matlab as the memory in it is non persistent
      //  make_persistent( sockets[index].packet );

        if ( sockets[index].packet == NULL ) {
            die_with_error("Error:  Cannot allocate memory for packet.");        
        }

        memset( sockets[index].packet, 0, sizeof(wl_trans_data_pkt));        
    }

    // Get the packet associcated with the index
    pkt = sockets[index].packet;

    // If we have a packet from the last recevie call, then zero out the address structure    
    if ( pkt->length != 0 ) {
        memset( &(pkt->address), 0, sizeof(socket_addr_size));
    }

    // Receive a response 
    size = recvfrom( sockets[index].handle, buffer, length, 0, 
                    (struct sockaddr *) &(pkt->address), (socklen_t *) &socket_addr_size );


    // Check on error conditions
    if ( size == SOCKET_ERROR )  {
        if ( get_last_error != EWOULDBLOCK ) {
            die_with_error("Error:  Socket Error.");
        } else {
            // If the socket is not ready, then just return a size of 0 so the function can be 
            // called again
            size = 0;
        }
    } else {
                    
        // Update the packet associated with the socket
        //   NOTE:  pkt.address was updated via the function call
        // printf("index = %d, received size = %d\n", index, size);
        pkt->buf     = buffer;
        pkt->offset  = 0;
    }

    // Update the packet length so we can determine when we need to zero out pkt.address
    pkt->length  = size;

    return size;
}


/*****************************************************************************/
/**
*  Function:  cleanup
*
*  Function called by atMexExit to close everything
*
******************************************************************************/
void cleanup( void ) {
    int i;

    // printf("MEX-file is terminating\n");

    // Close all sockets
    for ( i = 0; i < TRANSPORT_MAX_SOCKETS; i++ ) {
        if ( sockets[i].handle != INVALID_SOCKET ) {  close_socket( i ); }    
    }

#ifdef WIN32
    WSACleanup();  // Cleanup Winsock 
#endif

}


/*****************************************************************************/
/**
*  Function:  print_version
*
* This function will print the version of the wl_mex_udp_transport driver
*
******************************************************************************/
void print_version( ) {

    printf("WARPLab MEX UDP Transport v%s (compiled %s %s)\n", WL_MEX_UDP_TRANSPORT_VERSION, __DATE__, __TIME__);
    printf("Copyright 2013, Mango Communications. All rights reserved.\n");
    printf("Distributed under the WARP license:  http://warpproject.org/license  \n");
}


/*****************************************************************************/
/**
*  Function:  print_usage
*
* This function will print the usage of the wl_mex_udp_transport driver
*
******************************************************************************/
void print_usage( ) {

    printf("Usage:  WARPLab MEX Transport v%s \n", WL_MEX_UDP_TRANSPORT_VERSION );
    printf("Standard WARPLab transport functions: \n");
    printf("    1.                  wl_mex_udp_transport('version') \n");
    printf("    2. index          = wl_mex_udp_transport('init_socket') \n");
    printf("    3.                  wl_mex_udp_transport('set_so_timeout', index, timeout) \n");
    printf("    4.                  wl_mex_udp_transport('set_send_buf_size', index, size) \n");
    printf("    5. size           = wl_mex_udp_transport('get_send_buf_size', index) \n");
    printf("    6.                  wl_mex_udp_transport('set_rcvd_buf_size', index, size) \n");
    printf("    7. size           = wl_mex_udp_transport('get_rcvd_buf_size', index) \n");
    printf("    8.                  wl_mex_udp_transport('close', index) \n");
    printf("    9. size           = wl_mex_udp_transport('send', index, buffer, length, ip_addr, port) \n");
    printf("   10. [size, buffer] = wl_mex_udp_transport('receive', index, length ) \n");
    printf("\n");
    printf("Additional WARPLab MEX UDP transport functions: \n");
    printf("    1. [num_samples, cmds_used, samples]  = wl_mex_udp_transport('read_rssi' / 'read_iq', \n");
    printf("                                                index, buffer, length, ip_addr, port, \n");
    printf("                                                number_samples, buffer_id, start_sample) \n");
    printf("    2. cmds_used                          = wl_mex_udp_transport('write_iq', \n");
    printf("                                                index, cmd_buffer, max_length, ip_addr, port, \n");
    printf("                                                number_samples, sample_buffer, buffer_id, \n");
    printf("                                                start_sample, num_pkts, max_samples, hw_ver) \n");
    printf("\n");
    printf("See documentation for further details.\n");
    printf("\n");
}


/*****************************************************************************/
/**
*  Function:  die
*
*  Generates an error message and cause the program to halt
*
******************************************************************************/
void die( ) {
    //mexErrMsgTxt("Error:  See description above.");
    printf("Error:  See description above.");
	cleanup();
	exit(0);
}


/*****************************************************************************/
/**
*  Function:  die_with_error
*
* This function will error out of the wl_mex_udp_transport function call
*
******************************************************************************/
void die_with_error(char *errorMessage) {
    printf("%s \n   Socket Error Code: %d\n", errorMessage, get_last_error );
    die();
}



/*****************************************************************************/
/**
*  Function:  print_sockets
*
*  Debug function to print socket table
*
******************************************************************************/
void print_sockets( void ) {
    int i;
    
    printf("Sockets: \n");    
    
    for ( i = 0; i < TRANSPORT_MAX_SOCKETS; i++ ) {
        printf("    socket[%d]:  handle = 0x%4x,   timeout = 0x%4x,  status = 0x%4x", 
               i, sockets[i].handle, sockets[i].timeout, sockets[i].status);
    }

    printf("\n");
}


#ifdef _DEBUG_
/*****************************************************************************/
/**
*  Function:  print_buffer
*
*  Debug function to print a buffer
*
******************************************************************************/
void print_buffer(unsigned char *buf, int size) {
	int i;

	printf("Buffer: (0x%x bytes)\n", size);

	for (i=0; i<size; i++) {
        printf("%2x ", buf[i] );
        if ( (((i + 1) % 16) == 0) && ((i + 1) != size) ) {
            printf("\n");
        }
	}
	printf("\n\n");
}


/*****************************************************************************/
/**
*  Function:  print_buffer_16
*
*  Debug function to print a buffer
*
******************************************************************************/
void print_buffer_16(uint16 *buf, int size) {
	int i;

	printf("Buffer: (0x%x bytes)\n", (2*size));

	for (i=0; i<size; i++) {
        printf("%4x ", buf[i] );
        if ( (((i + 1) % 16) == 0) && ((i + 1) != size) ) {
            printf("\n");
        }
	}
	printf("\n\n");
}


/*****************************************************************************/
/**
*  Function:  print_buffer_32
*
*  Debug function to print a buffer
*
******************************************************************************/
void print_buffer_32(uint32 *buf, int size) {
	int i;

	printf("Buffer: (0x%x bytes)\n", (4*size));

	for (i=0; i<size; i++) {
        printf("%8x ", buf[i] );
        if ( (((i + 1) % 8) == 0) && ((i + 1) != size) ) {
            printf("\n");
        }
	}
	printf("\n\n");
}

#endif


/*****************************************************************************/
/**
*  Function:  endian_swap_16
*
* This function will perform an byte endian swap on a 16-bit value 
*
******************************************************************************/
uint16 endian_swap_16(uint16 value) {

	return (((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8));
}


/*****************************************************************************/
/**
*  Function:  endian_swap_32
*
* This function will perform an byte endian swap on a 32-bit value 
*
******************************************************************************/
uint32 endian_swap_32(uint32 value) {
	uint16 lo;
	uint16 hi;

	// get each of the half words from the 32 bit word
	lo = (uint16) (value & 0x0000FFFF);
	hi = (uint16) ((value & 0xFFFF0000) >> 16);

	// byte swap each of the 16 bit half words
	lo = (((lo & 0xFF00) >> 8) | ((lo & 0x00FF) << 8));
	hi = (((hi & 0xFF00) >> 8) | ((hi & 0x00FF) << 8));

	// swap the half words before returning the value
	return (uint32) ((lo << 16) | hi);
}



//------------------------------------------------------
        // size = wl_mex_udp_transport('send', handle, buffer, length, ip_addr, port)
        //   - Arguments:
        //     - handle (int)     - index to the requested socket
        //     - buffer (char *)  - Buffer of data to be sent
        //     - length (int)     - Length of data to be sent
        //     - ip_addr          - IP Address to send data to
        //     - port             - Port to send data to
        //   - Returns:
        //     - size (int)       - size of data sent (in bytes)

int sendData(int handle, char* buffer, int length, char* ip_addr, int port){

#ifdef _DEBUG_
            printf("Function : TRANSPORT_SEND\n");
#endif
            

#ifdef _DEBUG_
            printf("index = %d, length = %d, port = %d, ip_addr = %s \n", handle, length, port, ip_addr);
#endif
                       
            // Call function
            int size = 0;
			size = send_socket( handle, buffer, length, ip_addr, port );
            //free (ip_addr);


#ifdef _DEBUG_
            print_buffer( buffer, length );
            printf("END TRANSPORT_SEND \n");
#endif

			return size;
}


//------------------------------------------------------
        // [size, buffer] = wl_mex_udp_transport('receive', handle, length )
        //   - Arguments:
        //     - handle (int)     - index to the requested socket
        //     - length (int)     - length of data to be received (in bytes)
        //     - buffer (char *)  - Buffer of data received
		//   - Returns:
		//       size 


int receiveData(char* buffer, int handle, int length){

                        
#ifdef _DEBUG_
            printf("Function : TRANSPORT_RECEIVE\n");
#endif


#ifdef _DEBUG_
            printf("index = %d, length = %d \n", handle, length );
#endif

            // Allocate memory for the buffer
            int i;
			if( buffer == NULL ) { printf("Error: Did not receive a valid buffer"); die();}
            for ( i = 0; i < length; i++ ) { buffer[i] = 0; }

            // Call function
            int size = 0;
			size = receive_socket( handle, length, buffer );                       

            
#ifdef _DEBUG_
            if ( size != 0 ) {
                printf("Buffer size = %d \n", size);
                print_buffer( (char *) buffer, size );
            }
            printf("END TRANSPORT_RECEIVE \n");
#endif

			return size;
}


//------------------------------------------------------
        //[num_samples, cmds_used, samples]  = wl_mex_udp_transport('read_rssi' / 'read_iq', 
        //                                        handle, buffer, length, ip_addr, port,
        //                                        number_samples, buffer_id, start_sample);
        //   - Arguments:
        //     - samples      (double *)    - Array of samples received		  
        //     - handle       (int)         - index to the requested socket
        //     - buffer       (char *)      - Buffer of data to be sent
        //     - length       (int)         - Length of data to be sent
        //     - ip_addr      (char *)      - IP Address to send data to
        //     - port         (int)         - Port to send data to
        //     - num_samples  (int)         - Number of samples requested
        //     - buffer_id    (int)         - WARP RF buffer to obtain samples from
        //     - start_sample (int)         - Starting address in the array for the samples
		//     -max_length	(int)
		//     -num_pkts	(int)
        //   - Returns:
        //     - num_samples  (int)         - Number of samples received
        //     ?? cmds_used    (int)         - Number of transport commands used to obtain samples


int readSamples(double complex* samples, int handle, char* buffer, int length, char* ip_addr, int port, int num_samples, uint32 buffer_id, int start_sample, int max_length, int num_pkts){

#ifdef _DEBUG_
            printf("Function : TRANSPORT_READ_IQ \ TRANSPORT_READ_RSSI\n");
#endif
        
            int i;

    int     max_samples             = 0;
    uint32  num_cmds                = 0;
    uint32  start_sample_to_request = 0;
    uint32  num_samples_to_request  = 0;
    uint32  num_pkts_to_request     = 0;
    double  temp_I_val                = 0.0;
    double  temp_Q_val                = 0.0;
    uint32 *output_array            = NULL;
    uint32 *command_args            = NULL;			
	int size = 0;

			if( buffer == NULL ) { printf("Error: Did not receive a valid buffer"); die();}

            if( samples == NULL ) { printf("Error: Did not receive a valid samples buffer"); die();}
            
            //for ( i = 0; i < num_samples; i++ ) { samples[i] = 0; }



            // IP address input must be a string 
            

#ifdef _DEBUG_
            // Print initial debug information
            printf("index = %d, length = %d, port = %d, ip_addr = %s \n", handle, length, port, ip_addr);
            printf("num_sample = %d, start_sample = %d, buffer_id = %d \n", num_samples, start_sample, buffer_id);
            print_buffer( buffer, length );
#endif
            
            // Allocate memory and initialize the output buffer
            output_array      = (uint32 *) malloc( sizeof( uint32 ) * num_samples );
            if( output_array == NULL ) { printf("Error:  Could not allocate receive buffer"); die();}
            
            //for ( i = 0; i < num_samples; i++ ) { output_array[i] = 0; }

            
            // Set the useful RX buffer size to 90% of the RX buffer
            //     NOTE:  This is integer division so the rx_buffer_size will be truncated by the divide
            //
            uint32 useful_rx_buffer_size  = 0;
            useful_rx_buffer_size  = 9 * ( rx_buffer_size / 10 );
            

            // Update the buffer with the correct command arguments since it is too expensive to do in MATLAB
            command_args    = (uint32 *) ( buffer + sizeof( wl_transport_header ) + sizeof( wl_command_header ) );
            command_args[0] = endian_swap_32( buffer_id );
            command_args[3] = endian_swap_32( max_length );

            // printf("newBuffID = %d, old = %d: index=%d \n", endian_swap_32( command_args[0]) , buffer_id, handle);
			
                
            // Check to see if we have enough receive buffer space for the requested packets.
            // If not, then break the request up in to multiple requests.
            
            if( num_samples < ( useful_rx_buffer_size >> 2 ) ) {

                // Call receive function normally
                command_args[1] = endian_swap_32( start_sample );
                command_args[2] = endian_swap_32( num_samples );
                command_args[4] = endian_swap_32( num_pkts );

                // Call function


                size = wl_read_baseband_buffer( handle, buffer, length, ip_addr, port,
                                                num_samples, start_sample, buffer_id,
                                                output_array, &num_cmds );

            } else {

                // Since we are requesting more data than can fit in to the receive buffer, break this 
                // request in to multiple function calls, so we do not hit the timeout functions

                // Number of packets that can fit in the receive buffer
                num_pkts_to_request     = useful_rx_buffer_size / max_length;            // RX buffer size in bytes / Max packet size in bytes
                
                // Number of samples in a request (number of samples in a packet * number of packets in a request)
                num_samples_to_request  = (max_length >> 2) * num_pkts_to_request;
                
                // Starting sample
                start_sample_to_request = start_sample;

                // Error checking to make sure something bad did not happen
                if ( num_pkts_to_request > num_pkts ) {
                    printf("ERROR:  Read IQ / Read RSSI - Parameter mismatch \n");
                    printf("    Requested %d packet(s) and %d sample(s) in function call.  \n", num_pkts, num_samples);
                    printf("    Receive buffer can hold %d samples (ie %d packets).  \n", num_samples_to_request, num_pkts_to_request);
                    printf("    Since, the number of samples requested is greater than what the receive buffer can hold, \n");
                    printf("    the number of packets requested should be greater than what the receive buffer can hold. \n");
                    die_with_error("Error:  Read IQ / Read RSSI - Parameter mismatch.  See above for debug information.");
                }
                
                command_args[2] = endian_swap_32( num_samples_to_request );
                command_args[4] = endian_swap_32( num_pkts_to_request );

                for( i = num_pkts; i > 0; i -= num_pkts_to_request ) {

                    int j = i - num_pkts_to_request;
                
                    // If we are requesting the last set of packets, then just request the remaining samples
                    if ( j < 0 ) {
                        num_samples_to_request = num_samples - ( (num_pkts - i) * (max_length >> 2) );
                        command_args[2]        = endian_swap_32( num_samples_to_request );
                        
                        num_pkts_to_request    = i;
                        command_args[4]        = endian_swap_32( num_pkts_to_request );
                    }

                    command_args[1] = endian_swap_32( start_sample_to_request );

                    // Call function
                    size = wl_read_baseband_buffer( handle, buffer, length, ip_addr, port,
                                                    num_samples_to_request, start_sample_to_request, buffer_id,
                                                    output_array, &num_cmds );
                    
                    start_sample_to_request += num_samples_to_request;                    
                }
                
                size = num_samples;
            }


            

            if ( size != 0 ) {

                // Process returned output array
      //TODO: For now only consider IQ
				//         if ( function == TRANSPORT_READ_IQ ) {


                    // Need to unpack the WARPLab sample
                    //   NOTE:  This performs a conversion from an UFix_16_0 to a Fix_14_13 
                    //      (in WARPv3, we convert Fix_12_11 to Fix_14_13 by zeroing out the two LSBs)
                    //      Process:
                    //          1) Mask upper two bits
                    //          2) Sign exten the value so you have a true twos compliment 16 bit value
                    //          3) Divide by range / 2 to move the decimal point so resulting value is between +/- 1
                    
                    for ( i = 0; i < size; i++ ) {
                        // I samples
                        temp_I_val = (double) ((int16) (((output_array[i] >> 16) & 0x3FFF) | 
                                                      (((output_array[i] >> 29) & 0x1) * 0xC000)));
                        
                        // Q samples
                        temp_Q_val = (double) ((int16) ((output_array[i]        & 0x3FFF) | 
                                                      (((output_array[i] >> 13) & 0x1) * 0xC000)));
                        samples[i] = ( temp_I_val*0.00012207 ) + (temp_Q_val*0.00012207)*I;                        
                        //printf("temp_I_val = %f\n", (double) 0x2000);
                        // samples[i] =  (temp_Q_val/ 8192)*I;                        

                    }
                    
       //         } else { // TRANSPORT_READ_RSSI

                
        //               for ( i = 0; i < (2*size); i += 2 ) {
        //                 samples[i]     = (double)((output_array[(i/2)] >> 16) & 0x03FF);
        //                samples[i + 1] = (double) (output_array[(i/2)]        & 0x03FF);
        //            }
        //        }
            } else {
                // Return an empty array
				printf("Nothing returned fro read\n");
				// 
            }

            
            // Free allocated memory
            //free( ip_addr );
            free( output_array );
            
#ifdef _DEBUG_
            printf("END TRANSPORT_READ_IQ \ TRANSPORT_READ_RSSI\n");
#endif  


			return size;

}


//------------------------------------------------------
        // cmds_used = wl_mex_udp_transport('write_iq', handle, cmd_buffer, max_length, ip_addr, port, 
        //                                          number_samples, sample_buffer, buffer_id, start_sample, num_pkts, max_samples, hw_ver);
        //
        //   - Arguments:
        //     - handle          (int)      - Index to the requested socket
        //     - cmd_buffer      (char *)   - Buffer of data to be used as the header for Write IQ command
        //     - max_length      (int)      - Length of max data packet (in bytes)
        //     - ip_addr         (char *)   - IP Address to send samples to
        //     - port            (int)      - Port to send samples to
        //     - num_samples     (int)      - Number of samples to send
        //     - sample_I_buffer (uint16 *) - Array of I samples to be sent
        //     - sample_Q_buffer (uint16 *) - Array of Q samples to be sent
        //     - buffer_id       (int)      - WARP RF buffer to send samples to
        //     - start_sample    (int)      - Starting address in the array of samples
        //     - num_pkts        (int)      - Number of Ethernet packets it should take to send the samples
        //     - max_samples     (int)      - Max number of samples transmitted per packet
        //                                    (last packet may not send max_sample samples)
        //     - hw_ver          (int)      - Hardware version;  Since different HW versions have different 
        //                                    processing capbilities, we need to know the HW version for timing
        //   - Returns:
        //     - cmds_used   (int)  - number of transport commands used to send samples

int writeSamples(int handle, char* buffer, int max_length, char* ip_addr, int port, int num_samples, uint16* sample_I_buffer, uint16* sample_Q_buffer, int buffer_id, int start_sample, int num_pkts, int max_samples, int hw_ver){

#ifdef _DEBUG_
            printf("Function : TRANSPORT_WRITE_IQ\n");
#endif


            // Packet data must be an array of uint8
			if( buffer == NULL ) { printf("Error: Did not receive a valid header buffer"); die();}


            // IP address input must be a string 
            
            // Sample I/Q Buffer
			if( sample_I_buffer == NULL ) { printf("Error: Did not receive a valid sample I buffer"); die();}
			if( sample_Q_buffer == NULL ) { printf("Error: Did not receive a valid sample Q buffer"); die();}

                   
            // Sample I Buffer


            // NOTE:  Due to differences in rounding between Matlab and C when converting from double to int16
            //   we must preprocess the sample array so that it contains a uint32 consisting of the I and Q 
            //   for a given sample.  The code below can demonstrate the rounding difference between Matlab and C
            //  
            //
            // if ( mxIsComplex( prhs[7] ) != 1 ) { mexErrMsgTxt("Error: Sample Buffer input must be an array of complex doubles"); }            
            // if ( mxGetN( prhs[7] ) != 1 ) { mexErrMsgTxt("Error: Sample buffer input must be a column vector."); }
            //
            // temp_i = (double *) mxGetPr(prhs[7]);
            // temp_q = (double *) mxGetPi(prhs[7]);
            //
            // sample_buffer = (uint32 *) malloc( sizeof( uint32 ) * num_samples );
            // if( sample_buffer == NULL ) { mexErrMsgTxt("Error:  Could not allocate sample buffer"); }
            // for ( i = 0; i < num_samples; i++ ) { 
            //     sample_buffer[i] = (uint32)( ((int16)( temp_i[i] * (1 << 15) )) << 16 ) + ((int16)( temp_q[i] * ( 1 << 15 ) )); 
            // }
            // 
			int size = 0;
			int num_cmds = 0; 

#ifdef _DEBUG_
            // Print initial debug information
            printf("index = %d, length = %d, port = %d, ip_addr = %s \n", handle, max_length, port, ip_addr);
            printf("num_sample = %d, start_sample = %d, buffer_id = %d \n", num_samples, start_sample, buffer_id);
            printf("num_pkts = %d, max_samples = %d \n", num_pkts, max_samples);
            printf("wl_transport_header = %d, wl_command_header = %d, wl_sample_header = %d \n", 
                   sizeof( wl_transport_header ), sizeof( wl_command_header ), sizeof( wl_sample_header ));
            print_buffer( buffer, ( sizeof( wl_transport_header ) + sizeof( wl_command_header ) )  );
 //           print_buffer_32( sample_buffer, num_samples );
#endif
                        
            // Call function
            size = wl_write_baseband_buffer( handle, buffer, max_length, ip_addr, port,
                                             num_samples, start_sample, sample_I_buffer, sample_Q_buffer, buffer_id, num_pkts, max_samples, hw_ver,
                                             &num_cmds );

            
            if ( size == 0 ) {
                printf("Error:  Did not send any samples");
            }
            
                 
#ifdef _DEBUG_
            printf("END TRANSPORT_WRITE_IQ\n");
#endif  
			return num_cmds; 
}






/*****************************************************************************/
//           Additional WL Mex UDP Transport functionality
/*****************************************************************************/




/*****************************************************************************/
/**
*
* This function will read the baseband buffers and construct the sample array
*
* @param	index          - Index in to socket structure which will receive samples
* @param	buffer         - WARPLab command to request samples
* @param	length         - Length (in bytes) of buffer
* @param    ip_addr        - IP Address of node to retrieve samples
* @param    port           - Port of node to retrieve samples
* @param    num_samples    - Number of samples to process (should be the same as the argument in the WARPLab command)
* @param    start_sample   - Index of starting sample (should be the same as the agrument in the WARPLab command)
* @param    buffer_id      - Which buffer(s) do we need to retrieve samples from
* @param    output_array   - Return parameter - array of samples to return
* @param    num_cmds       - Return parameter - number of ethernet send commands used to request packets 
*                                (could be > 1 if there are transmission errors)
*
* @return	size           - Number of samples processed (also size of output_array)
*
* @note		This function requires the following pointers:
*   Input:
*       - Buffer containing command to request samples (char *)
*       - IP Address (char *)
*       - Array of buffer IDs (uint32 *)
*       - Allocated memory to return samples (uint32 *)
*       - Location to return the number of send commands used to generate the samples (uint32 *)
*
*   Output:
*       - Number of samples processed 
*
******************************************************************************/
int wl_read_baseband_buffer( int index, 
                             char *buffer, int length, char *ip_addr, int port,
                             int num_samples, int start_sample, uint32 buffer_id,
                             uint32 *output_array, uint32 *num_cmds ) {

    // Variable declaration
    int i;
    int                   done               = 0;
    
    uint32                buffer_id_cmd      = 0;
    uint32                start_sample_cmd   = 0;
    uint32                total_sample_cmd   = 0;
    uint32                bytes_per_pkt      = 0;
    uint32                num_pkts           = 0;
    
    uint32                output_buffer_size = 0;
    uint32                samples_per_pkt    = 0;

    int                   sent_size          = 0;

    uint32                rcvd_pkts          = 0;
    int                   rcvd_size          = 0;
    int                   total_rcvd_size    = 0;   
    int                   num_rcvd_samples   = 0;
    int                   sample_num         = 0;
    int                   sample_size        = 0;
    
    uint32                timeout            = 0;
    uint32                num_retrys         = 0;

    uint32                total_cmds         = 0;
    
    uint32                err_start_sample   = 0;
    uint32                err_num_samples    = 0;
    uint32                err_num_pkts       = 0;

    
    char                 *output_buffer;
    uint8                *samples;

    wl_transport_header  *transport_hdr;
    wl_command_header    *command_hdr;
    uint32               *command_args;
    wl_sample_header     *sample_hdr;
    wl_sample_tracker    *sample_tracker;

    // Compute some constants to be used later
    uint32                tport_hdr_size    = sizeof( wl_transport_header );
    uint32                cmd_hdr_size      = sizeof( wl_transport_header ) + sizeof( wl_command_header );
    uint32                all_hdr_size      = sizeof( wl_transport_header ) + sizeof( wl_command_header ) + sizeof( wl_sample_header );


    // Initialization
    transport_hdr  = (wl_transport_header *) buffer;
    command_hdr    = (wl_command_header   *) ( buffer + tport_hdr_size );
    command_args   = (uint32              *) ( buffer + cmd_hdr_size );
    
    buffer_id_cmd    = endian_swap_32( command_args[0] );
    start_sample_cmd = endian_swap_32( command_args[1] );
    total_sample_cmd = endian_swap_32( command_args[2] );
    bytes_per_pkt    = endian_swap_32( command_args[3] );            // Command contains payload size; add room for header
    num_pkts         = endian_swap_32( command_args[4] );

    output_buffer_size = bytes_per_pkt + 100;                        // Command contains payload size; add room for header
    samples_per_pkt    = ( bytes_per_pkt >> 2 );                     // Each WARPLab sample is 4 bytes
    
    
#ifdef _DEBUG_
    // Print command arguments    
    printf("index = %d, length = %d, port = %d, ip_addr = %s \n", index, length, port, ip_addr);
    printf("num_sample = %d, start_sample = %d, buffer_id = %d \n", num_samples, start_sample, buffer_id);
    printf("bytes_per_pkt = %d;  num_pkts = %d \n", bytes_per_pkt, num_pkts );
    print_buffer( buffer, length );
#endif

    // Perform a consistency check to make sure parameters are correct
    if ( buffer_id_cmd != buffer_id ) {
        printf("WARNING:  Buffer ID in command (%d) does not match function parameter (%d):index= %d\n", buffer_id_cmd, buffer_id, index);
    }
    if ( start_sample_cmd != start_sample ) {
        printf("WARNING:  Starting sample in command (%d) does not match function parameter (%d)\n", start_sample_cmd, start_sample);
    }
    if ( total_sample_cmd != num_samples ) {
        printf("WARNING:  Number of samples requested in command (%d) does not match function parameter (%d)\n", total_sample_cmd, num_samples);
    }

    
    // Malloc temporary buffer to process ethernet packets
    output_buffer  = (char *) malloc( sizeof( char ) * output_buffer_size );
    if( output_buffer == NULL ) { die_with_error("Error:  Could not allocate temp output buffer"); }
    
    // Malloc temporary array to track samples that have been received and initialize
    sample_tracker = (wl_sample_tracker *) malloc( sizeof( wl_sample_tracker ) * num_pkts );
    if( sample_tracker == NULL ) { die_with_error("Error:  Could not allocate sample tracker buffer"); }
    for ( i = 0; i < num_pkts; i++ ) { sample_tracker[i].start_sample = 0;  sample_tracker[i].num_samples = 0; }

    
    // Send packet to request samples
    sent_size   = send_socket( index, buffer, length, ip_addr, port );
    total_cmds += 1;

    if ( sent_size != length ) {
        die_with_error("Error:  Size of packet sent to request samples does not match length of packet.");
    }

//    struct timespec tsi, tsf;

  //  clock_gettime(CLOCKTYPE, &tsi);     

    // Initialize loop variables
    rcvd_pkts = 0;
    timeout   = 0;
    
    // Process each return packet
    while ( !done ) {
        
        // If we hit the timeout, then try to re-request the remaining samples
        if ( timeout >= TRANSPORT_TIMEOUT ) {
        
            // If we hit the max number of retrys, then abort
            if ( num_retrys >= TRANSPORT_MAX_RETRY ) {

                printf("ERROR:  Exceeded %d retrys for current Read IQ / Read RSSI request \n", TRANSPORT_MAX_RETRY);
                printf("    Requested %d samples from buffer %d starting from sample number %d \n", num_samples, buffer_id, start_sample);
                printf("    Received %d out of %d packets from node before timeout.\n", rcvd_pkts, num_pkts);
                printf("    Please check the node and look at the ethernet traffic to isolate the issue. \n");                
            
                die_with_error("Error:  Reached maximum number of retrys without a response... aborting.");
                
            } else {

                // NOTE:  We print a warning here because the Read IQ / Read RSSI case in the mex function above
                //        will split Read IQ / Read RSSI requests based on the receive buffer size.  Therefore,
                //        any timeouts we receive here should be legitmate issues that should be explored.
                //
                printf("WARNING:  index=%d Read IQ / Read RSSI request timed out.  Retrying remaining samples. \n", index);
            
                // Find the first packet error and request the remaining samples
                if ( wl_read_iq_find_error( sample_tracker, num_samples, start_sample, rcvd_pkts, samples_per_pkt,
                                            &err_num_samples, &err_start_sample, &err_num_pkts ) ) {
                    
                    command_args[1] = endian_swap_32( err_start_sample );
                    command_args[2] = endian_swap_32( err_num_samples );
                    command_args[4] = endian_swap_32( num_pkts - ( rcvd_pkts - err_num_pkts ) );

                    // Since there was an error in the packets we have already received, then we need to adjust rcvd_pkts
                    rcvd_pkts        -= err_num_pkts;
                    num_rcvd_samples  = num_samples - err_num_samples;
                } else {
                    // If we did not find an error, then the first rcvd_pkts are correct and we should request
                    //   the remaining packets
                    command_args[1] = endian_swap_32( err_start_sample );
                    command_args[2] = endian_swap_32( err_num_samples );
                    command_args[4] = endian_swap_32( num_pkts - rcvd_pkts );
                }

                // Retransmit the read IQ request packet
                sent_size   = send_socket( index, buffer, length, ip_addr, port );
                
                if ( sent_size != length ) {
                    die_with_error("Error:  Size of packet sent to request samples does not match length of packet.");
                }
                
                // Update control variables
                timeout     = 0;
                total_cmds += 1;
                num_retrys += 1;
            }
        }
        
        // Recieve packet
        rcvd_size = receive_socket( index, output_buffer_size, output_buffer );
        total_rcvd_size = total_rcvd_size + rcvd_size; 

        // recevie_socket() handles all socket related errors and will only return:
        //   - zero if no packet is available
        //   - non-zero if packet is available
        if ( rcvd_size > 0 ) {
            sample_hdr  = (wl_sample_header *) ( output_buffer + cmd_hdr_size );
            samples     = (uint8 *) ( output_buffer + all_hdr_size );
            sample_num  = endian_swap_32( sample_hdr->start );
            sample_size = endian_swap_32( sample_hdr->num_samples );

#ifdef _DEBUG_
            printf("num_sample = %d, start_sample = %d \n", sample_size, sample_num);
#endif

            // If we are tracking packets, record which samples have been recieved
            sample_tracker[rcvd_pkts].start_sample = sample_num;
            sample_tracker[rcvd_pkts].num_samples  = sample_size;
            
            // Place samples in the array (Ethernet packet is uint8, output array is uint32) 
            //   NOTE: Need to pack samples in the correct order
            for( i = 0; i < (4 * sample_size); i += 4 ) {
                output_array[ sample_num + (i / 4) ] = (uint32) ( (samples[i    ] << 24) | 
                                                                  (samples[i + 1] << 16) | 
                                                                  (samples[i + 2] <<  8) | 
                                                                  (samples[i + 3]      ) );
            }
            
            num_rcvd_samples += sample_size;
            rcvd_pkts        += 1;
            timeout           = 0;

            // Exit the loop when we have enough packets
            if ( rcvd_pkts == num_pkts ) {
            
                // Check to see if we have any packet errors
                //     NOTE:  This check will detect duplicate packets or sample indexing errors
                if ( wl_read_iq_sample_error( sample_tracker, num_samples, start_sample, rcvd_pkts, samples_per_pkt ) ) {

                    // In this case, there is probably some issue in the transmitting node not getting the
                    // correct number of samples or messing up the indexing of the transmit packets.  
                    // The wl_read_iq_sample_error() printed debug information, so we need to retry the packets
                    
                    if ( num_retrys >= TRANSPORT_MAX_RETRY ) {
                    
                        die_with_error("Error:  Errors in sample request from board.  Max number of re-transmissions reached.  See above for debug information.");
                        
                    } else {

                        // Find the first packet error and request the remaining samples
                        if ( wl_read_iq_find_error( sample_tracker, num_samples, start_sample, rcvd_pkts, samples_per_pkt,
                                                    &err_num_samples, &err_start_sample, &err_num_pkts ) ) {

                            command_args[1] = endian_swap_32( err_start_sample );
                            command_args[2] = endian_swap_32( err_num_samples );
                            command_args[4] = endian_swap_32( err_num_pkts );

                            // Retransmit the read IQ request packet
                            sent_size   = send_socket( index, buffer, length, ip_addr, port );
                            
                            if ( sent_size != length ) {
                                die_with_error("Error:  Size of packet sent to request samples does not match length of packet.");
                            }
                            
                            // We are re-requesting err_num_pkts, so we need to subtract err_num_pkts from what we have already recieved
                            rcvd_pkts        -= err_num_pkts;
                            num_rcvd_samples  = num_samples - err_num_samples;

                            // Update control variables
                            timeout     = 0;
                            total_cmds += 1;
                            num_retrys += 1;
                        
                        } else {
                            // Die since we could not find the error
                            die_with_error("Error:  Encountered error in sample packets but could not determine the error.  See above for debug information.");
                        }
                    }
                } else {

                    done = 1;
                }
            }
            
        } else {
        
            // Increment the timeout counter
            timeout += 1;            
            
        }  // END if ( rcvd_size > 0 )
        
    }  // END while( !done )

   // clock_gettime(CLOCKTYPE, &tsf);     

    // double elaps_s = difftime(tsf.tv_sec, tsi.tv_sec);
    // long elaps_ns = tsf.tv_nsec - tsi.tv_nsec;
    // printf("%f\n",  (elaps_s*1000 + ((double)elaps_ns)/1.0e6)); // in milliseconds

    // printf("%d packets, %f ms, start %f bytes, last %d bytes\n", rcvd_pkts, ((elaps_s*1000 + ((double)elaps_ns)/1.0e6)), ((double) total_rcvd_size - rcvd_size)/(rcvd_pkts-1), rcvd_size); // in milliseconds

    
    // Free locally allocated memory    
    free( output_buffer );    
    free( sample_tracker ); 

    // Finalize outputs   
    *num_cmds  += total_cmds;
    
    return num_rcvd_samples;
}



/*****************************************************************************/
/**
*  Function:  Read IQ sample check
*
*  Function to check if we received all the samples at the correct indecies
*
*  Returns:  0 if no errors
*            1 if if there is an error and prints debug information
*
******************************************************************************/
int wl_read_iq_sample_error( wl_sample_tracker *tracker, uint32 num_samples, uint32 start_sample, uint32 num_pkts, uint32 max_sample_size ) {

    int i;
    unsigned int start_sample_total;

    unsigned int num_samples_sum  = 0;
    unsigned int start_sample_sum = 0;

    // Compute the value of the start samples:
    //   We know that the start samples should follow the pattern:
    //       [ x, (x + y), (x + 2y), (x + 3y), ... , (x + (N - 1)y) ]
    //   where x = start_sample, y = max_sample_size, and N = num_pkts.  This is due
    //   to the fact that the node will fill all packets completely except the last packet.
    //   Therefore, the sum of all element in that array is:
    //       (N * x) + ((N * (N -1 ) * Y) / 2

    start_sample_total = (num_pkts * start_sample) + (((num_pkts * (num_pkts - 1)) * max_sample_size) >> 1); 
    
    // Compute the totals using the sample tracker
    for( i = 0; i < num_pkts; i++ ) {
    
        num_samples_sum  += tracker[i].num_samples;
        start_sample_sum += tracker[i].start_sample;
    }

    // Check the totals
    if ( ( num_samples_sum != num_samples ) || ( start_sample_sum != start_sample_total ) ) {
    
        // Print warning debug information if there is an error
        if ( num_samples_sum != num_samples ) { 
            printf("WARNING:  Number of samples received (%d) does not equal number of samples requested (%d).  \n", num_samples_sum, num_samples);
        } else {
            printf("WARNING:  Sample packet indecies not correct.  Expected the sum of sample indecies to be (%d) but received a sum of (%d).  Retrying ...\n", start_sample_total, start_sample_sum);
        }
        
        // Print debug information
        printf("Packet Tracking Information: \n");
        printf("    Requested Samples:  Number: %8d    Start Sample: %8d  \n", num_samples, start_sample);
        printf("    Received  Samples:  Number: %8d  \n", num_samples_sum);
        
        for ( i = 0; i < num_pkts; i++ ) {
            printf("         Packet[%4d]:   Number: %8d    Start Sample: %8d  \n", i, tracker[i].num_samples, tracker[i].start_sample );
        }

        return 1;
    } else {
        return 0;
    }
}



/*****************************************************************************/
/**
*  Function:  Read IQ find first error
*
*  Function to check if we received all the samples at the correct indecies.
*  This will also update the ret_* parameters to indicate values to use when 
*  requesting a new set of packets.
*
*  Returns:  0 if no error was found
*            1 if an error was found
*
******************************************************************************/
int wl_read_iq_find_error( wl_sample_tracker *tracker, uint32 num_samples, uint32 start_sample, uint32 num_pkts, uint32 max_sample_size,
                           uint32 *ret_num_samples, uint32 *ret_start_sample, uint32 *ret_num_pkts ) {

    unsigned int i, j;

    unsigned int value_found;
    unsigned int return_value            = 1;
    unsigned int start_sample_to_request = start_sample;
    unsigned int num_samples_left        = num_samples;
    unsigned int num_pkts_left           = num_pkts;

    // Iterate thru the array to find the first index that does not match the expected value
    //     NOTE:  This is performing a naive search and could be optimized.  Since we are 
    //         already in an error condition, we chose simplicity over performance.
    
    for( i = 0; i < num_pkts; i++ ) {
    
        value_found = 0;
    
        // Find element in the array   
        for ( j = 0; j < num_pkts; j ++ ) {
            if ( start_sample_to_request == tracker[i].start_sample ) {
                value_found = 1;
            }            
        }

        // If we did not finde the value, then exit the loop
        if ( !value_found ) {
            break;
        }
        
        // Update element to search for
        start_sample_to_request += max_sample_size;
        num_samples_left        -= max_sample_size;
        num_pkts_left           -= 1;
    }

    // Set the return value
    if ( num_pkts_left == 0 ) {
        return_value  = 0;
    } 
        
    // Return parameters
    *ret_start_sample = start_sample_to_request;
    *ret_num_samples  = num_samples_left;
    *ret_num_pkts     = num_pkts_left;
    
    return return_value;
}



/*****************************************************************************/
/**
* This function will write the baseband buffers 
*
* @param	index          - Index in to socket structure which will receive samples
* @param	buffer         - WARPLab command (includes transport header and command header)
* @param	max_length     - Length (in bytes) max data packet to send (Ethernet MTU size - Ethernet header)
* @param    ip_addr        - IP Address of node to retrieve samples
* @param    port           - Port of node to retrieve samples
* @param    num_samples    - Number of samples to process (should be the same as the argument in the WARPLab command)
* @param    start_sample   - Index of starting sample (should be the same as the agrument in the WARPLab command)
* @param    samples_i      - Array of I samples to be sent
* @param    samples_q      - Array of Q samples to be sent
* @param    buffer_id      - Which buffer(s) do we need to send samples to (all dimensionality of buffer_ids is handled by Matlab)
* @param    num_pkts       - Number of packets to transfer (precomputed by calling SW)
* @param    max_samples    - Max samples to send per packet (precomputed by calling SW)
* @param    hw_ver         - Hardware version of node
* @param    num_cmds       - Return parameter - number of ethernet send commands used to request packets 
*                                (could be > 1 if there are transmission errors)
*
* @return	samples_sent   - Number of samples processed 
*
* @note		This function requires the following pointers:
*   Input:
*       - Buffer containing command header to send samples (char *)
*       - IP Address (char *)
*       - Buffer containing the samples to be sent (uint32 *)
*       - Location to return the number of send commands used to generate the samples (uint32 *)
*
*   Output:
*       - Number of samples processed 
*
******************************************************************************/
int wl_write_baseband_buffer( int index, 
                              char *buffer, int max_length, char *ip_addr, int port,
                              int num_samples, int start_sample, uint16 *samples_i, uint16 *samples_q, uint32 buffer_id,
                              int num_pkts, int max_samples, int hw_ver, uint32 *num_cmds ) {

    // Variable declaration
    int i, j;
    int                   done              = 0;
    int                   length            = 0;
    int                   sent_size         = 0;
    int                   sample_num        = 0;
    int                   offset            = 0;
    int                   need_resp         = 0;
    int                   slow_write        = 0;
    uint16                transport_flags   = 0;
    uint32                timeout           = 0;
    int                   buffer_count      = 0;

    // Packet checksum tracking
    uint32                checksum          = 0;
    uint32                node_checksum     = 0;

    // Keep track of packet sequence number
    uint16                seq_num           = 0;
    uint16                seq_start_num     = 0;

    // Receive Buffer variables
    int                   rcvd_size         = 0;
    int                   rcvd_max_size     = 100;
    int                   num_retrys        = 0;
    unsigned char        *rcvd_buffer;
    uint32               *command_args;

    // Buffer of data for ethernet packet and pointers to components of the packet
    unsigned char        *send_buffer;
    wl_transport_header  *transport_hdr;
    wl_command_header    *command_hdr;
    wl_sample_header     *sample_hdr;
    uint32               *sample_payload;

    // Sleep timer
    uint32                wait_time;
        
    // Compute some constants to be used later
    uint32                tport_hdr_size    = sizeof( wl_transport_header );
    uint32                tport_hdr_size_np = sizeof( wl_transport_header ) - TRANSPORT_PADDING_SIZE;
    uint32                cmd_hdr_size      = sizeof( wl_transport_header ) + sizeof( wl_command_header );
    uint32                cmd_hdr_size_np   = sizeof( wl_transport_header ) + sizeof( wl_command_header ) - TRANSPORT_PADDING_SIZE;
    uint32                all_hdr_size      = sizeof( wl_transport_header ) + sizeof( wl_command_header ) + sizeof( wl_sample_header );
    uint32                all_hdr_size_np   = sizeof( wl_transport_header ) + sizeof( wl_command_header ) + sizeof( wl_sample_header ) - TRANSPORT_PADDING_SIZE;


#ifdef _DEBUG_
    // Print command arguments    
    printf("index = %d, length = %d, port = %d, ip_addr = %s \n", index, length, port, ip_addr);
    printf("num_sample = %d, start_sample = %d, buffer_id = %d \n", num_samples, start_sample, buffer_id);
    printf("num_pkts = %d, max_samples = %d, max_length = %d \n", num_pkts, max_samples, max_length);
#endif

    // Initialization

    // Malloc temporary buffer to receive ethernet packets    
    // rcvd_buffer  = (unsigned char *) malloc( sizeof( char ) * rcvd_max_size );
 
    {
    
    rcvd_buffer  = (unsigned char *) calloc(rcvd_max_size, sizeof (char));
    if( rcvd_buffer == NULL ) { die_with_error("Error:  Could not allocate temp receive buffer"); }

    // printf("index %d :rcvd_buffer address = %x to %x\n", index, rcvd_buffer, rcvd_buffer +  rcvd_max_size*sizeof(char)-1 );

    // Malloc temporary buffer to process ethernet packets    
    // send_buffer  = (unsigned char *) malloc( sizeof( char ) * max_length );
    send_buffer  = (unsigned char *) calloc( max_length, sizeof(char));

    // printf("index %d :send_buffer address = %x to %x\n", index, send_buffer, send_buffer +  max_length*sizeof(char)-1 );
    
    if( send_buffer == NULL ) { die_with_error("Error:  Could not allocate temp send buffer"); }
    for( i = 0; i < cmd_hdr_size; i++ ) { send_buffer[i] = buffer[i]; }     // Copy current header to send buffer 

    }    
    // printf("cmd_hdr_size = %d, all_hdr_size = %d\n", cmd_hdr_size, all_hdr_size);

    // Set up pointers to all the pieces of the ethernet packet    
    transport_hdr  = (wl_transport_header *) send_buffer;
    command_hdr    = (wl_command_header   *) ( send_buffer + tport_hdr_size );
    sample_hdr     = (wl_sample_header    *) ( send_buffer + cmd_hdr_size   );
    sample_payload = (uint32              *) ( send_buffer + all_hdr_size   );

    // Get necessary values from the packet buffer so we can send multiple packets
    seq_num         = endian_swap_16( transport_hdr->seq_num ) + 1;    // Current sequence number is from the last packet
    transport_flags = endian_swap_16( transport_hdr->flags );

    // Initialize loop variables
    slow_write    = 0;
    need_resp     = 0;
    offset        = start_sample;
    seq_start_num = seq_num;

    // gkchai
    // struct timespec tsi, tsf;
    // double elaps_s; 
    // long elaps_ns; 
    // clock_gettime(CLOCKTYPE, &tsi);

    
    // For each packet
    for( i = 0; i < num_pkts; i++ ) {
    
        // Determine how many samples we need to send in the packet
        if ( ( offset + max_samples ) <= num_samples ) {
            sample_num = max_samples;
        } else {
            sample_num = num_samples - offset;
        }

        // Determine the length of the packet (All WARPLab payload minus the padding for word alignment)
        length = all_hdr_size_np + (sample_num * sizeof( uint32 ));

        // Request that the board respond to the last packet or all packets if slow_write = 1
        if ( ( i == ( num_pkts - 1 ) ) || ( slow_write == 1 ) ) {
            need_resp       = 1;
            transport_flags = transport_flags | TRANSPORT_FLAG_ROBUST;
        } else {
            need_resp       = 0;
            transport_flags = transport_flags & ~TRANSPORT_FLAG_ROBUST;        
        }

        // Prepare transport
        //   NOTE:  The length of the packet is the maximum payload size supported by the transport.  However, the 
        //       maximum payload size returned by the node has the two byte Ethernet padding already subtracted out, so the 
        //       length of the transport command is the length minus the transport header plus the padding size, since
        //       we do not want to double count the padding.
        //
        transport_hdr->length   = endian_swap_16( length - tport_hdr_size_np );
        transport_hdr->seq_num  = endian_swap_16( seq_num );
        transport_hdr->flags    = endian_swap_16( transport_flags );
        
        // Prepare command
        //   NOTE:  See above comment about lenght.  Since there is one sample packet per command, we set the number
        //       number of command arguments to be 1.
        //
        command_hdr->length     = endian_swap_16( length - cmd_hdr_size_np );
        command_hdr->num_args   = endian_swap_16( 0x0001 );
        
        // Prepare sample packet
        sample_hdr->buffer_id   = endian_swap_16( buffer_id );
        if ( i == 0 ) {
            sample_hdr->flags   = SAMPLE_CHKSUM_RESET;
        } else {
            sample_hdr->flags   = SAMPLE_CHKSUM_NOT_RESET;        
        }
        sample_hdr->start       = endian_swap_32( offset );
        sample_hdr->num_samples = endian_swap_32( sample_num );

        // Copy the appropriate samples to the packet
        for( j = 0; j < sample_num; j++ ) {
				
				
            sample_payload[j] =  endian_swap_32( ( samples_i[j + offset] << 16 ) + samples_q[j + offset] );    
        }

        // Add back in the padding so we can send the packet
        length += TRANSPORT_PADDING_SIZE;

        // Send packet 
        sent_size = send_socket( index, (char *) send_buffer, length, ip_addr, port );

        if ( sent_size != length ) {
            die_with_error("Error:  Size of packet sent to with samples does not match length of packet.");
        }
        
        // Update loop variables
        offset   += sample_num;
        seq_num  += 1;

        // Compute checksum
        // NOTE:  Due to a weakness in the Fletcher 32 checksum (ie it cannot distinguish between
        //     blocks of all 0 bits and blocks of all 1 bits), we need to add additional information
        //     to the checksum so that we will not miss errors on packets that contain data of all 
        //     zero or all one.  Therefore, we add in the start sample for each packet since that 
        //     is readily availalbe on the node.
        //
        if ( i == 0 ) {
            checksum = wl_update_checksum( ( ( offset - sample_num ) & 0xFFFF ), SAMPLE_CHKSUM_RESET, index ); 
        } else {
            checksum = wl_update_checksum( ( ( offset - sample_num ) & 0xFFFF ), SAMPLE_CHKSUM_NOT_RESET, index ); 
        }

        checksum = wl_update_checksum( ( samples_i[offset - 1] ^ samples_q[offset - 1] ), SAMPLE_CHKSUM_NOT_RESET, index );

        // printf("Index %d offset %d Packet %d sampI %d sampQ %d Calculated Checksum = %x \n", index, offset, i,  samples_i[offset - 1], samples_q[offset - 1], checksum);

        
        // If we need a response, then wait for it
        if ( need_resp == 1 ) {


            // clock_gettime(CLOCKTYPE, &tsf);

            // elaps_s = difftime(tsf.tv_sec, tsi.tv_sec);
            // elaps_ns = tsf.tv_nsec - tsi.tv_nsec;
            // printf("%f\n",  (elaps_s*1000 + ((double)elaps_ns)/1.0e6)); // in milliseconds

            // Initialize loop variables
            timeout   = 0;
            done      = 0;
            rcvd_size = 0;
            
            // Process each return packet
            while ( !done ) {

                // If we hit the timeout, then try to re-transmit the packet
                if ( timeout >= TRANSPORT_TIMEOUT) {
                
                    // If we hit the max number of retrys, then abort
                    if ( num_retrys >= TRANSPORT_MAX_RETRY ) {
                         // free( send_buffer );
                         // free( rcvd_buffer );
                        die_with_error("Error:  Reached maximum number of retrys without a response... aborting.");                    
                    } else {
                        // Roll everything back and retransmit the packet
                        num_retrys += 1;
                        offset     -= sample_num;
                        i          -= 1;
                        break;
                    }
                }

                // Recieve packet (socket error checking done by function)
                rcvd_size = receive_socket( index, rcvd_max_size, (char *) rcvd_buffer );

                if ( rcvd_size > 0 ) {
                    command_args   = (uint32 *) ( rcvd_buffer + cmd_hdr_size );    
                    node_checksum  = endian_swap_32( command_args[0] );
                    // printf("Debug:  Checksums Expected = %x  Received = %x \n", checksum, node_checksum);

                    // Compare the checksum values
                    if ( node_checksum != checksum ) {
                    
                        // If we have a checksum error in fast write mode, then switch to slow write and start over
                        if ( slow_write == 0 ) {
                            printf("WARNING:  Checksums do not match on pkt %d, index = %d, rcvd_size = %d.  Expected = %x  Received = %x \n", i, index, rcvd_size, checksum, node_checksum);
                            printf("          Starting over with slow write.  If this message occurs frequently, please \n");
                            printf("          adjust the wait_time in wl_write_baseband_buffer().  The node might not \n");
                            printf("          be able to keep up with the current rate of packets. \n");
                            
                            slow_write = 1;
                            offset     = start_sample;
                            i          = -1;
                            break;
                        } else {
                            die_with_error("Error:  Checksums do not match when in slow write... aborting.");
                        }
                    }
                    
                    timeout = 0;
	                done    = 1;
                } else {
                    // If we do not have a packet, increment the timeout counter
                    timeout++;
                }
            }  // END while( !done )
        }  // END if need_resp
        
        // This function can saturate the ethernet wire.  However, for small packets the 
        // node cannot keep up and therefore we need to delay the next transmission based on:
        // 1) packet size and 2) number of buffers being written
        
        // NOTE:  This is a simplified implementation based on experimental data.  It is by no means optimized for 
        //     all cases.  Since WARP v2 and WARP v3 hardware have drastically different internal architectures,
        //     we first have to understand which HW the Write IQ is being performed and scale the wait_times
        //     accordingly.  Also, since we can have one transmission of IQ data be distributed to multiple buffers,
        //     we need to increase the timeout if we are transmitting to more buffers on the node in order to 
        //     compensate for the extended processing time.  One other thing to note is that if you view the traffic that
        //     this generates on an oscilloscope (at least on Window 7 Professional 64-bit), it is not necessarily regular 
        //     but will burst 2 or 3 packets followed by an extended break.  Fortunately there is enough buffering in 
        //     the data flow that this does not cause a problem, but it makes it more difficult to optimize the data flow 
        //     since we cannot guarentee the timing of the packets at the node.  
        //
        //     If you start receiving checksum failures and need to adjust timing, please do so in the code below.
        //
        switch ( hw_ver ) {
            case TRANSPORT_WARP_HW_v2:
                // WARP v2 Hardware only supports small ethernet packets
 
                buffer_count = 0;

                // Count the number of buffers in the buffer_id
                for( j = 0; j < TRANSPORT_WARP_RF_BUFFER_MAX; j++ ) {
                    if ( ( ( buffer_id >> j ) & 0x1 ) == 1 ) {
                        buffer_count++;
                    }
                }
            
                // Note:  Performance drops dramatically if this number is smaller than the processing time on
                //     the node.  This is due to the fact that you perform a slow write when the checksum fails.  
                //     For example, if you change this from 160 to 140, the Avg Write IQ per second goes from
                //     ~130 to ~30.  If this number gets too large, then you will also degrade performance
                //     given you are waiting longer than necessary.
                //
                // Currently, wait times are set at:
                //     1 buffer  = 160 us
                //     2 buffers = 240 us
                //     3 buffers = 320 us
                //     4 buffers = 400 us
                // 
                // This is due to the fact that processing on the node is done thru a memcpy and takes 
                // a fixed amount of time longer for each additonal buffer that is transferred.
                //                
                wait_time = 80 + ( buffer_count * 80 );
                
                wl_usleep( wait_time );   // Takes in a wait time in micro-seconds
            break;
            
            case TRANSPORT_WARP_HW_v3:
                // In WARP v3 hardware, we need to account for both small packets as well as jumbo frames.  Also,
                // since the WARP v3 hardware uses a DMA to transfer packet data, the processing overhead is much
                // less than on v2 (hence the smaller wait times).  Through experimental testing, we found that 
                // for jumbo frames, the processing overhead was smaller than the length of the ethernet transfer
                // and therefore we do not need to wait at all.  We have not done exhaustive testing on ethernet 
                // packet size vs wait time.  So in this simplified implementation, if your Ethernet MTU size is 
                // less than 9000 bytes (ie approximately 0x8B8 samples) then we will insert a 40 us, or 50 us, 
                // delay between transmissions to give the board time to keep up with the flow of packets.
                // 
                if ( max_samples < 0x800 ) {

                    // printf("introducing delay ! \n");
                    if ( buffer_id == 0xF ) {
                        wait_time = 50;
                    } else {
                        wait_time = 40;
                    }

                    wl_usleep( wait_time );   // Takes in a wait time in micro-seconds
                }
             break;
             
             default:
                printf("WARNING:  HW version of node (%d) is not recognized.  Please check your setup.\n", hw_ver);
             break;
        }
        
    }  // END for num_pkts

    // clock_gettime(CLOCKTYPE, &tsf);

    // elaps_s = difftime(tsf.tv_sec, tsi.tv_sec);
    // elaps_ns = tsf.tv_nsec - tsi.tv_nsec;
    // printf("total = %f\n",  (elaps_s*1000 + ((double)elaps_ns)/1.0e6)); // in milliseconds


    if ( offset != num_samples ) {
        printf("WARNING:  Issue with calling function.  \n");
        printf("    Requested %d samples, sent %d sample based on other packet information: \n", num_samples, offset);
        printf("    Number of packets to send %d, Max samples per packet %d \n", num_pkts, max_samples);
    }
    
    // Free locally allocated memory    
    free( send_buffer );
    free( rcvd_buffer );

    // Finalize outputs
    if ( seq_num > seq_start_num ) {
        *num_cmds += seq_num - seq_start_num;
    } else {
        *num_cmds += (0xFFFF - seq_start_num) + seq_num;
    }
    
    return offset;
}



/*****************************************************************************/
/**
*  Function:  wl_update_checksum
*
*  Function to calculate a Fletcher-32 checksum to detect packet loss
*
******************************************************************************/
unsigned int wl_update_checksum(unsigned short int newdata, unsigned char reset, int index){

    // Fletcher-32 Checksum

	if( reset ){ sum1[index] = 0; sum2[index] = 0; }

	sum1[index] = (sum1[index] + newdata) % 0xFFFF;
	sum2[index] = (sum2[index] + sum1[index]   ) % 0xFFFF;

	return ( ( sum2[index] << 16 ) + sum1[index]);

}



#ifdef WIN32

/*****************************************************************************/
/**
*  Function:  uSleep
*
*  Since the windows Sleep() function only has a resolution of 1 ms, we need
*  to implement a usleep() function that will allow for finer timing granularity
*
******************************************************************************/
void wl_mex_udp_transport_usleep( int wait_time ) {
    static bool     init = false;
    static LONGLONG ticks_per_usecond;

    LARGE_INTEGER   ticks_per_second;    
    LONGLONG        wait_ticks;
    LARGE_INTEGER   start_time;
    LONGLONG        stop_time;
    LARGE_INTEGER   counter_val;

    // Initialize the function
    if ( !init ) {
    
        if ( QueryPerformanceFrequency( &ticks_per_second ) ) {
            ticks_per_usecond = ticks_per_second.QuadPart / 1000000;
            init = true;
        } else {
            printf("QPF() failed with error %d\n", GetLastError());
        }
    }

    if ( ticks_per_usecond ) {
    
        // Calculate how many ticks we have to wait
        wait_ticks = wait_time * ticks_per_usecond;
    
        // Save the performance counter value
        if ( !QueryPerformanceCounter( &start_time ) )
            printf("QPC() failed with error %d\n", GetLastError());

        // Calculate the stop time
        stop_time = start_time.QuadPart + wait_ticks;

        // Wait until the time has expired
        while (1) {        

            if ( !QueryPerformanceCounter( &counter_val ) ) {
                printf("QPC() failed with error %d\n", GetLastError());
                break;
            }
            
            if ( counter_val.QuadPart >= stop_time ) {
                break;
            }
        }
    }
}
#endif


// void* multi_read(void* arg){

//   struct thread_data *arg_data;
//   arg_data = (struct thread_data *) arg;
// #ifdef _DEBUG_  
//   printf("Thread for port=%d starting...\n",arg_data->port);
// #endif  

//   readIQ(arg_data->samples, arg_data->handle, arg_data->readIQ_buffer , 42, arg_data->ip_addr, arg_data->port, arg_data->num_samples, arg_data->buffer_id, arg_data->start_sample, arg_data->max_length,arg_data->num_pkts);	

// #ifdef _DEBUG_
//   printf("Thread for port = %d done.", arg_data->port);
// #endif		    
//   pthread_exit((void*) arg);
// }


// single read: to be called when there is no threading 
// void single_read(struct thread_data* arg_data){

//   readIQ(arg_data->samples, arg_data->handle, arg_data->readIQ_buffer , 42, arg_data->ip_addr, arg_data->port, arg_data->num_samples, arg_data->buffer_id, arg_data->start_sample, arg_data->max_length,arg_data->num_pkts);    

// }




