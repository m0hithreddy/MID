/*
 * MID_err.h
 *
 *  Created on: 12-Apr-2020
 *      Author: root
 */

#ifndef SRC_MID_ERR_H_
#define SRC_MID_ERR_H_

#include"MID.h"
#include"MID_arguments.h"
#include<stdio.h>
#include<sys/file.h>

#define mid_flag_exit(exit_code,fmt, exit_args...) do\
	{\
	if(args!= NULL && !args->quiet_flag)\
	{\
		fprintf(stderr, fmt, ## exit_args);\
	}\
	close_files();\
	deregister_handler();\
	exit(exit_code);\
	}while(0)

#define mid_exit(exit_code) do\
	{\
	close_files();\
	deregister_handler();\
	exit(exit_code);\
	}while(0)

#define mid_cond_exit(exit_code,cond,fmt, exit_args...)\
	do\
	{\
		if(cond)\
		{\
			fprintf(stderr, fmt, ## exit_args);\
			close_files();\
			deregister_handler();\
			exit(exit_code);\
		}\
	}while(0)

#endif /* SRC_MID_ERR_H_ */
