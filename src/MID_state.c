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

int mid_error=0;

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
		ms_file=(char*)malloc(sizeof(char)*(4+strlen(args->url)));
		memcpy(ms_file,url_to_filename(args->url),strlen(args->url));

		ms_file[strlen(args->url)]='.';
		ms_file[strlen(args->url)+1]='m';
		ms_file[strlen(args->url)+2]='s';
		ms_file[strlen(args->url)+3]='\0';
#endif
	}
	else
	{
		ms_file=strdup(args->ms_file);
	}

	return ms_file;
}

void save_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct mid_bag* units_bag,struct units_progress* progress)
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
		mid_err("MID: Unable to open MID state file %s, not saving the state information\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		mid_err("MID: Unable to acquire lock on MID state file %s, not saving the state information\n\n",ms_file);

		fclose(ms_fp);

		return;
	}

	struct mid_bag* state_bag;

#ifdef LIBSSL_SANE
	if(args->detailed_save)
		state_bag=make_d_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
	else
		state_bag=make_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
#else
	state_bag=make_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
#endif

	struct network_data* state_data=flatten_mid_bag(state_bag);

	if(state_data==NULL || state_data->data==NULL)
	{
		mid_err("MID: Error when making MID state, not saving the state information. Returning...\n\n");

		return;
	}

	write(fileno(ms_fp),&state_data->len,sizeof(long));
	write(fileno(ms_fp),state_data->data,state_data->len);

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;
}

void resave_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct mid_bag* units_bag,struct units_progress* progress)
{
	if(gl_s_request==NULL || gl_s_response == NULL || gl_s_response->status_code[0]!='2') // Enough progress isn't made or not an expected status code to re-save the state.
		return;

	if(gl_s_response->accept_ranges==NULL || strcmp(gl_s_response->accept_ranges,"bytes") || gl_s_response->content_length==NULL) // Non resumable download
		return;

	if(base_unit_info == NULL || base_unit_info->file == NULL || (gl_s_response->content_encoding!=NULL && base_unit_info->up_file==NULL)) // Even files are not decided yet!
		return;

	if(units_bag==NULL) // Nothing to save !
		return;



	char* ms_file=get_ms_filename();

	if(atol(gl_s_response->content_length)==progress->content_length && u_fp==NULL) // If already downloaded and decoding is done (or) not required. remove the entry.
	{
		delete_ms_entry(ms_file,args->entry_number <=0 ? 1 : args->entry_number, args->quiet_flag ? MS_SILENT : MS_PRINT);
		return;
	}

	// If pased the above then eligible for saving state

	FILE* ms_fp=fopen(ms_file,"r+");

	if(ms_fp==NULL) // May be file name too long... Returning to prevent any state files clashes
	{
		mid_err("MID: Unable to open MID state file %s, not saving the state information\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		mid_err("MID: Unable to acquire lock on MID state file %s, not saving the state information\n\n",ms_file);

		fclose(ms_fp);

		return;
	}

	struct mid_bag* state_bag;

#ifdef LIBSSL_SANE
	if(args->detailed_save)
		state_bag=make_d_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
	else
		state_bag=make_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
#else
	state_bag=make_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
#endif

	struct network_data* state_data=flatten_mid_bag(state_bag);

	if(state_data==NULL || state_data->data==NULL)
	{
		mid_err("MID: Error when making MID state, not saving the state information. Returning...\n\n");

		return;
	}

	long entry_number = args->entry_number <=0 ? 1 : args->entry_number;

	long read_len=0;
	long prev_len=0;
	long en_size;
	long status;

	struct stat stat_buf;

	fstat(fileno(ms_fp),&stat_buf);

	while(entry_number)
	{
		prev_len=read_len;

		if((status=pread(fileno(ms_fp),&en_size,sizeof(long),read_len))==0) // EOF file.
			return;
		else if(status!=sizeof(long)) // Corrupted file.
			return;

		read_len=read_len+sizeof(long);

		if(read_len + en_size > stat_buf.st_size ) // Corrupted file.
			return;

		read_len=read_len+en_size;

		entry_number--;
	}

	if(pwrite(fileno(ms_fp),&state_data->len,sizeof(long),prev_len)!=sizeof(long))
	{
		mid_err("MID: Error writing to MS entry file %s. Returning...\n\n",ms_file);

		goto close_files;
	}

	prev_len=prev_len+sizeof(long);

	if(state_data->len > en_size) // sizeof(long) is neglected on both sides of the comparison & The last computed en_size is of the entry we intend to replace.
	{
		char p_buf[state_data->len > MAX_TRANSACTION_SIZE ? state_data->len : MAX_TRANSACTION_SIZE];
		char c_buf[sizeof(p_buf)];
		long p_status,c_status;

		memcpy(p_buf,state_data->data,state_data->len);
		p_status=state_data->len;

		int flag=1;

		while(1)
		{
			if(flag)
			{

				if(read_len>=stat_buf.st_size)
				{
					c_status=0;
					goto p_write;
				}

				c_status=pread(fileno(ms_fp),c_buf,(sizeof(c_buf) < stat_buf.st_size-read_len)? sizeof(c_buf) : stat_buf.st_size-read_len,read_len);

				p_write:

				if(pwrite(fileno(ms_fp),p_buf,p_status,prev_len)!=p_status)
				{
					mid_err("MID: Error writing to MS entry file %s. Returning...\n\n",ms_file);

					goto close_files;
				}

				prev_len=prev_len+p_status;

				if(c_status<0)
				{
					mid_err("MID: Error reading MS entry file %s. Returning...\n\n",ms_file);

					goto close_files;
				}
				else if(c_status==0)
				{
					break;
				}

				read_len=read_len+c_status;

				flag=0;
			}
			else
			{

				if(read_len>=stat_buf.st_size)
				{
					p_status=0;
					goto c_write;
				}

				p_status=pread(fileno(ms_fp),p_buf,(sizeof(p_buf) < stat_buf.st_size-read_len)? sizeof(p_buf) : stat_buf.st_size-read_len,read_len);

				c_write:

				if(pwrite(fileno(ms_fp),c_buf,c_status,prev_len)!=c_status)
				{
					mid_err("MID: Error writing to MS entry file %s. Returning...\n\n",ms_file);

					goto close_files;
				}

				prev_len=prev_len+c_status;

				if(p_status<0)
				{
					mid_err("MID: Error reading MS entry file %s. Returning...\n\n",ms_file);

					goto close_files;
				}
				else if(p_status==0)
				{
					break;
				}

				read_len=read_len+p_status;

				flag=1;
			}
		}
	}
	else if(state_data->len < en_size)
	{
		if(pwrite(fileno(ms_fp),state_data->data,state_data->len,prev_len)!=state_data->len)
		{
			mid_err("MID: Error writing to MS entry file %s. Returning...\n\n",ms_file);

			goto close_files;
		}

		prev_len=prev_len+state_data->len;

		char f_buf[MAX_TRANSACTION_SIZE];

		while(1)
		{
			status=pread(fileno(ms_fp),f_buf,MAX_TRANSACTION_SIZE,read_len);

			if(status<0)
			{
				mid_err("MID: Error reading MS entry file %s. Returning...\n\n",ms_file);

				goto close_files;
			}
			else if(status==0) // If less than 0, it may corrupt all other remaining MS entries (no mechanism for rolling back!)
				break;

			read_len=read_len+status;

			if(pwrite(fileno(ms_fp),f_buf,status,prev_len)!=status)
			{
				mid_err("MID: Error writing to MS entry file %s. Returning...\n\n",ms_file);

				goto close_files;
			}

			prev_len=prev_len+status;
		}

		ftruncate(fileno(ms_fp),prev_len);
	}
	else
	{
		if(pwrite(fileno(ms_fp),state_data->data,state_data->len,prev_len)!=state_data->len)
		{
			mid_err("MID: Error writing to MS entry file %s. Returning...\n\n",ms_file);

			goto close_files;
		}
	}

	close_files:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;
}

void dump_int(struct mid_bag* bag,int num)
{
	struct network_data n_data;

	n_data.data=(void*)&num;
	n_data.len=sizeof(int);
	place_data(bag,&n_data);
}

void dump_long(struct mid_bag* bag,long num)
{
	struct network_data n_data;

	n_data.data=(void*)&num;
	n_data.len=sizeof(long);
	place_data(bag,&n_data);
}

void dump_string(struct mid_bag* bag,char* string)
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

struct mid_bag* make_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct mid_bag* units_bag,struct units_progress* progress)
{

	struct network_data* tmp=flatten_mid_bag(units_bag);
	struct unit_info** units= (struct unit_info**)(tmp==NULL ? NULL : tmp->data);
	long units_len= tmp==NULL ? 0 : units_bag->n_pockets;

	struct mid_bag* state_bag=create_mid_bag();
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

	struct mid_bag* ranges_bag=create_mid_bag();

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


		if(range_counter!=0)
		{
			struct units_progress* l_ranges=(struct units_progress*)malloc(sizeof(struct units_progress));

			l_ranges->ranges=(struct http_range*)flatten_mid_bag(ranges_bag)->data;
			l_ranges->n_ranges=range_counter;

			l_ranges=merge_units_progress(l_ranges);

			struct network_data n_data;
			n_data.data=l_ranges->ranges;
			n_data.len=sizeof(struct http_range)*l_ranges->n_ranges;

			dump_long(state_bag,l_ranges->n_ranges);
			place_data(state_bag,&n_data);
		}
		else
		{
			dump_long(state_bag,0);
		}

	}

	// Return the data;

	return state_bag;
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

	if(read(fileno(ms_fp),&en->n_l_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	en->l_ranges=(struct http_range*)malloc(sizeof(struct http_range)*en->n_l_ranges);

	for(long i=0;i<en->n_l_ranges;i++)
	{
		if(read(fileno(ms_fp),&en->l_ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		en_len=en_len+sizeof(long);

		if(read(fileno(ms_fp),&en->l_ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		en_len=en_len+sizeof(long);
	}

	en->en_len=en_len;

	return en;
}

void print_ms_entry(struct ms_entry* en)
{
	if(en->type==0)
		printf("\n|-->Type: MID State Entry (0)");
	else if(en->type==1)
		printf("\n|-->Type: MID Detailed State Entry (1)");
	else
		return;

	printf("\n|-->Initial URL: %s",en->in_url);

	printf("\n|-->Redirected URL: %s",en->fin_url);

	printf("\n|-->File: %s",en->file);

	printf("\n|-->Unprocessed-File: %s",en->up_file[0]=='\0' ? "-" : en->up_file);

	printf("\n|-->Content-Length: %s",convert_data(en->content_length,0));

	printf("\n|-->Downloaded-Length: %s",convert_data(en->downloaded_length,0));

	printf("\n|-->Number of left over ranges: %ld",en->n_l_ranges);

	printf("\n|-->Left over ranges:");

	for(long i=0;i<en->n_l_ranges;i++)
	{
		printf(" (%ld, %ld)",en->l_ranges[i].start,en->l_ranges[i].end);

		if(i!=en->n_l_ranges-1)
			printf(" |");
	}

	printf("\n");
}

int validate_ms_entry(struct ms_entry* en,struct http_request* gl_s_request,struct http_response* gl_s_response,int flag)
{
	if(en->type!=0 && en->type!=1)
		return -1;

	int return_code=0;

	if(flag==MS_PRINT)
	{
		if(en->type==0)
			printf("\n|-->Type: Validation of MID State Entry (0)");
		else
			printf("\n|-->Type: Validation of MID Detailed State Entry (1)");
	}

	// Initial URL ;

	if(flag==MS_PRINT)
	{
		printf("\n|-->Initial URL: %s",en->in_url);
		printf(" || ");
		printf("%s",args->url);
	}
	else
	{
		if(strcmp(en->in_url,args->url))
		{
			if(!args->force_resume)
				return -1;

			return_code=1;
		}
	}

	// Final URL ;

	if(flag==MS_PRINT)
	{
		printf("\n|-->Redirected URL: %s",en->fin_url);
		printf(" || ");
		printf("%s",gl_s_request == NULL || gl_s_request->url == NULL ? "-" : gl_s_request->url);

	}
	else
	{
		if(gl_s_request == NULL || gl_s_request->url == NULL || strcmp(en->fin_url,gl_s_request->url))
		{
			if(!args->force_resume)
				return -1;

			return_code=1;
		}
	}

	// Checking for File status;

	if(flag==MS_PRINT)
	{
		printf("\n|-->File: %s", (en->file==NULL || en->file[0]=='\0') ? "-" : en->file);

		if(en->file!=NULL && en->file[0]!='\0')
		{
			FILE* fp=fopen(en->file,"r+");

			if(fp!=NULL)
			{
				fclose(fp);
				printf(" (RW) || (required)");
			}
			else
			{
				fp=fopen(en->file,"r");
				if(fp!=NULL)
				{
					fclose(fp);
					printf(" (R) || (required)");
				}
				else
				{
					printf(" () || (required)");
				}
			}
		}
		else
			printf(" () || (required)");
	}
	else
	{
		if(en->file==NULL || en->file[0]=='\0') // Every time an output file is required ! (fatal error)
			return -1;

		FILE* fp=fopen(en->file,"r+");

		if(fp==NULL) // Fatal error, output file is required.
			return -1;
	}

	// Check for Unprocessed file status ;

	if(flag==MS_PRINT)
	{
		printf("\n|-->Unprocessed-File: %s",(en->up_file==NULL || en->up_file[0]=='\0') ? "-" : en->up_file);

		if(en->up_file!=NULL && en->up_file[0]!='\0')
		{
			FILE* fp=fopen(en->up_file,"r+");

			if(fp!=NULL)
			{
				fclose(fp);
				printf("%s",gl_s_response->content_encoding==NULL ? " (RW) || (not required)" : " (RW) || (required)");
			}
			else
			{
				fp=fopen(en->up_file,"r");
				if(fp!=NULL)
				{
					fclose(fp);
					printf("%s",gl_s_response->content_encoding==NULL ? " (R) || (not required)" : " (R) || (required)");
				}
				else
				{
					printf("%s",gl_s_response->content_encoding==NULL ? " () || (not required)" : " () || (required)");
				}
			}
		}
		else
			printf("%s",gl_s_response->content_encoding==NULL ? " () || (not required)" : " () || (required)");
	}
	else
	{
		if(en->up_file!=NULL && en->up_file[0]!='\0') // up_file is saved
		{
			if(gl_s_response->content_encoding==NULL) // but this time it is not required.
			{
				if(!args->force_resume)
					return -1;

				return_code=1;
			}

			FILE* fp=fopen(en->up_file,"r+");

			if(fp==NULL) // if content_encoding == NULL , dont report error, as it is not expecting file any way.
			{
				if(gl_s_response->content_encoding!=NULL) // fatal error;
					return -1;

				if(!args->force_resume)
					return -1;

				return_code=1;
			}

		}
		else if(gl_s_response->content_encoding !=NULL) // up_file not saved, but this time up_file is expected (fatal error).
			return -1;
	}

	// Content Length ;

	if(flag==MS_PRINT)
	{
		printf("\n|-->Content-Length: %s",convert_data(en->content_length,0));
		printf(" || ");

		if(gl_s_response!=NULL && gl_s_response->content_length!=NULL)
			printf("%s",convert_data(atol(gl_s_response->content_length),0));
		else
			printf("-");

	}
	else
	{
		if(gl_s_response!=NULL && gl_s_response->content_length!=NULL && atol(gl_s_response->content_length)!=en->content_length)
		{
			return -1;
		}
	}

	// Downloaded Length + Number of Left Over Ranges + Left Over Ranges;

	if(flag==MS_PRINT)
	{
		printf("\n|-->Downloaded-Length: %s",convert_data(en->downloaded_length,0));
		printf(" || (no checks)");

		printf("\n|-->Number of left over ranges: %ld",en->n_l_ranges);
		printf(" || (no checks)");

		printf("\n|-->Left over ranges:");

		for(long i=0;i<en->n_l_ranges;i++)
		{
			printf(" (%ld, %ld)",en->l_ranges[i].start,en->l_ranges[i].end);

			if(i!=en->n_l_ranges-1)
				printf(" |");
		}

		printf(" || (no checks)");
		printf("\n");
	}

	return return_code;
}

#ifdef LIBSSL_SANE

struct mid_bag* make_d_mid_state(struct http_request* gl_s_request,struct http_response* gl_s_response,struct unit_info* base_unit_info,struct mid_bag* units_bag,struct units_progress* progress)
{
	struct mid_bag* state_bag=make_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);

	if(state_bag==NULL)
		return NULL;

	*((int*)state_bag->first->data)=1;

	struct mid_bag* d_state_bag=create_mid_bag();

	// Number of Ranges + Ranges along with their hashes

	dump_long(d_state_bag,progress->n_ranges);

	FILE* fp=NULL;

	if(gl_s_response->content_encoding==NULL)
		fp=fopen(base_unit_info->file,"r");
	else
		fp=fopen(base_unit_info->up_file,"r");


	if(fp==NULL)
	{
		return state_bag;
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
			return state_bag;
		}

		dump_long(d_state_bag,progress->ranges[i].start);

		dump_long(d_state_bag,progress->ranges[i].end);

		chunk_len=progress->ranges[i].end - progress->ranges[i].start + 1;

		if(lseek(fileno(fp),progress->ranges[i].start,SEEK_SET)!=progress->ranges[i].start)
		{
			return state_bag;
		}

		// Determine MD5 hash

		MD5_CTX md_context;

		MD5_Init(&md_context);

		while(1)
		{
			if(chunk_len<0)
			{
				return state_bag;
			}
			else if(chunk_len==0)
				break;

			status=read(fileno(fp),f_buf,sizeof(f_buf) < chunk_len ? sizeof(f_buf) : chunk_len);

			if(status<=0)
			{
				return state_bag;
			}

			chunk_len=chunk_len-status;

			MD5_Update(&md_context,f_buf,status);
		}

		MD5_Final(md_result, &md_context);

		for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
			sprintf(f_buf+2*i,"%02x", md_result[i]);

		dump_string(d_state_bag,f_buf);
	}

	// Return the data bag

	place_data(state_bag,flatten_mid_bag(d_state_bag));

	return state_bag;

}

struct d_ms_entry* process_d_ms_entry(FILE* ms_fp)
{
	struct d_ms_entry* d_en=(struct d_ms_entry*)calloc(1,sizeof(struct d_ms_entry));

	d_en->en=process_ms_entry(ms_fp);

	d_en->type=1;
	d_en->en->type=1;
	d_en->d_en_len=d_en->en->en_len;

	// Number of Ranges + Ranges along with their hash

	if(read(fileno(ms_fp),&d_en->n_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	d_en->d_en_len=d_en->d_en_len+sizeof(long);

	d_en->ranges=(struct http_range*)malloc(sizeof(struct http_range)*d_en->n_ranges);
	d_en->hashes=(char**)malloc(sizeof(char*)*d_en->n_ranges);

	for(long i=0;i<d_en->n_ranges;i++)
	{
		if(read(fileno(ms_fp),&d_en->ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		d_en->d_en_len=d_en->d_en_len+sizeof(long);

		if(read(fileno(ms_fp),&d_en->ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		d_en->d_en_len=d_en->d_en_len+sizeof(long);

		d_en->hashes[i]=extract_string(ms_fp);

		d_en->d_en_len=d_en->d_en_len+sizeof(long)+strlen(d_en->hashes[i])+1;
	}

	return d_en;
}

void print_d_ms_entry(struct d_ms_entry* d_en)
{
	if(d_en->type!=1)
		return;

	d_en->en->type=1;

	print_ms_entry(d_en->en);

	printf("|-->Number of fetched ranges: %ld",d_en->n_ranges);

	printf("\n|-->Fetched ranges:");

	for(long i=0;i<d_en->n_ranges;i++)
	{
		printf(" (%ld, %ld) [%s]",d_en->ranges[i].start,d_en->ranges[i].end,d_en->hashes[i]);

		if(i!=d_en->n_ranges-1)
			printf(" |");
	}

	printf("\n");
}

int validate_d_ms_entry(struct d_ms_entry* d_en,struct http_request* gl_s_request,struct http_response* gl_s_response,int flag)
{
	if(d_en->type!=1)
		return -1;

	int return_code=0;

	int en_status=validate_ms_entry(d_en->en,gl_s_request,gl_s_response,flag);

	if(en_status!=0)
	{
		if(flag!=MS_PRINT && (en_status==-1 || args->force_resume==0))
			return en_status;

		return_code=en_status;
	}

	// Number of Ranges + Ranges along with hashes

	if(flag==MS_PRINT)
	{
		printf("|-->Number of fetched ranges: %ld",d_en->n_ranges);
		printf(" || (no checks)");
		printf("\n|-->Fetched ranges:");
	}


	FILE* fp=NULL;

	if(d_en->en->up_file!=NULL && d_en->en->up_file[0]!='\0')
		fp=fopen(d_en->en->up_file,"r");
	else
		fp=fopen(d_en->en->file,"r");

	if(fp==NULL)
	{
		if(flag==MS_PRINT)
			printf(" ! || ! \n");

		return -1;
	}

	long chunk_len;
	long status=0;
	unsigned char md_result[MD5_DIGEST_LENGTH];
	char f_buf[MAX_TRANSACTION_SIZE > (1+MD5_DIGEST_LENGTH*8/4) ? MAX_TRANSACTION_SIZE : (1+MD5_DIGEST_LENGTH*8/4)];

	struct stat stat_buf;
	fstat(fileno(fp),&stat_buf);

	int miss_match=0;

	for(long i=0;i<d_en->n_ranges;i++)
	{

		if(flag==MS_PRINT)
			printf(" (%ld, %ld) [%s",d_en->ranges[i].start,d_en->ranges[i].end,d_en->hashes[i]);

		if(d_en->ranges[i].start >= stat_buf.st_size || d_en->ranges[i].end >= stat_buf.st_size)
		{
			if(flag==MS_PRINT)
				printf(" || !]\n");

			return -1;
		}

		if(lseek(fileno(fp),d_en->ranges[i].start,SEEK_SET)!=d_en->ranges[i].start)
		{
			if(flag==MS_PRINT)
					printf(" || !]\n");

			return -1;
		}

		chunk_len=d_en->ranges[i].end - d_en->ranges[i].start + 1;

		// Determine MD5 hash

		MD5_CTX md_context;

		MD5_Init(&md_context);

		while(1)
		{
			if(chunk_len<0)
			{
				if(flag==MS_PRINT)
							printf(" || !]\n");

				return -1;
			}
			else if(chunk_len==0)
				break;

			status=read(fileno(fp),f_buf,sizeof(f_buf) < chunk_len ? sizeof(f_buf) : chunk_len);

			if(status<=0)
			{
				if(flag==MS_PRINT)
							printf(" || !]\n");

				return -1;
			}

			chunk_len=chunk_len-status;

			MD5_Update(&md_context,f_buf,status);
		}

		MD5_Final(md_result, &md_context);

		for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
			sprintf(f_buf+2*i,"%02x", md_result[i]);

		if(flag==MS_PRINT)
		{
			printf(" || %s]",f_buf);
		}

		if(strcmp(d_en->hashes[i],f_buf))
		{
			if(flag==MS_PRINT)
			{
				miss_match=1;
			}
			else
			{
				if(!args->force_resume)
					return -1;

				return_code=1;
			}
		}

		if(flag==MS_PRINT)
		{
			if(i!=d_en->n_ranges-1)
				printf(" |");
		}
	}

	if(flag==MS_PRINT)
		printf(" (%s)\n", miss_match ? "Miss-Matched" : "Matched");

	return return_code;
}

#endif

void* read_ms_entry(char* ms_file,long entry_number,int flag)
{
	FILE* ms_fp=fopen(ms_file,"r");

	if(ms_fp==NULL)
	{
		if(flag==MS_PRINT) mid_err("MID: Unable to open the MID state file %s. Exiting...\n\n",ms_file);

		return NULL;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		if(flag==MS_PRINT) mid_err("MID: Unable to acquire lock on the MID state file %s. Exiting...\n\n",ms_file);

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
					if(flag==MS_PRINT) mid_err("MID: End of the file, entry number %ld not found. Exiting...\n\n",entry_number);

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
				if(flag==MS_PRINT) mid_err("MID: End of the file, entry number %ld not found. Exiting...\n\n",entry_number);

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
			d_en=process_d_ms_entry(ms_fp);

			if(d_en==NULL || d_en->d_en_len != *((long*)en_info))
				goto fatal_error;

			if(flag==MS_PRINT)
			{
				printf("Entry Count: %ld\n",en_counter);
				print_d_ms_entry(d_en);
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

	if(flag==MS_PRINT) mid_err("MID: %s is broken, errors encountered when processing the MID state file. Exiting...\n\n",ms_file);

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

void delete_ms_entry(char* ms_file,long entry_number,int flag)
{
	if(entry_number<=0)
	{
		if(flag==MS_PRINT) mid_err("MID: Entry number %ld is not valid. Exiting...\n\n");

		return;
	}

	FILE* ms_fp=fopen(ms_file,"r");

	if(ms_fp==NULL) // No file exits
	{
		if(flag==MS_PRINT) mid_err("MID: Unable to open MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	fclose(ms_fp);

	ms_fp=fopen(ms_file,"r+");

	if(ms_fp==NULL) // Cant open file in append mode
	{
		if(flag==MS_PRINT) mid_err("MID: Unable to open MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0) // Unable to acquire lock
	{
		if(flag==MS_PRINT) mid_err("MID: Unable to acquire lock on MID state file %s. Exiting...\n\n",ms_file);

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
			if(flag==MS_PRINT) mid_err("MID: EOF file reached, entry number %ld not found. Exiting...\n\n",entry_number);

			goto normal_return;
		}
		else if(status!=sizeof(long)) // File corrupted, but do nothing.
		{
			if(flag==MS_PRINT) mid_err("MID: %s is not a MID state file. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		read_len=read_len+sizeof(long);

		if(read_len+*((long*)(en_info)) > stat_buf.st_size) // File corrupted, but do nothing.
		{
			if(flag==MS_PRINT) mid_err("MID: %s is not a MID state file. Exiting...\n\n",ms_file);

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
			if(flag==MS_PRINT) mid_err("MID: Unable to read MID state file %s. Exiting...\n\n",ms_file);

			goto normal_return;
		}
		else if(status==0)
			break;


		read_len=read_len+status;

		if(pwrite(fileno(ms_fp),buf,status,prev_len)!=status) // Cant write to the file.
		{
			if(flag==MS_PRINT) mid_err("MID: Unable to write to MID state file %s. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		prev_len=prev_len+status;
	}

	if(prev_len==0)
	{
		flock(fileno(ms_fp),LOCK_UN);
		fclose(ms_fp);

		remove(ms_file);

		if(flag==MS_PRINT) mid_err("MID: %s has no more entries left. Deleting...\n\n",ms_file);

		return;
	}

	ftruncate(fileno(ms_fp),prev_len);

	normal_return:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;

}

void check_ms_entry(char* ms_file,long entry_number,struct http_request* gl_s_request,struct http_response* gl_s_response)
{
	if(entry_number<=0)
		entry_number=1;

	void* entry;

	entry=read_ms_entry(ms_file,entry_number,MS_RETURN);

	if(entry==NULL)
	{
		fprintf(stderr,"MID: Error when fetching the given MID state entry. Exiting...\n\n");

		return;
	}

	printf("Entry Count: %ld\n",entry_number);

	if(((struct ms_entry*)entry)->type==0)
		validate_ms_entry((struct ms_entry*)entry,gl_s_request,gl_s_response,MS_PRINT);
#ifdef LIBSSL_SANE
	else if(((struct ms_entry*)entry)->type==1)
		validate_d_ms_entry((struct d_ms_entry*)entry,gl_s_request,gl_s_response,MS_PRINT);
#endif
	else
	{
		fprintf(stderr,"MID: MS entry type unknown. Exiting...\n\n");
		return;
	}

	printf("\n");

}
