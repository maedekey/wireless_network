/**
 * Code for the sensor nodes
 */

#include "dev/leds.h"

#include "routing.h"
#include "trickle-timer.h"

#include <stdio.h>
#include <stdlib.h>
#include "random.h"
#include "serial-line.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

// Represents the attributes of this mote
mote_t mote;
uint8_t created = 0;

// Trickle timer for the periodic messages
trickle_timer_t t_timer;

// Broadcast connection
//static struct broadcast_conn broadcast;

// Reliable unicast (runicast) connection
//static struct runicast_conn runicast;



/////////////////////////
///  CALLBACK TIMERS  ///
/////////////////////////

// Callback timer to send information messages
struct ctimer send_timer;

// Callback timer to delete parent or children
struct ctimer children_timer;

/**
 * Callback function that will send the appropriate message when ctimer has expired.
 */
void send_callback(void *ptr) {

	// Send a DIO message
	LOG_INFO("DIO sent rank = %u \n", mote.rank);
	send_DIO(&mote);

	// Update the trickle timer
	trickle_update(&t_timer);

	// Restart the timer with a new random value
	ctimer_set(&send_timer, trickle_random(&t_timer), send_callback, NULL);

}

/**
 * Callback function that will delete unresponsive children from the routing table.
 */
void children_callback(void *ptr) {
	// Reset the timer
	ctimer_reset(&children_timer);

	// Delete children that haven't sent messages since a long time
	if (hashmap_delete_timeout(mote.routing_table)) {
		// Children have been deleted, reset trickle timer
		trickle_reset(&t_timer);
	}

}

/**
 * Resets the trickle timer and restarts the callback timer that uses it (sending timer)
 */
void reset_timers(trickle_timer_t *timer) {
	trickle_reset(&t_timer);
	ctimer_set(&send_timer, trickle_random(&t_timer),
		send_callback, NULL);
}



////////////////////////////
///  UNICAST CONNECTION  ///
////////////////////////////

/**
 * Callback function, called when an unicast packet is received
 */
void runicast_recv(const void* data, uint8_t len, const linkaddr_t *from) {

	uint8_t* typePtr = (uint8_t*) data;
	uint8_t type = *typePtr;

	if (type == DAO) {
		LOG_INFO("DAO received\n");

		//LOG_INFO("DAO message received from %u.%u\n", from->u8[0], from->u8[1]);

		DAO_message_t* message = (DAO_message_t*) data;

		// Address of the mote that sent the DAO packet
		linkaddr_t child_addr = message->src_addr;

		int err = hashmap_put(mote.routing_table, child_addr, *from);
		if (err == MAP_NEW) { // A new child was added to the routing table
			// Reset trickle timer and sending timer
			reset_timers(&t_timer);
		} else if (err != MAP_NEW && err != MAP_UPDATE) {
			LOG_INFO("Error adding to routing table\n");
		}

	} else if (type == DATA) {
		LOG_INFO("DATA received\n");
		DATA_message_t* message = (DATA_message_t*) data;
		LOG_INFO("%u/%u/%u\n", (unsigned int) message->type, (unsigned int) message->src_addr.u16, (unsigned int) message->data);

	} else {
		LOG_INFO("Unknown runicast message received.\n");
	}


}

/**
 * Callback function, called when an unicast packet is sent
 */
void runicast_sent(const linkaddr_t *to, uint8_t retransmissions) {
	// Nothing to do
}

/**
 * Callback function, called when an unicast packet has timed out
 */
void runicast_timeout(const linkaddr_t *to, uint8_t retransmissions) {
	// Nothing to do
}

//const struct runicast_callbacks runicast_callbacks = {runicast_recv, runicast_sent, runicast_timeout};


//////////////////////////////
///  BROADCAST CONNECTION  ///
//////////////////////////////

/**
 * Callback function, called when a broadcast packet is received
 */
void broadcast_recv(const void* data, uint16_t len, const linkaddr_t *from) {

	uint8_t* typePtr = (uint8_t*) data;
	uint8_t type = *typePtr;

	if (type == DIS) {
		LOG_INFO("DIS received\n");
		//LOG_INFO("DIS packet received.\n");
		// If the mote is already in a DODAG, send DIO packet
		if (mote.in_dodag) {
			LOG_INFO("Sending DIO\n");
			send_DIO(&mote);
		}
	}
}

//const struct broadcast_callbacks broadcast_call = {broadcast_recv};

void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
	if (linkaddr_cmp(dest, &linkaddr_null)){
		broadcast_recv(data, len, src);
	}else{
		runicast_recv(data,len,src);
	}
}


//////////////////////
///  MAIN PROCESS  ///
//////////////////////

// Create and start the process
PROCESS(root_mote, "Root mote");
PROCESS(server_communication, "Server communication");

AUTOSTART_PROCESSES(&root_mote, &server_communication);

PROCESS_THREAD(root_mote, ev, data) {

	if (!created) {
		init_root(&mote, 0);
		trickle_init(&t_timer);
		created = 1;
	}

//	PROCESS_EXITHANDLER(broadcast_close(&broadcast); runicast_close(&runicast);)

	PROCESS_BEGIN();

	nullnet_set_input_callback(input_callback);

	while(1) {

		// Start all the timers
		ctimer_set(&send_timer, trickle_random(&t_timer),
			send_callback, NULL);
		ctimer_set(&children_timer, CLOCK_SECOND*TIMEOUT_CHILDREN,
			children_callback, NULL);

		// Wait for the ctimer to trigger
		PROCESS_YIELD();

	}

	PROCESS_END();
}

PROCESS_THREAD(server_communication, ev, data) {
    PROCESS_BEGIN();

    nullnet_set_input_callback(input_callback);

    while(1) {
        PROCESS_YIELD();
    }
    PROCESS_END();
}
