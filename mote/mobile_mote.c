//code for the mobile terminal of the operator
#include "dev/leds.h"

#include "routing.h"
#include "trickle-timer.h"
#include <stdio.h>
#include <stdlib.h>

#include "random.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO


// Represents the attributes of this mote
mote_t mote;

// 1 if the mote has been created. Used to create the mote only once.
uint8_t created = 0;

// Trickle timer for the periodic messages
trickle_timer_t t_timer;

uint8_t cptACK = 0;



/////////////////////////
///  CALLBACK TIMERS  ///
/////////////////////////

// Callback timer to send information messages
struct ctimer send_timer;

// Callback timer to send DAO messages to parent
struct ctimer DAO_timer;

// Callback timer to detach from lost parent
struct ctimer parent_timer;


/**
 * Callback function that will send the appropriate message when ctimer has expired.
 */
void send_callback(void *ptr) {

	// Send the appropriate message
	if (!mote.in_dodag) {
		LOG_INFO("Sending DIS, finding a parent\n");
		send_DIS();
	}

	// Restart the timer with a new random value
//	ctimer_set(&send_timer, trickle_random(&t_timer), send_callback, NULL);

}

/**
 * Callback function that will send a DAO message to the parent, if the mote is in the DODAG.
 */
void DAO_callback(void *ptr) {
	if (mote.in_dodag) {
		//LOG_INFO("sending DAO\n");
		send_DAO(&mote);
	}
	// Restart the timer with a new random value
	ctimer_set(&DAO_timer, trickle_random(&t_timer), DAO_callback, NULL);
}

/**
 * Resets the trickle timer and restarts the callback timers that use it.
 * This function is called when there is a change in the network.
 */
void reset_timers() {
	trickle_reset(&t_timer);
	ctimer_set(&send_timer, trickle_random(&t_timer),
		send_callback, NULL);
	ctimer_set(&DAO_timer, trickle_random(&t_timer),
		DAO_callback, NULL);
}

/**
 * Resets the trickle timer, and stops all timers for events that happen when the mote is in the network.
 * This function is called when the mote detaches from the network.
 */
void stop_timers() {
	trickle_reset(&t_timer);
	ctimer_set(&send_timer, trickle_random(&t_timer),
		send_callback, NULL);
	ctimer_stop(&DAO_timer);
	ctimer_stop(&parent_timer);
}

/**
 * Callback function that will detach from the DODAG if the parent is lost.
 */
void parent_callback(void *ptr) {
	// Reset the timer
	ctimer_reset(&parent_timer);

	// Detach from DODAG only if node was already in DODAG
	if (mote.in_dodag) {
		// Detach from DODAG
		detach(&mote);
		// Reset and stop timers
		stop_timers();
	}

}



/////////////////////////////
///  RUNICAST CONNECTION  ///
/////////////////////////////


/**
 * Callback function, called when an unicast packet is received
 */
void runicast_recv(const void* data, uint8_t len, const linkaddr_t *from) {

	uint8_t* typePtr = (uint8_t*) data;
	uint8_t type = *typePtr;

	if (type == MAINTACK){
		cptACK++;
		if (cptACK == 3){
			LOG_INFO("Received all acks\n");
		}
	}else {
		LOG_INFO("Unknown runicast message received.\n");
	}

}



//////////////////////////////
///  BROADCAST CONNECTION  ///
//////////////////////////////


/**
 * Callback function, called when a broadcast packet is received
 */
void broadcast_recv(const void* data, uint16_t len, const linkaddr_t *from) {

	// Strength of the last received packet
	signed char rss = RADIO_PARAM_LAST_RSSI;

	uint8_t* typePtr = (uint8_t*) data;
	uint8_t type = *typePtr;
	if (type == DIO) { // DIO message received
		DIO_message_t* message = (DIO_message_t*) data;
		if (linkaddr_cmp(from, &(mote.parent->addr))) { // DIO message received from parent
			if (message->rank == INFINITE_RANK) { // Parent has detached from the DODAG
				detach(&mote);
				stop_timers();

			} else { // Update info
				// Restart timer to delete lost parent
				ctimer_set(&parent_timer, CLOCK_SECOND*TIMEOUT_PARENT,
					parent_callback, NULL);
			}

		} else {
	    		//DIO received from a new potential parent
			// DIO message received from other mote
			uint8_t code = choose_parent(&mote, from, message->rank, rss, message->typeMote);
		    	if (code == PARENT_NEW) {
				reset_timers();
				//Parent choosen, sending DAO
			    	send_DAO(&mote);
				send_MAINT(mote.addr,mote.parent->addr, &mote);
				send_MAINT(mote.addr,mote.parent->addr, &mote);
				send_MAINT(mote.addr,mote.parent->addr, &mote);

			    	// Start all timers that are used when mote is in DODAG
			    	ctimer_set(&send_timer, trickle_random(&t_timer),
						send_callback, NULL);
				ctimer_set(&DAO_timer, trickle_random(&t_timer),
						DAO_callback, NULL);
				ctimer_set(&parent_timer, CLOCK_SECOND*TIMEOUT_PARENT,
						parent_callback, NULL);

		    	} else if (code == PARENT_CHANGED) {
			    	// If parent has changed, send DIO message to update children
			    	// and DAO to update routing tables, then reset timers
				//Parent changed, sending DIO and DAO to update routing and children, updating rank

			    	send_DAO(&mote);
			    	reset_timers();
		    	}
		}

	} else { // Unknown message received
		LOG_INFO("Unknown broadcast message received.\n");
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
PROCESS(sensor_mote, "Sensor mote");
AUTOSTART_PROCESSES(&sensor_mote);


PROCESS_THREAD(sensor_mote, ev, data) {

	if (!created) {
		init_mote(&mote, 5);
		trickle_init(&t_timer);
		created = 1;
	}

	PROCESS_BEGIN();

	nullnet_set_input_callback(input_callback);


	while(1) {

		// Start the sending timer
		ctimer_set(&send_timer, trickle_random(&t_timer),
			send_callback, NULL);

		// Wait for the ctimers to trigger
		PROCESS_YIELD();

	}

	PROCESS_END();

}


