#
# This file is part of trust|me
# Copyright(c) 2013 - 2017 Fraunhofer AISEC
# Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2 (GPL 2), as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GPL 2 license for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>
#
# The full GNU General Public License is included in this distribution in
# the file called "COPYING".
#
# Contact Information:
# Fraunhofer AISEC <trustme@aisec.fraunhofer.de>
#

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := rilproxy

LOCAL_C_INCLUDES := hardware/ril/include/telephony

LOCAL_SRC_FILES := src/rilproxy.c \
	src/ril-storage.c \
	src/common/mem.c \
	src/common/list.c \
	src/common/logf.c

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
        librilutils \
	libnetutils

LOCAL_CFLAGS += -DCML_ANDROID
LOCAL_CFLAGS += -pedantic -Wall -Werror -std=c99

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CFLAGS += -DDEBUG_BUILD
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_MODULE_TAGS := eng

include $(BUILD_EXECUTABLE)


# Define common cflags and src files for unit tests
CMLD_COMMON_TEST_CFLAGS := $(CMLD_COMMON_CFLAGS) \
	-include src/common/utest.h

CMLD_COMMON_TEST_SRC_FILES := $(CMLD_COMMON_SRC_FILES) \
	scr/common/utest.c

########################
# Unit Test Targets     #
##########################

include $(CLEAR_VARS)

LOCAL_MODULE := trustme.cml.rilproxy.test.host
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -pedantic -Wall -Wextra -Werror -std=c99 -include src/common/utest.h

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
	LOCAL_CFLAGS += -DDEBUG_BUILD
endif

LOCAL_SRC_FILES := \
	src/common/utest.c \
	src/common/mem.c \
	src/common/list.c \
	src/common/logf.c \
	src/ril-storage.c \
	src/ril-storage.test.c
LOCAL_MODULE_CLASS := EXECUTABLES
include $(BUILD_HOST_EXECUTABLE)
