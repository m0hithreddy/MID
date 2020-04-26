/*
 * MID_structures.c
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */


#include"MID_structures.h"
#include<stdlib.h>
#include<string.h>
#include<stdio.h>

struct mid_bag* create_mid_bag()
{
	struct mid_bag *bag=(struct mid_bag*)malloc(sizeof(struct mid_bag));
	bag->n_pockets=0;
	bag->first=NULL;
	bag->end=NULL;

	return bag;
}

void clear_mid_bag(struct mid_bag* bag)
{
	struct mid_pocket *at,*prev;

	at=bag->end;

	while(at!=NULL)
	{
		free(at->data);
		prev=at->previous;
		free(at);
		at=prev;
	}

	bag->n_pockets=0;

	free(bag);
}

void append_mid_pocket(struct mid_bag* bag,long size)
{

	if(bag==NULL || size==0)
		return ;

	struct mid_pocket *pocket=(struct mid_pocket*)malloc(sizeof(struct mid_pocket));

	if(bag->n_pockets==0)
	{
		bag->n_pockets=1;
		bag->first=pocket;
		bag->end=pocket;
		pocket->len=0;
		pocket->previous=NULL;
		pocket->next=NULL;
		pocket->data=(char*)malloc(sizeof(char)*size);
	}
	else
	{
		pocket->previous=bag->end;
		pocket->next=NULL;
		pocket->len=0;
		pocket->data=(char*)malloc(sizeof(char)*size);
		bag->end->next=pocket;
		bag->end=pocket;
		bag->n_pockets=bag->n_pockets+1;
	}

}

int insert_mid_pocket(struct mid_bag* bag,struct mid_pocket* refer,struct mid_pocket* update,int flag)
{

	if(refer==NULL)
		return 0;

	if(flag==INSERT_BEFORE)
	{
		struct mid_pocket* prev=refer->previous;

		if(prev==NULL)
		{
			update->previous=NULL;
			update->next=refer;
			refer->previous=update;
			bag->first=update;
			bag->n_pockets=bag->n_pockets+1;
		}
		else
		{
			prev->next=update;
			update->previous=prev;
			update->next=refer;
			refer->previous=update;
			bag->n_pockets=bag->n_pockets+1;
		}

		return 1;
	}
	else if(flag==INSERT_AFTER)
	{
		struct mid_pocket* after=refer->next;

		if(after==NULL)
		{
			refer->next=update;
			update->previous=refer;
			update->next=NULL;
			bag->end=update;
			bag->n_pockets=bag->n_pockets+1;
		}
		else
		{
			refer->next=update;
			update->previous=refer;
			update->next=after;
			after->previous=update;
			bag->n_pockets=bag->n_pockets+1;
		}

		return 1;
	}
	else if(flag==INSERT_AT)
	{
		struct mid_pocket* prev=refer->previous;
		struct mid_pocket* after=refer->next;

		if(prev==NULL && after==NULL)
		{
			update->previous=NULL;
			update->next=NULL;
			bag->first=update;
			bag->end=update;
		}
		else if(prev==NULL)
		{
			bag->first=update;
			update->next=refer->next;
			update->previous=NULL;
			refer->next->previous=update;
		}
		else if(after==NULL)
		{
			bag->end=update;
			update->previous=refer->previous;
			refer->previous->next=update;
			update->next=NULL;
		}
		else
		{
			prev->next=update;
			update->previous=prev;
			update->next=after;
			after->previous=update;
		}

		return 1;
	}

	return 0;

}

int delete_mid_pocket(struct mid_bag* bag,struct mid_pocket* refer,int flag)
{
	if(bag==NULL || bag->n_pockets==0 || refer==NULL)
		return 0;

	if(flag==DELETE_BEFORE)
	{
		struct mid_pocket* prev=refer->previous;

		if(prev==NULL)
			return 0;

		struct mid_pocket* prev2=prev->previous;

		if(prev2==NULL)
		{
			refer->previous=NULL;
			bag->first=refer;
			bag->n_pockets=bag->n_pockets-1;
		}
		else
		{
			prev2->next=refer;
			refer->previous=prev2;
			bag->n_pockets=bag->n_pockets-1;
		}

		return 1;

	}
	else if(flag==DELETE_AFTER)
	{
		struct mid_pocket* after=refer->next;

		if(after==NULL)
			return 0;

		struct mid_pocket* after2=after->next;

		if(after2==NULL)
		{
			refer->next=NULL;
			bag->end=refer;
			bag->n_pockets=bag->n_pockets-1;
		}
		else
		{
			refer->next=after2;
			after2->previous=refer;
			bag->n_pockets=bag->n_pockets-1;
		}

		return 1;
	}
	else if(flag==DELETE_AT)
	{
		struct mid_pocket* prev=refer->previous;
		struct mid_pocket* after=refer->next;

		if(prev==NULL && after==NULL)
		{
			bag->first=NULL;
			bag->end=NULL;
			bag->n_pockets=bag->n_pockets-1;
		}
		else if(prev==NULL)
		{
			bag->first=after;
			after->previous=NULL;
			bag->n_pockets=bag->n_pockets-1;
		}
		else if(after==NULL)
		{
			bag->end=prev;
			prev->next=NULL;
			bag->n_pockets=bag->n_pockets-1;
		}
		else
		{
			prev->next=after;
			after->previous=prev;
			bag->n_pockets=bag->n_pockets-1;
		}

		return 1;
	}

	return 0;
}

int place_mid_data(struct mid_bag *bag,struct mid_data *n_data)
{
	if(bag==NULL || n_data==NULL || n_data->data==NULL || n_data->len==0)
		return -1;

	append_mid_pocket(bag,n_data->len);

	memcpy(bag->end->data,n_data->data,n_data->len);

	bag->end->len=n_data->len;

	return n_data->len;

}

struct mid_data* flatten_mid_bag(struct mid_bag *bag)
{

	if(bag==NULL)
		return NULL;

	struct mid_data* n_data=(struct mid_data*)malloc(sizeof(struct mid_data));

	struct mid_pocket *pocket=bag->first;
	long len=0;

	while(pocket!=NULL)
	{
		len=len+pocket->len;
		pocket=pocket->next;
	}

	if(len==0)
	{
		n_data->data=NULL;
		n_data->len=0;
		return n_data;
	}

	n_data->data=(char*)malloc(sizeof(char)*len);
	n_data->len=len;

	pocket=bag->first;

	long counter=0;

	while(pocket!=NULL)
	{
		memcpy(n_data->data+counter,pocket->data,pocket->len);
		counter=counter+pocket->len;
		pocket=pocket->next;
	}

	return n_data;
}
