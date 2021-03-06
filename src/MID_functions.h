/*
 * MID_functions.h
 *
 *  Created on: 09-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_FUNCTIONS_H_
#define MID_FUNCTIONS_H_

#include"MID_structures.h"

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#define MID_DELIMIT 1
#define MID_PERMIT 0

typedef struct mid_bag hashmap;

typedef void* (*two_to_one_func)(void*,void*);

struct hash_token
{
	void* token;
	long len;
};


void sort(void* arr,long ele_size,long start,long end,two_to_one_func compare);

char* strlocate(char* haystack,char* needle,long start,long end);

char* strcaselocate(char* haystack,char* needle,long start,long end);

struct mid_data* sseek(struct mid_data* n_data,char* delimiter,long len,int delimiting);  /*len<0 to neglect length*/

struct mid_data* scopy(struct mid_data* n_data, char* delimiter, char** dest, long len,int delimiting); /*len<0 to neglect length*/

#ifndef HAVE_MEMNDUP
void* memndup(void* src,long n_bytes);
#endif

hashmap* init_hashmap();

int insert_pair(hashmap* map,struct hash_token* key,struct hash_token* value);

struct hash_token* get_value(hashmap* map,struct hash_token* key);

struct hash_token** get_keys(hashmap* map);

struct hash_token** get_values(hashmap* map);

int delete_pair(hashmap* map,struct hash_token* key);

#endif /* MID_FUNCTIONS_H_ */
