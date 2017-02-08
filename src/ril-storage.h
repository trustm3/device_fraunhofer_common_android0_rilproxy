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

/**
 * @file ril-storage.h
 *
 * Implements a RIL Message storage tree.
 */

#ifndef RIL_STORAGE_H
#define RIL_STORAGE_H

#define RIl_MESSAGE_SIZE_MAX 500
#define MAX_MESSAGE_FIFO_SIZE 100
#define RIL_MESSAGE_MAX 177
#define RIL_MESSAGE_OFFSET 132
#define RIL_UNSOL_TEST_OFFSET (RIL_MESSAGE_MAX/2)
#define RIL_RADIO_STATES_MAX 1
#define RIL_SIM_STATES_MAX 1
#define FALSE 0
#define TRUE 1
#define RIL_SOL_REQUEST 1
#define RIL_SOL_ANSWER 2
#define RIL_UNSOL 3
#define	RIL_UNSOL_RESPONSE_BASE 1000

typedef enum {
	RIL_MESSAGE_ORIGIN_A0 = 0,
	RIL_MESSAGE_ORIGIN_A1,
	RIL_MESSAGE_ORIGIN_A2,
	RIL_MESSAGE_ORIGIN_MODEM
} ril_message_origin_t;

#include <stdint.h>

#if 1
#ifdef logD

        #undef logD
        #define logD(...)

#endif
#endif

typedef struct {
	unsigned int parcelsize;
	unsigned int event_id;
	unsigned int serial;
} ril_message_raw_t;

typedef struct {
	uint32_t parcelsize;
	uint32_t event_id;
	uint32_t serial;
	void *rildata;
	uint32_t radio_state;
	uint32_t sim_state;
	unsigned long long timestamp;
} ril_message_t;

typedef struct {
	ril_message_t *ril_sol_request;
	ril_message_t *ril_sol_answer;
	ril_message_t *ril_unsol;
} ril_storage_t;

unsigned long long rilstorage_get_ms_time();

unsigned int rilstorage_fifo_get_message_id_from_serial(list_t * ril_storage_list, ril_message_t * ril_message);

/**
 * update or create a ril request message FIFO
 *
 *
 * @param ril_storage_list The head of the storage tree
 * @param ril_message The new RIL Message to be added to the FIFO
 * @return The head of the list
 */

list_t *rilstorage_update_message_fifo(list_t * ril_storage_list, ril_message_t * ril_message, ril_message_origin_t origin);


/**
 * Create and allocate the complete RIL storage tree
 *
 *
 * @param ril_storage_list The head of the storage tree
 * @return The head of the list
 */

list_t *rilstorage_new(list_t * ril_storage_list);

/**
 * Free all allocated memory used by the RIL storage tree
 *
 *
 * @param ril_storage_list The head of the storage tree
 * @return Void
 */

void rilstorage_free(list_t * ril_storage_list);

/**
 * update or create a ril_message tree element
 *
 *
 * @param ril_storage_list The head of the storage tree
 * @param ril_message The new RIL Message to be added to the tree
 * @param origin The origin oth the message (modem/a0/a1/a2)
 * @return The head of the list
 */

list_t *rilstorage_update(list_t * ril_storage_list, ril_message_t * ril_message, ril_message_origin_t origin);

/**
 * Create emulated Answer for a Ril Request Message
 *
 *
 * @param ril_storage_list The head of the storage tree
 * @param ril_message The RIL Message to be emulated by the ril storage
 * @return The emulated RIL messages ( Answer + UNSOL )
 */

ril_storage_t *rilstorage_emulate(list_t * ril_storage_list, ril_message_t * ril_message);




ril_message_t * rilstorage_create_message(ril_message_t * ril_message);

void rilstorage_ril_storage_el_free(ril_storage_t * ril_storage);

#endif
