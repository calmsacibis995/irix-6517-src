#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "node.h"
#include "parser.h"
#include "globals.h"


// Local variables used by code generation routines:
//
static char *subsystemname;		// subsystem name
static int subsystemid;			// subsystem ID


// Local routines to generate output files from parse tree:
//
//   *_mesg.h   -- RPC message definitions for use in client and server stubs.
//   invk_*_stubs.h -- prototypes for invoking stubs.
//   I_*_stubs.h -- prototypes for implementation functions
//   I_*_stubs.c -- server RPC message demux code and calls to user 
//			 provided interface implementation routines.
//
enum output_file { mesg_h, client_h, server_h, server_c, noutput_files };
static const char *output_prefix[noutput_files] = {
	"", "invk_", "I_", "I_"
};
static const char *output_suffix[noutput_files] = {
	"_mesg.h", "_stubs.h", "_stubs.h", "_stubs.c"
};
static char *output_filename[noutput_files];

static int emit_mesg_h();
static int emit_client_h();
static int emit_server_h();
static int emit_server_c();
extern int check_mesg_h();

extern char *msg_channel;

// Emit the files message definition, client and server files for the
// IDL specification.  Verify that none of the messages is longer than
// MESG_SIZE.  If there are any errors, delete all the output files and
// return 0.  Return 1 on success.
int
emit_files()
{
	// Check to make sure that at least one interface was defined.  The
	// parser does a majority of the consistency checking for things that
	// *are* included in the specification.  But if it never sees an,
	// interface it can't complain about missing subsystemname and
	// subsystemid declarations, etc.  We could enhance the parser to
	// expect at least one interface but that would make the grammar
	// specification more contorted and be harder to generate an
	// intelligable error messge for.

	listiter_t nodes(parsetree);
	idl_node_t *node;

	while (node = (idl_node_t *)nodes.next())
		if (node->nodeid == idl_node_t::interface_node)
			break;
	if (!node) {
		fprintf(stderr, "%s: at least one interface must be defined.\n",
			myname);
		return(0);
	}

	// Grab the subsystemname so we know what to name the interface stubs
	// and output files, and the subsystemid so we know the proper
	// subsystemid ID to use in our mesg_rpc() and mesg_send() calls.
	//
	assert(subsystem_node);
	subsystemname = subsystem_node->name;
	subsystemid = subsystem_node->id;

	// Generate the output file names.
	//
	const int subsys_len = strlen(subsystemname);
	int fileidx;
	for (fileidx = 0; fileidx < noutput_files; fileidx++) {
		char *&filename = output_filename[fileidx];
		const char *prefix = output_prefix[fileidx];
		const int prefix_len = strlen(prefix);
		const char *suffix = output_suffix[fileidx];
		const int suffix_len = strlen(suffix);
		const int output_dir_len = strlen(output_dir);
		const int filename_len = output_dir_len + prefix_len +
					 subsys_len + suffix_len + 1;

		filename = (char *)alloca(filename_len+1);
		assert(filename);
		sprintf(filename, "%s/%s%s%s", output_dir, prefix,
				subsystemname, suffix);
	}

	// Generate the output files.  Remove all output files that we created
	// if any errors.
	//
	int output_created[noutput_files] = { 0 };
	if ((output_created[mesg_h]   = emit_mesg_h()) &&
	    (output_created[client_h] = emit_client_h()) &&
	    (output_created[server_h] = emit_server_h()) &&
	    (output_created[server_c] = emit_server_c()))
		return(1);

	for (fileidx = 0; fileidx < noutput_files; fileidx++)
		if (output_created[fileidx]) unlink(output_filename[fileidx]);
			return(0);
}


// Emit *_mesg.h file: contains interface message definitions.  Return 1
// if file successfully generated, 0 on failure.
static int
emit_mesg_h()
{
	FILE *file = fopen(output_filename[mesg_h], "w");
	int	struct_printed;

	if (!file) {
		fprintf(stderr, "%s: unable to open \"%s\": %s\n", myname,
				output_filename[mesg_h], strerror(errno));
		return(0);
	}

	listiter_t nodes(parsetree);
	idl_node_t *node;

	// Emit standard protection against being included multiple times ...
	//
	fprintf(file,
		"#if !defined(__%s_mesg_h__)\n"
		"#define __%s_mesg_h__ 1\n\n",
		subsystemname, subsystemname);

	// Emit includes for standard message types, etc.
	//
	fputs("#include <ksys/cell/remote.h>\n"
	      "#include <ksys/cell/mesg.h>\n",
	      file);

	// Emit message structure definitions.  
	//
	fputc('\n', file);
	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		char	*iface;

		if (node->nodeid != idl_node_t::interface_node) 
			continue;
		idl_interface_t *interface = (idl_interface_t *)node;

		listiter_t params(interface->parameters);
		idl_parameter_t *param;

		if (interface->synchronicity == ASYNCHRONOUS)
			iface = "asynchronous";
		else if (interface->synchronicity == SYNCHRONOUS)
			iface = "synchronous";
		else
			iface = "callback";
		
		fprintf(file,
			"/***** Message structures for %s%s interface %s *****/\n\n",
			interface->condition ? "possibly " : "",
			iface,
			interface->name);

		// Do any vectorof parameter structures
		if (interface->vector_params) {
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->valref == VECTOROF) {
					fprintf(file,
						"typedef struct {\n"
						"\t%s\t*%s_ptr;\n"
						"\tsize_t\t%s_count;\n"
						"} %s_%svector_t;\n\n",
						param->type,
						param->name,
						param->name,
						param->name,
						param->inout == IN ? "in" : "out");
				}
			}
		}

		// First the in parameters. There will always be one of
		// these
		//
		fputs("typedef struct {\n", file);

		if (!(interface->synchronicity == ASYNCHRONOUS &&
		    !interface->condition))
			fputs("\ttrans_t\t_transid;\n", file);

		if (interface->in_params) {
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout != IN ||
				    param->valref == BUFFEROF ||
				    param->valref == VECTOROF)
					continue;
				fprintf(file, "\t%s\t%s;\n",
						param->type,
						param->name);
				param->mesg_name =
						new char[strlen(param->name) +
						sizeof "m_in."];
				assert(param->mesg_name);
				sprintf(param->mesg_name, "m_in.%s",
							param->name);
			}
		}
		fprintf(file, "} %s_%s_in_t;\n\n",
				subsystemname, interface->name);


		// Next the inout parameter structure
		//
		if (interface->inout_ref_params) {
			struct_printed = 0;
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout != INOUT)
					continue;
				if (!struct_printed) {
					struct_printed = 1;
					fputs("typedef struct {\n", file);
				}
				fprintf(file, "\t%s\t%s;\n",
						param->type,
						param->name);
				param->mesg_name =
						new char[strlen(param->name) +
						sizeof "m_inout."];
				assert(param->mesg_name);
				sprintf(param->mesg_name, "m_inout.%s",
							param->name);
			}
			if (struct_printed) {
				fprintf(file, "} %s_%s_inout_t;\n\n",
						subsystemname, interface->name);
			}
		}

		// Next the out parameter structure
		//
		if (!(interface->synchronicity == ASYNCHRONOUS &&
		    !interface->condition)) {
			fputs("typedef struct {\n"
			      "\ttres_t\t_result;\n",
			      file);
			if (interface->out_ref_params) {
				params.reset();
				struct_printed = 0;
				while (param = (idl_parameter_t *)params.next()) {
					if (param->inout != OUT ||
					    param->valref == BUFFEROF)
						continue;
					fprintf(file, "\t%s\t%s;\n",
							param->type,
							param->name);
					param->mesg_name =
						new char[strlen(param->name) +
						sizeof "m_out."];
					assert(param->mesg_name);
					sprintf(param->mesg_name, "m_out.%s",
							param->name);
				}
			}
			fprintf(file, "} %s_%s_out_t;\n\n",
						subsystemname, interface->name);
		}

		// Finally the send message and reply message formats
		//
		fputs("typedef struct {\n", file);
		if (interface->inout_ref_params)
			fprintf(file, "\t%s_%s_inout_t\tm_inout;\n",
						subsystemname, interface->name);
		fprintf(file, "\t%s_%s_in_t\tm_in;\n",
						subsystemname, interface->name);
		fprintf(file, "} %s_%s_mesg_t;\n\n",
						subsystemname, interface->name);

		if (!(interface->synchronicity == ASYNCHRONOUS &&
		    !interface->condition)) {
			fputs("typedef struct {\n", file);
			if (interface->inout_ref_params)
				fprintf(file, "\t%s_%s_inout_t\tm_inout;\n",
						subsystemname, interface->name);
			fprintf(file, "\t%s_%s_out_t\tm_out;\n",
						subsystemname, interface->name);
			fprintf(file, "} %s_%s_reply_t;\n\n",
						subsystemname, interface->name);
		}

	}

	// Emit #endif to close standard protection against being included
	// multiple times.
	//
	fprintf(file, "#endif /* !defined(__%s_mesg_h__) */\n",
			subsystemname);

	// Return successfully ...
	//
	fclose(file);
	return(1);
}


// Emit *_client.h file: contains inline implementations of client
// interface stubs.  Return 1 if file successfully generated, 0 on
// failure.
static int
emit_client_h()
{
	FILE *file = fopen(output_filename[client_h], "w");
	char *tab;

	if (!file) {
		fprintf(stderr, "%s: unable to open \"%s\": %s\n", myname,
			output_filename[client_h], strerror(errno));
		return(0);
	}

	listiter_t nodes(parsetree);
	idl_node_t *node;

	// Emit standard protection against being included multiple times ...
	//
	fprintf(file,
		"#if !defined(__invk_%s_h__)\n"
		"#define __invk_%s_h__\n\n",
		subsystemname, subsystemname);

	// Emit #include's for files noted for "import".
	//
	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		if (node->nodeid == idl_node_t::import_node)
			fprintf(file, "#include %s\n",
				((idl_import_t *)node)->file);
	}

	// Emit include of <stddef.h> and <sys/debug.h> so the client stubs can
	// verify that the request and reply messages are less than or equal to
	// the maximum message size.
	//
	fputs("\n"
	      "#include <stddef.h>\n"
	      "#include <sys/systm.h>\n"
	      "#ifdef DEBUG\n"
	      "#include <sys/ktrace.h>\n"
	      "#endif /* DEBUG */\n"
	      "#include <sys/errno.h>\n"
	      "#include <sys/debug.h>\n"
	      "#include <sys/cmn_err.h>\n"
              "#include <sys/timers.h>\n"
	      "#include <ksys/cell.h>\n"
	      "#include <ksys/cell/service.h>\n",
	      file);

	// Emit include for our message definition header file.
	//
	fprintf(file, "\n#include \"%s\"\n", output_filename[mesg_h]);

	// Emit client stub specifications for interfaces.
	//
	fputc('\n', file);
	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		if (node->nodeid != idl_node_t::interface_node)
			continue;

		idl_interface_t *interface = (idl_interface_t *)node;

		// Emit inline prototype header.
		//
		fprintf(file,
			"__inline int\n"
			"invk_%s_%s(\n",
			subsystemname, interface->name);
		if (interface->synchronicity != CALLBACK)
			fprintf(file, "\tservice_t\tsvc,\n");

		listiter_t params(interface->parameters);
		idl_parameter_t *param;
		while (param = (idl_parameter_t *)params.next()) {
			char	*tab;
			char	*ptr;

			if (strlen(param->type) < 8)
				tab = "\t";
			else
				tab = "";
			switch (param->valref) {
			case	VALUEOF:
				ptr = "";
				break;
			case	VECTOROF:
			case	BUFFEROF:
				if (param->inout == OUT) {
					ptr = "**";
					break;
				}
				/* FALLTHROUGH */
			case	POINTERTO:
				ptr = "*";
				break;
			}
			if (param->valref == VECTOROF)
				fprintf(file, "\t%s_%svector_t", param->name,
					param->inout == IN ? "in" : "out");
			else
				fprintf(file, "\t%s%s", param->type, tab);
			fprintf(file, "\t%s%s%s\n",
				ptr,
				param->name,
				params.is_last() ? ")" : ",");
		}
		fputs("{\n", file);

		fprintf(file, "#if DEBUG\n"
			"\t__uint64_t\t_mid;\n"
			"#endif\n"
			"\tstruct mesgstat\t*mesgp;\n"
			"\t__uint64_t\tetime = get_timestamp();\n"
			"\t__uint64_t\tctime = 0; /* For later use */\n");

		// Emit definition for local interface message pointer.
		//
		fputs("\tidl_msg_t\t*idl_mesg;\n", file);
		fprintf(file, "\t%s_%s_mesg_t\t*mesg;\n",
				subsystemname, interface->name);
		if (!(interface->synchronicity == ASYNCHRONOUS &&
		    !interface->condition))
			fprintf(file, "\t%s_%s_reply_t\t*reply;\n"
				"\tidl_msg_t\t*idl_reply;\n",
				subsystemname, interface->name);
		fputs("\tmesg_channel_t\tchannel;\n"
		      "\tidl_hdr_t\t*idl_hdr;\n"
		      "\tint\t\t_error;\n", file);

		if (interface->vector_params)
			fputs("\tint\t\ti;\n", file);

		if (interface->out_buf_params)
			fputs("\tsize_t\t\t_nobuffer = (size_t)-1;\n"
			      "\tint\t\t_bufcnt = 1;\n",
			      file);

		if (interface->synchronicity != ASYNCHRONOUS) {
			fprintf(file, "\tcredid_t\tcurcredid;\n");
		}

		if (interface->synchronicity == CALLBACK)
			fprintf(file, 
				"\tservice_t\tsvc;\n"
				"\n"
				"\tsvc = mesg_get_callback_svc();\n");

		if (interface->synchronicity != ASYNCHRONOUS) {
			fprintf(file, "\n"
				"\tcurcredid = get_current_credid();\n");
		}
		fprintf(file, "\n"
		    "\tchannel = cell_to_channel(SERVICE_TO_CELL(svc), MESG_CHANNEL_%s);\n"
		    "\tif (channel == NULL)\n"
		    "\t\treturn(ECELLDOWN);\n",
		    msg_channel);

		// If a debug prologue was specified emit it now -- prevent
		// potential abuse of prologues by enclosing code in
		// #ifdef DEBUG ... #endif
		//
		if (interface->prologue)
			fprintf(file,
				"\n#ifdef DEBUG\n"
				"%s\n"
				"#endif\n\n",
				interface->prologue);

		// Emit code for message tracing
		// first arg is subsystemid
		// second arg is destination cell
		// third arg is our cellid
		// fourth arg is function address
		fprintf(file, "#if DEBUG\n"
			"\tif (mesg_trace_mask & (1L << %d)) {\n"
			"\t\t_mid = atomicAddUint64(&mesg_id, 1L);\n"
			"\t\tktrace_enter(mesg_trace,\n"
			"\t\t\t(void *)(__psint_t)%d,\n"
			"\t\t\t(void *)_mid,\n"
			"\t\t\t(void *)(__psint_t)(svc.un.int_value),\n"
			"\t\t\t(void *)(__psint_t)cellid(),\n"
			"\t\t\t(void *)(__psint_t)%d,\n",
			subsystemid, subsystemid, interface->opcode);
		int i = 5;
		int haveoutspec = 0;
		params.reset();
		while (param = (idl_parameter_t *)params.next()) {
			if (param->inout == OUT) {
				fprintf(file, "\t\t\t(void *)(__psunsigned_t)%s",
					param->name);
				if (param->debugsize)
					haveoutspec = 1;
			} else if (param->valref != VALUEOF) {
				fprintf(file, "\t\t\t(void *)(__psunsigned_t)");
				switch (param->debugsize) {
				case 0:
				default:
					fprintf(file, "%s", param->name);
					break;
				case 8:
					fprintf(file, "*(uchar_t *)%s",
							param->name);
					break;
				case 16:
					fprintf(file, "*(ushort_t *)%s",
							param->name);
					break;
				case 32:
					fprintf(file, "*(__uint32_t *)%s",
							param->name);
					break;
				case 64:
					fprintf(file, "*(__uint64_t *)%s",
							param->name);
					break;
				}
			} else {
				/* VALUEOF */
				fprintf(file, "\t\t\t(void *)(__psunsigned_t)");
				switch (param->debugsize) {
				case 0:
					fprintf(file, "%s", param->name);
					break;
				case 32:
					fprintf(file, "*(__uint32_t *)&%s",
							param->name);
					break;
				case 64:
					fprintf(file, "*(__uint64_t *)&%s",
							param->name);
					break;
				default:
					/* print address */
					fprintf(file, "&%s", param->name);
					break;
				}
			}
			if (++i < 16)
				fprintf(file, ",\n");
			else
				break;
		}

		while (i < 16) {
			fprintf(file, "\t\t\t(void *)0xdeadL");
			if (++i < 16)
				fprintf(file, ",\n");
			else
				break;
		}
		fprintf(file, ");\n"
			  "\t}\n");

		fprintf(file, "#endif\n");

                // Save code for message totals
                fprintf(file, "\n"
                        "\tmesgstat_enter(%d,\n"
                        "\t\t(void *)invk_%s_%s,\n"
                        "\t\t\"invk_%s_%s\",\n"
                        "\t\t(void *)__return_address,\n"
                        "\t\t&mesgp);\n",
                        subsystemid, subsystemname, interface->name,
                        subsystemname, interface->name);

		// make sure not sending message to self!
		fprintf(file, "\tASSERT(cellid() != SERVICE_TO_CELL(svc));\n");

		fprintf(file,
			"\n\tidl_mesg = mesg_allocate(channel);\n"
			"\tif (idl_mesg == NULL) {\n"
			"\t\tmesg_channel_rele(channel);\n"
			"\t\treturn(ECELLDOWN);\n"
			"\t}\n");

		// allocate descriptors for parameters to be sent out of line 
		//
		if (interface->in_buf_params) {
			fputc('\n', file);
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				int	isvector;

				if (param->inout != IN ||
				    (param->valref != BUFFEROF &&
				     param->valref != VECTOROF))
					continue;
				fprintf(file,
					"\t/* Marshaling parameter %s */\n",
					param->name);
				if (param->valref == VECTOROF) {
					fprintf(file,
					    "\tfor (i = 0; i < %s_count; i++)\n",
					    param->name);
					tab = "\t";
					isvector = 1;
				} else {
					tab = "";
					isvector = 0;
				}
				fprintf(file,
					"%s\tmesg_marshal_indirect(idl_mesg,\n"
					"%s\t\t(caddr_t)%s%s%s%s,\n"
					"%s\t\t%s%s%s_count * sizeof(%s));\n",
					tab,
					tab, param->name,
					isvector ? "[i]." : "",
					isvector ? param->name : "",
					isvector ? "_ptr" : "",
					tab,
					isvector ? param->name : "",
					isvector ? "[i]." : "",
					param->name, param->type);
			}
		}

		fprintf(file, "\n"
			"\tidl_hdr = mesg_get_immediate_ptr(idl_mesg,\n"
			"\t\t\tsizeof(%s_%s_mesg_t));\n"
			"\tif (idl_hdr == NULL)\n"
			"\t\tcmn_err(CE_PANIC,\n"
			"\t\t\t\"IDL %s_%s_mesg_t too large (%%d)\",\n"
			"\t\t\tsizeof(%s_%s_mesg_t));\n"
			"\tmesg = (%s_%s_mesg_t *)(idl_hdr + 1);\n",
			subsystemname, interface->name,
			subsystemname, interface->name,
			subsystemname, interface->name,
			subsystemname, interface->name);

		fprintf(file, "\n"
			"\tidl_hdr->hdr_subsys = %d;\n"
			"\tidl_hdr->hdr_opcode = %d;\n",
			subsystemid, interface->opcode);

		// Marshal any "in" or "inout" parameters into outgoing message.
		//
		if (interface->in_params) {
			fputc('\n', file);
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->valref == BUFFEROF ||
				    param->valref == VECTOROF) {
					// in bufferof parameters have already
					// been marshalled as out of line data
					if (param->inout == IN)
						continue;
					// check for null pointers for
					// out bufferofs
					fprintf(file,
						"\tif (%s == NULL)\n"
						"\t\t%s_count = &_nobuffer;\n",
						param->name,
						param->name);
				}
				if (param->inout == OUT)
					continue;
				fprintf(file,
					"\tmesg->%s = %s%s;\n",
					param->mesg_name,
					param->valref != VALUEOF ? "*" : "",
					param->name);
			}
		}

		// Emit mesg_send()/mesg_rpc() code.  If interface is
		// synchronous, then use mesg_rpc().  If asynchronous, then
		// mesg_send().  If a synchronicity condition has been
		// specified, then emit code to conditionally use one or the
		// other depending on the value of the condition.
		//
		fputc('\n', file);

		if (interface->condition) {
			assert(!interface->out_params && !interface->epilogue);
			fprintf(file,
				"\tif (%s%s) {\n",
				interface->synchronicity == SYNCHRONOUS ?
					"!" : "",
				interface->condition);
			tab = "\t";
		} else
			tab = "";

		if (interface->synchronicity == ASYNCHRONOUS ||
		    interface->condition) {
			assert(!interface->out_params && !interface->epilogue);
			fprintf(file,
				"%s\t_error = mesg_send(channel, idl_mesg, %d);\n"
				"%s\tif (_error)\n"
				"%s\t\t_error = ECELLDOWN;\n",
				tab, interface->in_buf_params ? 1 : 0,
				tab, tab);
			if (!interface->condition) {

				// Emit code to generate statistics for message
				// before returning

				fprintf(file, "\n"
				   "\tmesgstat_exit(mesgp, etime, ctime);\n");

				fputs("\tmesg_channel_rele(channel);\n"
				      "\treturn(_error);\n"
				      "}\n\n", file);
				continue;
			}
		}

		if (interface->condition)
			fputs("\t} else {\n", file);

		fprintf(file,
		    "%s\tremote_transaction_start(svc, &mesg->m_in._transid, "
						  "curcredid);\n", tab);

		fprintf(file, 
			"%s\tidl_reply = mesg_%s(channel, idl_mesg);\n",
			tab, interface->synchronicity == CALLBACK ?
					"callback" : "rpc");
		fprintf(file,
		    "%s\tif (idl_reply == NULL) {\n"
		    "%s\t\t_error = ECELLDOWN;\n"
		    "%s\t\tremote_transaction_end(NULL, NULL);\n"
		    "%s\t} else {\n",
		    tab,
		    tab,
		    tab,
		    tab);

		if (interface->out_buf_params) {
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout != OUT ||
				    (param->valref != BUFFEROF &&
				     param->valref != VECTOROF))
					continue;
				fprintf(file, "%s\t\tsize_t\t_%s_count;\n",
					tab, param->name);
			}
		}

		fprintf(file,
		    "%s\t\treply = (%s_%s_reply_t *)IDL_MESG_BODY(idl_reply);\n"
		    "%s\t\tremote_transaction_end(&reply->m_out._result, &_error);\n",
		    tab, subsystemname, interface->name,
		    tab);

		if (interface->out_params) {
			params.reset();
			fputs("\n", file);
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout == IN)
					continue;
				assert(param->valref != VALUEOF);
				switch (param->valref) {
				case	POINTERTO:
					fprintf(file,
						"%s\t\t%s%s = reply->%s;\n",
						tab,
						param->valref == POINTERTO ?
							"*" : "",
						param->name, param->mesg_name);
					break;
				case	VECTOROF:
					fprintf(file,
					    "%s\t\t/* Marshal out %s */\n"
					    "%s\t\tif (*%s_count != MESG_NO_BUFFER) {\n"
					    "%s\t\t\tif (*%s == NULL) {\n"
					    "%s\t\t\t\t_%s_count = reply->m_inout.%s_count *\n"
					    "%s\t\t\t\t\t\tsizeof(%s_outvector_t);\n"
					    "%s\t\t\t\t*%s = (%s_outvector_t *)kmem_zalloc(_%s_count, KM_SLEEP);\n"
					    "%s\t\t\t}\n"
					    "%s\t\t\tASSERT(*%s_count >= reply->m_inout.%s_count ||\n"
					    "%s\t\t\t\t*%s_count == 0);\n"
					    "%s\t\t\tfor (i = 0; i < reply->m_inout.%s_count; i++) {\n"
					    "%s\t\t\t\t_%s_count = (*%s)[i].%s_count * sizeof(%s);\n"
					    "%s\t\t\t\tmesg_unmarshal_indirect(idl_reply, _bufcnt,\n"
					    "%s\t\t\t\t\t(void **)&(*%s)[i].%s_ptr,\n"
					    "%s\t\t\t\t\t&_%s_count);\n"
					    "%s\t\t\t\t_bufcnt++;\n"
					    "%s\t\t\t\t_%s_count /= sizeof(%s);\n"
					    "%s\t\t\t\tASSERT((_%s_count <= (*%s)[i].%s_count) ||\n"
					    "%s\t\t\t\t\t((*%s)[i].%s_count == 0));\n"
					    "%s\t\t\t\t(*%s)[i].%s_count = _%s_count;\n"
					    "%s\t\t\t}\n"
					    "%s\t\t}\n",
					    tab, param->name,
					    tab, param->name,
					    tab, param->name,
					    tab, param->name, param->name,
					    tab, param->name,
					    tab, param->name, param->name, param->name,
					    tab,
					    tab, param->name, param->name,
					    tab, param->name,
					    tab, param->name,
					    tab, param->name, param->name, param->name, param->type,
					    tab,
					    tab, param->name, param->name,
					    tab, param->name,
					    tab,
					    tab, param->name, param->type,
					    tab, param->name, param->name, param->name,
					    tab, param->name, param->name,
					    tab, param->name, param->name, param->name,
					    tab,
					    tab);
					break;
				case	BUFFEROF:
					fprintf(file,
					    "%s\t\t/* Marshal out %s */\n"
					    "%s\t\tif (*%s_count != MESG_NO_BUFFER) {\n"
					    "%s\t\t\t_%s_count = *%s_count * sizeof(%s);\n"
					    "%s\t\t\tmesg_unmarshal_indirect(idl_reply, _bufcnt, (void **)%s,\n"
					    "%s\t\t\t\t&_%s_count);\n"
					    "%s\t\t\t_bufcnt++;\n"
					    "%s\t\t\t_%s_count /= sizeof(%s);\n"
					    "%s\t\t\tASSERT(_%s_count == reply->m_inout.%s_count);\n"
					    "%s\t\t\tASSERT((_%s_count <= *%s_count) || (*%s_count == 0));\n"
					    "%s\t\t}\n",
					    tab, param->name,
					    tab, param->name,
					    tab, param->name, param->name, param->type,
					    tab, param->name,
					    tab, param->name,
					    tab,
					    tab, param->name, param->type, 
					    tab, param->name, param->name,
					    tab, param->name, param->name, param->name,
					    tab);
				}
			}
		}

		// Free up message buffer, return error and end interface
		// definition.
		//
		fprintf(file, "\n"
		        "%s\t\tmesg_free(channel, idl_reply);\n"
			"%s\t}\n",
			tab, tab);

		// If has any OUT parameters and they have specified
		// how to dump them, put out return tracing.
		if (haveoutspec) {

			// Emit code for message tracing
			// first arg is subsystemid
			// second arg is our cell
			// third arg is destination cellid
			// fourth arg is function address
			fprintf(file, "#if DEBUG\n"
				"%s\tif (mesg_trace_mask & (1L << %d))\n"
				"%s\t\tktrace_enter(mesg_trace,\n"
				"%s\t\t\t(void *)(__psint_t)%d,\n"
				"%s\t\t\t(void *)_mid,\n"
				"%s\t\t\t0L,\n"
				"%s\t\t\t(void *)-1L,\n"
				"%s\t\t\t(void *)0xdeadL,\n",
				tab, subsystemid,
				tab,
				tab, subsystemid,
				tab,
				tab,
				tab,
				tab);
			int i = 5;
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout == IN ||
				    param->valref == VALUEOF) {
					fprintf(file, "%s\t\t\t(void *)(__psunsigned_t)0xdeadL", tab);
				} else {
					// must be POINTERTO right??
					fprintf(file, "%s\t\t\t(void *)(__psunsigned_t)", tab);
					switch (param->debugsize) {
					case 0:
					default:
						fprintf(file, "%s",
								param->name);
						break;
					case 8:
						fprintf(file, "*(uchar_t *)%s",
								param->name);
						break;
					case 16:
						fprintf(file, "*(ushort_t *)%s",
								param->name);
						break;
					case 32:
						fprintf(file,
							"*(__uint32_t *)%s",
							param->name);
						break;
					case 64:
						fprintf(file,
							"*(__uint64_t *)%s",
							param->name);
						break;
					}
				}
				if (++i < 16)
					fprintf(file, ",\n");
				else
					break;
			}

			while (i < 16) {
				fprintf(file, "%s\t\t\t(void *)0xdeadL", tab);
				if (++i < 16)
					fprintf(file, ",\n");
				else
					break;
			}
			fprintf(file, ");\n#endif\n");
		} // end of if haveoutspec

		// If a debug epilogue was specified emit it now -- prevent
		// potential abuse of epilogues by enclosing code in #ifdef
		// DEBUG ... #endif
		//
		if (interface->epilogue)
			fprintf(file, 
				"\n#ifdef DEBUG\n"
				"%s\n"
				"#endif\n",
				interface->epilogue);

		if (interface->condition)
			fputs("\t}\n", file);

                // Emit code to generate statistics for message
                fprintf(file, "\n"
                        "\tmesgstat_exit(mesgp, etime, ctime);\n");

		fputs("\tmesg_channel_rele(channel);\n"
		      "\treturn(_error);\n"
		      "}\n\n",
		      file);
	}

	// Emit #endif to close standard protection against being included
	// multiple times.
	//
	fprintf(file, "#endif /* !defined(__invk_%s_h__) */\n",
		subsystemname);

	// Return successfully ...
	//
	fclose(file);
	return(1);
}


// Emit *_client.h file: contains prototypes for subsystem message
// demultiplex routine and user provided server interface implementation
// routines.  Return 1 if file successfully generated, 0 on failure.
static int
emit_server_h()
{
	FILE *file = fopen(output_filename[server_h], "w");
	if (!file) {
		fprintf(stderr, "%s: unable to open \"%s\": %s\n", myname,
			output_filename[server_h], strerror(errno));
		return(0);
	}

	listiter_t nodes(parsetree);
	idl_node_t *node;

	// Emit standard protection against being included multiple times ...
	//
	fprintf(file,
		"#if !defined(__I_%s_h__)\n"
		"#define __I_%s_h__ 1\n\n",
		subsystemname, subsystemname);

	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		if (node->nodeid == idl_node_t::import_node)
			fprintf(file, "#include %s\n",
				((idl_import_t *)node)->file);
	}

	// Emit include for our message definition header file.
	//
	fprintf(file,
		"#include <ksys/cell/mesg.h>\n"
		"#include \"./%s_mesg.h\"\n",
		subsystemname);

	// Emit prototype for server interface demultiplex routine.
	//
	fprintf(file, "\n"
		"extern void\n"
		"%s_msg_dispatcher(mesg_channel_t /* chan */, idl_msg_t * /* idl_mesg */);\n\n",
		subsystemname);

	// Emit server stub prototypes for interfaces.  The code that
	// implements these prototypes will be written by the user and
	// called from the server interface demultiplex routine.
	//
	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		int	I_done_needed = 0;
		char	*comma;

		if (node->nodeid != idl_node_t::interface_node)
			continue;

		idl_interface_t *interface = (idl_interface_t *)node;

		// Emit prototype.
		//
		fprintf(file, "extern %s\nI_%s_%s(",
				interface->idl_type,
				subsystemname, interface->name);
		listiter_t params(interface->parameters);
		idl_parameter_t *param;
		comma = "";
		while (param = (idl_parameter_t *)params.next()) {
			char	*tab;
			char	*ptr;

			switch (param->valref) {
			case	VALUEOF:
				ptr = "";
				break;
			case	VECTOROF:
			case	BUFFEROF:
				if (param->inout == OUT) {
					ptr = "**";
					I_done_needed++;
					break;
				}
				/* FALLTHROUGH */
			case	POINTERTO:
				ptr = "*";
				break;
			}
			if ((strlen(param->type) + strlen(ptr)) < 8)
				tab = "\t";
			else
				tab = "";
			fprintf(file, "%s\n\t", comma);
			if (param->valref == VECTOROF)
				fprintf(file, "%s_%svector_t%s\t", param->name,
					param->inout == IN ? "in" : "out",
					ptr);
			else
				fprintf(file, "%s%s\t%s", param->type, ptr, tab);
			fprintf(file, "/* %s */", param->name);
			comma = ",";
		}
		if (I_done_needed)
			fprintf(file, ",\n\tvoid **\t\t/* bufdesc */");
		fputs(");\n\n", file);

		if (!I_done_needed)
			continue;

		/*
		 * We have some <out bufferof> parameters that the subsystem
		 * will need to know when they are finished with
		 */
		fprintf(file, "extern void\nI_%s_%s_done(",
				subsystemname, interface->name);
		params.reset();
		comma = "";
		while (param = (idl_parameter_t *)params.next()) {
			char	*tab;

			if (param->inout != OUT ||
			    (param->valref != BUFFEROF &&
			     param->valref != VECTOROF))
				continue;

			if ((strlen(param->type) + 1) < 8)
				tab = "\t";
			else
				tab = "";
			fprintf(file, "%s\n\t", comma);
			if (param->valref == VECTOROF)
				fprintf(file, "%s_outvector_t*", param->name);
			else
				fprintf(file, "%s*%s", param->type, tab);
			fprintf(file,
				"\t/* %s */,\n"
				"\tsize_t\t\t/* %s_count */",
				param->name,
				param->name);
			comma = ",";
		}
		fprintf(file, ",\n\tvoid *\t\t/* bufdesc */");
		fputs(");\n\n", file);
	}

	// Emit #endif to close standard protection against being included
	// multiple times.
	//
	fprintf(file, "\n#endif /* !defined(__I_%s_h__) */\n", subsystemname);

	// Return successfully ...
	//
	fclose(file);
	return(1);
}

// Emit *_server.c file: contains subsystem message demultiplex routine.
// Return 1 if file successfully generated, 0 on failure.
static int
emit_server_c()
{
	FILE	*file = fopen(output_filename[server_c], "w");
	int	rcvalid;
	char	*tab;
	int	may_reply;

	if (!file) {
		fprintf(stderr, "%s: unable to open \"%s\": %s\n", myname,
				output_filename[server_c], strerror(errno));
		return(0);
	}

	listiter_t nodes(parsetree);
	idl_node_t *node;

	// Emit include of <sys/debug.h> so the server demultiplex routine can
	// verify that the incoming message is has the correct subsystem ID.
	// Also need <systm.h> for declaration of panic() prototype.
	//
	fputs("#include <sys/debug.h>\n"
	      "#include <sys/types.h>\n"
	      "#include <sys/systm.h>\n"
	      "#include <sys/kmem.h>\n"
	      "#include <sys/cmn_err.h>\n"
	      "#include <stddef.h>\n", file);

	// Emit #include's for files noted for "import".
	//
	nodes.reset();
	while (node = (idl_node_t *)nodes.next())
		if (node->nodeid == idl_node_t::import_node)
			fprintf(file, "#include %s\n",
					((idl_import_t *)node)->file);

	// Emit includes for our message defintion and server interface
	// specification header files.
	//
	fprintf(file,
		"\n"
		"#include \"%s\"\n"
		"#include \"%s\"\n\n",
		output_filename[server_h], output_filename[mesg_h]);


	// Emit definition of a macro to free memory of non-zero size
	// - this avoids embedding more generated code deep within the bowels.
	//
	fprintf(file,
		"#define KMEM_FREE(a,n) if (n) kmem_free(a,n)\n");

	// Emit server interface demultiplex routine prototype header
	// and start of demultiplex switch.  Also emit ASSERT() to
	// verify that the message subsystem type is correct.
	//
	fprintf(file,
		"\nvoid\n"
		"%s_msg_dispatcher(mesg_channel_t channel, idl_msg_t *idl_mesg)\n"
		"{\n"
		"\tidl_hdr_t\t*idl_hdr;\n\n"
		"\tidl_hdr = IDL_MESG_HDR(idl_mesg);\n",
		subsystemname);

	fprintf(file,
		"\n\tASSERT(idl_hdr->hdr_subsys == %d);\n\n"
		"\tswitch (idl_hdr->hdr_opcode) {\n",
		subsystemid);

	// Emit body of demultiplex switch for each interface.
	//
	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		if (node->nodeid != idl_node_t::interface_node)
			continue;
		idl_interface_t *interface = (idl_interface_t *)node;
		listiter_t params(interface->parameters);
		idl_parameter_t *param;

		// Emit switch case and call to user implemented server
		// interface routine.  Emit definition of local
		// interface message pointer at the same time ...
		//
		fprintf(file,
			"\tcase\t%d:\n"
			"\t\t{\n",
			interface->opcode);

		if (strcmp(interface->idl_type, "int") == 0) {
			rcvalid = 1;
			fprintf(file,
				"\t\tint\t\t\trc;\n");
		} else
			rcvalid = 0;

		fprintf(file,
			"\t\t%s_%s_mesg_t\t*mesg;\n",
			subsystemname, interface->name);

		if (!(interface->synchronicity == ASYNCHRONOUS &&
		    !interface->condition)) {
			may_reply = 1;
			if (interface->condition)
				fputs("\t\tint\t\t\tis_rpc;\n", file);

			fprintf(file,
				"\t\tidl_msg_t\t\t*idl_reply;\n"
				"\t\t%s_%s_reply_t\treply;\n"
				"\t\t%s_%s_reply_t\t*reply_msg;\n",
				subsystemname, interface->name,
				subsystemname, interface->name);
		} else
			may_reply = 0;

		if (interface->vector_params)
			fprintf(file, "\t\tint\t\t\ti;\n");

		if (interface->in_buf_params)
			fprintf(file, "\t\tint\t\t\t_bufcnt = 1;\n");

		if (interface->in_buf_params || interface->out_buf_params) {
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				char	*tab;
				int	typelen;

				switch (param->valref) {
				case	BUFFEROF:
					typelen = strlen(param->type);
					if (param->inout == IN)
						fprintf(file,
							"\t\tsize_t\t\t\t%s_count = 0;\n",
							param->name);
					break;
				case	VECTOROF:
					typelen = strlen(param->name) +
						(param->inout == IN ? 2 : 3) +
						9;
					break;
				default:
					continue;
				}
				assert(param->inout != INOUT);

				if (typelen < 8)
					tab = "\t\t";
				else if (typelen >= 16)
					tab = "";
				else
					tab = "\t";
				fputs("\t\t", file);
				if (param->valref == VECTOROF) {
					fprintf(file, "%s_%svector_t",
					    param->name,
					    param->inout == IN ? "in" : "out");
				} else
					fprintf(file, "%s", param->type);
				fprintf(file,
					"%s\t*%s%s;\n",
					tab,
					param->name,
					param->inout == OUT ? " = NULL" : "");
			}
			if (interface->out_buf_params)
				fprintf(file, "\t\tvoid\t\t\t*bufdesc;\n");
		}

		fprintf(file,
			"\n\t\tmesg = (%s_%s_mesg_t *)(idl_hdr + 1);\n",
			subsystemname, interface->name);

		if (interface->in_buf_params) {
			fputs("\n", file);
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				int	isvector;

				if (param->inout != IN ||
				    (param->valref != BUFFEROF &&
				     param->valref != VECTOROF))
					continue;

				fprintf(file, "\t\t/* Unmarshal %s */\n",
						param->name);
				if (param->valref == VECTOROF) {
					fprintf(file,
					    "\t\tif (mesg->m_in.%s_count == 0)\n"
					    "\t\t\t%s = NULL;\n"
					    "\t\telse\n"
					    "\t\t\t%s = (%s_invector_t *)kmem_zalloc(\n"
					    "\t\t\t\tsizeof(%s_invector_t) *\n"
					    "\t\t\t\tmesg->m_in.%s_count, KM_SLEEP);\n"
					    "\t\tfor (i = 0; i < mesg->m_in.%s_count; i++) {\n",
					    param->name,
					    param->name,
					    param->name, param->name,
					    param->name, param->name,
					    param->name);
					tab = "\t";
					isvector = 1;
				} else {
					tab = "";
					isvector = 0;
				}
				fprintf(file,
				    "%s\t\tmesg_unmarshal_indirect(idl_mesg, _bufcnt,\n"
				    "%s\t\t\t(void **)&%s%s%s%s, &%s%s%s_count);\n"
				    "%s\t\t_bufcnt++;\n",
				    tab,
				    tab, 
				    isvector ? param->name : "",
				    isvector ? "[i]." : "",
				    param->name,
				    isvector ? "_ptr" : "",
				    isvector ? param->name : "",
				    isvector ? "[i]." : "",
				    param->name,
				    tab);
				if (isvector) {
					fprintf(file,
						"\t\t\t%s[i].%s_count /= sizeof(%s);\n"
						"\t\t}\n",
						param->name, param->name,
						param->type);
				}
			}
		}

		if (may_reply) {
			if (interface->condition) {
				fprintf(file,
				   "\n\t\tis_rpc = "
					"mesg_is_rpc(idl_mesg);\n");

				fprintf(file, "\t\tif (is_rpc) {\n");

				tab = "\t";
			} else
				tab = "";

			fprintf(file,
			 "%s\t\tstart_remote_service(&mesg->m_in._transid);\n",
			 tab);
			if (interface->condition)
				fputs("\t\t}", file);
		}

		// call server interface routine passing appropriate parameters
		fprintf(file,
			"\n\t\t%sI_%s_%s(",
			rcvalid ? "rc = " : "",
			subsystemname, interface->name);
		params.reset();
		while (param = (idl_parameter_t *)params.next()) {
			char	*addr;
			char	*from;
			int	isbuffered;

			if (param->valref == BUFFEROF ||
			    param->valref == VECTOROF)
				isbuffered = 1;
			else
				isbuffered = 0;

			fprintf(file, "\n\t\t\t");

			if (isbuffered && param->inout == OUT)
				fprintf(file,
				    "mesg->m_inout.%s_count == (size_t)-1 ?"
				    " NULL : ",
				    param->name);
			if (param->valref == VALUEOF ||
			    (isbuffered && param->inout == IN))
				addr = "";
			else
				addr = "&";

			if (isbuffered)
				from = "";
			else if (param->inout == OUT)
				from = "reply.";
			else
				from = "mesg->";

			fprintf(file, "%s%s%s%s", 
				addr,
				from,
				isbuffered ? param->name : param->mesg_name,
				params.is_last() ? "" : ",");
		}
		if (interface->out_buf_params)
			fprintf(file, ",\n\t\t\t&bufdesc");
		fputs(");\n", file);

		if (interface->in_buf_params) {
			params.reset();
			/*
			 * Free the memory allocated for the "in bufferof"
			 * params by xpc
			 */
			fputs("\n", file);
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout != IN ||
				    (param->valref != BUFFEROF &&
				     param->valref != VECTOROF))
					continue;

				if (param->valref == BUFFEROF) {
					fprintf(file,
					    "%s\t\tKMEM_FREE((caddr_t)%s, %s_count);\n",
					    tab, param->name, param->name);
					continue;
				}
				/* this must be vectorof */
				
				fprintf(file,
				    "\t\tif (mesg->m_in.%s_count != 0) {\n"
				    "\t\t\tfor (i = 0; i < mesg->m_in.%s_count; i++)\n"
				    "\t\t\t\tKMEM_FREE((caddr_t)%s[i].%s_ptr,\n"
				    "\t\t\t\t\t%s[i].%s_count);\n"
				    "\t\t\tKMEM_FREE((caddr_t)%s, sizeof(%s_invector_t) *\n"
				    "\t\t\t\tmesg->m_in.%s_count);\n"

				    "\t\t}\n",
				    param->name,
				    param->name,
				    param->name, param->name,
				    param->name, param->name,
				    param->name, param->name,
				    param->name);
			}
		}

		if (!may_reply) {
			// end switch case
			fputs("\n\t\tbreak;\n"
			      "\t\t}\n\n", file);
			continue;
		}

		if (interface->condition)
			fprintf(file, "\n\t\tif (is_rpc) {");

		fprintf(file, "\n"
		  "\t\t%send_remote_service(&reply.m_out._result, %s);\n",
		  tab, rcvalid ? "rc" : "0");

		fprintf(file, "\n"
		   "%s\t\tidl_reply = mesg_allocate(channel);\n"
		   "%s\t\tif (idl_reply == NULL)\n"
		   "%s\t\t\tbreak;\n",
		   tab,
		   tab,
		   tab);

		if (interface->out_buf_params) {
			params.reset();
			fputs("\n", file);
			while (param = (idl_parameter_t *)params.next()) {
				char	*tab2;
				int	isvector;

				if (param->inout != OUT ||
				    (param->valref != BUFFEROF &&
				     param->valref != VECTOROF))
					continue;

				fprintf(file,
					"%s\t\t/* Marshaling parameter %s */\n"
					"%s\t\tif (mesg->m_inout.%s_count != (size_t)-1)",
					tab, param->name,
					tab, param->name);

				if (param->valref == VECTOROF) {
					fprintf(file, " {\n"
					    "%s\t\t\tfor (i = 0; i < mesg->m_inout.%s_count; i++)\n",
					    tab, param->name);
					tab2 = "\t";
					isvector = 1;
				} else {
					fputs("\n", file);
					tab2 = "";
					isvector = 0;
				}
				fprintf(file,
					"%s%s\t\t\tmesg_marshal_indirect(idl_reply,\n"
					"%s%s\t\t\t\t(caddr_t)%s%s%s%s,\n"
					"%s%s\t\t\t\t%s%s.%s_count * sizeof(%s));\n",
					tab, tab2,
					tab, tab2, param->name,
					isvector ? "[i]." : "",
					isvector ? param->name : "",
					isvector ? "_ptr" : "",
					tab, tab2,
					isvector ? param->name : "mesg->m_inout",
					isvector ? "[i]" : "",
					param->name, param->type);
				if (isvector)
					fprintf(file, "%s\t\t}\n", tab);
			}
		}

		fprintf(file, "\n"
			"%s\t\tidl_hdr = mesg_get_immediate_ptr(idl_reply,\n"
			"%s\t\t\t\tsizeof(%s_%s_reply_t));\n"
			"%s\t\tif (idl_hdr == NULL)\n"
			"%s\t\t\tcmn_err(CE_PANIC,\n"
			"%s\t\t\t\t\"IDL %s_%s_reply_t too large (%%d)\",\n"
			"%s\t\t\t\tsizeof(%s_%s_reply_t));\n"
			"%s\t\treply_msg = (%s_%s_reply_t *)(idl_hdr + 1);\n",
			tab,
			tab, subsystemname, interface->name,
			tab,
			tab,
			tab, subsystemname, interface->name,
			tab, subsystemname, interface->name,
			tab, subsystemname, interface->name);

		if (interface->inout_ref_params)
			fprintf(file, "\n"
			   "\t\t%sreply_msg->m_inout = mesg->m_inout;\n",
			   tab);

		fprintf(file, "\n"
			"%s\t\treply_msg->m_out = reply.m_out;\n\n"
			"%s\t\tmesg_reply(channel, idl_reply, idl_mesg, %d);\n",
			tab,
			tab, interface->out_buf_params ? 1 : 0);

		if (interface->out_buf_params) {
			fprintf(file,
				"\n\t\tI_%s_%s_done(",
				subsystemname, interface->name);
			params.reset();
			while (param = (idl_parameter_t *)params.next()) {
				if (param->inout != OUT ||
				    (param->valref != BUFFEROF &&
				     param->valref != VECTOROF))
					continue;

				fprintf(file,
					"\n\t\t\tmesg->m_inout.%s_count == (size_t)-1 ? NULL : %s,"
					"\n\t\t\tmesg->m_inout.%s_count,", 
					param->name, param->name,
					param->name);
			}
			fputs("\n\t\t\tbufdesc);\n", file);
		}

		if (interface->condition)
			fprintf(file, "\t\t}\n");

		// end switch case
		fputs("\t\tbreak;\n"
		      "\t\t}\n\n", file);
	}

	// End demultiplex switch with default ``panic'' case.
	//
	fprintf(file,
		"\tdefault:\n"
		"\t\tpanic(\"%s_message_dispatcher: unknown opcode\");\n"
		"\t}\n\n",
		subsystemname);

	fprintf(file, "\tmesg_free(channel, idl_mesg);\n");

	// End defintion of server interface demultiplex routine.
	//
	fputs("}\n", file);

	// Emit message list used by debugger to match opcodes with functions
	//
	fprintf(file, "\n"
		"void *\n"
		"%s_msg_list[] = {\n",
		subsystemname);
	nodes.reset();
	while (node = (idl_node_t *)nodes.next()) {
		if (node->nodeid != idl_node_t::interface_node)
			continue;
		idl_interface_t *interface = (idl_interface_t *)node;
		listiter_t params(interface->parameters);

		// Emit definition of local interface message pointer.
		fprintf(file,
			"\t(void *)I_%s_%s,\n", subsystemname, interface->name);
	}
	// End defintion of message list
	//
	fputs("};\n", file);

	// Return successfully ...
	//
	fclose(file);
	return(1);
}
