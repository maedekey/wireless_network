/**
 * Defines data and functions useful for the routing protocol of the motes, based on RPL.
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "dev/leds.h"
#include "dev/nullradio.h"
#include "sys/etimer.h"

#include <stdio.h>
#include <stdlib.h>
#include "random.h"

#include "hashmap.h"


///////////////////
///  CONSTANTS  ///
///////////////////

// Infinite rank constant
#define INFINITE_RANK 255

// Constants for runicast sending functions
#define SENT       1
#define NOT_SENT  -1
#define NO_PARENT -2

// Return values for choose_parent function
#define PARENT_NOT_CHANGED  0
#define PARENT_NEW          1
#define PARENT_CHANGED      2

// Threshold to change parent (in dB)
#define RSS_THRESHOLD 3

// Maximum number of retransmissions for reliable unicast transport
#define MAX_RETRANSMISSIONS 4

// Timeout value to detach from unresponsive parent
#define TIMEOUT_PARENT 50


// Values for the different types of messages
const uint8_t DIS;
const uint8_t DIO;
const uint8_t DAO;
const uint8_t DATA;
const uint8_t TURNON;




// Size of control messages
const size_t DIS_size;
const size_t DIO_size;
const size_t DAO_size;
const size_t DATA_size;
const size_t TURNON_size;




////////////////////
///  DATA TYPES  ///
////////////////////

// Represents the parent of a certain mote
// We use another struct since we don't need all the information of the mote struct
typedef struct parent_mote {
	linkaddr_t addr;
	uint8_t rank;
	signed char rss;
	uint8_t typeMote;
} parent_t;

// Represents the attributes of a mote
typedef struct mote {
	linkaddr_t addr;
	uint8_t in_dodag;
	uint8_t rank;
	parent_t* parent;
	hashmap_map* routing_table;
	uint8_t typeMote;
} mote_t;


// Represents a DIS control message
typedef struct DIS_message {
	uint8_t type;
} DIS_message_t;

// Represents a DIO control message
typedef struct DIO_message {
	uint8_t type;
	uint8_t rank;
	uint8_t typeMote;
} DIO_message_t;

// Represents a DAO control message
typedef struct DAO_message {
	uint8_t type;
	linkaddr_t src_addr;
	uint8_t typeMote;
} DAO_message_t;

// Represents a DATA message, that carries the data from a sensor mote to the server
typedef struct DATA_message {
	uint8_t type;
	linkaddr_t src_addr;
	uint16_t data;
} DATA_message_t;

typedef struct TURNON_message {
	uint8_t type;
	uint8_t typeMote;
} TURNON_message_t;



///////////////////
///  FUNCTIONS  ///
///////////////////

/**
 * Initializes the attributes of a mote.
 */
void init_mote(mote_t *mote, uint8_t type);

/**
 * Initializes the attributes of a root mote.
 */
void init_root(mote_t *mote, uint8_t type);

/**
 * Initializes the parent of a mote.
 */
void init_parent(mote_t *mote, const linkaddr_t *parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote);

/**
 * Updates the attributes of the parent of a mote.
 * Returns 1 if the rank of the parent has changed, 0 if it hasn't changed.
 */
uint8_t update_parent(mote_t *mote, uint8_t parent_rank, signed char rss, uint8_t typeMote);

/**
 * Changes the parent of a mote.
 */
void change_parent(mote_t *mote, const linkaddr_t *parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote);

/**
 * Detaches a mote from the DODAG.
 * Deletes the parent, and sets in_dodag and rank to 0.
 */
void detach(mote_t *mote);

/**
 * Broadcasts a DIS message.
 */
void send_DIS();

/**
 * Broadcasts a DIO message, containing the rank of the node.
 */
void send_DIO(mote_t *mote);

/**
 * Sends a DAO message to the parent of this node.
 */
void send_DAO(mote_t *mote);

/**
 * Forwards the DAO message, to the parent of this node.
 */
void forward_DAO(DAO_message_t *message, mote_t *mote);

/**
 * Selects the parent, if it has a lower rank and a better rss
 */
uint8_t choose_parent(mote_t *mote, const linkaddr_t* parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote);

/**
 * Sends a DATA message, containing a random value, to the parent of the mote.
 */
void send_DATA(mote_t *mote);


/**
 * Forwards a DATA message to the parent of the mote.
 */
void forward_DATA(DATA_message_t *message, mote_t *mote);

void send_TURNON(uint8_t typeMote, linkaddr_t dest, mote_t *mote);

void forward_TURNON(uint8_t typeMote, mote_t *mote);

unsigned isInArray(linkaddr_t* dst, unsigned effectiveSize, linkaddr_t *val);
