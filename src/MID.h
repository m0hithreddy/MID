/*
 * MID_macros.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_H_
#define MID_H_

#define MID_DEFAULT_HTTP_VERSION "1.1"
#define MS_PRINT 00
#define MS_RETURN 01
#define MS_SILENT 10

#include"MID_arguments.h"
#include"MID_functions.h"
#include"MID_structures.h"
#include"MID_http.h"
#include<stdio.h>
#include<sys/file.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#define MID_USER_AGENT "MID/"PACKAGE_VERSION

extern struct mid_args* args;
extern FILE* o_fp;
extern FILE* u_fp;

struct signal_handler_info
{
	pthread_t tid;
	pthread_t ptid;
	int quit;
	sigset_t mask;
	pthread_mutex_t lock;
};



void close_files();

void* signal_handler(void* v_s_hd_info);

void deregister_handler(struct signal_handler_info* s_hd_info);

void init_resume();

#endif /* MID_H_ */
