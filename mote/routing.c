/**
 * Data and functions useful for the routing protocol of the motes, based on RPL.
 */

#include "routing.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

///////////////////
///  CONSTANTS  ///
///////////////////

// Values for the different types of RPL control messages
const uint8_t DIS = 2;
const uint8_t DIO = 3;
const uint8_t DAO = 4;
const uint8_t DATA = 0;
const uint8_t TURNON = 5;
const uint8_t ACK = 10;



// Size of control messages
const size_t DIS_size = sizeof(DIS_message_t);
const size_t DIO_size = sizeof(DIO_message_t);
const size_t DAO_size = sizeof(DAO_message_t);
const size_t DATA_size = sizeof(DATA_message_t);
const size_t TURNON_size = sizeof(TURNON_message_t);
const size_t ACK_size = sizeof(ACK_message_t);


const uint16_t ACK_value = 100;


///////////////////
///  FUNCTIONS  ///
///////////////////

/**
 * Initializes the attributes of a mote.
 */
void init_mote(mote_t *mote, uint8_t typeMote) {

	// Set the Rime address
	linkaddr_copy(&(mote->addr), &linkaddr_node_addr);

	// Initialize routing table
	mote->routing_table = hashmap_new();

	if (!mote->routing_table) {
		LOG_INFO("init_mote() of mote with address %u.%u : could not allocate enough memory\n", (mote->addr).u8[0], (mote->addr).u8[1]);
		exit(-1);
	}

	mote->in_dodag = 0;
	mote->rank = INFINITE_RANK;
	mote->typeMote = typeMote;

}

/**
 * Initializes the attributes of a root mote.
 */
void init_root(mote_t *mote, uint8_t typeMote) {

	// Set the Rime address
	linkaddr_copy(&(mote->addr), &linkaddr_node_addr);

	// Initialize routing table
	mote->routing_table = hashmap_new();

	if (!mote->routing_table) {
		LOG_INFO("init_root() of mote with address %u.%u : could not allocate enough memory\n", (mote->addr).u8[0], (mote->addr).u8[1]);
		exit(-1);
	}

	mote->in_dodag = 1;
	mote->rank = 0;
	mote->typeMote = typeMote;
}

/**
 * Initializes the parent of a mote.
 */
void init_parent(mote_t *mote, const linkaddr_t *parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote) {

	// Set the Rime address
	mote->parent = (parent_t*) malloc(sizeof(parent_t));
	linkaddr_copy(&(mote->parent->addr), parent_addr);

	// Set the attributes of the parent
	mote->parent->rank = parent_rank;
	mote->parent->rss = rss;
	mote->parent->typeMote = typeMote;

	// Update the attributes of the mote
	mote->in_dodag = 1;
	mote->rank = parent_rank + 1;

}

/**
 * Updates the attributes of the parent of a mote.
 * Returns 1 if the rank of the parent has changed, 0 if it hasn't changed.
 */
uint8_t update_parent(mote_t *mote, uint8_t parent_rank, signed char rss, uint8_t typeMote) {
	mote->parent->rss = rss;
	mote->parent->typeMote = typeMote;
	if (parent_rank != mote->parent->rank) {
		mote->parent->rank = parent_rank;
		mote->rank = parent_rank + 1;
		return 1;
	} else {
		return 0;
	}
}

/**
 * Changes the parent of a mote
 */
void change_parent(mote_t *mote, const linkaddr_t *parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote) {

	// Set the Rime address
	linkaddr_copy(&(mote->parent->addr), parent_addr);

	// Set the attributes of the parent
	mote->parent->rank = parent_rank;
	mote->parent->rss = rss;
	mote->parent->typeMote = typeMote;

	// Update the rank of the mote
	mote->rank = parent_rank + 1;

}

/**
 * Detaches a mote from the DODAG.
 * Deletes the parent, and sets in_dodag and rank to 0.
 * Restart its routing table.
 */
void detach(mote_t *mote) {
	if (mote->in_dodag) { // No need to detach the mote if it isn't already in the DODAG
		free(mote->parent);
		hashmap_free(mote->routing_table);
		mote->in_dodag = 0;
		mote->rank = INFINITE_RANK;
		mote->routing_table = hashmap_new();
	}
}

/**
 * Broadcasts a DIS message.
 */
void send_DIS() {

	DIS_message_t *message = (DIS_message_t*) malloc(DIS_size);
	message->type = DIS;
	//memcpy(nullnet_buf, (void*) message, DIS_size);
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = DIS_size;

	//packetbuf_copyfrom((void*) message, DIS_size);
	free(message);
	NETSTACK_NETWORK.output(NULL);

	//broadcast_send(conn);

}

/**
 * Broadcasts a DIO message, containing the rank of the node.
 */
void send_DIO(mote_t *mote) {

	uint8_t rank = mote->rank;
	uint8_t type = mote->typeMote;

	DIO_message_t *message = (DIO_message_t*) malloc(DIO_size);
	message->type = DIO;
	message->rank = rank;
	message->typeMote = type;
	nullnet_buf = (uint8_t*) message;

	nullnet_len = DIO_size;


	free(message);
	NETSTACK_NETWORK.output(NULL);


}

/**
 * Sends a DAO message to the parent of this node.
 */
void send_DAO(mote_t *mote) {

	DAO_message_t *message = (DAO_message_t*) malloc(DAO_size);
	message->type = DAO;
	message->src_addr = mote->addr;
	message->typeMote = mote->typeMote;
	nullnet_buf = (uint8_t*) message;
	nullnet_len = DAO_size;


	free(message);

	NETSTACK_NETWORK.output(&(mote->parent->addr));

}

/**
 * Forwards a DAO message, to the parent of this node.
 */
void forward_DAO(DAO_message_t *message, mote_t *mote) {
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = DAO_size;
	
	NETSTACK_NETWORK.output(&(mote->parent->addr));
}

/**
 * Returns 1 if the potential parent is better than the current parent, 0 otherwise.
 * A parent is better than another if it has a lower rank, or if it has the same rank
 * and a better signal strength (RSS), with a small threshold to avoid changing all the time
 * in an unstable network.
 */
uint8_t is_better_parent(mote_t *mote, uint8_t parent_rank, signed char rss, uint8_t typeMote) {
	if(mote->typeMote == 2 && typeMote > 0){
		if (mote->parent->typeMote == typeMote){
			uint8_t lower_rank = parent_rank < mote->parent->rank;
			uint8_t same_rank = parent_rank == mote->parent->rank;
			uint8_t better_rss = rss > mote->parent->rss + RSS_THRESHOLD;
			return lower_rank || (same_rank && better_rss);	
		}else{
			return mote->parent->typeMote > typeMote;
		}
	}else if (mote->typeMote == 1 && typeMote != 0){
		return 0;
	}
	return 0;
}

/**
 * Selects the parent. Returns a code depending on if the parent has changed or not.
 */
uint8_t choose_parent(mote_t *mote, const linkaddr_t* parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote) {
	if (!mote->in_dodag) {
		if (!(mote->typeMote == 2 && typeMote == 0)){ 
			// Mote not in DODAG yet, initialize parent
			init_parent(mote, parent_addr, parent_rank, rss, typeMote);
			return PARENT_NEW;
		}
	} else if (is_better_parent(mote, parent_rank, rss, typeMote)) {
		// Better parent found, change parent
		change_parent(mote, parent_addr, parent_rank, rss, typeMote);
		return PARENT_CHANGED;
	} else {
		// Already has a better parent
		return PARENT_NOT_CHANGED;
	}
	return PARENT_NOT_CHANGED;
}

/**
 * Sends a DATA message, containing a random value, to the parent of the mote.
 */
void send_DATA(mote_t *mote) {

	DATA_message_t *message = (DATA_message_t*) malloc(DATA_size);
	message->type = DATA;
	message->src_addr = mote->addr;
	message->data = (uint16_t) (random_rand() % 501); // US A.Q.I. goes from 0 to 500
	
	nullnet_buf = (uint8_t*) message;
	nullnet_len = DATA_size;

	free(message);

	NETSTACK_NETWORK.output(&(mote->parent->addr));

}

/**
 * Forwards a DATA message to the parent of the mote.
 */
void forward_DATA(DATA_message_t *message, mote_t *mote) {
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = DATA_size;
	NETSTACK_NETWORK.output(&(mote->parent->addr));
}

void forward_ACK(ACK_message_t *message, mote_t *mote){
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = ACK_size;
	NETSTACK_NETWORK.output(&(mote->parent->addr));
}

void send_TURNON(linkaddr_t src_addr, linkaddr_t dst_addr, mote_t *mote) {
	// Address of the next-hop mote towards destination
	linkaddr_t next_hop;
	if (hashmap_get(mote->routing_table, dst_addr, &next_hop) == MAP_OK) {
		// Node is correctly in the routing table
		TURNON_message_t* message = (TURNON_message_t*) malloc(TURNON_size);
		message->type = TURNON;
		message->src_addr = src_addr;
		message->dst_addr = dst_addr;
		nullnet_buf = (uint8_t*) message;
		nullnet_len = DATA_size;

		free(message);

		NETSTACK_NETWORK.output(&next_hop);
	} else {
		// Destination mote wasn't present in routing table
		LOG_INFO("Mote not in routing table.\n");
	}
}


void send_ACK(linkaddr_t dest_addr, mote_t *mote){
// Address of the next-hop mote towards destination
	ACK_message_t *message = (ACK_message_t*) malloc(ACK_size);
	message->type = ACK;
	message->typeMote = mote->typeMote;
	message->src_addr = mote->addr;
	message->dst_addr = dest_addr;
	
	nullnet_buf = (uint8_t*) message;
	nullnet_len = ACK_size;

	free(message);

	NETSTACK_NETWORK.output(&(mote->parent->addr));
	
}

void forward_TURNON(TURNON_message_t *message, mote_t *mote) {
	// Address of the next-hop mote towards destination
	linkaddr_t next_hop;
	if (hashmap_get(mote->routing_table, message->dst_addr, &next_hop) == MAP_OK) {
		// Forward to next_hop
		nullnet_buf = (uint8_t*) message;
		nullnet_len = TURNON_size;

		free(message);

		NETSTACK_NETWORK.output(&next_hop);
	} else {
		LOG_INFO("Error in forwarding TURNON message.\n");
	}
}
