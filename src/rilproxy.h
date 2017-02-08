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

#ifndef ANDROID_RIL_PROXY_H
#define ANDROID_RIL_PROXY_H

#define RILD_SOCKET_NAME  "rild"
#define SOCKET_NAME_RIL  "/data/trustme-com/radio/rild-proxy.sock"
#define SOCKET_NAME_RIL_PRIV  "/data/misc/radio/rild-proxy.sock"

#define SOCKET_NAME_RIL_1  "/data/trustme-com/radio/rild-proxy1.sock"
#define SOCKET_NAME_RIL_2  "/data/trustme-com/radio/rild-proxy2.sock"

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/capability.h>
#include <linux/capability.h>
#include <linux/prctl.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "common/list.h"
#include "common/macro.h"
#include "common/logf.h"
#include "common/mem.h"
#include "common/utest.h"

#include "log.h"
#include "ril-storage.h"

#include <sys/capability.h>
#include <cutils/sockets.h>
#include <android/log.h>
#include <telephony/record_stream.h>
#include <telephony/ril.h>

// match with constant in RIL.java
#define MAX_COMMAND_BYTES (8 * 1024)

#endif // ANDROID_RIL_PROXY_H
