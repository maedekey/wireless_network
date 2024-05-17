/**
 * Code for the gateway
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
		DAO_message_t* message = (DAO_message_t*) data;

		// Address of the mote that sent the DAO packet
		linkaddr_t child_addr = message->src_addr;

		int err = hashmap_put(mote.routing_table, child_addr, message->typeMote, *from);
		
		if (err == MAP_NEW) { // A new child was added to the routing table
			// Reset trickle timer and sending timer
			reset_timers(&t_timer);
		}


	} else if (type == ACK) {
		ACK_message_t* message = (ACK_message_t*) data;
		if (mote.typeMote == 0){		
			printf("Ack received from: \n");
			printf("%u \n", message->typeMote);	
		}
		else{	//only the root mote (gateway) sends ack to the server
			forward_ACK(message,&mote);	
		}

	} else if (type == LIGHT){
		LIGHT_message_t* message = (LIGHT_message_t*) data;
		if (mote.typeMote == 0){
			printf("LIGHTSENSOR");	//Gateway indicates the server that it's sending the light level
			printf("%u \n", message->light_level);
			printf("LIGHTSENSOR");
		}
		else{
			forward_LIGHT(message,&mote);	
		}
	} else if (type == MAINT){
		MAINT_message_t* message = (MAINT_message_t*) data;
		//root mote isn't supposed to receive maintenance message
		forward_MAINT(message->src_addr, &mote);
	} else if (type == MAINTACK){
		MAINTACK_message_t* message = (MAINTACK_message_t*) data; //same for maintack	
		forward_MAINTACK(message, &mote);
	} else {
		LOG_INFO("Unknown runicast message received. type is %u\n, from %u", type, from->u16[0]);
	}


}

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
		// If the mote is already in a DODAG, send DIO packet
		if (mote.in_dodag) {
			send_DIO(&mote);
		}
	}
}


/**
* Send a MAINACK message to the dest addr given. If the dest mote (the mobile terminal) is not known locally, it is sent to the parent of the mote
*/
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
		init_mote(&mote, 0);
		trickle_init(&t_timer);
		created = 1;
	}

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
	serial_line_init();
  	uart0_set_input(serial_line_input_byte);
    nullnet_set_input_callback(input_callback);
    while(1) {
        PROCESS_YIELD();
        if(ev==serial_line_event_message){ //if the message received from the server is "water": we send to the typemote 3 (the sprinkler) to turn on
		if (strcmp((char*) data, "WATER") == 0) {	
			forward_TURNON(3, &mote);		  
    		}    		
		if (strcmp((char*) data, "LIGHTBULBS") == 0) { //if it is "lightbulbs": to the typemote 4 (lightbulbs)
			forward_TURNON(4, &mote);
   		}	  
    	}
    }
    PROCESS_END();
}
