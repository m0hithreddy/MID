/*
 * MID_state.c
 *
 *  Created on: 13-Apr-2020
 *      Author: Mohith Reddy
 */

#include"MID_state.h"
#include"MID_ssl_socket.h"
#include"MID_http.h"
#include"MID_unit.h"
#include"MID.h"
#include"MID_structures.h"
#include"MID_err.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/file.h>
#include<sys/stat.h>
#include<sys/types.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#ifdef LIBSSL_SANE
#include<openssl/md5.h>
#endif

char* get_ms_filename()
{
	char* ms_file;

	if(args->ms_file==NULL)
	{
#ifdef LIBSSL_SANE
		unsigned char md_result[MD5_DIGEST_LENGTH];

		MD5_CTX md_context;

		MD5_Init(&md_context);

		MD5_Update(&md_context, args->url, strlen(args->url));
		MD5_Final(md_result, &md_context);

		ms_file=(char*)malloc(sizeof(char)*(4+(MD5_DIGEST_LENGTH*8)/4));

		for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
			sprintf(ms_file+2*i,"%02x", md_result[i]);

		ms_file[MD5_DIGEST_LENGTH]='.';
		ms_file[MD5_DIGEST_LENGTH+1]='m';
		ms_file[MD5_DIGEST_LENGTH+2]='s';
		ms_file[MD5_DIGEST_LENGTH+3]='\0';

#else
		ms_file=(char*)malloc(sizeof(char)*(4+strlen(gl_s_request->url)));
		memcpy(ms_file,url_to_filename(gl_s_request->url),strlen(gl_s_request->url));

		ms_file[strlen(gl_s_request->url)]='.';
		ms_file[strlen(gl_s_request->url)+1]='m';
		ms_file[strlen(gl_s_request->url)+2]='s';
		ms_file[strlen(gl_s_request->url)+3]='\0';
#endif
	}
	else
	{
		ms_file=strdup(args->ms_file);
	}

	return ms_file;
}

void save_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress)
{
	if(gl_s_request==NULL || gl_s_response == NULL || gl_s_response->status_code[0]!='2') // Enough progress isn't made or not an expected status code to save the state.
		return;

	if(gl_s_response->accept_ranges==NULL || strcmp(gl_s_response->accept_ranges,"bytes") || gl_s_response->content_length==NULL) // Non resumable download
		return;

	if(base_unit_info == NULL || base_unit_info->file == NULL || (gl_s_response->content_encoding!=NULL && base_unit_info->up_file==NULL)) // Even files are not decided yet!
		return;

	if(units_bag==NULL) // No download is initiated yet!
		return;

	if(atol(gl_s_response->content_length)==progress->content_length && gl_s_response->content_encoding==NULL) // Already downloaded and decoding is not required.
		return;

	// If pased the above then eligible for saving state

	char* ms_file=get_ms_filename();

	FILE* ms_fp=fopen(ms_file,"a+");

	if(ms_fp==NULL) // May be file name too long... Returning to prevent any state files clashes
	{
		mid_cond_print(!args->quiet_flag,"MID: Unable to open MID state file %s, not saving the state information\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		mid_cond_print(!args->quiet_flag,"MID: Unable to acquire lock on MID state file %s, not saving the state information\n\n",ms_file);

		fclose(ms_fp);

		return;
	}

#ifdef LIBSSL_SANE
	if(args->detailed_save)
		dump_detailed_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
	else
		dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
#else
	dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
#endif

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;
}

void dump_int(struct data_bag* bag,int num)
{
	struct network_data n_data;

	n_data.data=(void*)&num;
	n_data.len=sizeof(int);
	place_data(bag,&n_data);
}

void dump_long(struct data_bag* bag,long num)
{
	struct network_data n_data;

	n_data.data=(void*)&num;
	n_data.len=sizeof(long);
	place_data(bag,&n_data);
}

void dump_string(struct data_bag* bag,char* string)
{
	struct network_data n_data;
	long len=strlen(string)+1;

	dump_long(bag,len);

	n_data.data=(void*)string;
	n_data.len=len;
	place_data(bag,&n_data);
}

char* extract_string(FILE* ms_fp)
{
	long len;

	if(read(fileno(ms_fp),&len,sizeof(long))!=sizeof(long))
		return NULL;

	if(len==0)
		return NULL;

	char* string=(char*)malloc(sizeof(char)*len);

	if(read(fileno(ms_fp),string,len)!=len)
		return NULL;

	return string;
}

void dump_mid_state(FILE* ms_fp,struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress)
{

	struct network_data* tmp=flatten_data_bag(units_bag);
	struct unit_info** units= (struct unit_info**)(tmp==NULL ? NULL : tmp->data);
	long units_len= tmp==NULL ? 0 : units_bag->n_pockets;

	struct data_bag* state_bag=create_data_bag();
	struct network_data* state_data;

	// Type of file 0 => MID state file | 1 => MID detailed state file

	dump_int(state_bag,0);

	// in_url_len + in_url

	dump_string(state_bag,args->url);

	// fin_url_len + fin_url

	dump_string(state_bag,gl_s_request->url);

	// file_len + file

	dump_string(state_bag,base_unit_info->file);

	// up_file_len + up_file

	if(base_unit_info->up_file==NULL)
		dump_string(state_bag,"");
	else
		dump_string(state_bag,base_unit_info->up_file);

	// content_length

	dump_long(state_bag,atol(gl_s_response->content_length));

	// downloaded_length

	dump_long(state_bag,progress->content_length);

	// number of left over ranges + left over ranges

	struct data_bag* ranges_bag=create_data_bag();

	if(units==NULL || units_len==0)
	{
		dump_long(state_bag,1);

		dump_long(state_bag,0);

		dump_long(state_bag,atol(gl_s_response->content_length)-1);

	}
	else
	{
		long range_counter=0;

		for(long i=0;i<units_len;i++)
		{
			if(units[i]->range->end-units[i]->range->start+1-units[i]->current_size > 0)
			{
				range_counter++;

				dump_long(ranges_bag,units[i]->range->start+units[i]->current_size);

				dump_long(ranges_bag,units[i]->range->end);
			}
		}

		dump_long(state_bag,range_counter);

		if(range_counter!=0)
		{
			place_data(state_bag,flatten_data_bag(ranges_bag));
		}
	}

	// Dump the data

	state_data=flatten_data_bag(state_bag);

	write(fileno(ms_fp),&state_data->len,sizeof(long)); // Structure size

	write(fileno(ms_fp),state_data->data,state_data->len); // Structure data

}

struct ms_entry* process_ms_entry(FILE* ms_fp)
{
	struct ms_entry* en=(struct ms_entry*)calloc(1,sizeof(struct ms_entry));

	en->type=0;

	long en_len=4; // Type of file

	// Initial URL

	en->in_url=extract_string(ms_fp);

	if(en->in_url==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->in_url)+1;

	// Final URL

	en->fin_url=extract_string(ms_fp);

	if(en->fin_url==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->fin_url)+1;

	// File

	en->file=extract_string(ms_fp);

	if(en->file==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->file)+1;

	// Unprocessed File

	en->up_file=extract_string(ms_fp);

	if(en->up_file==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->up_file)+1;

	// Content Length

	if(read(fileno(ms_fp),&en->content_length,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	// Download Length

	if(read(fileno(ms_fp),&en->downloaded_length,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	// Number of left over ranges + left over ranges

	if(read(fileno(ms_fp),&en->n_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	en->ranges=(struct http_range*)malloc(sizeof(struct http_range)*en->n_ranges);

	for(long i=0;i<en->n_ranges;i++)
	{
		if(read(fileno(ms_fp),&en->ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		en_len=en_len+sizeof(long);

		if(read(fileno(ms_fp),&en->ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		en_len=en_len+sizeof(long);
	}

	en->en_len=en_len;

	return en;
}

void print_ms_entry(struct ms_entry* en)
{
	printf("\n|-->Type: MID state information entry (0)");

	printf("\n|-->Initial URL: %s",en->in_url);

	printf("\n|-->Redirected URL: %s",en->fin_url);

	printf("\n|-->File: %s",en->file);

	printf("\n|-->Unprocessed-File: %s",en->up_file[0]=='\0' ? "-" : en->up_file);

	printf("\n|-->Content-Length: %s",convert_data(en->content_length,0));

	printf("\n|-->Downloaded-Length: %s",convert_data(en->downloaded_length,0));

	printf("\n|-->Number of left over ranges: %ld",en->n_ranges);

	printf("\n|-->Left over ranges:");

	for(long i=0;i<en->n_ranges;i++)
	{
		printf(" (%ld, %ld)",en->ranges[i].start,en->ranges[i].end);

		if(i!=en->n_ranges-1)
			printf(" |");
	}

	printf("\n");
}

void check_ms_entry(struct ms_entry* en)
{

}

#ifdef LIBSSL_SANE
void dump_detailed_mid_state(FILE* ms_fp,struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct data_bag* units_bag,struct units_progress* progress)
{
	struct network_data* tmp=flatten_data_bag(units_bag);
	struct unit_info** units= (struct unit_info**)(tmp==NULL ? NULL : tmp->data);
	long units_len= tmp==NULL ? 0 : units_bag->n_pockets;


	struct data_bag* state_bag=create_data_bag();
	struct network_data* state_data;

	// Type of file 0 => MID state file | 1 => MID detailed state file

	dump_int(state_bag,1);

	// in_url_len + in_url

	dump_string(state_bag,args->url);

	// fin_url_len + fin_url

	dump_string(state_bag,gl_s_request->url);

	// file_len + file

	dump_string(state_bag,base_unit_info->file);

	// up_file_len + up_file

	if(base_unit_info->up_file==NULL)
		dump_string(state_bag,"");
	else
		dump_string(state_bag,base_unit_info->up_file);

	// content_length

	dump_long(state_bag,atol(gl_s_response->content_length));

	// downloaded_length

	dump_long(state_bag,progress->content_length);

	// number of left over ranges + left over ranges

	struct data_bag* ranges_bag=create_data_bag();

	if(units==NULL || units_len==0)
	{
		dump_long(state_bag,1);

		dump_long(state_bag,0);

		dump_long(state_bag,atol(gl_s_response->content_length)-1);

	}
	else
	{
		long range_counter=0;

		for(long i=0;i<units_len;i++)
		{
			if(units[i]->range->end-units[i]->range->start+1-units[i]->current_size > 0)
			{
				range_counter++;

				dump_long(ranges_bag,units[i]->range->start+units[i]->current_size);

				dump_long(ranges_bag,units[i]->range->end);
			}
		}

		dump_long(state_bag,range_counter);

		if(range_counter!=0)
		{
			place_data(state_bag,flatten_data_bag(ranges_bag));
		}
	}

	// Number of Ranges + Ranges along with their hashes

	dump_long(state_bag,progress->n_ranges);

	FILE* fp=NULL;

	if(gl_s_response->content_encoding==NULL)
		fp=fopen(base_unit_info->file,"r");
	else
		fp=fopen(base_unit_info->up_file,"r");


	if(fp==NULL)
	{
		dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
		return;
	}

	unsigned char md_result[MD5_DIGEST_LENGTH];
	long chunk_len;
	long status;

	char f_buf[MAX_TRANSACTION_SIZE > (1+MD5_DIGEST_LENGTH*8/4) ? MAX_TRANSACTION_SIZE : (1+MD5_DIGEST_LENGTH*8/4)];

	struct stat stat_buf;

	fstat(fileno(fp),&stat_buf);

	for(long i=0;i<progress->n_ranges;i++)
	{

		if(progress->ranges[i].start >= stat_buf.st_size)
		{
			dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
			return;
		}

		dump_long(state_bag,progress->ranges[i].start);

		dump_long(state_bag,progress->ranges[i].end);

		chunk_len=progress->ranges[i].end - progress->ranges[i].start + 1;

		if(lseek(fileno(fp),progress->ranges[i].start,SEEK_SET)!=progress->ranges[i].start)
		{
			dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
			return;
		}

		// Determine MD5 hash

		MD5_CTX md_context;

		MD5_Init(&md_context);

		while(1)
		{
			if(chunk_len<0)
			{
				dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
				return;
			}
			else if(chunk_len==0)
				break;

			status=read(fileno(fp),f_buf,sizeof(f_buf) < chunk_len ? sizeof(f_buf) : chunk_len);

			if(status<=0)
			{
				dump_mid_state(ms_fp,gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
				return;
			}

			chunk_len=chunk_len-status;

			MD5_Update(&md_context,f_buf,status);
		}

		MD5_Final(md_result, &md_context);

		for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
			sprintf(f_buf+2*i,"%02x", md_result[i]);

		dump_string(state_bag,f_buf);
	}

	// Dump the data

	state_data=flatten_data_bag(state_bag);

	write(fileno(ms_fp),&state_data->len,sizeof(long)); // Structure size

	write(fileno(ms_fp),state_data->data,state_data->len); // Structure data

}

struct d_ms_entry* process_detailed_ms_entry(FILE* ms_fp)
{
	struct d_ms_entry* d_en=(struct d_ms_entry*)calloc(1,sizeof(struct d_ms_entry));

	d_en->type=1;

	long d_en_len=4; // Type of file

	// Initial URL

	d_en->in_url=extract_string(ms_fp);

	if(d_en->in_url==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->in_url)+1;

	// Final URL

	d_en->fin_url=extract_string(ms_fp);

	if(d_en->fin_url==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->fin_url)+1;

	// File

	d_en->file=extract_string(ms_fp);

	if(d_en->file==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->file)+1;

	// Unprocessed File

	d_en->up_file=extract_string(ms_fp);

	if(d_en->up_file==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->up_file)+1;

	// Content Length

	if(read(fileno(ms_fp),&d_en->content_length,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	// Download Length

	if(read(fileno(ms_fp),&d_en->downloaded_length,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	// Number of left over ranges + left over ranges

	if(read(fileno(ms_fp),&d_en->n_left_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	d_en->left_ranges=(struct http_range*)malloc(sizeof(struct http_range)*d_en->n_left_ranges);

	for(long i=0;i<d_en->n_left_ranges;i++)
	{
		if(read(fileno(ms_fp),&d_en->left_ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);

		if(read(fileno(ms_fp),&d_en->left_ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);
	}

	// Number of Ranges + Ranges along with their hash

	if(read(fileno(ms_fp),&d_en->n_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	d_en->ranges=(struct http_range*)malloc(sizeof(struct http_range)*d_en->n_ranges);
	d_en->hashes=(char**)malloc(sizeof(char*)*d_en->n_ranges);

	for(long i=0;i<d_en->n_ranges;i++)
	{
		if(read(fileno(ms_fp),&d_en->ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);

		if(read(fileno(ms_fp),&d_en->ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);

		d_en->hashes[i]=extract_string(ms_fp);

		d_en_len=d_en_len+sizeof(long)+strlen(d_en->hashes[i])+1;
	}

	d_en->d_en_len=d_en_len;

	return d_en;
}

void print_detailed_ms_entry(struct d_ms_entry* d_en)
{
	printf("\n|-->Type: MID detailed state information entry (1)");

	printf("\n|-->Initial URL: %s",d_en->in_url);

	printf("\n|-->Redirected URL: %s",d_en->fin_url);

	printf("\n|-->File: %s",d_en->file);

	printf("\n|-->Unprocessed-File: %s",d_en->up_file[0]=='\0' ? "-" : d_en->up_file);

	printf("\n|-->Content-Length: %s",convert_data(d_en->content_length,0));

	printf("\n|-->Downloaded-Length: %s",convert_data(d_en->downloaded_length,0));

	printf("\n|-->Number of left over ranges: %ld",d_en->n_left_ranges);

	printf("\n|-->Left over ranges:");

	for(long i=0;i<d_en->n_left_ranges;i++)
	{
		printf(" (%ld, %ld)",d_en->left_ranges[i].start,d_en->left_ranges[i].end);

		if(i!=d_en->n_left_ranges-1)
			printf(" |");
	}

	printf("\n|-->Number of fetched ranges: %ld",d_en->n_ranges);

	printf("\n|-->Fetched ranges:");

	for(long i=0;i<d_en->n_ranges;i++)
	{
		printf(" (%ld, %ld) [%s]",d_en->ranges[i].start,d_en->ranges[i].end,d_en->hashes[i]);

		if(i!=d_en->n_ranges-1)
			printf(" |");
	}

	printf("\n");
}
#endif

void* read_ms_file(char* ms_file,long entry_number,int flag)
{
	FILE* ms_fp=fopen(ms_file,"r");

	if(ms_fp==NULL)
	{
		mid_cond_print(flag==MS_PRINT,"MID: Unable to open the MID state file %s. Exiting...\n\n",ms_file);

		return NULL;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		mid_cond_print(flag==MS_PRINT,"MID: Unable to acquire lock on the MID state file %s. Exiting...\n\n",ms_file);

		return NULL;
	}

	void* en_info=malloc(sizeof(long)+sizeof(int));

	struct ms_entry* en=NULL;
	struct d_ms_entry* d_en=NULL;
	long status;

	long en_counter=1;
	long read_len=0;

	struct stat stat_buf;
	fstat(fileno(ms_fp),&stat_buf);

	while(1)
	{

		if(entry_number) // If a specific entry number is given
		{
			if(entry_number!=en_counter) // Seek all entries before the given entry number
			{

				if((status=read(fileno(ms_fp),en_info,sizeof(long)))==0)
				{
					mid_cond_print(flag==MS_PRINT,"\nMID: End of the file, entry number %ld not found. Exiting...\n\n",entry_number);

					goto normal_exit;
				}
				else if(status!=sizeof(long))
				{
					goto fatal_error;
				}

				read_len=read_len+sizeof(long);

				if(read_len+*((long*)(en_info)) > stat_buf.st_size)
					goto fatal_error;

				read_len=read_len+*((long*)(en_info));

				if(lseek(fileno(ms_fp),read_len,SEEK_SET)!=read_len)
					goto fatal_error;

				en_counter++;

				continue;
			}
		}

		if((status=read(fileno(ms_fp),en_info,sizeof(long)+sizeof(int)))==0)
		{
			if(entry_number)
				mid_cond_print(flag==MS_PRINT,"MID: End of the file, entry number %ld not found. Exiting...\n\n",entry_number);

			goto normal_exit;
		}
		else if(status!=sizeof(long)+sizeof(int))
		{
			goto fatal_error;
		}

		if(*((int*)(en_info+sizeof(long)))==0)
		{
			en=process_ms_entry(ms_fp);

			if(en==NULL || en->en_len != *((long*)en_info))
				goto fatal_error;

			if(flag==MS_PRINT)
			{
				printf("Entry Count: %ld\n",en_counter);
				print_ms_entry(en);
				printf("\n");

				en_counter++;
			}
			else
			{
				flock(fileno(ms_fp),LOCK_UN);
				fclose(ms_fp);

				return (void*)en;
			}
		}
#ifdef LIBSSL_SANE
		else if(*((int*)(en_info+sizeof(long)))==1)
		{
			d_en=process_detailed_ms_entry(ms_fp);

			if(d_en==NULL || d_en->d_en_len != *((long*)en_info))
				goto fatal_error;

			if(flag==MS_PRINT)
			{
				printf("Entry Count: %ld\n",en_counter);
				print_detailed_ms_entry(d_en);
				printf("\n");
				en_counter++;
			}
			else
			{
				flock(fileno(ms_fp),LOCK_UN);
				fclose(ms_fp);

				return (void*)d_en;
			}
		}
#endif
		else
		{
			goto fatal_error;
		}

		if(entry_number) // If a entry number is set exit after printing the information
		{
			goto normal_exit;
		}
	}

	fatal_error:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	mid_cond_print(flag==MS_PRINT,"MID: %s is broken, errors encountered when processing the MID state file. Exiting...\n\n",ms_file);

	return NULL;

	normal_exit:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	if(flag==MS_PRINT)
	{
		exit(0);
	}
	else
		return NULL;

}

void clear_ms_entry(char* ms_file,long entry_number,int flag)
{
	if(entry_number<=0)
	{
		mid_cond_print(flag==MS_PRINT,"MID: Entry number %ld is not valid. Exiting...\n\n");

		return;
	}

	FILE* ms_fp=fopen(ms_file,"r");

	if(ms_fp==NULL) // No file exits
	{
		mid_cond_print(flag==MS_PRINT,"MID: Unable to open MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	fclose(ms_fp);

	ms_fp=fopen(ms_file,"r+");

	if(ms_fp==NULL) // Cant open file in append mode
	{
		mid_cond_print(flag==MS_PRINT,"MID: Unable to open MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0) // Unable to acquire lock
	{
		mid_cond_print(flag==MS_PRINT,"MID: Unable to acquire lock on MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	struct stat stat_buf;
	fstat(fileno(ms_fp),&stat_buf);

	void* en_info=malloc(sizeof(long));
	long prev_len=0;
	long read_len=0;
	long status=0;
	long en_counter=0;

	while(en_counter!=entry_number) // Seek to the entry that has to be cleared.
	{
		prev_len=read_len;

		if((status=pread(fileno(ms_fp),en_info,sizeof(long),read_len))==0) // EOF reached, no such entry
		{
			mid_cond_print(flag==MS_PRINT,"MID: EOF file reached, entry number %ld not found. Exiting...\n\n",entry_number);

			goto normal_return;
		}
		else if(status!=sizeof(long)) // File corrupted, but do nothing.
		{
			mid_cond_print(flag==MS_PRINT,"MID: %s is not a MID state file. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		read_len=read_len+sizeof(long);

		if(read_len+*((long*)(en_info)) > stat_buf.st_size) // File corrupted, but do nothing.
		{
			mid_cond_print(flag==MS_PRINT,"MID: %s is not a MID state file. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		read_len=read_len+*((long*)(en_info));

		en_counter++;
	}

	char buf[MAX_TRANSACTION_SIZE];

	while(1) // Start copying data from read_len -> prev_len.
	{

		status=pread(fileno(ms_fp),buf,MAX_TRANSACTION_SIZE,read_len);

		if(status<0)
		{
			mid_cond_print(flag==MS_PRINT,"MID: Unable to read MID state file %s. Exiting...\n\n",ms_file);

			goto normal_return;
		}
		else if(status==0)
			break;


		read_len=read_len+status;

		if(pwrite(fileno(ms_fp),buf,status,prev_len)!=status) // Cant write to the file.
		{
			mid_cond_print(flag==MS_PRINT,"MID: Unable to write to MID state file %s. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		prev_len=prev_len+status;
	}

	if(prev_len==0)
	{
		flock(fileno(ms_fp),LOCK_UN);
		fclose(ms_fp);

		remove(ms_file);

		mid_cond_print(flag==MS_PRINT,"MID: %s has no more entries left. Deleting...\n\n",ms_file);

		return;
	}

	ftruncate(fileno(ms_fp),prev_len);

	normal_return:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;

}
