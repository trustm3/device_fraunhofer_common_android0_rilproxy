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

/** @file ril-storage.test.c
 *
 * ril storage unit test
 *
 */

#include "common/list.h"
#include "common/macro.h"
#include "common/logf.h"
#include "common/mem.h"
#include "common/utest.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ril-storage.h"

#include <inttypes.h>

#define is_bigendian() ( (*(char*)&i) == 0 )

void mem_debug()
{
}

const int i = 1;

int main(void)
{

	list_t *ril_storage_list = NULL;
	ril_message_t ril_message;

	ril_message.radio_state = 1;
	ril_message.sim_state = 1;
	ril_message.event_id = 1;
	ril_message.parcelsize = 13;
	ril_message.serial = 1;

	mem_debug();

	char *rildata1 = mem_printf("data1");
	char *rildata2 = mem_printf("data2");
	char *rildata3 = mem_printf("data3");
	char *rildata4 = mem_printf("data4");
	char *rildata5 = mem_printf("data5");
	char *rildata6 = mem_printf("data6");

	ril_storage_t *emu = NULL;

	ril_message.rildata = rildata1;

	logf_register(&logf_file_write, stdout);
	DEBUG("Unit Test: ril-storage.test.c");

	/* create storage tree */

	ril_storage_list = rilstorage_new(ril_storage_list);
	ASSERT(ril_storage_list);
	/* add RIL Request to tree */

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_A0);
		ril_message.serial++;
	}

	/* add Ril Answers to tree */

	ril_message.rildata = rildata2;
	ril_message.event_id = 0;
	ril_message.parcelsize = 13;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.serial = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_MODEM);
	}

	/* check if already added Answers where found */

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.serial = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_MODEM);
	}

	/* get emlated ril request and answer */
	ril_message.parcelsize = 13;
	ril_message.rildata = rildata1;
	ril_message.serial = 20000;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		emu = rilstorage_emulate(ril_storage_list, &ril_message);

		if (emu->ril_sol_request != NULL) {
			DEBUG(" emu SOL Request: : ID: %i Serial:%i", emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
			if (emu->ril_sol_request->rildata != NULL)
				mem_free(emu->ril_sol_request->rildata);
			if (emu->ril_sol_request != NULL)
				mem_free(emu->ril_sol_request);
		} else
			DEBUG(" No  SOL Request found");

		if (emu->ril_sol_answer != NULL) {
			DEBUG(" emu SOL Answer: ID: %i Serial:%i", emu->ril_sol_answer->event_id, emu->ril_sol_answer->serial);
			if (emu->ril_sol_answer->rildata != NULL)
				mem_free(emu->ril_sol_answer->rildata);
			if (emu->ril_sol_answer != NULL)
				mem_free(emu->ril_sol_answer);
		} else
			DEBUG(" No SOL Answer: found");

		if (emu != NULL)
			mem_free(emu);
		ril_message.serial++;
	}

	/* add new RIL Requests for serial 100+ to tree */
	ril_message.parcelsize = 13;
	ril_message.rildata = rildata3;
	ril_message.serial = 100;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_A0);
		ril_message.serial++;
	}

	/* add Ril Answers for serial 100+ to tree */
	ril_message.rildata = rildata4;
	ril_message.event_id = 0;
	ril_message.parcelsize = 13;
	ril_message.serial = 100;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_MODEM);
		ril_message.serial++;
	}

	/* get emlated ril request and answer for rildata1 */
	ril_message.parcelsize = 13;
	ril_message.rildata = rildata1;
	ril_message.serial = 30000;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		emu = rilstorage_emulate(ril_storage_list, &ril_message);

		if (emu->ril_sol_request != NULL) {
			DEBUG(" emu SOL Request: ID: %i Serial:%i", emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
			if (emu->ril_sol_request->rildata != NULL)
				mem_free(emu->ril_sol_request->rildata);
			if (emu->ril_sol_request != NULL)
				mem_free(emu->ril_sol_request);
		} else
			DEBUG(" No  SOL Request found");

		if (emu->ril_sol_answer != NULL) {
			DEBUG(" emu SOL Answer: ID: %i Serial:%i", emu->ril_sol_answer->event_id, emu->ril_sol_answer->serial);
			if (emu->ril_sol_answer->rildata != NULL)
				mem_free(emu->ril_sol_answer->rildata);
			if (emu->ril_sol_answer != NULL)
				mem_free(emu->ril_sol_answer);
		} else
			DEBUG(" No SOL Answer: found");

		if (emu != NULL)
			mem_free(emu);
		ril_message.serial++;
	}

	/* get emlated ril request and answer for rildata3 */
	ril_message.parcelsize = 13;
	ril_message.rildata = rildata3;
	ril_message.serial = 40000;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		emu = rilstorage_emulate(ril_storage_list, &ril_message);

		if (emu->ril_sol_request != NULL) {
			DEBUG(" emu SOL Request: ID: %i Serial:%i", emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
			if (emu->ril_sol_request->rildata != NULL)
				mem_free(emu->ril_sol_request->rildata);
			if (emu->ril_sol_request != NULL)
				mem_free(emu->ril_sol_request);
		} else
			DEBUG(" No  SOL Request found");

		if (emu->ril_sol_answer != NULL) {
			DEBUG(" emu SOL Answer: ID: %i Serial:%i", emu->ril_sol_answer->event_id, emu->ril_sol_answer->serial);
			if (emu->ril_sol_answer->rildata != NULL)
				mem_free(emu->ril_sol_answer->rildata);
			if (emu->ril_sol_answer != NULL)
				mem_free(emu->ril_sol_answer);
		} else
			DEBUG(" No SOL Answer: found");

		if (emu != NULL)
			mem_free(emu);
		ril_message.serial++;
	}

	/* get emlated ril request and answer for rildata5 should not befound */
	ril_message.parcelsize = 13;
	ril_message.rildata = rildata5;
	ril_message.serial = 50000;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		emu = rilstorage_emulate(ril_storage_list, &ril_message);

		if (emu->ril_sol_request != NULL) {
			DEBUG(" emu SOL Request: ID: %i Serial:%i", emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
			if (emu->ril_sol_request->rildata != NULL)
				mem_free(emu->ril_sol_request->rildata);
			if (emu->ril_sol_request != NULL)
				mem_free(emu->ril_sol_request);
		} else
			DEBUG(" No  SOL Request found");

		if (emu->ril_sol_answer != NULL) {
			DEBUG(" emu SOL Answer: ID: %i Serial:%i", emu->ril_sol_answer->event_id, emu->ril_sol_answer->serial);
			if (emu->ril_sol_answer->rildata != NULL)
				mem_free(emu->ril_sol_answer->rildata);
			if (emu->ril_sol_answer != NULL)
				mem_free(emu->ril_sol_answer);
		} else
			DEBUG(" No SOL Answer: found");

		if (emu != NULL)
			mem_free(emu);
		ril_message.serial++;
	}

	/* add new RIL Requests for serial 100+ to tree */
	ril_message.parcelsize = 13;
	ril_message.rildata = rildata5;
	ril_message.serial = 200;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_A0);
		ril_message.serial++;
	}

	/* get emlated ril request and answer for rildata5 only request should be found */

	ril_message.parcelsize = 13;
	ril_message.rildata = rildata5;
	ril_message.serial = 60000;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		emu = rilstorage_emulate(ril_storage_list, &ril_message);

		if (emu->ril_sol_request != NULL) {
			DEBUG(" emu SOL Request: ID: %i Serial:%i", emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
			if (emu->ril_sol_request->rildata != NULL)
				mem_free(emu->ril_sol_request->rildata);
			if (emu->ril_sol_request != NULL)
				mem_free(emu->ril_sol_request);
		} else
			DEBUG(" No  SOL Request found");

		if (emu->ril_sol_answer != NULL) {
			DEBUG(" emu SOL Answer: ID: %i Serial:%i", emu->ril_sol_answer->event_id, emu->ril_sol_answer->serial);
			if (emu->ril_sol_answer->rildata != NULL)
				mem_free(emu->ril_sol_answer->rildata);
			if (emu->ril_sol_answer != NULL)
				mem_free(emu->ril_sol_answer);
		} else
			DEBUG(" No SOL Answer: found");

		if (emu != NULL)
			mem_free(emu);
		ril_message.serial++;
	}

	/* add Ril Answers for serial 200+ to tree */
	ril_message.rildata = rildata6;
	ril_message.event_id = 0;
	ril_message.parcelsize = 13;
	ril_message.serial = 200;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.timestamp = rilstorage_get_ms_time();
		ril_storage_list = rilstorage_update(ril_storage_list, &ril_message, RIL_MESSAGE_ORIGIN_MODEM);
		ril_message.serial++;
	}

	/* get emlated ril request and answer for rildata5 */

	ril_message.parcelsize = 13;
	ril_message.rildata = rildata5;
	ril_message.serial = 70000;

	for (int i = 1; i <= RIL_MESSAGE_MAX - RIL_UNSOL_TEST_OFFSET; i++) {
		ril_message.event_id = i;
		ril_message.timestamp = rilstorage_get_ms_time();
		emu = rilstorage_emulate(ril_storage_list, &ril_message);

		if (emu->ril_sol_request != NULL) {
			DEBUG(" emu SOL Request: ID: %i Serial:%i", emu->ril_sol_request->event_id, emu->ril_sol_request->serial);
			if (emu->ril_sol_request->rildata != NULL)
				mem_free(emu->ril_sol_request->rildata);
			if (emu->ril_sol_request != NULL)
				mem_free(emu->ril_sol_request);
		} else
			DEBUG(" No  SOL Request found");

		if (emu->ril_sol_answer != NULL) {
			DEBUG(" emu SOL Answer: ID: %i Serial:%i", emu->ril_sol_answer->event_id, emu->ril_sol_answer->serial);
			if (emu->ril_sol_answer->rildata != NULL)
				mem_free(emu->ril_sol_answer->rildata);
			if (emu->ril_sol_answer != NULL)
				mem_free(emu->ril_sol_answer);
		} else
			DEBUG(" No SOL Answer: found");

		if (emu != NULL)
			mem_free(emu);
		ril_message.serial++;
	}

	mem_debug();
	/* free the ril storage tree */
	rilstorage_free(ril_storage_list);
	mem_free(rildata1);
	mem_free(rildata2);
	mem_free(rildata3);
	mem_free(rildata4);
	mem_free(rildata5);
	mem_free(rildata6);
	mem_debug();
	return 0;
}
