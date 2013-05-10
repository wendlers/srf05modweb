/*
 * Application that retrives the values from an SRF05 ultrasonic sensor. 
 *
 * Copyright (C) 2012 Stefan Wendler <sw@kaltpost.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Mongoose based web-service for providing sensor data from SRF05 
 * ultrasonic distance sensor. 
 *
 * Requires the corresponding kernel modulte "srf05.ko" to be loaded.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "mongoose.h"

/**
 * Root directory for HTML-Documents
 */
#define DOC_ROOT			"/opt/app/html"

/**
 * Directory where the CSV based history files (one per day) are stored 
 */
#define HISTORY_FILE_PATH	"/opt/app/html/history"

/**
 * Interval (in sec.) at which a new value is written to the history file
 */
#define HISTORY_INTERVAL	3600

/**
 * HTTP service port to listen on for client requests 
 */
#define HTTP_PORT			"80"

/**
 * Prefix for sysfs based data files (as created by srf05.ko)
 */
#define DATA_FILE_PREFIX	"/sys/kernel/srf05/distance_"

/**
 * Sysfs path to file indicating sensors current status
 */
#define STATUS_FILE			"/sys/kernel/srf05/status"

/**
 * Number of items to keep in the in memory history (used to display in web browser)
 */
#define MAX_HISTORY			48	

/**
 * If set to 1 (on SIGINT) main loop and webserver are terminated
 */
int interrupted = 0;

/**
 * Defines one history entry
 */
typedef struct {

	/**
 	 * Timestamp
 	 */
	time_t	ts;

	/**
 	 * Distance measured by sensor (in cm)
 	 */
	long	dcm;

	/**
 	 * Distance measured by sensor (raw value)
 	 */
	long	draw;

	/**
 	 * State (ERROR/OPERATIONAL) of sensor
 	 */
	char	stat;

} history_entry;

/**
 * Defines the ring buffer to which the in memory history is stored
 */
typedef struct {
	/**
	 * Array of history entries
	 */
	history_entry entries[MAX_HISTORY];

	/**
	 * Maximum size of the history array 
	 */
	int size;

	/**
 	 * Index of most current entry in the history
 	 */
	int	curr;

	/**
 	 * Current number of entries in the history
 	 */
	int count;

} history_table;

/**
 * Global instance of the in memory history
 */
static history_table history = {
	.size = MAX_HISTORY,
	.curr = -1,
	.count = 0,
};

/**
 * If SIGINT is received, stop the main loop and the webserver. 
 *
 * @param[in]	*signum		number of signal received
 */
static void sig_handler(int signum) {
	interrupted = 1;
};

/**
 * Read current distance from the ultrasonic sensor.  
 *
 * @param[in]	*unit	unit in which to measure the distance: "cm" for cm or "raw" for raw
 * @return				the current distance in the requested unit on success, or -1 on failure
 */
static long srf05_read_distance(const char *unit) {

	char data_file[128];
	char data[32] = "-1";

	FILE *file = NULL;

	snprintf(data_file, sizeof(data_file), "%s%s", DATA_FILE_PREFIX, unit);
 
	file = fopen(data_file, "r");

	if(file == NULL) {
		return -1;
	}

	fgets(data, sizeof(data), file);

	fclose(file);

	return atol(data);
}

/**
 * Read current status of ultrasonic sensor. 
 *
 * @param[out]	*status		pointer to buffer for storing the status ("UNKNOWN", "ERROR" or "OPERAIONAL")
 * @param[in]	len			length of status buffer
 * @return					pointer to status buffer
 */
static char *srf05_read_status(char *status, int len) {

	FILE *file = NULL;

	file = fopen(STATUS_FILE, "r");

	if(file == NULL) {
		strncpy(status, "UNKNOWN", len);
	}
	else {
		fgets(status, len, file);
		status[strlen(status) - 1] = 0;
		fclose(file);
	}

	return status;
}

/**
 * Read the current raw value and the current cm value from sensor and store it to 
 * the in memory history.
 */ 
static void add_history(void) {

	char status[32]; 

	int pos = history.curr + 1;

	if(pos >= history.size) {
		pos = 0; 
		history.curr = -1;
	}	

	if(history.count < history.size) {
		history.count++;
	}

	time(&history.entries[pos].ts);

	history.entries[pos].dcm  = srf05_read_distance("cm");
	history.entries[pos].draw = srf05_read_distance("raw");

	if(strcmp(srf05_read_status(status, sizeof(status)), "OPERATIONAL") == 0) {
		history.entries[pos].stat = 1;
	}
	else {
		history.entries[pos].stat = 0;
	}

	history.curr++;
}

/**
 * Append the most current value from the in memory history to the history file.
 * A new history file is created each day using a naming scheme of: YYYY-MM-DD.csv.
 */ 
static void dump_history(void) {

	FILE   *file; 
	struct 	tm *lt;
	int 	pos = history.curr;
	char    file_name[256];
	
	lt = localtime(&history.entries[pos].ts);

	// One file per day 
	snprintf(file_name, sizeof(file_name), "%s/%04d-%02d-%02d.csv", HISTORY_FILE_PATH, 	
		lt->tm_year + 1900,
		lt->tm_mon + 1,
		lt->tm_mday);
	
	// Append if exists, or create
	file = fopen(file_name, "a+");

	if(file == NULL) {
		printf("ERROR: unable to open history file %s\n", file_name);
	}
	
	fprintf(file, "\"%04d-%02d-%02d\",\"%02d:%02d:%02d\",\"%s\",%ld,%ld\n", 
		lt->tm_year + 1900,
		lt->tm_mon + 1,
		lt->tm_mday,
		lt->tm_hour,
		lt->tm_min,
		lt->tm_sec,
		(history.entries[pos].stat == 0 ? "ERROR" : "OPERATIONAL"), 
		history.entries[pos].dcm,
		history.entries[pos].draw);

	fclose(file);
}

/**
 * Handle HTTP client requests. The handler will process the following URIs:
 *
 * 	- /sensor/srf05			Current value in JSON
 * 	- /sensor/srf05/history	Complete in momory history in JSON
 *
 * @param	*conn		connection as defined by mongoose
 * @return				1 if the handler handled the request, 0 otherwise
 */
static int begin_request_handler(struct mg_connection *conn) {

	const struct mg_request_info *request_info = mg_get_request_info(conn);

	char status[32]; 
	char histline[128] = "";
	char content[MAX_HISTORY * 128] = "";
	int  content_length = 0; 

	int  	i = 0; 
	int 	pos = history.curr;
	struct 	tm *lt;
	int     first = 1;

	if(strcmp(request_info->uri, "/sensor/srf05") == 0) {

		content_length = snprintf(content, sizeof(content), 
			"{\"distance_raw\" : %ld, \"distance_cm\" : %ld, \"status\" : \"%s\"}", 
			srf05_read_distance("raw"), 
			srf05_read_distance("cm"),
			srf05_read_status(status, sizeof(status)));
	}
	else if(strcmp(request_info->uri, "/sensor/srf05/history") == 0) {

		strcat(content, "{ \"history\" : [ ");

		for(i = history.count - 1; i >= 0; i--) {
			
			lt = localtime(&history.entries[pos].ts);
 
			snprintf(histline, sizeof(histline), 
				"%s{\"date\" : \"%04d-%02d-%02d\", \"time\" : \"%02d:%02d:%02d\", \"distance_raw\" : %ld, \"distance_cm\" : %ld, \"status\" : \"%s\"}", 
				(first == 1 ? "" : ", "),
				lt->tm_year + 1900,
				lt->tm_mon + 1,
				lt->tm_mday,
				lt->tm_hour,
				lt->tm_min,
				lt->tm_sec,
				history.entries[pos].draw,
				history.entries[pos].dcm,
				(history.entries[pos].stat == 0 ? "ERROR" : "OPERATIONAL"));

			strcat(content, histline);

			if(--pos < 0) {
				pos = history.size - 1;
			}
			
			if(first == 1) first = 0;
		}
		
		strcat(content, "] }");

		content_length = strlen(content);
	}
	else {
		return 0;
	}

  	// Send HTTP reply to the client
  	mg_printf(conn,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: %d\r\n"        // Always set Content-Length
		"\r\n"
		"%s",
		content_length, content);

  return 1;
}

/**
 * Main
 *
 * @return 		0 on succes, -1 on failure
 */
int main(void) {

	struct mg_context *ctx;
	struct mg_callbacks callbacks;
	
	int loopcnt = HISTORY_INTERVAL;

	// Change to doc-root first
	if(chdir(DOC_ROOT) != 0) {
		printf("ERROR: doc-root %s not found!\n", DOC_ROOT);
		return -1;
	} 

	// SIGINT will stop the main loop
	signal(SIGINT, sig_handler);
		
	// List of options. Last element must be NULL.
	const char *options[] = {
		"listening_ports", HTTP_PORT, NULL,
	};

	// Prepare callbacks structure. We have only one callback, the rest are NULL.
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.begin_request = begin_request_handler;

	// Start the web server. Requests are handled in threads
	ctx = mg_start(&callbacks, NULL, options);
	 
	// Servers main loop: add sensor values to history, dump history to file
	while(!interrupted) {
		sleep(1);

		if(loopcnt++ >= HISTORY_INTERVAL) {
			add_history();
			dump_history();
			loopcnt = 0;
		}
	}

	// Stop the server.
	mg_stop(ctx);

	return 0;
}
