#ifndef _CMOC_VERSION_
#include <conio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#else
#include <cmoc.h>
#include <coco.h>
#include "conio_wrapper.h"
#endif
#include "connection.h"
#include "data.h"
#include "display.h"
#include "debug.h"
#include "delay.h"
#include "double_buffer.h"
#include "fujinet-network.h"
#include "keyboard.h"
#include "sound.h"
#include "status.h"
#include "world.h"

#ifdef __ATARI__
#include "dlist.h"
extern bool is_playing_collision;
#endif

#ifdef __PMD85__
#include "conio_wrapper.h"
#endif

#ifdef BUILD_OS9
// NitrOS-9: the next request goes to the server before a frame is drawn,
// so that the FujiNet and the server are at work while the 6309 draws.
// Anything else sent to the server (by handle_app_status() and handle_kb())
// goes between a reply and the next request, as the replies come in order.
void run_simulation() {
	int n;
	uint8_t new_step_id = 0;
	bool draw;

	init_screen();

	is_alt_screen = 0;
	is_running_sim = 1;

	is_showing_info = 0;
	is_showing_broadcast = 0;
	is_showing_clients = 0;
	flash_on_collision = 0;
	play_sound_on_collision = 0;

	get_world_clients();
	request_client_data();

	while(is_running_sim) {
		memset(app_data, 0, APP_DATA_SIZE);
		n = read_response_min((uint8_t *) app_data, 1, APP_PAYLOAD_SIZE);
		draw = false;

		if (n > 0) {
			new_step_id = app_payload[0];
			draw = new_step_id != current_step;

			app_status = app_payload[1];
			if (app_status != 0) {
				// The clients and the commands are read into app_data,
				// over this frame.
				if (app_status & (CLIENT_CHANGE | CLIENT_CMD)) {
					draw = false;
				}
				handle_app_status();
			}
		}

		handle_kb();
		if (!is_running_sim) {
			break;
		}

		request_client_data();
		if (draw) {
			current_step = new_step_id;
			show_screen();
		}
	}

	disconnect_service();
}
#else
void run_simulation() {
	int n;
	uint8_t new_step_id;

	init_screen();

	// flag we are on initial screen for double buffering across the platforms
	is_alt_screen = 0;
	// Ensure that the is_running_sim flag is set if it was turned off previously
	is_running_sim = 1;

	// Reset info, broadcast, clients flags;
	is_showing_info = 0;
	is_showing_broadcast = 0;
	is_showing_clients = 0;
	flash_on_collision = 0;
	play_sound_on_collision = 0;

	get_world_clients();

	while(is_running_sim) {
		n = fetch_client_state();

		// Nothing to show this round, so reloop and check again
		if (n <= 0) {
			// debug();
			continue;
		}

		// handle any app events
		app_status = app_payload[1];
		if (app_status != 0) {
			handle_app_status();
		}

		// check if the server is frozen or not, by checking the step id
		new_step_id = app_payload[0];
		if (new_step_id != current_step) {
			current_step = new_step_id;
			show_screen();
		}

		// handle any keyboard events
		handle_kb();

		// debug: slow things down so we can monitor the logs and catch them when it errors.
		// pause(60);
	}

	// either errored, or quit by user, so deregister from the server
	disconnect_service();

}
#endif // BUILD_OS9
