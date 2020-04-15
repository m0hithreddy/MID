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

#define mid_flag_exit1(exit_code,fmt, exit_args...)\
	do\
	{\
		if(args!= NULL && !args->quiet_flag)\
		{\
			fprintf(stderr, fmt, ## exit_args);\
		}\
		exit(exit_code);\
	}while(0)

#define mid_flag_exit2(exit_code,fmt, exit_args...)\
	do\
	{\
		if(args!= NULL && !args->quiet_flag)\
		{\
			fprintf(stderr, fmt, ## exit_args);\
		}\
		close_files();\
		deregister_handler(s_hd_info);\
		exit(exit_code);\
	}while(0)

#define mid_flag_exit3(exit_code,fmt, exit_args...)\
	do\
	{\
		if(args!= NULL && !args->quiet_flag)\
		{\
			fprintf(stderr, fmt, ## exit_args);\
		}\
		if(resume_status)\
		{\
			resave_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);\
		}\
		else\
		{\
			save_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);\
		}\
		close_files();\
		deregister_handler(s_hd_info);\
		exit(exit_code);\
	}while(0)

#define mid_exit(exit_code)\
	do\
	{\
		close_files();\
		deregister_handler(s_hd_info);\
		exit(exit_code);\
	}while(0)

#define mid_cond_print(cond,fmt, exit_args...)\
	do\
	{\
		if(cond)\
		{\
			fprintf(stderr, fmt, ## exit_args);\
		}\
	}while(0)

#endif /* SRC_MID_ERR_H_ */
