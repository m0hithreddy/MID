/*
 * MID_funcitions.c
 *
 *  Created on: 09-Mar-2020
 *      Author: Mohith Reddy
 */


#define _GNU_SOURCE
#include"MID_functions.h"
#include"MID_structures.h"
#include<string.h>
#include<stdlib.h>
#include<stdio.h>

void sort(void* arr,long ele_size,long start,long end,two_to_one_func compare)
{

	if (start < end)
	{

		/* Partitioning index */

		void* x = arr+end*ele_size;
		long i = (start - 1);
		void* tmp=malloc(ele_size);

		for (long j = start; j <= end - 1; j++)
		{
			if (*(int*)compare(arr+j*ele_size,x))
			{
				i++;
				memcpy(tmp,arr+i*ele_size,ele_size);
				memcpy(arr+i*ele_size,arr+j*ele_size,ele_size);
				memcpy(arr+j*ele_size,tmp,ele_size);
			}
		}

		memcpy(tmp,arr+(i+1)*ele_size,ele_size);
		memcpy(arr+(i+1)*ele_size,arr+end*ele_size,ele_size);
		memcpy(arr+end*ele_size,tmp,ele_size);

		i= (i + 1);

		sort(arr,ele_size,start, i - 1,compare);
		sort(arr,ele_size,i + 1, end,compare);
	}
}

char* strlocate(char* haystack, char* needle,long start,long end)
{
	if(haystack==NULL || needle==NULL || start<0 || end<0 || end<start)
		return NULL;

	char* mod_haystack=(char*)malloc(sizeof(char)*(end-start+2));

	memcpy(mod_haystack,haystack+start,end-start+1);
	mod_haystack[end-start+1]='\0';

	return strstr(haystack,needle);
}

char* strcaselocate(char* haystack,char* needle,long start,long end)
{
	if(haystack==NULL || needle==NULL || start<0 || end<0 || end<start)
		return NULL;

	char* mod_haystack=(char*)malloc(sizeof(char)*(end-start+2));

	memcpy(mod_haystack,haystack+start,end-start+1);
	mod_haystack[end-start+1]='\0';

	return strcasestr(haystack,needle);
}

struct network_data* sseek(struct network_data* n_data,char* delimiter)
{
	if(n_data==NULL || n_data->data==NULL)
		return NULL;

	struct network_data *update=(struct network_data*)malloc(sizeof(struct network_data));

	char in[2]; in[1]='\0';

	int i=0;
	for( ;i<n_data->len;i++)
	{
		in[0]=((char*)n_data->data)[i];
		if(strlocate(delimiter,in,0,strlen(delimiter))==NULL)
			break;
	}

	update->data=n_data->data+i;
	update->len=n_data->len-i;
	return update;
}

struct network_data* scopy(struct network_data* n_data,char* delimiter,char** dest,long len)
{
	if(n_data==NULL || n_data->data==NULL)
		return NULL;

	struct network_data* buf=(struct network_data*)malloc(sizeof(struct network_data));

	buf->data=(char*)malloc(2*sizeof(char));
	((char*)buf->data)[1]='\0';
	buf->len=1;

	struct data_bag *bag=create_data_bag();

	long i;

	for(i=0;i!=len && i<n_data->len;i++)
	{
		((char*)buf->data)[0]=((char*)n_data->data)[i];

		if(strlocate(delimiter,buf->data,0,strlen(delimiter))!=NULL)
			break;

		if(dest!=NULL)
			place_data(bag,buf);
	}

	struct network_data* update=(struct network_data*)malloc(sizeof(struct network_data));

	update->data=n_data->data+i;
	update->len=n_data->len-i;

	if(dest==NULL)
		return update;

	buf=flatten_data_bag(bag);

	*dest=(char*)malloc(sizeof(char)*(buf->len+1));

	if(buf->data!=NULL)
		memcpy(*dest,buf->data,buf->len);

	(*dest)[buf->len]='\0';

	return update;
}

hashmap* init_hashmap()
{
	return (hashmap*)create_data_bag();
}

int insert_pair(hashmap* map,struct hash_token* key,struct hash_token* value)
{
	if(map==NULL)
		return -1;

	struct data_pocket* pocket=map->first;

	while(pocket!=NULL)
	{
		if(key->len==pocket->len && !memcmp(pocket->data,key->token,pocket->len))
		{
			pocket=pocket->next;

			if(pocket==NULL)
				return -1;

			pocket->data=(char*)malloc(sizeof(char)*value->len);
			memcpy(pocket->data,value->token,value->len);
			pocket->len=value->len;
			return 1;
		}
		pocket=pocket->next->next;
	}

	append_data_pocket(map,key->len);
	memcpy(map->end,key->token,key->len);

	append_data_pocket(map,value->len);
	memcpy(map->end,value->token,value->len);

	return 1;
}

struct hash_token* get_value(hashmap* map,struct hash_token* key)
{
	struct data_pocket* pocket=map->first;

	while(pocket!=NULL)
	{
		if(pocket->len==key->len && !memcmp(pocket->data,key->token,pocket->len))
		{
			pocket=pocket->next;

			if(pocket==NULL)
				return NULL;

			struct hash_token* value=(struct hash_token*)malloc(sizeof(struct hash_token));
			value->token=(char*)malloc(sizeof(char)*pocket->len);
			memcpy(value->token,pocket->data,pocket->len);
			value->len=pocket->len;
			return value;
		}
		pocket=pocket->next->next;
	}

	return NULL;
}

struct hash_token** get_keys(hashmap* map)
{

	struct hash_token** keys=(struct hash_token**)malloc(sizeof(struct hashmap*)*(map->n_pockets/2+1));

	long counter=0;

	struct data_pocket* pocket=map->first;

	while(pocket!=NULL)
	{
		keys[counter]=(struct hash_token*)malloc(sizeof(struct hash_token));

		keys[counter]->token=(char*)malloc(sizeof(char)*pocket->len);
		memcpy(keys[counter]->token,pocket->data,pocket->len);
		keys[counter]->len=pocket->len;

		pocket=pocket->next;

		if(pocket==NULL)
		{
			keys[counter]=NULL;
			return keys;
		}

		pocket=pocket->next;
		counter++;
	}

	keys[counter]=NULL;

	return keys;
}

struct hash_token** get_values(hashmap* map)
{
	struct hash_token** values=(struct hash_token**)malloc(sizeof(struct hashmap*)*(map->n_pockets/2+1));

	long counter=0;

	struct data_pocket* pocket=map->first;

	while(pocket!=NULL)
	{
		pocket=pocket->next;

		if(pocket==NULL)
		{
			values[counter]=NULL;
			return values;
		}

		values[counter]=(struct hash_token*)malloc(sizeof(struct hash_token));

		values[counter]->token=(char*)malloc(sizeof(char)*pocket->len);
		memcpy(values[counter]->token,pocket->data,pocket->len);
		values[counter]->len=pocket->len;

		counter++;
		pocket=pocket->next;

	}

	values[counter]=NULL;

	return values;
}
