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

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#define MID_USER_AGENT "MID/"PACKAGE_URL

extern struct mid_args* args;
extern FILE* o_fp;
extern FILE* u_fp;

struct signal_handler_info
{
	pthread_t tid;
	int quit;
	sigset_t mask;
	pthread_mutex_t lock;
};

struct ms_entry
{
	int type;
	char* in_url;
	char* fin_url;
	char* file;
	char* up_file;
	long content_length;
	long downloaded_length;
	struct http_range* ranges;
	long n_ranges;
	long en_len;
};

#ifdef LIBSSL_SANE
struct d_ms_entry
{
	int type;
	char* in_url;
	char* fin_url;
	char* file;
	char* up_file;
	long content_length;
	long downloaded_length;
	struct http_range* left_ranges;
	long n_left_ranges;
	struct http_range* ranges;
	long n_ranges;
	char** hashes;
	long d_en_len;
};
#endif

void* signal_handler(void* v_s_hd_info);

char* get_ms_filename();

void save_mid_state();

void dump_int(struct data_bag* bag,int num);

void dump_long(struct data_bag* bag,long num);

void dump_string(struct data_bag* bag,char* string);

char* extract_string(FILE* ms_fp);

void dump_mid_state(FILE* ms_fp);

struct ms_entry* process_ms_entry(FILE* ms_fp);

void print_ms_entry(struct ms_entry* en);

void check_ms_entry(struct ms_entry* en);

#ifdef LIBSSL_SANE
void dump_detailed_mid_state(FILE* ms_fp);

struct d_ms_entry* process_detailed_ms_entry(FILE* ms_fp);

void print_detailed_ms_entry(struct d_ms_entry* d_en);

void check_detailed_ms_entry(struct d_ms_entry* d_en);
#endif

void* read_ms_file(char* ms_file,long entry_number,int flag);

void clear_ms_entry(char* ms_file,long entry_number,int flag);

void init_resume();

#endif /* MID_H_ */
