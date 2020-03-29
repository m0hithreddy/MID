/*
 * MID_functions.h
 *
 *  Created on: 09-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_FUNCTIONS_H_
#define MID_FUNCTIONS_H_

#include"MID_structures.h"

typedef struct data_bag hashmap;

typedef void* (*two_to_one_func)(void*,void*);

struct hash_token
{
	void* token;
	long len;
};


void sort(void* arr,long ele_size,long start,long end,two_to_one_func compare);

char* strlocate(char* haystack,char* needle,long start,long end);

char* strcaselocate(char* haystack,char* needle,long start,long end);

struct network_data* sseek(struct network_data* n_data,char* delimiter);

struct network_data* scopy(struct network_data* n_data, char* delimiter, char** dest, long len); /*len<0 to neglect length*/

hashmap* init_hashmap();

int insert_pair(hashmap* map,struct hash_token* key,struct hash_token* value);

struct hash_token* get_value(hashmap* map,struct hash_token* key);

struct hash_token** get_keys(hashmap* map);

struct hash_token** get_values(hashmap* map);

int delete_pair(hashmap* map,struct hash_token* key);

#endif /* MID_FUNCTIONS_H_ */


