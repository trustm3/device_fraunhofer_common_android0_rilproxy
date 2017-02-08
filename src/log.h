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



#ifndef LOG_H_
#define LOG_H_

#define MODULENAME "TRUSTME_RILPROXY"
#define strfy(line) #line


#if defined(ANDROID) || defined(__ANDROID__)


#include <android/log.h>

#define log(...) __android_log_print(ANDROID_LOG_VERBOSE, MODULENAME,__VA_ARGS__)

#define cline(file,line, ...) __android_log_print(ANDROID_LOG_VERBOSE, MODULENAME, " ("file":"strfy(line)") "__VA_ARGS__)
#define logD(...) cline(__FILE__,__LINE__,__VA_ARGS__)

#define eline(file,line, ...) __android_log_print(ANDROID_LOG_ERROR, MODULENAME, " ("file":"strfy(line)") "__VA_ARGS__)
#define logE(...) cline(__FILE__,__LINE__,__VA_ARGS__)

#else

#include <stdio.h>

#define log(...) {printf(__VA_ARGS__);fflush(stdout);}

#define cline(file,line, ...) {printf(" ("file":"strfy(line)") "__VA_ARGS__);fflush(stdout);}
#define logD(...) cline(__FILE__,__LINE__,__VA_ARGS__)

#define logE(...) cline(__FILE__,__LINE__,__VA_ARGS__)
#define LOGE(...) cline(__FILE__,__LINE__,__VA_ARGS__)

#endif


#endif /* LOG_H_ */

