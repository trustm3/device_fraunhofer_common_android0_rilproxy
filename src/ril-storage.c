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

/** @file ril-storage.c
 *
 * ril storage implementation
 *
 */

#define  LOGF_LOG_MIN_PRIO LOGF_PRIO_DEBUG
#include "common/list.h"
#include "common/macro.h"

#ifdef DEBUG
#undef DEBUG
#endif


#include "log.h"

#define DEBUG logD

#include "common/logf.h"
#include "common/mem.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "ril-storage.h"

ril_message_t *
rilstorage_create_message(ril_message_t * ril_message)
{
	ril_message_t *nrm = mem_new0(ril_message_t, 1);
	ASSERT(nrm);
	memcpy(nrm, ril_message, sizeof(ril_message_t));

	/* copy payload, if parcel contains data */
	if (ril_message->parcelsize > 8) {
		int size = ril_message->parcelsize - 8;
		char *nrdata = mem_new0(char, size);
		ASSERT(nrdata);
		memcpy(nrdata, ril_message->rildata, size);
		nrm->rildata = nrdata;
	} else
		nrm->rildata = NULL;
	return nrm;
}

unsigned long long
rilstorage_get_ms_time(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return ((time.tv_sec * 1000) + (time.tv_usec / 1000));
}

unsigned int
rilstorage_fifo_get_message_id_from_serial(list_t * ril_storage_list, ril_message_t * ril_message)
{

	list_t *fifo_message_list = NULL;
	ril_message_t *ril_fifo_message = NULL;

	fifo_message_list = list_nth_data(ril_storage_list, 0);
	list_t *l = NULL;

	for (l = list_tail(fifo_message_list); l; l = l->prev) {
		ril_fifo_message = l->data;
		if ((ril_fifo_message ) && (ril_message->serial == ril_fifo_message->serial)) {
			DEBUG
			    ("Message request found in FIFO for answer : ID %i Serial: %i",
			     ril_fifo_message->event_id, ril_fifo_message->serial);
			return ril_fifo_message->event_id;
		}
	}
	DEBUG("Message request not found in FIFO with Serial: %i", ril_message->serial);
	return 0;
}

list_t *
rilstorage_update_message_fifo(list_t * ril_storage_list, ril_message_t * ril_message, ril_message_origin_t origin)
{
	list_t *fifo_message_list = NULL;
	list_t *replaced_list = NULL;
	list_t *deleted_list = NULL;
	list_t *local_ril_storage_list = NULL;
	ril_message_t *rmd = NULL;

	/* only add request message to fifo */
	if (ril_message->event_id == 0) {
		DEBUG("Fifo: RIL_SOL_ANSWER discarded");
		return ril_storage_list;
	}

	if (origin == RIL_MESSAGE_ORIGIN_MODEM){
		if ((ril_message->event_id == 1)  && (ril_message->serial >= RIL_UNSOL_RESPONSE_BASE)) {
			DEBUG("Fifo: RIL_UNSOL discarded");
			return ril_storage_list;
		}
	}

	/* get rilstorage list */
	fifo_message_list = list_nth_data(ril_storage_list, 0);
	replaced_list = list_nth(ril_storage_list, 0);
	local_ril_storage_list = ril_storage_list;

	//DEBUG("enter rilstorgage_create_message");
	ril_message_t *nrm = rilstorage_create_message(ril_message);
	//DEBUG("leave rilstorage_create_message");

	if (fifo_message_list->data == NULL) {
		//DEBUG("list_replace 1");
		fifo_message_list = list_replace(fifo_message_list, fifo_message_list, nrm);
		//DEBUG("list_replace 2");
		local_ril_storage_list = list_replace(ril_storage_list, replaced_list, fifo_message_list);
		DEBUG(" Message FIFO created : ID %i Serial: %i", nrm->event_id, nrm->serial);
		return local_ril_storage_list;
	}
	//DEBUG("list_length");
	unsigned int fifo_length = list_length(fifo_message_list);

	if (fifo_length > MAX_MESSAGE_FIFO_SIZE) {
		DEBUG("fifo_length > MAX_MESSAGE_FIFO_SIZE");
		deleted_list = list_nth_data(local_ril_storage_list, 0);
		if (deleted_list->data != NULL) {
			rmd = deleted_list->data;
			if (rmd != NULL) {
				if (rmd->rildata != NULL)
					mem_free(rmd->rildata);
				mem_free(rmd);
			}
		}
		fifo_message_list = list_unlink(deleted_list, deleted_list);
		DEBUG(" Message deleted from  FIFO ");
	}
	//DEBUG("list_append");
	fifo_message_list = list_append(fifo_message_list, nrm);
	DEBUG(" Message appened into FIFO: ID %i Serial: %i", nrm->event_id, nrm->serial);

	//DEBUG("list_replace");
	local_ril_storage_list = list_replace(ril_storage_list, replaced_list, fifo_message_list);

	return local_ril_storage_list;
}

list_t *
rilstorage_new(list_t * ril_storage_list)
{
	list_t *eventid_list = NULL;

	for (int i = 0; i <= RIL_MESSAGE_MAX + 1; i++) {
		eventid_list = mem_new0(list_t, 1);
		ASSERT(eventid_list);
		ril_storage_list = list_append(ril_storage_list, eventid_list);
	}

	return ril_storage_list;
}

void
rilstorage_ril_storage_el_free(ril_storage_t * ril_storage)
{
	if (!ril_storage)
		return;

	if (ril_storage->ril_sol_request != NULL) {
		if (ril_storage->ril_sol_request->rildata != NULL)
			mem_free(ril_storage->ril_sol_request->rildata);
		mem_free(ril_storage->ril_sol_request);
	}

	if (ril_storage->ril_sol_answer != NULL) {
		if (ril_storage->ril_sol_answer->rildata != NULL)
			mem_free(ril_storage->ril_sol_answer->rildata);
		mem_free(ril_storage->ril_sol_answer);
	}

	if (ril_storage->ril_unsol != NULL) {
		if (ril_storage->ril_unsol->rildata != NULL)
			mem_free(ril_storage->ril_unsol->rildata);
		mem_free(ril_storage->ril_unsol);
	}

	mem_free(ril_storage);
}

void
rilstorage_free(list_t * ril_storage_list)
{
	list_t *fifo_message_list = NULL;
	ril_message_t *ril_message = NULL;

	/* free message fifo */

	fifo_message_list = list_nth_data(ril_storage_list, 0);
	for (list_t * ll = fifo_message_list; ll; ll = ll->next) {
		ril_message = ll->data;
		if (ril_message != NULL)
			if (ril_message->rildata != NULL)
				mem_free(ril_message->rildata);
		mem_free(ril_message);
	}
	list_delete(fifo_message_list);

	// free storage lists */

	list_t *l = NULL;

	for (l = ril_storage_list->next; l; l = l->next) {
		list_t *sl = l->data;
		for (list_t * ll = sl; ll; ll = ll->next) {
			if (ll->data != NULL) {
				rilstorage_ril_storage_el_free(ll->data);
			}
		}
		list_delete(l->data);
	}
	list_delete(ril_storage_list);
}

static bool
rilstorage_same_sim_radio_state(ril_message_t * ril_msg1, ril_message_t * ril_msg2)
{
	unsigned int *check_nullpointer = NULL;

	DEBUG("rilstorage_same_sim_radio_state");
	check_nullpointer = &(ril_msg1->radio_state);
	if (check_nullpointer == NULL){
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ril_msg1->radio_state");
	}
	check_nullpointer = &(ril_msg2->radio_state);
	if (check_nullpointer == NULL){
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ril_msg2->radio_state");
	}
	check_nullpointer = &(ril_msg1->sim_state);
	if (check_nullpointer == NULL){
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ril_msg1->sim_state");
	}
	check_nullpointer = &(ril_msg2->sim_state);
	if (check_nullpointer == NULL){
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ril_msg2->sim_state");
	}
	if (ril_msg1 == NULL) {
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ril_msg1");
		return (FALSE);
	}

	if (ril_msg2 == NULL) {
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ril_msg2");
		return (FALSE);
	}

	if ((&(ril_msg1->radio_state) == NULL) ||
	    (&(ril_msg2->radio_state) == NULL) ||
	    (&(ril_msg1->sim_state) == NULL) || (&(ril_msg2->sim_state) == NULL)) {
		DEBUG("rilstorage_same_sim_radio_state NULL Pointer problem ");
		return (FALSE);
	}

	return ((ril_msg1->radio_state == ril_msg2->radio_state)
		&& (ril_msg1->sim_state == ril_msg2->sim_state));
}

static bool
rilstorage_update_sol_requests(ril_message_t * ril_message, ril_storage_t * ril_storage)
{
	ril_message_t *rm_sol_request = ril_storage->ril_sol_request;

	if ((ril_message == NULL) || (rm_sol_request == NULL)) {
		DEBUG("rilstorage_update_sol_request NULL Pointer ");
		return FALSE;
	}

	if (!rilstorage_same_sim_radio_state(ril_message, rm_sol_request)) {
		DEBUG("!rilstorage_same_sim_radio_state(ril_message, rm_sol_request) ");
		return FALSE;
	}

	if (ril_message->event_id == 27 ){
		rm_sol_request->serial = ril_message->serial;
		DEBUG (" Sol Request Serial and Data updated for SETUP_DATA_CALL : ID: %i Serial:%i", ril_message->event_id, ril_message->serial);
		return TRUE;
	}

	if (rm_sol_request->parcelsize != ril_message->parcelsize) {
		DEBUG("rm_sol_request->parcelsize != ril_message->parcelsize) ");
		return FALSE;
	}

	if (ril_message->parcelsize == 8) {	// message without data
		rm_sol_request->serial = ril_message->serial;
		DEBUG(" SOL Request serial updated : ID: %i Serial:%i", rm_sol_request->event_id, rm_sol_request->serial);
		return TRUE;

	}

	if (ril_message->parcelsize > 8) {	// message with data
		if ((rm_sol_request->rildata != NULL)
		    && (ril_message->rildata != NULL))
			if (memcmp(rm_sol_request->rildata, ril_message->rildata, ril_message->parcelsize - 8) == 0) {
				/* update Request serial */
				rm_sol_request->serial = ril_message->serial;
				DEBUG
				    (" Sol Request Serial and Data updated : ID: %i Serial:%i", ril_message->event_id, ril_message->serial);
				return TRUE;
			}
	} else {
		DEBUG(" Request found : ID: %i Serial:%i", ril_message->event_id, ril_message->serial);
		return TRUE;
	}
	return FALSE;
}

static bool
rilstorage_update_sol_answers(ril_message_t * ril_message, ril_storage_t * ril_storage)
{
	ril_message_t *rm_sol_request = ril_storage->ril_sol_request;
	ril_message_t *rm_sol_answer = ril_storage->ril_sol_answer;

	DEBUG("rilstorage_update_sol_answers");

	if (rm_sol_request == NULL) {
		DEBUG("rilstorage_update_sol_answers NULL Pointer problem: rm_sol_request == NULL");
		return FALSE;
	}

	if (rm_sol_answer != NULL) {
		DEBUG("rm_sol_answer->serial= %i", rm_sol_answer->serial);
		DEBUG("rm_sol_answer->event_id= %i ", rm_sol_answer->event_id);
		//return FALSE;
	}

	if (ril_message == NULL) {
		DEBUG("rilstorage_update_sol_answers NULL Pointer problem: ril_message == NULL ");
		return FALSE;
	}

	DEBUG("ril_message->serial= %i", ril_message->serial);
	DEBUG("ril_message->event_id= %i", ril_message->event_id);
	DEBUG("rm_sol_request->serial= %i", rm_sol_request->serial);
	DEBUG("rm_sol_request->event_id= %i ", rm_sol_request->event_id);

	if (!rilstorage_same_sim_radio_state(ril_message, rm_sol_request)) {
		DEBUG("!rilstorage_same_sim_radio_state(ril_message, rm_sol_request) ");
		return FALSE;
	}
	if (ril_message->serial > rm_sol_request->serial) {
		DEBUG
		    ("ril_message->serial= %i  >  rm_sol_request->serial= %i ",
		     ril_message->serial, rm_sol_request->serial);
		return FALSE;
	}

	if (ril_storage->ril_sol_answer == NULL) {
		ril_storage->ril_sol_answer = rilstorage_create_message(ril_message);
		DEBUG("Sol Answer added: ID: %i Serial:%i ", ril_message->event_id, ril_message->serial);
		return TRUE;
	}

	if (ril_message->parcelsize == 8) {	/* message without data */
		rm_sol_answer->serial = ril_message->serial;
		DEBUG("Sol Answer serial updated: ID: %i Serial:%i ", ril_message->event_id, ril_message->serial);
		return TRUE;
	
	}

	if (ril_message->parcelsize > 8) {	/* message with data */
		if (memcmp(rm_sol_answer->rildata, ril_message->rildata, ril_message->parcelsize - 8) != 0) {
			/* delete old answer and add new answer */
			mem_free(rm_sol_answer);
			ril_storage->ril_sol_answer = rilstorage_create_message(ril_message);
			DEBUG(" Sol Answer serial and Data  updated: ID: %i Serial:%i ", ril_message->event_id, ril_message->serial);
			return TRUE;
		}
	}
	DEBUG("Sol Answer found: ID: %i Serial:%i ", ril_message->event_id, ril_message->serial);
	return TRUE;
}

static bool
rilstorage_update_unsol_messages(ril_message_t * ril_message, ril_storage_t * ril_storage)
{
	ril_message_t *rm_unsol = ril_storage->ril_unsol;

	if (!rilstorage_same_sim_radio_state(ril_message, rm_unsol)){
		DEBUG("!rilstorage_same_sim_radio_state(ril_message, rm_sol_request) ");
		return FALSE;
	}

	if (ril_message->parcelsize > 8) {	/* unsol message with data */
		if (memcmp(rm_unsol->rildata, ril_message->rildata, ril_message->parcelsize - 8) != 0) {
			/* delete old unsol and add new */
			mem_free(rm_unsol);
			ril_storage->ril_unsol = rilstorage_create_message(ril_message);
			DEBUG(" Unsol updated: ID: %i Serial:%i ", ril_message->event_id, ril_message->serial);
			return TRUE;
		}
	}

	/* nothing to do because of exact same message already in storage */
	DEBUG(" unsol found: ID: %i Serial:%i ", ril_message->event_id, ril_message->serial);
	return TRUE;
}

list_t *
rilstorage_update(list_t * ril_storage_list, ril_message_t * ril_message, ril_message_origin_t origin)
{

	list_t *local_ril_storage_list = NULL;
	list_t *l = NULL;
	list_t *replaced_list = NULL;

	bool ril_message_found = FALSE;

	uint32_t ril_message_type = RIL_SOL_REQUEST;
	uint32_t offset = 0;

	ril_storage_t *ril_storage = NULL;

	DEBUG("rilstorage_update begin");
	DEBUG("ril_message->serial= %i", ril_message->serial);
	DEBUG("ril_message->event_id= %i", ril_message->event_id);
	DEBUG("origin = %i", origin);

	if (origin == RIL_MESSAGE_ORIGIN_MODEM) {
		if (ril_message->serial >= RIL_UNSOL_RESPONSE_BASE) {
			if (ril_message->event_id == 1) {
				ril_message_type = RIL_UNSOL;
				offset = ril_message->serial;
				offset = offset - RIL_UNSOL_RESPONSE_BASE + RIL_MESSAGE_OFFSET + 1 ;
				DEBUG("Unsol Message ");
				//return ril_storage_list;
			}
		}

		if (ril_message->event_id == 0) {
			ril_message_type = RIL_SOL_ANSWER;
			//if (ril_message->serial >= 1) {
				DEBUG("fifo_get_message_id_from_serial");
				offset = rilstorage_fifo_get_message_id_from_serial(ril_storage_list, ril_message);
				if (offset == 0) {
					return ril_storage_list;
				}
			//}
		}

	} else {
		
		if (ril_message->event_id >= 1) {
			offset = ril_message->event_id;
		}
	}

	DEBUG("rilstorage_update_message_fifo");
	ril_storage_list = rilstorage_update_message_fifo(ril_storage_list, ril_message, origin);

	DEBUG("offset = %i", offset);
	/* get rilstorage list */
	local_ril_storage_list = list_nth_data(ril_storage_list, offset);

	ASSERT(local_ril_storage_list);

	/* check for message update */

	uint32_t ll = 1;

	for (l = local_ril_storage_list; l; l = l->next) {
		ril_storage = l->data;
		DEBUG("for (l = local_ril_storage_list; l; l = l->next) -> count: %i",ll);
		ll++;
		if (ril_storage == NULL) {	/* first time usage */
#if 0
			if (ril_message->event_id < RIL_UNSOL_RESPONSE_BASE) {
				replaced_list = list_nth(ril_storage_list, ril_message->event_id);
				DEBUG("replaced_list = list_nth(ril_storage_list, ril_message->event_id)");
			} else {
#endif
			replaced_list = list_nth(ril_storage_list, offset);
			DEBUG(" replaced_list = list_nth(ril_storage_list, offset");
//			}
			break;
		}

		switch (ril_message_type) {
		case RIL_SOL_REQUEST:
			DEBUG("rilstorage_update_sol_request");
			ril_message_found = rilstorage_update_sol_requests(ril_message, ril_storage);
			break;

		case RIL_SOL_ANSWER:
			DEBUG("rilstorage_update_sol_answer");
			ril_message_found = rilstorage_update_sol_answers(ril_message, ril_storage);
			break;

		case RIL_UNSOL:
			DEBUG("rilstorage_update_unsol_messages");
			ril_message_found = rilstorage_update_unsol_messages(ril_message, ril_storage);
			break;
		}
		if (ril_message_found == true)
			break;
	}

	if ((ril_message_found == FALSE) && (ril_message_type != RIL_SOL_ANSWER)) {
		// create or append new message to tree
		ril_storage_t *nrs = mem_new0(ril_storage_t, 1);
		//ASSERT(nrs);

		ril_message_t *nrm = rilstorage_create_message(ril_message);

		switch (ril_message_type) {
		case RIL_SOL_REQUEST:
			nrs->ril_sol_request = nrm;
			break;
#if 0
		case RIL_SOL_ANSWER:
			nrs->ril_sol_answer = nrm;
			break;
#endif
		case RIL_UNSOL:
			nrs->ril_unsol = nrm;
			break;
		}

		if (ril_storage == NULL) {
			local_ril_storage_list = list_replace(local_ril_storage_list, local_ril_storage_list, nrs);
			ril_storage_list = list_replace(ril_storage_list, replaced_list, local_ril_storage_list);
			DEBUG(" Message created: ID %i Serial: %i", ril_message->event_id, ril_message->serial);

		} else {
			local_ril_storage_list = list_append(local_ril_storage_list, nrs);
			DEBUG(" Message appened: ID %i Serial: %i", ril_message->event_id, ril_message->serial);
		}
	}
	return ril_storage_list;
}

static void
rilstorage_handle_emulate(ril_storage_t * ril_storage_emulate,
			  ril_message_t * ril_message, ril_message_t * rm_sol_answer)
{
	ril_message_t *nrmrequest = rilstorage_create_message(ril_message);
	ril_storage_emulate->ril_sol_request = nrmrequest;

	if (rm_sol_answer != NULL) {

		DEBUG("create_message");
		ril_message_t *nranswer = rilstorage_create_message(rm_sol_answer);
		/* emulate serial number from request */
		nranswer->serial = nrmrequest->serial;

		ril_storage_emulate->ril_sol_answer = nranswer;
	}
	DEBUG(" Request emulated: ID: %i Serial:%i", ril_message->event_id, ril_message->serial);
}

ril_storage_t *
rilstorage_emulate(list_t * ril_storage_list, ril_message_t * ril_message)
{

	list_t *local_ril_storage_list = NULL;
	list_t *l = NULL;
	ril_storage_t *ril_storage_emulate = NULL;

	uint32_t ril_message_type = 0;
	uint32_t offset = 0;

	ril_storage_t *ril_storage = NULL;

	DEBUG("rilstorage_emulate");
	DEBUG("ril_message->serial= %i", ril_message->serial);
	DEBUG("ril_message->event_id= %i", ril_message->event_id);

	if (ril_message->event_id >= RIL_UNSOL_RESPONSE_BASE) {
		return NULL;
	}

	if (ril_message->event_id == 0) {
		return NULL;
	}

	if (ril_message->event_id >= 1) {
		ril_message_type = RIL_SOL_REQUEST;
		offset = ril_message->event_id;
		DEBUG("offset = %i", offset);
	}
	// create new emulated storage

	DEBUG("mem_new0");
	ril_storage_emulate = mem_new0(ril_storage_t, 1);
	ASSERT(ril_storage_emulate);


	DEBUG("list_nth_data");
	/* get rilstorage list */
	local_ril_storage_list = list_nth_data(ril_storage_list, offset);

	if (local_ril_storage_list == NULL ) {
		DEBUG("local_ril_storage_list == NULL");
		return NULL;
	}


	ASSERT(local_ril_storage_list);

	uint32_t ll= 1;
	for (l = local_ril_storage_list; l; l = l->next) {
		DEBUG("for (l = local_ril_storage_list; l; l = l->next) -> count: %i",ll);
		ll++;
		ril_storage = l->data;

		if (ril_storage == NULL) {
			DEBUG("ril_storage == NULL");
			return NULL;
		}

		ASSERT(ril_storage);

		ril_message_t *rm_sol_request = ril_storage->ril_sol_request;
		ril_message_t *rm_sol_answer = ril_storage->ril_sol_answer;

		switch (ril_message_type) {
		case RIL_SOL_REQUEST:
			if (rm_sol_request == NULL){
				DEBUG("if (rm_sol_request == NULL)");
				break;
			}

			if (rm_sol_answer == NULL){
				DEBUG("if (rm_sol_answer== NULL)");
				break;
			}


			if (ril_message->radio_state != rm_sol_request->radio_state){
				DEBUG("ril_message->radio_state != rm_sol_request->radio_state)");
				break;
			}

			if (ril_message->sim_state != rm_sol_request->sim_state){
				DEBUG("(ril_message->sim_state != rm_sol_request->sim_state)");
				break;
			}

			if (ril_message->parcelsize > 8) {	// message with data
				if ((rm_sol_request->rildata == NULL)
				    && (ril_message->rildata == NULL)){
					DEBUG("(rm_sol_request->rildata == NULL) &&  (ril_message->rildata == NULL)");
					break;
				}
				DEBUG("memcmp");

				if (memcmp (rm_sol_request->rildata, ril_message->rildata, ril_message->parcelsize - 8) == 0) {
					rilstorage_handle_emulate(ril_storage_emulate, ril_message, rm_sol_answer);
				} else {
					if (offset == 27 ){ // RIL_REQUEST_SETUP_DATA_CALL 27 need special handling , because of different data
						DEBUG("RIL_REQUEST_SETUP_DATA_CALL emulation");
						rilstorage_handle_emulate(ril_storage_emulate, ril_message, rm_sol_answer);
						char * rildata = rm_sol_answer->rildata;

						if (*(rildata + 12) == 0x00) {       //  Status = OK
							DEBUG("RIL_REQUEST_SETUP_DATA_CALL emulation found with status = 0 (OK)");
							return ril_storage_emulate;
						}
							else {
								DEBUG("RIL_REQUEST_SETUP_DATA_CALL emulation found with status = %i ",(int) *(rildata + 12) );
							}
						}
				}

			} else {

				DEBUG("handle_emulate");
				rilstorage_handle_emulate(ril_storage_emulate, ril_message, rm_sol_answer);
			}
			break;
		}

	}

	return ril_storage_emulate;
}
