/*
 * MID_arguments.c
 *
 *  Created on: 20-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID_arguments.h"
#include"MID_functions.h"
#include"MID_structures.h"
#include"MID_socket.h"
#include"MID_unit.h"
#include"MID_state.h"
#include"MID.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

/* Store custom HTTP headers given by user (conf_file + cmd_line) */

static struct mid_bag* hdr_bag = NULL;

static void fill_mid_args(char* key,char* value,struct mid_args* args,int conf_flag)
{
	if(key==NULL)
		return;

	struct mid_data* op_data=(struct mid_data*)malloc(sizeof(struct mid_data));
	struct mid_bag* op_bag=create_mid_bag();

	if(value==NULL)
	{
		if(conf_flag>=1)
		{
			mid_help("MID: Configuration file not understood, \"%s\" option used but value not specified", key);
		}
		else
		{
			mid_help("MID: \"%s\" option used but value not specified", key);
		}

		exit(0);
	}

	if(!strcmp(key,"output-file"))
	{
		args->output_file=strdup(value);
	}

	else if(!strcmp(key,"interfaces") || !strcmp(key,"exclude-interfaces"))
	{
		int in_flag=0;

		if(!strcmp(key,"interfaces"))
		{
			in_flag=1;
		}


		char*** ifs;
		long* ifs_count;

		if(in_flag)
		{
			ifs=&args->include_ifs;
			ifs_count=&args->include_ifs_count;
		}
		else
		{
			ifs=&args->exclude_ifs;
			ifs_count=&args->exclude_ifs_count;
		}

		struct mid_data* n_data=(struct mid_data*)malloc(sizeof(struct mid_data));

		n_data->data=value;
		n_data->len=strlen(value);

		struct mid_bag* if_bag=create_mid_bag();
		struct mid_data* if_data=(struct mid_data*)malloc(sizeof(struct mid_data));

		while(1)
		{

			n_data=sseek(n_data,",",-1,MID_PERMIT);

			if(n_data==NULL || n_data->data==NULL || n_data->len==0)
			{
				break;
			}

			n_data=scopy(n_data,",",(char**)(&if_data->data),-1,MID_DELIMIT);
			if_data->len=strlen(if_data->data)+1;

			place_mid_data(if_bag,if_data);

			if(n_data==NULL || n_data->data==NULL || n_data->len==0)
			{
				break;
			}
		}

		if(if_bag->n_pockets==0)
		{
			*ifs=NULL;
			*ifs_count=0;
			return;
		}

		*ifs=(char**)malloc(sizeof(char*)*if_bag->n_pockets);
		*ifs_count=if_bag->n_pockets;

		struct mid_pocket* pocket=if_bag->first;
		long if_count=0;

		while(pocket!=NULL)
		{
			(*ifs)[if_count]=(char*)malloc(sizeof(char)*pocket->len);
			memcpy((*ifs)[if_count],pocket->data,pocket->len);
			pocket=pocket->next;
			if_count++;
		}

	}

	else if(!strcmp(key, "help"))
	{
		if (atoi(value) > 0)
			args->help = 1;
		else
			args->help = 0;
	}

	else if(!strcmp(key,"url"))
	{
		args->url=strdup(value);
	}

	else if(!strcmp(key,"unprocessed-file"))
	{
		args->up_file=strdup(value);
	}

	else if(!strcmp(key,"scheduler-algorithm"))
	{
		if(!strcasecmp(value,"MAXOUT"))
			args->schd_alg=maxout_scheduler;
		else if(!strcasecmp(value,"ALL"))
			args->schd_alg=all_scheduler;
		else
		{
			if(conf_flag)
				mid_help("MID: Configuration file not understood, value given for \"scheduler-algorithm\" unknown");
			else
				mid_help("MID: Value given for \"scheduler-algorithm\" unknown");

			exit(0);
		}
	}

	else if(!strcmp(key,"max-parallel-downloads"))
	{
		args->max_parallel_downloads=atol(value);
	}

	else if(!strcmp(key,"max-unit-retries"))
	{
		args->max_unit_retries=atol(value);
	}

	else if(!strcmp(key,"unit-break"))
	{
		struct mid_data* v_data=(struct mid_data*)malloc(sizeof(struct mid_data));
		v_data->data=value;
		v_data->len=strlen(value);

		char* num;
		char* unit="b";

		v_data=sseek(v_data," ",-1,MID_PERMIT);
		if(v_data==NULL || v_data->data==NULL || v_data->len==0)
			goto illegal_unit_break;

		v_data=scopy(v_data,"0123456789",&num,-1,MID_PERMIT);
		if(v_data==NULL || v_data->data==NULL || v_data->len==0)
			goto compute_unit_break;

		v_data=sseek(v_data," ",-1,MID_PERMIT);
		if(v_data==NULL || v_data->data==NULL || v_data->len==0)
			goto compute_unit_break;

		v_data=scopy(v_data," ",&unit,-1,MID_DELIMIT);
		if(v_data==NULL || v_data->data==NULL || v_data->len==0)
			goto compute_unit_break;

		v_data=sseek(v_data," ",-1,MID_PERMIT);
		if(v_data!=NULL && v_data->len!=0)
			goto illegal_unit_break;

		compute_unit_break:

		if(!strcmp(unit,"B") || !strcmp(unit,"b"))
			args->unit_break=atol(num);
		else if(!strcmp(unit,"K"))
			args->unit_break=atol(num)*1024;
		else if(!strcmp(unit,"k"))
			args->unit_break=atol(num)*1000;
		else if(!strcmp(unit,"M"))
			args->unit_break=atol(num)*1024*1024;
		else if(!strcmp(unit,"m"))
			args->unit_break=atol(num)*1000*1000;
		else if(!strcmp(unit,"G"))
			args->unit_break=atol(num)*1024*1024*1024;
		else if(!strcmp(unit,"g"))
			args->unit_break=atol(num)*1000*1000*1000;
		else
			goto illegal_unit_break;

		return;

		illegal_unit_break:

		if(conf_flag==1)
			mid_help("MID: Configuration file not understood, \"unit-break\" format not correct");
		else
			mid_help("MID: \"unit-break\" format not correct");
		exit(0);
	}

	else if(!strcmp(key,"max-redirects"))
	{
		args->max_redirects=atol(value);
	}

	else if(!strcmp(key,"max-tcp-syn-retransmits"))
	{
		args->max_tcp_syn_retransmits=atol(value);
	}

	else if(!strcmp(key,"unit-sleep-time"))
	{
		args->unit_retry_sleep_time=atol(value);
	}

	else if(!strcmp(key,"io-timeout"))
	{
		args->io_timeout=atol(value);
	}

	else if(!strcmp(key, "ipv4"))
	{
		if(atol(value) <= 0)
		{
			args->ipv4 = 0;
		}
		else
		{
			args->ipv4 = 1;
		}
	}

	else if(!strcmp(key, "ipv6"))
	{
		if(atol(value) <= 0)
		{
			args->ipv6 = 0;
		}
		else
		{
			args->ipv6 = 1;
		}
	}

	else if(!strcmp(key,"progress-update-time"))
	{
		args->progress_update_time=atol(value);
	}

	else if(!strcmp(key,"entry-number"))
	{
		args->entry_number=atol(value);

		if(args->entry_number<0)
			args->entry_number=0;
	}

	else if(!strcmp(key,"ms-file"))
	{
		args->ms_file=strdup(value);
	}

	else if(!strcmp(key,"print-ms"))
	{
		args->print_ms=1;
		args->pm_file=strdup(value);

	}

	else if(!strcmp(key,"delete-ms"))
	{
		args->delete_ms=1;
		args->dm_file=strdup(value);

	}

	else if(!strcmp(key,"validate-ms"))
	{
		args->validate_ms=1;
		args->cm_file=strdup(value);

	}

	else if(!strcmp(key, "version"))
	{
		if (atoi(value) > 0)
			args->version = 1;
		else
			args->version = 0;
	}

	else if(!strcmp(key,"header"))
	{

		struct mid_data* n_data=(struct mid_data*)malloc(sizeof(struct mid_data));
		struct mid_data* hdr_data=(struct mid_data*)malloc(sizeof(struct mid_data));

		hdr_data->data=value;
		hdr_data->len=strlen(value)+1;

		hdr_data=scopy(hdr_data,"= ",(char**)(&n_data->data),-1,MID_DELIMIT);

		if(hdr_data==NULL || hdr_data->data==NULL || hdr_data->len==0 || hdr_data->len==1)
		{
			if(conf_flag==0)
				mid_help("MID: Configuration file not understood, header format not correct");
			else
				mid_help("MID: Header format not correct");
			exit(0);
		}


		hdr_data=sseek(hdr_data,"= ",-1,MID_PERMIT);

		if(hdr_data==NULL || hdr_data->data==NULL || hdr_data->len==0)
		{
			if(conf_flag==0)
				mid_help("MID: Configuration file not understood, header format not correct");
			else
				mid_help("MID: Header format not correct");
			exit(0);
		}

		n_data->len=strlen(n_data->data)+1;

		place_mid_data(hdr_bag,n_data);
		place_mid_data(hdr_bag,hdr_data);
	}

	else if(!strcmp(key,"detailed-progress"))
	{
		if(atol(value)<=0)
		{
			args->detailed_progress=0;
		}
		else
		{
			args->detailed_progress=1;
		}
	}

	else if(!strcmp(key,"force-resume"))
	{
		if(atol(value)<=0)
		{
			args->force_resume=0;
		}
		else
		{
			args->force_resume=1;
		}
	}

	else if(!strcmp(key,"no-resume"))
	{
		if(atol(value)<=0)
		{
			args->no_resume=0;
		}
		else
		{
			args->no_resume=1;
		}
	}
#ifdef LIBSSL_SANE
	else if(!strcmp(key,"detailed-save"))
	{
		if(atol(value)<=0)
		{
			args->detailed_save=0;
		}
		else
		{
			args->detailed_save=1;
		}
	}
#endif
	else if(!strcmp(key,"quiet"))
	{
		if(atol(value)<=0)
		{
			args->quiet_flag=0;
		}
		else
		{
			args->quiet_flag=1;
		}
	}

	else if(!strcmp(key,"verbose"))
	{
		if(atol(value)<=0)
		{
			args->verbose_flag=0;
		}
		else
		{
			args->verbose_flag=1;
		}
	}

	else if(!strcmp(key,"vverbose"))
	{
		if(atol(value)<=0)
		{
			args->vverbose_flag=0;
		}
		else
		{
			args->vverbose_flag=1;
			args->verbose_flag=1;
		}
	}

	else if(!strcmp(key,"surpass-root-check"))
	{
		if(atol(value)<=0)
		{
			args->surpass_root_check=0;
		}
		else
		{
			args->surpass_root_check=1;
		}
	}

	else // If option not valid
	{
		if(conf_flag>=1)
		{
			mid_help("MID: Configuration file not understood, option \"%s\" not known", key);
		}
		else
		{
			mid_help("MID: Option \"%s\" not known", key);
		}

		exit(0);
	}
}

static int read_mid_conf(char* config_file, struct mid_args* args, int rc_flags)
{
	if (config_file == NULL || args == NULL)  // Invalid input
		return MID_ERROR_INVAL;

	/* Open config_file */

	FILE* config_fp = fopen(config_file, "r");

	if (config_fp == NULL)
	{
		if (rc_flags & MID_MODE_PRINT_HELP)
			mid_help("MID: Error reading configuration file \"%s\"", config_file);

		return MID_ERROR_FATAL;
	}

	/* Read the config_file data into a mid_bag then into a mid_data*/

	struct mid_bag* config_bag = create_mid_bag();

	struct mid_data* config_data = (struct mid_data*) malloc(sizeof(struct mid_data));
	config_data->data = malloc(MAX_TRANSACTION_SIZE);
	config_data->len = MAX_TRANSACTION_SIZE;

	long rd_status = 0;

	for ( ; ; )
	{
		rd_status = read(fileno(config_fp), config_data->data, MAX_TRANSACTION_SIZE);

		if (rd_status < 0)
		{
			fclose(config_fp);

			if (rc_flags & MID_MODE_PRINT_HELP)
				mid_help("MID: Error reading configuration file \"%s\"", config_file);

			return MID_ERROR_FATAL;
		}
		else if (rd_status == 0)
		{
			fclose(config_fp);
			break;
		}

		config_data->len = rd_status;
		place_mid_data(config_bag, config_data);
	}

	config_data = flatten_mid_bag(config_bag);

	free_mid_bag(config_bag);

	/* Parse the config_file data */

	char* arg_key = NULL;
	char* arg_value = NULL;
	int key_read = 0;

	for ( ; ; )
	{
		/* Skip the spaces and empty lines */

		config_data = sseek(config_data, " \n", -1, MID_PERMIT);

		if (config_data == NULL || config_data->data == NULL || config_data->len <= 0)
		{
			if (key_read == 0)   // If no arg_key is read, config_file parsing successful.
				break;
			else   // arg_key read but no value is provided.
			{
				mid_help("MID: Configuration file not understood, \"%s\" option used but no value specified", \
						arg_key);

				return MID_ERROR_FATAL;
			}
		}

		/* Read the Text */

		if (((char*) config_data->data)[0] == '#')	{	/* If text is starting with '#',
		 consider the whole line as comment. */

			config_data = sseek(config_data, "\n", -1, MID_DELIMIT);
		}
		else // read arg_key or arg_value.
		{

			if (((char*) config_data->data)[0] == '\'' || \
					((char*) config_data->data)[0] == '"')	{	/* If starting with ' or " ,
					read till ' or " */

				int quote = ((char*) config_data->data)[0] == '\'' ? 1 : 0;

				config_data->data = config_data->data + 1;
				config_data->len = config_data->len - 1;

				config_data = scopy(config_data, quote ? "'" : "\"", key_read ? \
						(arg_value = NULL, &arg_value) : (arg_key = NULL, &arg_key), -1, MID_DELIMIT);

				if (config_data == NULL || config_data->data == NULL || config_data->len < 1)
				{
					if (rc_flags & MID_MODE_PRINT_HELP)
						mid_help("MID: Configuration file not understood, missing \" %s \"", \
								quote ? "'" : "\"");

					return MID_ERROR_FATAL;
				}

				config_data->data = config_data->data + 1;
				config_data->len = config_data->len - 1;
			}
			else
			{
				config_data = scopy(config_data, " \n#", key_read ? (arg_value = NULL, &arg_value) : \
						(arg_key = NULL, &arg_key), -1, MID_DELIMIT);
			}

			if (key_read)
				fill_mid_args(arg_key, arg_value, args, 1);

			key_read = !key_read;
		}
	}

	return MID_ERROR_NONE;
}

int parse_mid_args(char** argv, long argc, int pa_flags, struct mid_bag* pa_result)
{
	if (((pa_flags & MID_MODE_READ_CMD_LINE) && (argv == NULL || argc <= 0)) || pa_result == NULL)  // Invalid Input.
		return MID_ERROR_INVAL;

	/* Initialize data structures */

	struct mid_args* args = (struct mid_args*) calloc(1, sizeof(struct mid_args));

	if (args == NULL)
		return MID_ERROR_FATAL;

	hdr_bag == NULL ? (hdr_bag = create_mid_bag()) : 0;

	/* Initialize args with default values, might get replaced with conf_file and cmd_line.
	 * cmd_line > conf_file > default */

	if (pa_flags & MID_MODE_READ_DEFAULT_VALUES)
	{
		args->schd_alg = MID_DEFAULT_SCHEDULER_ALOGORITHM;
		args->max_unit_retries = MID_MAX_UNIT_RETRIES;
		args->max_redirects = MID_DEFAULT_HTTP_REDIRECTS;
		args->max_parallel_downloads = MID_DEFAULT_PARALLEL_DOWNLOADS;
		args->max_tcp_syn_retransmits = MID_DEFAULT_TCP_SYN_RETRANSMITS;
		args->unit_retry_sleep_time = MID_DEFAULT_UNIT_RETRY_SLEEP_TIME;
		args->io_timeout = MID_DEFAULT_IO_TIMEOUT;
		args->unit_break = MID_DEFAULT_UNIT_BREAK_THRESHOLD_SIZE;
		args->progress_update_time = MID_DEFAULT_PROGRESS_UPDATE_TIME;
	}

	/* Read the conf_file, might get replaced with cmd_line's. cmd_line > conf_file */

	if (pa_flags & MID_MODE_READ_CONF_FILE)
	{
		int rc_return = MID_ERROR_NONE;

		for (long arg_count = 0; arg_count < argc; arg_count++)
		{
			if (strcmp(argv[arg_count], "--conf") == 0 || strcmp(argv[arg_count], "-c") == 0)
			{
				if (arg_count == argc - 1)
				{
					if (pa_flags & MID_MODE_PRINT_HELP)
						mid_help("MID: \"--conf\" option used but no value specified");

					return MID_ERROR_FATAL;
				}

				/* Read the user specified conf_file */

				rc_return = read_mid_conf(argv[arg_count + 1], args, (pa_flags & MID_MODE_PRINT_HELP) ? \
						MID_MODE_PRINT_HELP : 0);
				break;
			}

			/* If end of args reached, try reading default conf_file, if it exists */

			if (arg_count == argc - 1 && access(MID_DEFAULT_CONFIG_FILE, R_OK) == 0)
				rc_return = read_mid_conf(MID_DEFAULT_CONFIG_FILE, args, (pa_flags & MID_MODE_PRINT_HELP) ? \
						MID_MODE_PRINT_HELP : 0);
		}

		if (rc_return != MID_ERROR_NONE)
			return rc_return;
	}

	/* Read command line arguments */

	if (pa_flags & MID_MODE_READ_CMD_LINE)
	{
		long arg_count = 0;

		/* Command name and Arguments count */

		args->cmd = strdup(argv[arg_count]);
		args->args_count = argc;

		arg_count++;

		/*  Argument struct{} initializations */

		struct mid_data args_str, \
		*targs_str = (struct mid_data*) malloc(sizeof(struct mid_data));

		args_str.data = strdup(MID_CONSTANT_ARGUMENTS);
		args_str.len = strlen(MID_CONSTANT_ARGUMENTS);

		/* Parse arguments */

		char* arg_key = NULL;  // Argument key, to handle argument specific operation.
		char* arg_type = NULL;  // Argument type. (Value or Flag argument).

		for ( ; arg_count < argc; arg_count++)
		{
			/* Make argument compare buffer */

			char* cmp_buf = (char*) malloc(sizeof(char) * (strlen(argv[arg_count]) + \
					3)); // ' ' + arg + ' ' + '\0'

			cmp_buf[0] = ' ';
			memcpy(cmp_buf + 1, argv[arg_count], strlen(argv[arg_count]));
			cmp_buf[strlen(argv[arg_count]) + 1] = ' ';
			cmp_buf[strlen(argv[arg_count]) + 2] = '\0';

			/* Try the argument in args_str buffer */

			*targs_str = args_str;

			targs_str->data = (void *) strlocate((char*) targs_str->data, cmp_buf, 0, \
					targs_str->len);  // Locate cmp_buf occurrence in the val string.

			if (targs_str->data == NULL)
			{
				if (pa_flags & MID_MODE_PRINT_HELP)
					mid_help("MID: Option \"%s\" not known", argv[arg_count]);

				return MID_ERROR_FATAL;
			}

			/* Skip the space and compute remaining buffer length */

			targs_str->data = targs_str->data + 1;
			targs_str->len = args_str.len - (long) (targs_str->data - args_str.data);

			if (targs_str->len < 2)
				continue;

			if (((char*) targs_str->data)[0] != '-' || ((char*) targs_str->data)[1] != '-')  // If short argument, get the long version.
			{
				targs_str = sseek(targs_str, " ", -1, MID_DELIMIT); // Skip the argument.

				if (targs_str == NULL || targs_str->data == NULL || targs_str->len <= 0)
					continue;

				targs_str = sseek(targs_str, " ", -1, MID_PERMIT);  // Skip the spaces.

				if (targs_str == NULL || targs_str->data == NULL || targs_str->len <= 0)
					continue;
			}

			/* Get the argument key */

			if (targs_str->len < 2)
				continue;

			targs_str->data = targs_str->data + 2; // Skip "--"
			targs_str->len = targs_str->len - 2;

			arg_key = NULL;
			targs_str = scopy(targs_str, " ", &arg_key, -1, MID_DELIMIT);

			if (arg_key == NULL || targs_str == NULL || targs_str->data == NULL || targs_str->len <= 0)
				continue;

			/* Determine argument type */

			targs_str = sseek(targs_str, " ", -1, MID_PERMIT);  // Seek the spaces

			if (targs_str == NULL || targs_str->data == NULL || targs_str->len <=0)
				continue;

			arg_type = NULL;
			targs_str = scopy(targs_str, " ", &arg_type, -1, MID_DELIMIT);  // Copy argument type.

			if (arg_type == NULL)
				continue;

			int _arg_type = atoi(arg_type);

			/* Fill the mid_args structure */

			if (_arg_type == 0)  // Value type argument.
			{
				arg_count++;
				fill_mid_args(arg_key, arg_count < argc ? argv[arg_count] : NULL, args, 0);
			}
			else if (_arg_type == 1)  // Flag type argument.
			{
				char* enable_arg = "1";
				fill_mid_args(arg_key, enable_arg, args, 0);
			}
			else if (_arg_type == 2)  // Ignore_Value type argument.
				arg_count++;
			else
				continue;
		}
	}

	/* Set IPv4 and IPV6 flags if none is set by user */

	if (args->ipv4 == 0 && args->ipv6 == 0)
	{
		args->ipv4 = MID_DEFAULT_IPV4_SCHEME;
		args->ipv6 = MID_DEFAULT_IPV6_SCHEME;
	}

	/* Fill the custom HTTP headers accumulated from conf_file and cmd_line */

	if (hdr_bag != NULL && hdr_bag->n_pockets != 0)
	{
		if ((hdr_bag->n_pockets) % 2 != 0)
		{
			if (pa_flags & MID_MODE_PRINT_HELP)
				mid_help("MID: Headers format not correct");

			return MID_ERROR_FATAL;
		}

		char*** custom_headers = (char***) malloc(sizeof(char**) * (1 + hdr_bag->n_pockets / 2));

		long hdr_count = 0;

		for (struct mid_pocket* pocket = hdr_bag->first; \
		pocket != NULL; pocket = pocket->next, hdr_count++)  {

			custom_headers[hdr_count] = (char**) malloc(sizeof(char*) * 2);

			custom_headers[hdr_count][0] = (char*) memndup(pocket->data, pocket->len);

			pocket = pocket->next;

			custom_headers[hdr_count][1] = (char*) memndup(pocket->data, pocket->len);
		}

		custom_headers[hdr_count] = NULL;

		args->custom_headers = custom_headers;
	}

	/* Fill pa_result and return success */

	struct mid_data arg_data;
	arg_data.data = (void*) args;
	arg_data.len = sizeof(struct mid_args);

	place_mid_data(pa_result, &arg_data);

	return MID_ERROR_NONE;
}

int args_check(struct mid_args* args)
{
	if (args->help)
	{
		mid_help(NULL);
		exit(0);
	}

	if (args->version)
	{
		printf("%s version %s (%s) ", PACKAGE_NAME, PACKAGE_VERSION, HOST_ARCH);

#ifdef LIBSSL_SANE
		printf("+ssl +crypto");
#else
		printf("-ssl -crypto");
#endif

#ifdef LIBZ_SANE
		printf(" +z");
#else
		printf(" -z");
#endif

		printf("\n\n");
		printf("Project homepage: [ %s ]", PACKAGE_URL);
		printf("\n\n");

		exit(1);
	}

	if (args->print_ms)
	{
		read_ms_entry(args->pm_file,args->entry_number,MS_PRINT);
		exit(0);
	}

	if (args->delete_ms)
	{
		delete_ms_entry(args->dm_file,args->entry_number,MS_PRINT);
		exit(0);
	}

	// Check for mandatory URL argument {--url | -u}

	if (args->url == NULL)
	{
		mid_help("MID: URL must be specified using {--url | -u} option");
		exit(0);
	}

	return MID_ERROR_NONE;
}

void mid_help_strings()
{
	fprintf(stderr,"Usage: {MID | mid} --url URL [OPTIONS]\n\n");
	fprintf(stderr,"   --output-file file                        -o file                  Use this output file instead of determining from the URL. \n");
	fprintf(stderr,"   --interfaces nic1,nic2...                 -i nic1,nic2...          Network-interfaces which are used in the file download. \n");
	fprintf(stderr,"   --exclude-interfaces nic1,nic2...         -ni nic1,nic2...         Network-interfaces which are excluded from the file download. \n");
	fprintf(stderr,"   --help                                    -h                       Print this help message. \n");
	fprintf(stderr,"   --url URL                                 -u URL                   URL to be used in the download. \n");
	fprintf(stderr,"   --unprocessed-file file                   -up file                 Use this .up file instead of determining from the URL. \n");
	fprintf(stderr,"   --scheduler-algorithm alg                 -sa alg                  Use alg scheduler algorithm (case insensitive). alg => MAXOUT, ALL. (see man page). \n");
	fprintf(stderr,"   --max-parallel-downloads x                -n x                     At max x parallel connections are opened. \n");
	fprintf(stderr,"   --max-unit-retries x                      -ur x                    At max x retries are made by a unit to download a chunk. \n");
	fprintf(stderr,"   --unit-break Nu                           -ub Nu                   Chunk size limit to split the unit further, default is 100K. \n");
	fprintf(stderr,"                                                                      u => {' ',B,b}=*1, K=*1024, k=*1000, M=K*1024, m=k*1000, G=M*1024, g=m*1000\n");
	fprintf(stderr,"   --max-redirects x                         -R x                     At max x HTTP redirects are followed. \n");
	fprintf(stderr,"   --max-tcp-syn-retransmits x               -sr x                    At max x TCP SYNs are retransmitted. \n");
	fprintf(stderr,"   --unit-sleep-time t                       -us t                    Download unit sleeps for t seconds before retrying. \n");
	fprintf(stderr,"   --io-timeout t                            -io t                    Set connect and I/O timeout of t seconds. \n");
	fprintf(stderr,"   --ipv4                                    -4                       Use IPv4 address scheme. \n");
	fprintf(stderr,"   --ipv6                                    -6                       Use IPv6 address scheme. \n");
	fprintf(stderr,"   --progress-update-time t                  -pu t                    Progress information updates after evert t seconds. \n");
	fprintf(stderr,"   --detailed-progress                       -dp                      Show detailed download progress. \n");
	fprintf(stderr,"   --force-resume                            -fr                      Skip the checks and start the download. \n");
	fprintf(stderr,"   --no-resume                               -nr                      Do not resume the partial downloads. Default action is to resume. \n");
#ifdef LIBSSL_SANE
	fprintf(stderr,"   --detailed-save                           -ds                      Save the hashes of downloaded ranges to .ms file. Default action is not to save. \n");
#endif
	fprintf(stderr,"   --entry-number x                          -e x                     If multiple partial downloads exists, select the x-th download. \n");
	fprintf(stderr,"   --ms-file file                            -ms file                 Use this .ms file instead of determining from the URL. \n");
	fprintf(stderr,"   --print-ms file                           -pm file                 Print the MID state information and exit. \n");
	fprintf(stderr,"   --delete-ms file                          -dm file                 Delete ms entry, entry number should be specified with option -e. \n");
	fprintf(stderr,"   --validate-ms file                        -vm file                 Validate ms entry, entry number should be specified with option -e. \n");
	fprintf(stderr,"   --version                                 -V                       Print the version and exit. \n");
	fprintf(stderr,"   --header h=v                              -H h=v                   Add custom headers to HTTP request message (can be used multiple times). \n");
	fprintf(stderr,"   --quiet                                   -q                       Silent mode, don't output anything. \n");
	fprintf(stderr,"   --verbose                                 -v                       Print the verbose information. \n");
	fprintf(stderr,"   --vverbose                                -vv                      Print the very verbose information. \n");
	fprintf(stderr,"   --surpass-root-check                      -s                       If had the sufficient permissions, use -s to surpass the root-check. \n");
	fprintf(stderr,"   --conf file                               -c file                  Specify the conf file. Preference order: cmd_line > conf_file > default_values. \n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Project homepage: { %s }",PACKAGE_URL);
	fprintf(stderr,"\n\n");
}

