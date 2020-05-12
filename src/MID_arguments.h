/*
 * MID_arguments.h
 *
 *  Created on: 20-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_ARGUMENTS_H_
#define MID_ARGUMENTS_H_

#define MID_DEFAULT_CONFIG_FILE "/usr/local/etc/MID.conf"

#define MID_MODE_READ_DEFAULT_VALUES 0b0001
#define MID_MODE_READ_CONF_FILE 0b0010
#define MID_MODE_READ_CMD_LINE 0b0100
#define MID_MODE_PRINT_HELP 0b1000

#define MID_CONSTANT_ARGUMENTS " -o --output-file 0 \
-i --interfaces 0 \
-ni --exclude-interfaces 0 \
-u --url 0 \
-up --unprocessed-file 0 \
-sa --scheduler-algorithm 0 \
-n --max-parallel-downloads 0 \
-ur --max-unit-retries 0 \
-ub --unit-break 0 \
-R --max-redirects 0 \
-sr --max-tcp-syn-retransmits 0 \
-us --unit-sleep-time 0 \
-io --io-timeout 0 \
-pu --progress-update-time 0 \
-e --entry-number 0 \
-ms --ms-file 0 \
-pm --print-ms 0 \
-dm --delete-ms 0 \
-vm --validate-ms 0 \
-H --header 0 \
-4 --ipv4 1 \
-6 --ipv6 1 \
-dp --detailed-progress 1 \
-fr --force-resume 1 \
-nr --no-resume 1 \
-ds --detailed-save 1 \
-q --quiet 1 \
-v --verbose 1 \
-vv --vverbose 1 \
-s --surpass-root-check 1 \
-V --version 1 \
-h --help 1 \
-c --conf 2 "

#define mid_help(msg, msg_args...)\
	do\
	{\
		if (msg != NULL)\
		{\
			fprintf(stderr, msg, ## msg_args);\
			fprintf(stderr, "\n\nTry {MID | mid} --help for more information. \n\n");\
		}\
		else\
		{\
			mid_help_strings();\
		}\
	}while(0)

#include "MID_unit.h"
#include "MID_structures.h"

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
	long args_count;
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
	int version;
	int help;
};

static void fill_mid_args(char* key,char* value,struct mid_args* args,int conf_flag);

static int read_mid_conf(char* config_file, struct mid_args* args, int rc_flags);

int parse_mid_args(char** argv, long argc, int pa_flags, struct mid_bag* pa_result);

int args_check(struct mid_args* args);

void mid_help_strings();

#endif /* MID_ARGUMENTS_H_ */

