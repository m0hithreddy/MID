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
#define MS_ERR_NO 000
#define MS_ERROR_FPNULL 001
#define MS_ERROR_LOCKEX 010
#define MS_ERROR_EOF 011
#define MS_ERROR_BROKEN 100

#include"MID_http.h"
#include"MID_unit.h"
#include"MID_structures.h"
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
	struct http_range* l_ranges;
	long n_l_ranges;
	long en_len;
};

#ifdef LIBSSL_SANE
struct d_ms_entry
{
	int type;
	struct ms_entry* en;
	struct http_range* ranges;
	long n_ranges;
	char** hashes;
	long d_en_len;
};
#endif

char* get_ms_filename();

void save_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

void resave_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

void dump_int(struct data_bag* bag,int num);

void dump_long(struct data_bag* bag,long num);

void dump_string(struct data_bag* bag,char* string);

char* extract_string(FILE* ms_fp);

struct data_bag* make_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

struct ms_entry* process_ms_entry(FILE* ms_fp);

void print_ms_entry(struct ms_entry* en);

int validate_ms_entry(struct ms_entry* en,struct http_request* gl_s_request,struct http_response* gl_s_response,int flag);

#ifdef LIBSSL_SANE
struct data_bag* make_d_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress);

struct d_ms_entry* process_d_ms_entry(FILE* ms_fp);

void print_d_ms_entry(struct d_ms_entry* d_en);

int validate_d_ms_entry(struct d_ms_entry* en,struct http_request* gl_s_request,struct http_response* gl_s_response,int flag);
#endif

void* read_ms_entry(char* ms_file,long entry_number,int flag);

void delete_ms_entry(char* ms_file,long entry_number,int flag);

void check_ms_entry(char* ms_file,long entry_number,struct http_request* gl_s_request,struct http_response* gl_s_response);

#endif /* SRC_MID_STATE_H_ */

