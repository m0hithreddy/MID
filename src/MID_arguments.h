/*
 * MID_arguments.h
 *
 *  Created on: 20-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_ARGUMENTS_H_
#define MID_ARGUMENTS_H_

#define DEFAULT_CONFIG_FILE "/etc/MID/MID.conf"

struct mid_args
{
	char* cmd;
	char* output_file;
	char* url;
	char** include_ifs;
	char** exclude_ifs;
	char*** custom_headers;
	long include_ifs_count;
	long exclude_ifs_count;
	long arg_count;
	long max_parallel_downloads;
	long max_unit_retries;
	long max_redirects;
	long max_tcp_syn_retransmits;
	long unit_retry_sleep_time;
	long progress_update_time;
	int detailed_progress;
	int quiet_flag;
	int verbose_flag;
	int vverbose_flag ;
	int surpass_root_check;
};

void fill_mid_args(char* key,char* value,struct mid_args* args,int conf_flag);

void read_conf(char* conf,struct mid_args* args);

struct mid_args* parse_mid_args(char** argv,long argc);

void mid_help(char* err_msg);

#endif /* MID_ARGUMENTS_H_ */
