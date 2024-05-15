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
const uint8_t TURNON = 5;
const uint8_t ACK = 6;
const uint8_t LIGHT = 7;
const uint8_t MAINT = 8;
const uint8_t MAINTACK = 9;



// Size of control messages
const size_t DIS_size = sizeof(DIS_message_t);
const size_t DIO_size = sizeof(DIO_message_t);
const size_t DAO_size = sizeof(DAO_message_t);
const size_t TURNON_size = sizeof(TURNON_message_t);
const size_t LIGHT_size = sizeof(LIGHT_message_t);
const size_t ACK_size = sizeof(ACK_message_t);
const size_t MAINT_size = sizeof(MAINT_message_t);
const size_t MAINTACK_size = sizeof(MAINTACK_message_t);

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
	if (typeMote == 0){
		mote->in_dodag = 1;
		mote->rank = 0;
	}else{
		mote->in_dodag = 0;
		mote->rank = INFINITE_RANK;		
	}
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
	if (mote->typeMote == 1 && typeMote != 0){
		return 0;
	}else if (mote->typeMote > 1 && typeMote == 0) {
		return 0;
	}else if(mote->typeMote > 1){
		if (mote->parent->typeMote == typeMote){
			uint8_t lower_rank = parent_rank < mote->parent->rank;
			uint8_t same_rank = parent_rank == mote->parent->rank;
			uint8_t better_rss = rss > mote->parent->rss + RSS_THRESHOLD;
			return lower_rank || (same_rank && better_rss);	
		}else{
			return mote->parent->typeMote > typeMote;
		}
	}
	return 0;
}

/**
 * Selects the parent. Returns a code depending on if the parent has changed or not.
 */
uint8_t choose_parent(mote_t *mote, const linkaddr_t* parent_addr, uint8_t parent_rank, signed char rss, uint8_t typeMote) {
	if (!mote->in_dodag) {
		if (mote->typeMote > 1 && typeMote != 0){ 
			// Mote not in DODAG yet, initialize parent
			init_parent(mote, parent_addr, parent_rank, rss, typeMote);
			return PARENT_NEW;
		} else if (mote->typeMote == 1 && typeMote == 0){
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
 * Sends a LIGHT message, containing a random value, to the parent of the mote.
 */
void send_LIGHT(mote_t *mote) {

	LIGHT_message_t *message = (LIGHT_message_t*) malloc(LIGHT_size);
	message->type = LIGHT;
	message->light_level = (uint16_t) (random_rand() % 250);

	nullnet_buf = (uint8_t*) message;
	nullnet_len = LIGHT_size;

	free(message);

	NETSTACK_NETWORK.output(&(mote->parent->addr));

}

/**
 * Forwards a LIGHT message to the parent of the mote.
 */
void forward_LIGHT(LIGHT_message_t *message, mote_t *mote) {
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = LIGHT_size;
	NETSTACK_NETWORK.output(&(mote->parent->addr));
}

void send_TURNON(uint8_t typeMote, linkaddr_t dest, mote_t *mote) {

	LOG_INFO("sending turnon to %u\n ", dest.u16[0]);
	TURNON_message_t* message = (TURNON_message_t*) malloc(TURNON_size);
	message->type = TURNON;
	message->typeMote = typeMote;
	nullnet_buf = (uint8_t*) message;
	nullnet_len = TURNON_size;

	free(message);

	NETSTACK_NETWORK.output(&dest);
}


void send_ACK(mote_t *mote) {
	LOG_INFO("sending ack to parent \n ");
	ACK_message_t* message = (ACK_message_t*) malloc(ACK_size);
	message->type = ACK;
	message->typeMote = mote->typeMote;
	nullnet_buf = (uint8_t*) message;
	nullnet_len = ACK_size;

	free(message);

	NETSTACK_NETWORK.output(&(mote->parent->addr));
}

void forward_ACK(ACK_message_t *message, mote_t *mote){
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = ACK_size;
	NETSTACK_NETWORK.output(&(mote->parent->addr));
}

void forward_TURNON(uint8_t typeMote, mote_t *mote) {	
	// Address of the next-hop mote towards destination

	hashmap_element* map = mote->routing_table->data;
	int i;
	unsigned index = 0;
	linkaddr_t dst[mote->routing_table->table_size];
	for (i = 0; i < mote->routing_table-> table_size; i++) {
		hashmap_element elem = *(map+i);
		if (elem.in_use && elem.typeMote == typeMote) {
			if(!isInArray(dst, index, &elem.data)){
				dst[index] = elem.data;
				index++;
			}
		}
	}
	for (i = 0; i < index; i++) {
		send_TURNON(typeMote, dst[i], mote);
	}
	memset(dst, 0, sizeof(linkaddr_t)*index);
}

void send_MAINT(linkaddr_t src_addr, linkaddr_t dest, mote_t *mote){
	LOG_INFO("sending maintenance to %u\n ", dest.u16[0]);
	MAINT_message_t* message = (MAINT_message_t*) malloc(MAINT_size);
	message->type = MAINT;
	message->src_addr = src_addr;
	nullnet_buf = (uint8_t*) message;
	nullnet_len = MAINT_size;

	free(message);

	NETSTACK_NETWORK.output(&dest);
}

void forward_MAINT(linkaddr_t src_addr, mote_t *mote){
	hashmap_element* map = mote->routing_table->data;
	int i;
	int cpt = 0;
	linkaddr_t dst;
	for (i = 0; i < mote->routing_table->table_size; i++) {
		hashmap_element elem = *(map+i);
		if (elem.in_use && elem.typeMote == 2) {
			dst = elem.data;
			cpt = 1;
			break;
		}	
	}	
	if (cpt == 0){
		LOG_INFO("sending maintenance to parent ");
		send_MAINT(src_addr, mote->parent->addr, mote);	
	}else{
		LOG_INFO("sending maintenance to child : %u", dst.u16[0]);
		send_MAINT(src_addr, dst, mote);	
	}
}

void send_MAINTACK(mote_t *mote, linkaddr_t dst_addr){
	LOG_INFO("sending maintenance ack \n ");
	MAINTACK_message_t* message = (MAINTACK_message_t*) malloc(MAINTACK_size);
	message->type = MAINTACK;
	message->dst_addr = dst_addr;
	
	linkaddr_t nexthop;
	uint8_t typeMote;
	if(hashmap_get(mote->routing_table, dst_addr, &typeMote, &nexthop) != MAP_OK){
		nexthop = mote->parent->addr;
	}
	
	
	nullnet_buf = (uint8_t*) message;
	nullnet_len = MAINTACK_size;

	free(message);

	NETSTACK_NETWORK.output(&nexthop);
}

void forward_MAINTACK(MAINTACK_message_t *message, mote_t *mote){
	LOG_INFO("forwarding maintenance ack \n ");
	linkaddr_t nexthop;
	uint8_t typeMote;
	if(hashmap_get(mote->routing_table, message->dst_addr, &typeMote, &nexthop) != MAP_OK){
		nexthop = mote->parent->addr;
	}
	nullnet_buf = (uint8_t*) message;	
	nullnet_len = MAINTACK_size;
	NETSTACK_NETWORK.output(&nexthop);
	
}


unsigned isInArray(linkaddr_t* dst, unsigned effectiveSize, linkaddr_t* val){
	unsigned i = 0;
	for (i = 0; i <= effectiveSize; i++){
		if (dst[i].u16[0] == val->u16[0]){
			return 1;
		}
	}
	return 0;
}
