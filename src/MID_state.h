/*
 * MID_state.h
 *
 *  Created on: 13-Apr-2020
 *      Author: root
 */

#ifndef SRC_MID_STATE_H_
#define SRC_MID_STATE_H_

#define MS_PRINT 00
#define MS_RETURN 01
#define MS_SILENT 10

#include"MID_http.h"
#include"MID_unit.h"
#include<stdio.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

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

char* get_ms_filename();

void save_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

void dump_int(struct data_bag* bag,int num);

void dump_long(struct data_bag* bag,long num);

void dump_string(struct data_bag* bag,char* string);

char* extract_string(FILE* ms_fp);

void dump_mid_state(FILE* ms_fp,struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

struct ms_entry* process_ms_entry(FILE* ms_fp);

void print_ms_entry(struct ms_entry* en);

void check_ms_entry(struct ms_entry* en);

#ifdef LIBSSL_SANE
void dump_detailed_mid_state(FILE* ms_fp,struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

struct d_ms_entry* process_detailed_ms_entry(FILE* ms_fp);

void print_detailed_ms_entry(struct d_ms_entry* d_en);

void check_detailed_ms_entry(struct d_ms_entry* d_en);
#endif

void* read_ms_file(char* ms_file,long entry_number,int flag);

void clear_ms_entry(char* ms_file,long entry_number,int flag);

#endif /* SRC_MID_STATE_H_ */
