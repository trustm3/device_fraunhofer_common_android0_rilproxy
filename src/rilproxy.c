/*
 * This file is part of trust|me
 * Copyright(c) 2013 - 2017 Fraunhofer AISEC
 * Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 (GPL 2), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GPL 2 license for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Fraunhofer AISEC <trustme@aisec.fraunhofer.de>
 */

#include "rilproxy.h"
#include <sys/resource.h>
#include <sys/prctl.h>

#include <unistd.h>

#if 1  // fixme remove after debugging SIM_IO
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <sys/capability.h>
#endif

#define HEADER_PARAM_FIELD_SIZE 8

typedef struct {
	int fd;
	int connected;
	ril_message_origin_t origin;
	RecordStream *rs;
	struct pollfd *fd_connector;
} ril_socket_t;

static void
rilproxy_handle_raw_msg_from_a0(ril_socket_t *client, int fd_rild);
static void
rilproxy_handle_raw_msg_from_aX(ril_socket_t *client, int fd_rild);
static void
rilproxy_handle_raw_msg_from_modem(ril_socket_t *client, int fd_a0);

ril_message_t *ril_message_unsol_ril_connected = NULL;
ril_message_t *ril_message_unsol_radio_state_on = NULL;
ril_message_t *ril_message_unsol_radio_state_off = NULL;
ril_message_t *ril_message_unsol_stk_procative_command = NULL;
ril_message_t *ril_message_unsol_stk_session_end = NULL;
ril_message_t *ril_message_sol_get_sim_status = NULL;

static list_t *ril_global_storage_list = NULL;
static list_t *ril_a1_filter_list = NULL;
static list_t *ril_a2_filter_list = NULL;
static list_t *ril_socket_list = NULL;

static unsigned int wait_for_data_call_response = 0;
static list_t *client_with_pending_commands_list = NULL;

/*
 * default emulated messages (see rilproxy_init_rilstorage_list())
 */
static char ril_request_deactivate_data_call[28] = {0x29, 0, 0, 0, 0x81, 0, 0, 0, 2, 0, 0, 0,
						1, 0, 0, 0, 0x30, 0, 0, 0, 1, 0, 0, 0, 0x30, 0, 0, 0};
static char ril_answer_deactivate_data_call[12] =  {0, 0, 0, 0, 0x81, 0, 0, 0, 0, 0, 0, 0};

static char ril_request_deactivate_data_call1[28] = {0x29, 0, 0, 0, 0x82, 0, 0, 0, 2, 0, 0, 0,
						1, 0, 0, 0, 0x30, 0, 0, 0, 1, 0, 0, 0, 0x31, 0, 0, 0};
static char ril_answer_deactivate_data_call1[12] = {0, 0, 0, 0, 0x82, 0, 0, 0, 0, 0, 0, 0};

static char ril_request_radio_off[16] = {0x17, 0, 0, 0, 0x90, 0xda, 0x45, 0x60, 1, 0, 0, 0, 0, 0, 0, 0};
static char ril_request_radio_on[16] = {0x17, 0, 0, 0, 0x96, 0xda, 0x45, 0x60, 1, 0, 0, 0, 1, 0, 0, 0};

static char ril_answer_radio_off[12] = {0, 0, 0, 0, 0x90, 0xda, 0x45, 0x60, 0, 0, 0, 0};
static char ril_answer_radio_on[12] = {0, 0, 0, 0, 0x96, 0xda, 0x45, 0x60, 0, 0, 0, 0};

static char ril_unsol_radio_off[12] = {1, 0, 0, 0, 0xe8, 3, 0, 0, 0, 0, 0, 0};
static char ril_unsol_radio_on[12] = {1, 0, 0, 0, 0xe8, 3, 0, 0, 0x0a, 0, 0, 0};

static char ril_request_setup_data_call[128] ={ 0x1b, 0, 0, 0, 0x48, 0xcd, 0x86, 0x9c, 7, 0, 0, 0, 1, 0, 0, 0, 0x34, 0, 0, 0,
						1, 0, 0, 0, 0x30, 0, 0, 0, 0x10, 0, 0, 0, 0x69, 0, 0x6e, 0, 0x74, 0, 0x65, 0, 0x72, 0, 0x6e,
						0, 0x65, 0, 0x74, 0, 0x2e, 0, 0x74, 0, 0x65, 0, 0x6c, 0, 0x65, 0, 0x6b, 0, 0x6f, 0, 0x6d, 0,
						0, 0, 0, 0, 7, 0, 0, 0, 0x74, 0, 0x65, 0, 0x6c, 0, 0x65, 0, 0x6b, 0, 0x6f, 0, 0x6d, 0, 0, 0,
						7, 0, 0, 0, 0x74, 0, 0x65, 0, 0x6c, 0, 0x65, 0, 0x6b, 0, 0x6f, 0, 0x6d, 0, 0, 0, 1, 0, 0, 0,
						0x31, 0, 0, 0, 2, 0, 0, 0, 0x49, 0, 0x50, 0, 0, 0, 0, 0};

static char ril_answer_setup_data_call_failed [76] ={	0, 0, 0, 0, 0x48, 0xcd, 0x86, 0x9c, 0, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0,
							0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
							0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
							0, 0, 0, 0, 0, 0 };

static void
core_dump_enable(void)
{
	struct rlimit core_limit;
	core_limit.rlim_cur = RLIM_INFINITY;
	core_limit.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &core_limit) < 0)
		logE("could not set rlimits for core dump generation");
}

static void
userswitch(void)
{
#if 1 // fixme remove after debugging SIM_IO

	char debuggable[PROP_VALUE_MAX];
	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
	setuid(1001) ;//setuid(AID_RADIO);

	struct __user_cap_header_struct header;
	memset(&header, 0, sizeof(header));
	header.version = _LINUX_CAPABILITY_VERSION_3;
	header.pid = 0;
	struct __user_cap_data_struct data[2];

	memset(&data, 0, sizeof(data));
	data[CAP_TO_INDEX(CAP_NET_ADMIN)].effective |= CAP_TO_MASK(CAP_NET_ADMIN);
	data[CAP_TO_INDEX(CAP_NET_ADMIN)].permitted |= CAP_TO_MASK(CAP_NET_ADMIN);

	data[CAP_TO_INDEX(CAP_NET_RAW)].effective |= CAP_TO_MASK(CAP_NET_RAW);
	data[CAP_TO_INDEX(CAP_NET_RAW)].permitted |= CAP_TO_MASK(CAP_NET_RAW);

	if (capset(&header, &data[0]) == -1) {
		logD("capset failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*
	 *      * Debuggable build only:
	 *           * Set DUMPABLE that was cleared by setuid() to have tombstone on RIL crash
	 *                */
	property_get("ro.debuggable", debuggable, "0");

	if (strcmp(debuggable, "1") == 0) {
	}

	logD("ro.debuggable: %s", debuggable);
	prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
#else

	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
	setuid(1001);

	struct __user_cap_data_struct my_cap;
	struct __user_cap_header_struct my_cap_header;

	my_cap_header.pid = 0;
	my_cap_header.version = _LINUX_CAPABILITY_VERSION;
	my_cap.inheritable = 0;
	my_cap.effective = my_cap.permitted = 1 << CAP_NET_ADMIN;
	capset(&my_cap_header, &my_cap);
#endif

}

static int
blockingWrite(int fd, const void *buffer, size_t len)
{
	size_t writeOffset = 0;
	const uint8_t *toWrite;

	toWrite = (const uint8_t *)buffer;

	while (writeOffset < len) {
		ssize_t written;
		do {
			written = write (fd, toWrite + writeOffset,
					len - writeOffset);
		} while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));

		if (written >= 0) {
			writeOffset += written;
		} else {   // written < 0
			logE ("RIL Response: unexpected error on write errno:%d", errno);
			close(fd);
			return -1;
		}
	}

	return 0;
}

static int
rilproxy_send_parcel(int fd, const void *data, size_t data_len)
{
	int ret;
	uint32_t header;

	if (fd < 0) {
		return -1;
	}

	if (data_len > MAX_COMMAND_BYTES) {
		logE("RIL: packet larger than %u (%u)",
			MAX_COMMAND_BYTES, (unsigned int )data_len);

		return -1;
	}

	header = htonl(data_len);

	ret = blockingWrite(fd, (void *)&header, sizeof(header));

	if (ret < 0) {
		return ret;
	}

	ret = blockingWrite(fd, data, data_len);

	if (ret < 0) {
		return ret;
	}

	return 0;
}

const char *
socket_name(const char *value, unsigned int id)
{
	static int offset = 0;
	static char buffer[128];

	if (id > 0) {
		const char *p = buffer + offset;
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s%u", value, id);
		offset++;
		return p;
	}

	return value;
}

static void
dumpRILData(char *data, unsigned int parcelsize)
{
	char *output = mem_new0(char, (parcelsize * 3) + 20);

	sprintf(output, "size: %u pdata: %02x", parcelsize, *data++);

	for (unsigned int i = 1; i < (parcelsize); i++) {
		sprintf(output, "%s %02x", output, *data++);
	}
	logD("%s", output);
	mem_free(output);

}

static ril_message_t *
RIL_create_message(char *rilcommand, size_t command_len, ril_message_origin_t origin)
{
	unsigned int *idata = NULL;
	ril_message_t *ril_message = NULL;
	ril_message = mem_new0(ril_message_t, 1);

	//ril_message->parcelsize = ((unsigned int)*(rilcommand + 2) << 8) + (unsigned int)*(rilcommand + 3);
	ril_message->parcelsize = command_len;

	if (origin == RIL_MESSAGE_ORIGIN_MODEM) {
		idata = (unsigned int *)(rilcommand);
		ril_message->serial = *idata;
		idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
		ril_message->event_id = *idata;
	} else {
		idata = (unsigned int *)(rilcommand);
		ril_message->event_id = *idata;
		idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
		ril_message->serial = *idata;
	}

	if (ril_message->parcelsize > HEADER_PARAM_FIELD_SIZE)
		ril_message->rildata = (rilcommand + HEADER_PARAM_FIELD_SIZE);
	else
		ril_message->rildata = NULL;

	return ril_message;
}

static list_t *
RIL_Storage(list_t * ril_storage_list, char *rilcommand, size_t command_len, ril_message_origin_t origin)
{
	ril_message_t ril_message;
	unsigned int *idata = NULL;
	ril_message.radio_state = 1;
	ril_message.sim_state = 1;

	if (rilcommand == NULL) {
		logD(" rilcommand == NULL\n");
		return NULL;
	}

	//ril_message.parcelsize = ((unsigned int)*(rilcommand + 2) << 8) + (unsigned int)*(rilcommand + 3);
	ril_message.parcelsize = command_len;

	idata = (unsigned int *)(rilcommand);
	ril_message.event_id = *idata;
	idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
	ril_message.serial = *idata;

	if (ril_message.parcelsize > HEADER_PARAM_FIELD_SIZE)
		ril_message.rildata = (rilcommand + HEADER_PARAM_FIELD_SIZE);
	else
		ril_message.rildata = NULL;

	if (ril_message.parcelsize < HEADER_PARAM_FIELD_SIZE) {	// corrupt parcel
		logD("ril_message.parcelsize < %d", HEADER_PARAM_FIELD_SIZE);
		return ril_storage_list;
	}

	// RIL_UNSOL_STK_SESSION_END 1012
	if ((ril_message.serial == 1012) && (ril_message.event_id == 1) && (origin == RIL_MESSAGE_ORIGIN_MODEM)) {	/// RIL_UNSOL_STK_SESSION_END
		logD("RIL_UNSOL_STK_SESSION_END");
		logD("ril_message.parcelsize = %i", ril_message.parcelsize);
		logD("ril_message.serial = %i", ril_message.serial);
		logD("ril_message.event_id = %i", ril_message.event_id);

		ril_message_unsol_stk_session_end = rilstorage_create_message(&ril_message);

		logD("ril_message_unsol_stk_session_end->parcelsize = %i", ril_message_unsol_stk_session_end->parcelsize);
		logD("ril_message_unsol_stk_session_end->serial = %i", ril_message_unsol_stk_session_end->serial);
		logD("ril_message_unsol_stk_session_end->event_id = %i", ril_message_unsol_stk_session_end->event_id);
	}


	// RIL_UNSOL_STK_PROACTIVE_COMMAND 1013
	if ((ril_message.serial == 1013) && (ril_message.event_id == 1) && (origin == RIL_MESSAGE_ORIGIN_MODEM)) {	/// RIL_UNSOL_STK_PROACTIVE_COMMAND
		logD("create RIL_UNSOL_STK_PROACTIVE_COMMAND");
		logD("ril_message.parcelsize = %i", ril_message.parcelsize);
		logD("ril_message.serial = %i", ril_message.serial);
		logD("ril_message.event_id = %i", ril_message.event_id);

		ril_message_unsol_stk_procative_command = rilstorage_create_message(&ril_message);

		logD("ril_message_unsol_stk_procative_command->parcelsize = %i", ril_message_unsol_stk_procative_command->parcelsize);
		logD("ril_message_unsol_stk_procative_command->serial = %i", ril_message_unsol_stk_procative_command->serial);
		logD("ril_message_unsol_stk_procative_command->event_id = %i", ril_message_unsol_stk_procative_command->event_id);
	}



	if ((ril_message.serial == 1034) && (ril_message.event_id == 1) && (origin == RIL_MESSAGE_ORIGIN_MODEM)) {	/// unsol_ril_connected
		logD("create ril_message_unsol_ril_connected");
		logD("ril_message.parcelsize = %i", ril_message.parcelsize);
		logD("ril_message.serial = %i", ril_message.serial);
		logD("ril_message.event_id = %i", ril_message.event_id);

		ril_message_unsol_ril_connected = rilstorage_create_message(&ril_message);

		logD("ril_message_unsol_ril_connected->parcelsize = %i", ril_message_unsol_ril_connected->parcelsize);
		logD("ril_message_unsol_ril_connected->serial = %i", ril_message_unsol_ril_connected->serial);
		logD("ril_message_unsol_ril_connected->event_id = %i", ril_message_unsol_ril_connected->event_id);
	}

	if ((ril_message.serial == 1000) && (ril_message.event_id == 1) && (origin == RIL_MESSAGE_ORIGIN_MODEM)) {	/// unsol_response_radio_state_changed
		logD("unsol_response_radio_state_changed");
		if (((char*)ril_message.rildata)[0] == 0x0a) {	//  RADIO_POWER on
			logD("RADIO_POWER on");
			if (ril_message_unsol_radio_state_on == NULL)
				ril_message_unsol_radio_state_on = rilstorage_create_message(&ril_message);
		} else {	// RADIO_POWER off
			logD("RADIO_POWER off");
			if (ril_message_unsol_radio_state_off == NULL)
				ril_message_unsol_radio_state_off = rilstorage_create_message(&ril_message);
		}

	}

	if ((ril_message.event_id == 61)&& (origin != RIL_MESSAGE_ORIGIN_MODEM)) {
		logD("filter out : ril_message.event_id == 61 ");	//screen on / off command
		return ril_storage_list;
	}

	if ((ril_message.event_id == 42)&& (origin != RIL_MESSAGE_ORIGIN_MODEM)) {
		logD("RIL_REQUEST_QUERY_FACILITY_LOCK ");
	}

	if ((ril_message.event_id == 28)&& (origin != RIL_MESSAGE_ORIGIN_MODEM)) {
		logD("RIL_REQUEST_SIM_IO 28");
	}

	if ((ril_message.event_id == 27)&& (origin != RIL_MESSAGE_ORIGIN_MODEM)) {
		logD("RIL_REQUEST_SETUP_DATA_CALL 27");
	}

	// RIL_REQUEST_GET_SIM_STATUS
	if ((ril_message.event_id == 1)&& (origin != RIL_MESSAGE_ORIGIN_MODEM)) {
		logD("RIL_REQUEST_GET_SIM_STATUS 1");
	}

	ril_message.timestamp = rilstorage_get_ms_time();

	ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, origin);

	return ril_storage_list;
}

static char *
RIL_create_parcel(ril_message_t * ril_message)
{
	char *nparcel = NULL;

	//char *nsize  = (char *) &ril_message->parcelsize ;
	nparcel = mem_new0(char, ril_message->parcelsize);
	ASSERT(nparcel);

	memcpy(nparcel, &ril_message->event_id, sizeof(uint32_t));
	memcpy(nparcel + HEADER_PARAM_FIELD_SIZE/2, &ril_message->serial, sizeof(uint32_t));

	if (ril_message->parcelsize > HEADER_PARAM_FIELD_SIZE) {
		int size = ril_message->parcelsize - HEADER_PARAM_FIELD_SIZE;
		memcpy(nparcel + HEADER_PARAM_FIELD_SIZE, ril_message->rildata, size);
	}
	return nparcel;
}

static ril_storage_t *
RIL_Emulator(list_t * ril_storage_list, char *rilcommand, size_t cmd_len, ril_message_origin_t origin)
{
	ril_storage_t *emu = NULL;
	ril_message_t ril_message;
	unsigned int *idata = NULL;
	ril_message.radio_state = 1;
	ril_message.sim_state = 1;

	logD("rilstorage_emulate");

	//ril_message.parcelsize = ((unsigned int)*(rilcommand + 2) << 8) + (unsigned int)*(rilcommand + 3);
	ril_message.parcelsize = cmd_len;

	if (origin == RIL_MESSAGE_ORIGIN_MODEM) {
		idata = (unsigned int *)(rilcommand);
		ril_message.serial = *idata;
		idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
		ril_message.event_id = *idata;
	} else {
		idata = (unsigned int *)(rilcommand);
		ril_message.event_id = *idata;
		idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
		ril_message.serial = *idata;
	}

	if (ril_message.parcelsize > HEADER_PARAM_FIELD_SIZE)
		ril_message.rildata = (rilcommand + HEADER_PARAM_FIELD_SIZE);
	else
		ril_message.rildata = NULL;

	emu = rilstorage_emulate(ril_storage_list, &ril_message);

	if (!emu) {
		logD("emu == NULL");
		return NULL;
	}

	if (emu->ril_sol_request != NULL) {
		logD(" emu SOL Request: ID: %i Serial:%i",
		     emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
	} else{
		logD("error No  SOL Request found");
	}

	if (emu->ril_sol_answer != NULL) {
		logD(" emu SOL Answer: ID: %i Serial: %i Size: %i radio: %i sim: %i", emu->ril_sol_answer->event_id,
		     emu->ril_sol_answer->serial, emu->ril_sol_answer->parcelsize, emu->ril_sol_answer->radio_state,
		     emu->ril_sol_answer->sim_state);

		if (emu->ril_sol_answer->rildata != NULL) {
			logD("emu->ril_sol_answer->rildata != NULL");

			dumpRILData(emu->ril_sol_answer->rildata, emu->ril_sol_answer->parcelsize - HEADER_PARAM_FIELD_SIZE);
		}
	} else{
		logD("error No SOL Answer: found");
	}

	if (ril_message.event_id == 23) {	//RADIO_Power

		if (((char *)ril_message.rildata)[4] == 0x01) {	//  RADIO_POWER on
			if (ril_message_unsol_radio_state_on != NULL){
				logD("unsol RADIO_POWER on emulated");
				emu->ril_unsol = rilstorage_create_message(ril_message_unsol_radio_state_on);
			}
		} else {	// RADIO_POWER off
			if (ril_message_unsol_radio_state_off != NULL){
				logD("unsil RADIO_POWER off emulated");
				emu->ril_unsol = rilstorage_create_message(ril_message_unsol_radio_state_off);
			}
		}

	}

	// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
	if (ril_message_unsol_stk_procative_command != NULL){
		if (ril_message.event_id == 103) {	// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING
			logD("add ril_message_unsol_stk_procative_command for RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103 ");
			emu->ril_unsol = rilstorage_create_message(ril_message_unsol_stk_procative_command);
		}
	}

	// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
	if (ril_message_unsol_stk_session_end != NULL) {
		if (ril_message.event_id == 70) {	// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE
			logD("add ril_message_unsol_stk_session_end for RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70 ");
			emu->ril_unsol = rilstorage_create_message(ril_message_unsol_stk_session_end);
		}
	}
#if 0 //TODO remove this section  after more testing
	// RIL_REQUEST_GET_SIM_STATUS
	if (ril_message.event_id == 1) {	// RIL_REQUEST_GET_SIM_STATUS
		logD("emulate RIL_REQUEST_GET_SIM_STATUS 1 ");
		if (emu->ril_sol_answer != NULL) {
			if ((emu->ril_sol_answer->parcelsize> 136) && (emu->ril_sol_answer->rildata != NULL)) {

				* ((char*)emu->ril_sol_answer->rildata + 124) = 2;
				//logD("changed SIM STATE %02x to RIL_PINSTATE_ENABLED_VERIFIED = 2",(int) pinstate);

				//* ((char*)emu->ril_sol_answer->rildata + 124) = 3;
				//logD("changed SIM STATE %02x to RIL_PINSTATE_DISABLED = 3",(int) pinstate);
				dumpRILData(emu->ril_sol_answer->rildata, emu->ril_sol_answer->parcelsize - HEADER_PARAM_FIELD_SIZE);
				}
			}


	}

	// RIL_REQUEST_QUERY_FACILITY_LOCK
	if (ril_message.event_id == 42) {	// RIL_REQUEST_QUERY_FACILITY_LOCK
		logD("emulate RIL_REQUEST_QUERY_FACILITY_LOCK 42 ");
		if (emu->ril_sol_answer != NULL) {
			if (emu->ril_sol_answer->rildata != NULL) {
				//char state = (*((char*)(emu->ril_sol_answer->rildata) + 8));
				* ((char*)emu->ril_sol_answer->rildata + 8) = 0;
				//logD("changed STATE %02x to 0 for unlock",(int) state);
				dumpRILData(emu->ril_sol_answer->rildata, emu->ril_sol_answer->parcelsize - HEADER_PARAM_FIELD_SIZE);
				}
		}
	}
#endif
	// RIL_REQUEST_SIM_OPEN_CHANNEL 115
	if (ril_message.event_id == 115) {	// RIL_REQUEST_SIM_OPEN_CHANNEL 115
		logD("emulate RIL_REQUEST_SIM_OPEN_CHANNEL 115");
	}

	// RIL_REQUEST_SIM_IO 28
	if (ril_message.event_id == 28) {	//  RIL_REQUEST_SIM_IO 28
		logD("emulate RIL_REQUEST_SIM_IO 28");
	}

	// RIL_REQUEST_SETUP_DATA_CALL 27
	if (ril_message.event_id == 27) {
		logD("emulate RIL_REQUEST_SETUP_DATA_CALL 27");
	}


	logD("rilstorage_emulate finished");

	return emu;
}

static int
rilproxy_create_socket(const char *path)
{
	int sock = 0;
	int err = -1;
	struct sockaddr_un server;
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == sock) {
		WARN_ERRNO("Failed to create UNIX socket.");
		goto err_socket;
	}

	err = unlink(path);

	memset(&server, 0, sizeof(server));
	server.sun_family = AF_UNIX;
	snprintf(server.sun_path, UNIX_PATH_MAX, "%s", path);
	err = bind(sock, (struct sockaddr *)&server, sizeof(server));
	if (err) {
		logD("couldn't bind socket: %s", strerror(errno));
		goto err_socket;
	}

	err = listen(sock, 5);
	if (err) {
		logD("couldn't listen: %s", strerror(errno));
		goto err_socket;
	}

	return sock;

err_socket:
	close(sock);
	return 0;
}

static int
getnextRILCommand(RecordStream * param, void **vrilcommand)
{
	size_t recordlen = 0;
	int ret;
	if (param == NULL) {
		logE("param == NULL");
		return -2 ;
	}

	logD("+record_stream_get_next:%p:", (void *)param);
	ret = record_stream_get_next(param, vrilcommand, &recordlen);
	logD("-record_stream_get_next:%p: ret=%i recordlen=%i",
	     (void *)param, ret, (int)recordlen);

	if (ret == 0) {
		if (*vrilcommand != NULL)
			return (int)recordlen;
		else
			return 0; // no more data to read
	}
	logD("record_stream_get_next: ret=%i (errno=%d)", ret, errno);
	if (errno == EAGAIN)
		return 0; // more data to follow

	return -1;
}

static void
logRilMessage(char *rilcommand, size_t cmd_len, ril_message_origin_t origin)
{
	unsigned int *idata = NULL;
	ril_message_raw_t rilraw;

	//rilraw.parcelsize = ((unsigned int)(*(rilcommand + 2)) << 8) + ((unsigned int)(*(rilcommand + 3)));
	rilraw.parcelsize = cmd_len;

	if (origin == RIL_MESSAGE_ORIGIN_MODEM) {
		idata = (unsigned int *)(rilcommand);
		rilraw.serial = *idata;

		idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
		rilraw.event_id = *idata;
	} else {
		idata = (unsigned int *)(rilcommand);
		rilraw.event_id = *idata;

		idata = (unsigned int *)(rilcommand + HEADER_PARAM_FIELD_SIZE/2);
		rilraw.serial = *idata;
	}

	logD("parcelsize: %u event_id: %u serial: %u ", rilraw.parcelsize, rilraw.event_id, rilraw.serial);

}

static list_t *
rilproxy_init_rilstorage_list(list_t *ril_storage_list)
{
	logD("add default emulated messages to ril storage");

	dumpRILData(ril_request_deactivate_data_call, sizeof(ril_request_deactivate_data_call));
	dumpRILData(ril_answer_deactivate_data_call, sizeof(ril_answer_deactivate_data_call));

	logRilMessage(ril_request_deactivate_data_call, sizeof(ril_request_deactivate_data_call), RIL_MESSAGE_ORIGIN_A0);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_request_deactivate_data_call, sizeof(ril_request_deactivate_data_call), RIL_MESSAGE_ORIGIN_A0);

	logRilMessage(ril_answer_deactivate_data_call, sizeof(ril_answer_deactivate_data_call), RIL_MESSAGE_ORIGIN_MODEM);
	ril_storage_list = RIL_Storage(ril_storage_list, 
		ril_answer_deactivate_data_call, sizeof(ril_answer_deactivate_data_call), RIL_MESSAGE_ORIGIN_MODEM);

	dumpRILData(ril_request_deactivate_data_call1, sizeof(ril_request_deactivate_data_call1));
	dumpRILData(ril_answer_deactivate_data_call1, sizeof(ril_answer_deactivate_data_call1));

	logRilMessage(ril_request_deactivate_data_call1, sizeof(ril_request_deactivate_data_call1), RIL_MESSAGE_ORIGIN_A0);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_request_deactivate_data_call1, sizeof(ril_request_deactivate_data_call1), RIL_MESSAGE_ORIGIN_A0);

	logRilMessage(ril_answer_deactivate_data_call1, sizeof(ril_answer_deactivate_data_call1), RIL_MESSAGE_ORIGIN_MODEM);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_answer_deactivate_data_call1, sizeof(ril_answer_deactivate_data_call1), RIL_MESSAGE_ORIGIN_MODEM);

	dumpRILData(ril_request_radio_off, sizeof(ril_request_radio_off));
	dumpRILData(ril_answer_radio_off, sizeof(ril_answer_radio_off));

	logRilMessage(ril_request_radio_off, sizeof(ril_request_radio_off), RIL_MESSAGE_ORIGIN_A0);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_request_radio_off, sizeof(ril_request_radio_off), RIL_MESSAGE_ORIGIN_A0);

	logRilMessage(ril_answer_radio_off, sizeof(ril_answer_radio_off), RIL_MESSAGE_ORIGIN_MODEM);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_answer_radio_off, sizeof(ril_answer_radio_off), RIL_MESSAGE_ORIGIN_MODEM);

	dumpRILData(ril_request_radio_on, sizeof(ril_request_radio_on));
	dumpRILData(ril_answer_radio_on, sizeof(ril_answer_radio_on));

	logRilMessage(ril_request_radio_on, sizeof(ril_request_radio_on), RIL_MESSAGE_ORIGIN_A0);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_request_radio_on, sizeof(ril_request_radio_on), RIL_MESSAGE_ORIGIN_A0);

	logRilMessage(ril_answer_radio_on, sizeof(ril_answer_radio_on), RIL_MESSAGE_ORIGIN_MODEM);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_answer_radio_on, sizeof(ril_answer_radio_on), RIL_MESSAGE_ORIGIN_MODEM);

	ril_message_unsol_radio_state_on = RIL_create_message(ril_unsol_radio_on, sizeof(ril_unsol_radio_on), RIL_MESSAGE_ORIGIN_A0);
	ril_message_unsol_radio_state_off = RIL_create_message(ril_unsol_radio_off, sizeof(ril_unsol_radio_off), RIL_MESSAGE_ORIGIN_A0);


	dumpRILData(ril_request_setup_data_call, sizeof(ril_request_setup_data_call));
	dumpRILData(ril_answer_setup_data_call_failed, sizeof(ril_answer_setup_data_call_failed));

	logRilMessage(ril_request_setup_data_call, sizeof(ril_request_setup_data_call), RIL_MESSAGE_ORIGIN_A0);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_request_setup_data_call, sizeof(ril_request_setup_data_call), RIL_MESSAGE_ORIGIN_A0);

	logRilMessage(ril_answer_setup_data_call_failed, sizeof(ril_answer_setup_data_call_failed), RIL_MESSAGE_ORIGIN_MODEM);
	ril_storage_list = RIL_Storage(ril_storage_list,
		ril_answer_setup_data_call_failed, sizeof(ril_answer_setup_data_call_failed), RIL_MESSAGE_ORIGIN_MODEM);


	return ril_storage_list;
}

static int pollfds_count = 2;
static int pollfds_max = 8;

static struct pollfd*
get_next_free_pollfd(struct pollfd pollfds[])
{
	int i;
	if (pollfds_count < pollfds_max) {
		return &pollfds[pollfds_count++];
	}

	// pollfd[0] and [1] are reserved for accapt sockets and handled in main
	for (i=2; i < pollfds_max; i++) {
		if (pollfds[i].fd == -1) {
			memset(&pollfds[i], 0, sizeof(struct pollfd));
			break;
		}
	}

	return (i < pollfds_max) ? &pollfds[i] : NULL;
}

static int
rilproxy_handle_accept_and_connect_new(int client_sock, struct pollfd pollfds[],
	ril_message_origin_t origin, bool emul_unsol_connect)
{
	if (client_sock < 0) {
		logE("Error on accept() errno:%d", errno);
		return -1;
	}

	struct pollfd *fd_con = get_next_free_pollfd(pollfds);
	if (!fd_con) {
		logE("No free pollfd!");
		return -1;
	}

	ril_socket_t *ril_client_sock = mem_new(ril_socket_t, 1);
	ril_client_sock->origin = origin;
	ril_client_sock->fd = client_sock;

	fd_con->fd = client_sock;
	fd_con->events = POLLIN;
	fd_con->revents = 0;

	ril_client_sock->fd_connector = fd_con;

	int ret = fcntl(client_sock, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		logE("Error setting O_NONBLOCK errno:%d", errno);
	}

	ril_client_sock->rs = record_stream_new(client_sock, MAX_COMMAND_BYTES);
	logD("record_stream_new:%p: ril client socket fd=%d max-bytes=%d",
	     (void *)ril_client_sock->rs, ril_client_sock->fd, MAX_COMMAND_BYTES);

	ril_socket_list = list_append(ril_socket_list, ril_client_sock);

	if (emul_unsol_connect) {
		char *rilcommand_raw = NULL;

		usleep(100000);
		usleep(100000);

		logD("send unsol_ril_connected ");
		rilcommand_raw = RIL_create_parcel(ril_message_unsol_ril_connected);
		dumpRILData(rilcommand_raw, ril_message_unsol_ril_connected->parcelsize);
		rilproxy_send_parcel(client_sock, rilcommand_raw, ril_message_unsol_ril_connected->parcelsize);
		//mem_free(ril_message_unsol_ril_connected);
		mem_free(rilcommand_raw);
	}

	ril_client_sock->connected = 1;

	return 0;
}

static int
rilproxy_send_response_emulated(char* rilcommand, size_t cmd_len, ril_socket_t *client)
{
	int ret = 0;
	char* rilcommand_resp = NULL;

	ril_storage_t *emulated_rilstorage = RIL_Emulator(ril_global_storage_list, rilcommand, cmd_len, client->origin);

	if (emulated_rilstorage == NULL)
		return ret;

	if (emulated_rilstorage->ril_sol_answer != NULL) {
		rilcommand_resp = RIL_create_parcel(emulated_rilstorage->ril_sol_answer);
		dumpRILData(rilcommand_resp, emulated_rilstorage->ril_sol_answer->parcelsize);

		usleep(1000);

		ret = rilproxy_send_parcel(client->fd, rilcommand_resp,
		      emulated_rilstorage->ril_sol_answer->parcelsize);

		logD("emulated ril_sol");
		mem_free(rilcommand_resp);

		if (emulated_rilstorage->ril_unsol != NULL) {
			logD("if (emulated_rilstorage->ril_unsol != NULL)");
			rilcommand_resp = RIL_create_parcel(emulated_rilstorage->ril_unsol);
			dumpRILData(rilcommand_resp, emulated_rilstorage->ril_unsol->parcelsize);

			ret = rilproxy_send_parcel(client->fd, rilcommand_resp,
				emulated_rilstorage->ril_unsol->parcelsize);

			logD("emulated ril_unsol");
			mem_free(rilcommand_resp);
		}
	}
	rilstorage_ril_storage_el_free(emulated_rilstorage);

	return ret;
}

/*----------------------------------------------------------------------------------------------------------------*/

static void
rilproxy_handle_raw_msg_from_a0(ril_socket_t *client, int fd_rild)
{
	void *vrilcommand = NULL;
	int cmd_len;
	while ((cmd_len = getnextRILCommand(client->rs, &vrilcommand)) > 0) {
		char *rilcommand = (char *)vrilcommand;

		logD(" rilj a0 received message: (%d bytes)\n", cmd_len);
		//dumpRILParcel(rilcommand);
		dumpRILData(rilcommand, cmd_len);
		logRilMessage(rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_A0);

		ril_global_storage_list = RIL_Storage(ril_global_storage_list, rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_A0);

		ril_message_t *ril_message = RIL_create_message(rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_A0);
		if (ril_message) {
			if (ril_message->event_id == RIL_REQUEST_SETUP_DATA_CALL) {
				logD("received data call request from a0, storing serial=%d", ril_message->serial);
				wait_for_data_call_response = ril_message->serial;
			}
			mem_free(ril_message);
		}

		if (-1 == rilproxy_send_parcel(fd_rild, rilcommand, cmd_len)) {
			logD("Failed to send to rilproxy socket...");
		}
	}
}

static void
rilproxy_handle_raw_msg_from_modem(ril_socket_t *client, int rilproxy_a0)
{
	void *vrilcommand = NULL;
	int cmd_len = 0;

	while ((cmd_len = getnextRILCommand(client->rs, &vrilcommand)) > 0) {
		char *rilcommand = (char *)vrilcommand;
		char *ril_emulated_command = NULL;
		bool sendto_a0 = true;
		unsigned int filter_message_id = 0;

		logD("rild received message: (%d bytes)\n", cmd_len);
		dumpRILData(rilcommand, cmd_len);
		logRilMessage(rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_MODEM);

		ril_global_storage_list = RIL_Storage(ril_global_storage_list, rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_MODEM);

		// unsol message filter
		ril_message_t *ril_message = RIL_create_message(rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_MODEM);

		if (ril_message) {
			if ((ril_message->event_id == 1003) ||	// RIL_UNSOL_RESPONSE_NEW_SMS
			    (ril_message->event_id == 1018) ||	// RIL_UNSOL_CALL_RING 1018
			    (ril_message->event_id == 1001) 	// RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED 1001
			    ) {
				// send messages to telephony container only
				for (list_t *l = ril_socket_list; l; l = l->next) {
					ril_socket_t *client_telephony = l->data;
					if (client_telephony->origin != RIL_MESSAGE_ORIGIN_A1)
						continue;
					logD("RIL_UNSOL_RESPONSE  -> AX (telephony)");
					rilproxy_send_parcel(client_telephony->fd, rilcommand, cmd_len);
				}
				mem_free(ril_message);
				continue;
			}
			if ((ril_message->event_id == 1002) ||	// RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED
			    (ril_message->event_id == 1010) ||	// RIL_UNSOL_DATA_CALL_LIST_CHANGED
			    (ril_message->event_id == 1035) ||	// RIL_UNSOL_VOICE_RADIO_TECH_CHANGED 1035
			    (ril_message->event_id == 1009) ||	// RIL_UNSOL_SIGNAL_STRENGTH  1009
			    (ril_message->event_id == 1037) ||	// RIL_UNSOL_RESPONSE_DATA_NETWORK_STATE_CHANGED 1037
			    (ril_message->event_id == 1008) ||	// RIL_UNSOL_NITZ_TIME_RECEIVED  1008
			    (ril_message->event_id == 1036) ||	// RIL_UNSOL_CELL_INFO_LIST 1036
			    (ril_message->event_id == 1023)	// RIL_UNSOL_RESTRICTED_STATE_CHANGED 1023
			    ) {
				// send messages to a0 and aX
				for (list_t *l = ril_socket_list; l; l = l->next) {
					ril_socket_t *client_data = l->data;
					if (client_data->origin == RIL_MESSAGE_ORIGIN_MODEM)
						continue;
					rilproxy_send_parcel(client_data->fd, rilcommand, cmd_len);
				}
				mem_free(ril_message);
				continue;
			}
			mem_free(ril_message);
		}

		// sol responses
		ril_message = RIL_create_message(rilcommand, cmd_len, RIL_MESSAGE_ORIGIN_A0);
		if (ril_message) {
			// received response to data call
			if ((ril_message->event_id == 0) && (ril_message->serial == wait_for_data_call_response)) {
				logD("Received response to data call");
				wait_for_data_call_response = 0;

				// forward to a0
				rilproxy_send_parcel(rilproxy_a0, rilcommand, cmd_len);
				// answer to pending setup data calls of aX containers
				for (list_t *l = client_with_pending_commands_list; l; l = l->next) {
					ril_socket_t *client = l->data;
					rilproxy_handle_raw_msg_from_aX(client, client->fd);
				}
				list_delete(client_with_pending_commands_list);
				client_with_pending_commands_list = NULL;
				mem_free(ril_message);
				continue;
			}

			filter_message_id = rilstorage_fifo_get_message_id_from_serial(ril_a1_filter_list, ril_message);
			logD("a1: filter_message_id: %i", filter_message_id);

			if (filter_message_id != 0) {
				// send sol responses to telephony container only
				for (list_t *l = ril_socket_list; l; l = l->next) {
					ril_socket_t *client_telephony = l->data;
					if (client_telephony->origin != RIL_MESSAGE_ORIGIN_A1)
						continue;
					logD("RIL Filter  -> A%d", client_telephony->origin);
					rilproxy_send_parcel(client_telephony->fd, rilcommand, cmd_len);
				}
				sendto_a0 = false;
			}

			filter_message_id = rilstorage_fifo_get_message_id_from_serial(ril_a2_filter_list, ril_message);
			logD("a2: filter_message_id: %i", filter_message_id);

			if (filter_message_id != 0) {
				// send sol responses to non-telephony containers only
				for (list_t *l = ril_socket_list; l; l = l->next) {
					ril_socket_t *client_data = l->data;
					if (client_data->origin != RIL_MESSAGE_ORIGIN_A2)
						continue;
					logD("RIL Filter  -> A%d", client_data->origin);
					rilproxy_send_parcel(client_data->fd, rilcommand, cmd_len);
				}
				sendto_a0 = false;
			}
			mem_free(ril_message);
		}

		if (sendto_a0)
			rilproxy_send_parcel(rilproxy_a0, rilcommand, cmd_len);
	}
}

static bool
rilproxy_message_is_allowed(ril_message_t *ril_message, ril_message_origin_t origin)
{
	switch (origin) {

		case RIL_MESSAGE_ORIGIN_A0:
		case RIL_MESSAGE_ORIGIN_MODEM:
			return true;
		case RIL_MESSAGE_ORIGIN_A1:
			return (
				(ril_message->event_id == 25) ||	// RIL_REQUEST_SEND_SMS 25
				(ril_message->event_id ==  9) ||	// RIL_REQUEST_GET_CURRENT_CALLS 9
				(ril_message->event_id == 37) ||	// RIL_REQUEST_SMS_ACKNOWLEDGE  37
				(ril_message->event_id == 53) ||	// RIL_REQUEST_SET_MUTE 53
				(ril_message->event_id == 54) ||	// RIL_REQUEST_GET_MUTE 54
				(ril_message->event_id == 10) ||	// RIL_REQUEST_DIAL 10
				(ril_message->event_id == 14) ||	// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
				(ril_message->event_id == 56) ||	// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
				(ril_message->event_id == 18) ||	// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
				(ril_message->event_id == 12) ||	// RIL_REQUEST_HANGUP 12
				(ril_message->event_id == 13) ||	// HANGUP_WAITING_OR_BACKGROUND 13
				(ril_message->event_id == 15) ||	// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
				(ril_message->event_id == 24) ||	// RIL_REQUEST_DTMF 24
				(ril_message->event_id == 17) ||	// RIL_REQUEST_UDUB 17
				(ril_message->event_id == 26) ||	// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
				(ril_message->event_id == 16) ||	// RIL_REQUEST_CONFERENCE 16
				(ril_message->event_id == 61) ||    // RIL_REQUEST_SCREEN_STATE 61
				//(ril_message->event_id == 22) ||    // RIL_REQUEST_OPERATOR 22
				(ril_message->event_id == 31) ||    // RIL_REQUEST_GET_CLIR 31
				(ril_message->event_id == 32) ||    // RIL_REQUEST_SET_CLIR 32
				(ril_message->event_id == 33) ||    // RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
				(ril_message->event_id == 34) ||    // RIL_REQUEST_SET_CALL_FORWARD 34
				(ril_message->event_id == 35) ||    // RIL_REQUEST_QUERY_CALL_WAITING 35
				(ril_message->event_id == 36) ||    // RIL_REQUEST_SET_CALL_WAITING 36
				(ril_message->event_id == 37) ||    // RIL_REQUEST_SMS_ACKNOWLEDGE  37
				(ril_message->event_id == 49) ||    // RIL_REQUEST_DTMF_START 49
				(ril_message->event_id == 50) ||    // RIL_REQUEST_DTMF_STOP 50
				(ril_message->event_id == 55) ||    // RIL_REQUEST_QUERY_CLIP 55
				(ril_message->event_id == 50) ||    // RIL_REQUEST_DTMF_STOP 50
				(ril_message->event_id == 50) ||    // RIL_REQUEST_DTMF_STOP 50
				(ril_message->event_id == 40) ||	// RIL_REQUEST_ANSWER 40
				(ril_message->event_id == 109) ||	// RIL_REQUEST_GET_CELL_INFO_LIST 109
				(ril_message->event_id == 75) ||	// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
				//((ril_message->event_id == 28) && (memcmp (rilcommand+24,ril_sim_io_read_sim_command,23) == 0))	//RIL_REQUEST_SIM_IO 28
				((ril_message->event_id == 28) )	//RIL_REQUEST_SIM_IO 28
			);
		case RIL_MESSAGE_ORIGIN_A2:
			return (
				(ril_message->event_id == 61)  ||	// RIL_REQUEST_SCREEN_STATE 61
				(ril_message->event_id == 109) ||	// RIL_REQUEST_GET_CELL_INFO_LIST 109
				(ril_message->event_id == 75) ||	// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
				//(ril_message->event_id == 22)  ||       // RIL_REQUEST_OPERATOR 22
				(ril_message->event_id == 28) 	//RIL_REQUEST_SIM_IO 28
#if 0
				(ril_message->event_id ==  9) ||	// RIL_REQUEST_GET_CURRENT_CALLS 9
				(ril_message->event_id == 37) ||	// RIL_REQUEST_SMS_ACKNOWLEDGE  37
				(ril_message->event_id == 53) ||	// RIL_REQUEST_SET_MUTE 53
				(ril_message->event_id == 54) ||	// RIL_REQUEST_GET_MUTE 54
				(ril_message->event_id == 10) ||	// RIL_REQUEST_DIAL 10
				(ril_message->event_id == 14) ||	// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
				(ril_message->event_id == 56) ||	// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
				(ril_message->event_id == 18) ||	// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
				(ril_message->event_id == 12) ||	// RIL_REQUEST_HANGUP 12
				(ril_message->event_id == 13) ||	// HANGUP_WAITING_OR_BACKGROUND 13
				(ril_message->event_id == 15) ||	// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
				(ril_message->event_id == 24) ||	// RIL_REQUEST_DTMF 24
				(ril_message->event_id == 17) ||	// RIL_REQUEST_UDUB 17
				(ril_message->event_id == 26) ||	// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
				(ril_message->event_id == 16) ||	// RIL_REQUEST_CONFERENCE 16
				(ril_message->event_id == 25) ||    // RIL_REQUEST_SEND_SMS 25
				(ril_message->event_id == 31) ||    // RIL_REQUEST_GET_CLIR 31
				(ril_message->event_id == 32) ||    // RIL_REQUEST_SET_CLIR 32
				(ril_message->event_id == 33) ||    // RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
				(ril_message->event_id == 34) ||    // RIL_REQUEST_SET_CALL_FORWARD 34
				(ril_message->event_id == 35) ||    // RIL_REQUEST_QUERY_CALL_WAITING 35
				(ril_message->event_id == 36) ||    // RIL_REQUEST_SET_CALL_WAITING 36
				(ril_message->event_id == 37) ||    // RIL_REQUEST_SMS_ACKNOWLEDGE  37
				(ril_message->event_id == 49) ||    // RIL_REQUEST_DTMF_START 49
				(ril_message->event_id == 50) ||    // RIL_REQUEST_DTMF_STOP 50
				(ril_message->event_id == 55) ||    // RIL_REQUEST_QUERY_CLIP 55
				(ril_message->event_id == 50) ||    // RIL_REQUEST_DTMF_STOP 50
				(ril_message->event_id == 50) ||    // RIL_REQUEST_DTMF_STOP 50
				(ril_message->event_id == 40)	// RIL_REQUEST_ANSWER 40
#endif
			);
	}
	return false;
}

static void
rilproxy_handle_raw_msg_from_aX(ril_socket_t *client, int fd_rild)
{
	void *vrilcommand = NULL;
	int cmd_len = 0;

	// don't handle messages for conatiners while waiting for data call response
	if (wait_for_data_call_response > 0) {
		client_with_pending_commands_list = list_append(client_with_pending_commands_list, client);
		return;
	}

	while ((cmd_len = getnextRILCommand(client->rs, &vrilcommand)) > 0) {
		char *rilcommand = (char *)vrilcommand;
		logD(" rilj a%d received message: (%d bytes)\n", client->origin, cmd_len);

		//dumpRILParcel(rilcommand);
		dumpRILData(rilcommand, cmd_len);
		logRilMessage(rilcommand, cmd_len, client->origin);

		ril_message_t *ril_message = RIL_create_message(rilcommand, cmd_len, client->origin);
		if (ril_message == NULL)
			continue;

		// send directly to modem
		if (rilproxy_message_is_allowed(ril_message, client->origin)) {
			if (client->origin == RIL_MESSAGE_ORIGIN_A1) {
				logD(" rilj a1 send  message -> rild");
				ril_a1_filter_list = rilstorage_update_message_fifo(ril_a1_filter_list,
					ril_message, RIL_MESSAGE_ORIGIN_A1);
			} else if (client->origin == RIL_MESSAGE_ORIGIN_A2) {
				logD(" rilj A2 send  message -> rild");
				ril_a2_filter_list = rilstorage_update_message_fifo(ril_a2_filter_list,
					ril_message, RIL_MESSAGE_ORIGIN_A2);
			}

			if (-1 == rilproxy_send_parcel(fd_rild, rilcommand, cmd_len)) {
				logD("Failed to send to rilproxy socket, ignoring...");
			}
			mem_free(ril_message);
			continue;
		}

		// or need emulation
		if (-1 == rilproxy_send_response_emulated(rilcommand, cmd_len, client)) {
			logD("Failed to send to container's socket");
		}
		logD(" rilj a%d msg handled", client->origin);
		mem_free(ril_message);
	}
}


/*----------------------------------------------------------------------------------------------------------------*/

int
main(UNUSED int argc, UNUSED char **argv)
{
	int fd_rild = 0;
	int fd_a0 = 0;

	int fd_data = -1;
	int fd_telephony = -1;

	const char *rild_socket = NULL;
	int connected = 0;
	struct pollfd pollfds[pollfds_max];

	int ril_connector = 0;
	int ril_connector2 = 0;

	unsigned int client_id = 0;

	core_dump_enable();

	logD("create ril storage");

	ril_global_storage_list = rilstorage_new(ril_global_storage_list);
	ASSERT(ril_global_storage_list);
	ril_global_storage_list = rilproxy_init_rilstorage_list(ril_global_storage_list);

	logD("create ril filter a1");

	ril_a1_filter_list = rilstorage_new(ril_a1_filter_list);
	ASSERT(ril_a1_filter_list);

	logD("create ril filter a2");

	ril_a2_filter_list = rilstorage_new(ril_a2_filter_list);
	ASSERT(ril_a2_filter_list);


	userswitch();

	logD("create bind and listen for framwork on socket %s", SOCKET_NAME_RIL_PRIV);
	ril_connector = rilproxy_create_socket(SOCKET_NAME_RIL_PRIV);
	if(fcntl(ril_connector, F_SETFL, O_NONBLOCK) < 0) {
		logE("Error setting O_NONBLOCK errno:%d", errno);
		return 0;
	}
	logD("Waiting on socket for A0");

	pollfds[0].fd = ril_connector;
	pollfds[0].revents = 0;
	pollfds[0].events = POLLIN;
	poll(pollfds, 1, -1);


	fd_a0 = accept(ril_connector, 0, 0);
	if (rilproxy_handle_accept_and_connect_new(fd_a0, pollfds, RIL_MESSAGE_ORIGIN_A0, false) < 0) {
		logE("Error connecting a0");
		return 0;
	}

	// we only accept one client at a0 socket, so close the listening socket
	// we reuse the released pollfds[0] for the aX client listener later
	close(ril_connector);

	rild_socket = socket_name(RILD_SOCKET_NAME, client_id);

	while (true) {
		logD("Connecting to socket %s\n", rild_socket);
		fd_rild = socket_local_client(rild_socket, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
		if (fd_rild >= 0) {
			break;
		}
		logE("Could not connect to %s socket, retrying: %s\n", rild_socket, strerror(errno));

		poll(pollfds, 1, 1000);
		pollfds[0].revents = 0;
	}

	if (rilproxy_handle_accept_and_connect_new(fd_rild, pollfds, RIL_MESSAGE_ORIGIN_MODEM, false) < 0) {
		logE("Error connecting rild");
		return 0;
	}

	logD("rild Socket connected");

	logD("create bind and listen on socket %s", SOCKET_NAME_RIL_1);
	ril_connector = rilproxy_create_socket(SOCKET_NAME_RIL_1);
	if(fcntl(ril_connector, F_SETFL, O_NONBLOCK) < 0) {
		logE("Error setting O_NONBLOCK errno:%d", errno);
		return 0;
	}

	pollfds[0].fd = ril_connector;
	pollfds[0].revents = 0;
	pollfds[0].events = POLLIN;

	logD("create bind and listen on socket %s", SOCKET_NAME_RIL_2);
	ril_connector2 = rilproxy_create_socket(SOCKET_NAME_RIL_2);
	if(fcntl(ril_connector2, F_SETFL, O_NONBLOCK) < 0) {
		logE("Error setting O_NONBLOCK errno:%d", errno);
		return 0;
	}

	pollfds[1].fd = ril_connector2;
	pollfds[1].revents = 0;
	pollfds[1].events = POLLIN;

	logD("Waiting on socket for AX");

	connected = 1;

	int error = 0;

	while (connected) {
		//logD("going to poll on %d pollfds", pollfds_count);
		for(int i=0; i < pollfds_count; ++i) {
			if (pollfds[i].revents ){
				logD("clear pollfds[%d].revents = %d  events on  fd=%d",i,pollfds[i].revents , pollfds[i].fd);
				pollfds[i].revents = 0;
			}
		}

		while (true) {
			logD("poll.start ");

			error = poll(pollfds, pollfds_count, -1);

			logD("poll.return = %i", error);

			if (error > 0)
				break;
		}

		if (pollfds[0].revents & POLLIN) {
			logD("pollfds[0].revents & POLLIN)");
			pollfds[0].revents = 0;
			fd_telephony = accept(ril_connector, 0, 0);
			if (rilproxy_handle_accept_and_connect_new(fd_telephony, pollfds, RIL_MESSAGE_ORIGIN_A1, true) < 0) {
				logE("client A1 failed to connect");
			} else {
				logD("rilproxy telephony Socket A1 connected");
			}
		}
		if (pollfds[1].revents & POLLIN) {
			logD("(pollfds[1].revents & POLLIN)");
			pollfds[1].revents = 0;
			fd_data = accept(ril_connector2, 0, 0);
			if (rilproxy_handle_accept_and_connect_new(fd_data, pollfds, RIL_MESSAGE_ORIGIN_A2, true) < 0) {
				logE("client A2 failed to connect");
			} else {
				logD("rilproxy data-only Socket A2 connected");
			}
		}

		list_t *delete_clients_list = NULL;

		for (list_t *l = ril_socket_list; l; l = l->next) {
			ril_socket_t *client = l->data;

			if ((client->fd_connector->revents & POLLHUP) || (client->fd_connector->revents & POLLERR)){
				logD(" error :client->fd_connector->revents = %d ",client->fd_connector->revents);
				switch (client->origin) {
					case RIL_MESSAGE_ORIGIN_A0:
					case RIL_MESSAGE_ORIGIN_MODEM:
						connected = 0;
						break;
					case RIL_MESSAGE_ORIGIN_A1:
					case RIL_MESSAGE_ORIGIN_A2:
						logD("Disconnecting container!");
						delete_clients_list = list_append(delete_clients_list, client);
						break;
				}
			}

			if (client->fd_connector->revents & POLLIN) {
				client->fd_connector->revents = 0;

				switch (client->origin) {
					case RIL_MESSAGE_ORIGIN_A0:
						logD("client->origin =RIL_MESSAGE_ORIGIN_A0");
						rilproxy_handle_raw_msg_from_a0(client, fd_rild);
						break;
					case RIL_MESSAGE_ORIGIN_MODEM:
						logD("client->origin =RIL_MESSAGE_ORIGIN_MODEM");
						rilproxy_handle_raw_msg_from_modem(client, fd_a0);
						break;
					case RIL_MESSAGE_ORIGIN_A1:
						logD("client->origin =RIL_MESSAGE_ORIGIN_A1");
						rilproxy_handle_raw_msg_from_aX(client, fd_rild);
						break;
					case RIL_MESSAGE_ORIGIN_A2:
						logD("client->origin =RIL_MESSAGE_ORIGIN_A2");
						rilproxy_handle_raw_msg_from_aX(client, fd_rild);
						break;
				}
			}
		}

		// cleanup disconnected clients
		for (list_t *l = delete_clients_list; l; l = l->next) {
			ril_socket_t *client = l->data;
			ril_socket_list = list_remove(ril_socket_list, client);
			client->fd_connector->fd = -1;
			close(client->fd);
			mem_free(client);
		}
		list_delete(delete_clients_list);

	} //while connected

	logE("shutdown rilproxy");
	rilstorage_free(ril_global_storage_list);
	return 0;
}
