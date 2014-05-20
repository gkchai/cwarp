/***************************** Include Files *********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <complex.h>
#include <time.h>
//#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#ifdef WIN32

#include <Windows.h>
#include <WinBase.h>
#include <winsock.h>

#else

#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#endif



/*************************** Constant Definitions ****************************/

// Use to print debug message to the console
//#define _DEBUG_


#ifdef WIN32

// Define printf for future compatibility
#define usleep(x)                       Sleep((x)/1000)
#define close(x)                        closesocket(x)
#define non_blocking_socket(x)        { unsigned long optval = 1; ioctlsocket( x, FIONBIO, &optval ); }
#define wl_usleep(x)                    wl_mex_udp_transport_usleep(x)
#define SOCKET                          SOCKET
#define get_last_error                  WSAGetLastError()
#define EWOULDBLOCK                     WSAEWOULDBLOCK
#define socklen_t                       int

#else

// Define printf for future compatibility
#define usleep(x)                       usleep(x)
#define close(x)                        close(x)
#define non_blocking_socket(x)          fcntl( x, F_SETFL, O_NONBLOCK )
#define wl_usleep(x)                    usleep(x)
#define SOCKET                          int
#define get_last_error                  errno
#define INVALID_SOCKET                  0xFFFFFFFF
#define SOCKET_ERROR                    -1

#endif



// Version WARPLab MEX Transport Driver
#define WL_MEX_UDP_TRANSPORT_VERSION    "1.0.0a"

// WL_MEX_UDP_TRANSPORT Commands
#define TRANSPORT_REVISION              0
#define TRANSPORT_INIT_SOCKET           1
#define TRANSPORT_SET_SO_TIMEOUT        2
#define TRANSPORT_SET_SEND_BUF_SIZE     3
#define TRANSPORT_GET_SEND_BUF_SIZE     4
#define TRANSPORT_SET_RCVD_BUF_SIZE     5
#define TRANSPORT_GET_RCVD_BUF_SIZE     6
#define TRANSPORT_CLOSE                 7
#define TRANSPORT_SEND                  8
#define TRANSPORT_RECEIVE               9
#define TRANSPORT_READ_IQ              10
#define TRANSPORT_READ_RSSI            11
#define TRANSPORT_WRITE_IQ             12

// Maximum number of sockets that can be allocated
#define TRANSPORT_MAX_SOCKETS           20 //5

// Maximum size of a packet
#define TRANSPORT_MAX_PKT_LENGTH        9050

// Socket state
#define TRANSPORT_SOCKET_FREE           0
#define TRANSPORT_SOCKET_IN_USE         1

// Transport defines
#define TRANSPORT_NUM_PENDING           20
#define TRANSPORT_MIN_SEND_SIZE         1000
#define TRANSPORT_SLEEP_TIME            10000
#define TRANSPORT_FLAG_ROBUST           0x0001
#define TRANSPORT_PADDING_SIZE          2
#define TRANSPORT_TIMEOUT               1000000
#define TRANSPORT_MAX_RETRY             50

// Sample defines
#define SAMPLE_CHKSUM_RESET             0x01
#define SAMPLE_CHKSUM_NOT_RESET         0x00

// WARP HW version defines
#define TRANSPORT_WARP_HW_v2            2
#define TRANSPORT_WARP_HW_v3            3

// WARP Buffers defines
#define TRANSPORT_WARP_RF_BUFFER_MAX    4

#define CLOCKTYPE CLOCK_MONOTONIC

/*************************** Variable Definitions ****************************/

// Define types for different size data
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;

typedef char            int8;
typedef short           int16;
typedef int             int32;


// Data packet structure
typedef struct
{
    char              *buf;       // Pointer to the data buffer
    int                length;    // Length of the buffer (buffer must be pre-allocated)
    int                offset;    // Offset of data to be sent or received
    struct sockaddr_in address;   // Address information of data to be sent / recevied    
} wl_trans_data_pkt;

// Socket structure
typedef struct
{
    SOCKET              handle;   // Handle to the socket
    int                 timeout;  // Timeout value
    int                 status;   // Status of the socket
    wl_trans_data_pkt  *packet;   // Pointer to a data_packet
} wl_trans_socket;

// WARPLAB Transport Header
typedef struct
{
    uint16             padding;        // Padding for memory alignment
    uint16             dest_id;        // Destination ID
    uint16             src_id;         // Source ID
    uint8              rsvd;           // Reserved
    uint8              pkt_type;       // Packet type
    uint16             length;         // Length
    uint16             seq_num;        // Sequence Number
    uint16             flags;          // Flags
} wl_transport_header;

// WARPLAB Command Header
typedef struct
{
    uint32             command_id;     // Command ID
    uint16             length;         // Length
    uint16             num_args;       // Number of Arguments
} wl_command_header;

// WARPLAB Sample Header
typedef struct
{
    uint16             buffer_id;      // Buffer ID
    uint8              flags;          // Flags
    uint8              rsvd;           // Reserved
    uint32             start;          // Starting sample
    uint32             num_samples;    // Number of samples
} wl_sample_header;

// WARPLAB Sample Tracket
typedef struct
{
    uint32             start_sample;   // Starting sample
    uint32             num_samples;    // Number of samples
} wl_sample_tracker;


struct thread_data{
    double complex* samples; 
    int handle;
    char* readIQ_buffer;
    char* ip_addr;
    int port;
    uint32 buffer_id;
    int start_sample; 
    int max_length;
    int num_samples;
    int num_pkts;
};


extern int initialized; // variable visible across multiple files

unsigned int sum1[20]; // needed to run checksum methods in multiple threads
unsigned int sum2[20]; // ""

#ifdef WIN32
WSADATA          wsaData;              // Structure for WinSock setup communication 
#endif

/*************************** Function Prototypes *****************************/


// Socket functions
void         print_version( void );
void         init_wl_mex_udp_transport( void );
int          init_socket( void );
void         set_so_timeout( int index, int value );
void         set_reuse_address( int index, int value );
void         set_broadcast( int index, int value );
void         set_send_buffer_size( int index, int size );
int          get_send_buffer_size( int index );
void         set_receive_buffer_size( int index, int size );
int          get_receive_buffer_size( int index );
void         close_socket( int index );
int          send_socket( int index, char *buffer, int length, char *ip_addr, int port );
int          receive_socket( int index, int length, char * buffer );

// Debug / Error functions
void         print_usage( void );
void         print_sockets( void );
void         print_buffer(unsigned char *buf, int size);
void         die( void );
void         die_with_error( char *errorMessage );
void         cleanup( void );

// Helper functions

uint16       endian_swap_16(uint16 value);
uint32       endian_swap_32(uint32 value);


int sendData(int handle, char* buffer, int length, char* ip_addr, int port);
int receiveData(char* buffer, int handle, int length);
int readSamples(double complex* samples, int handle, char* buffer, int length, char* ip_addr, int port, int num_samples, uint32 buffer_id, int start_sample, int max_length, int num_pkts);
int writeSamples(int handle, char* buffer, int max_length, char* ip_addr, int port, int num_samples, uint16* sample_I_buffer, uint16* sample_Q_buffer, int buffer_id, int start_sample, int num_pkts, int max_samples, int hw_ver);


void         wl_mex_udp_transport_usleep( int wait_time );
unsigned int wl_update_checksum(unsigned short int newdata, unsigned char reset, int index);
int          wl_read_iq_sample_error( wl_sample_tracker *tracker, uint32 num_samples, uint32 start_sample, uint32 num_pkts, uint32 max_sample_size );
int          wl_read_iq_find_error( wl_sample_tracker *tracker, uint32 num_samples, uint32 start_sample, uint32 num_pkts, uint32 max_sample_size,
                                    uint32 *ret_num_samples, uint32 *ret_start_sample, uint32 *ret_num_pkts );

// WARPLab Functions
int          wl_read_baseband_buffer( int index, char *buffer, int length, char *ip_addr, int port,
                                      int num_samples, int start_sample, uint32 buffer_id, 
                                      uint32 *output_array, uint32 *num_cmds );

int          wl_write_baseband_buffer( int index, char *buffer, int max_length, char *ip_addr, int port,
                                       int num_samples, int start_sample, uint16 *samples_i, uint16 *samples_q, uint32 buffer_id,
                                       int num_pkts, int max_samples, int hw_ver, uint32 *num_cmds );


void* multi_read(void* arg);
void single_read(struct thread_data* arg_data);


