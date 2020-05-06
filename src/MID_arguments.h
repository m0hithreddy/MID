/*
 * MID_arguments.h
 *
 *  Created on: 20-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_ARGUMENTS_H_
#define MID_ARGUMENTS_H_

#define DEFAULT_CONFIG_FILE "/usr/local/etc/MID.conf"

#include"MID_unit.h"

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

struct mid_args
{
	char* cmd;
	char* output_file;
	char* url;
	char* up_file;
	char* ms_file;
	char* pm_file;
	char* dm_file;
	char* cm_file;
	char** include_ifs;
	char** exclude_ifs;
	char*** custom_headers;
	scheduler schd_alg;
	long include_ifs_count;
	long exclude_ifs_count;
	long arg_count;
	long max_parallel_downloads;
	long max_unit_retries;
	long max_redirects;
	long max_tcp_syn_retransmits;
	long unit_retry_sleep_time;
	long io_timeout;
	long unit_break;
	long progress_update_time;
	long entry_number;
	int root_mode;
	int detailed_progress;
	int print_ms;
	int delete_ms;
	int validate_ms;
	int force_resume;
	int no_resume;
#ifdef LIBSSL_SANE
	int detailed_save;
#endif
	int quiet_flag;
	int verbose_flag;
	int vverbose_flag ;
	int surpass_root_check;
	int ipv4;
	int ipv6;
};

void fill_mid_args(char* key,char* value,struct mid_args* args,int conf_flag);

void read_mid_conf(char* conf,struct mid_args* args);

struct mid_args* parse_mid_args(char** argv,long argc);

void mid_help(char* err_msg);

#endif /* MID_ARGUMENTS_H_ */

