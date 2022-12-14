#if !defined(_node_h_)
#define _node_h_

#include "list.h"

class idl_node_t : public listitem_t {
public:
    enum nodetype { subsystem_node, import_node, opcode_node, interface_node };
    enum nodetype nodeid;		// type of this node

    idl_node_t(enum nodetype _nodeid)
	: nodeid(_nodeid) {}
    virtual ~idl_node_t() {}
};

class idl_subsystem_t : public idl_node_t {
public:
    char *name;				// subsystem name for names and files
    int id;				// RPC subsystemid ID for message demux

    idl_subsystem_t(char *_name, int _id)
	: idl_node_t(subsystem_node), name(_name), id(_id) {}
    ~idl_subsystem_t() { delete name; }
};

class idl_import_t : public idl_node_t {
public:
    char *file;				// "file.h" or <file.h>

    idl_import_t(char *_file)
	: idl_node_t(import_node), file(_file) {}
    ~idl_import_t() { delete file; }
};

class idl_opcode_t : public idl_node_t {
public:
    int opcode;				// RPC message opcode

    idl_opcode_t(int _opcode)
	: idl_node_t(opcode_node), opcode(_opcode) {}
};

class idl_parameter_t : public listitem_t {
public:
    int inout;				// IN, OUT, or INOUT
    int valref;				// POINTERTO or VALUEOF
    int debugsize;			// size in bits for structs for tracing
    char *type;				// type of parameter
    char *name;				// name of parameter

    char *mesg_name;			// name of parameter in interface
					//   message definition (filled in
					//   when *_mesg.h is constructed)

    idl_parameter_t(int _inout, int _valref, int _debugsize, char *_type, char *_name)
	: inout(_inout), valref(_valref), debugsize(_debugsize),
	  type(_type), name(_name), mesg_name(NULL)
    {}
    ~idl_parameter_t()
    {
	delete type;
	delete name;
	delete mesg_name;
    }

    void chain(idl_parameter_t *after)
    {
	/*
	 * We only chain two guys together
	 */
	assert(prev == NULL);
	assert(next == NULL);
	assert(after->prev == NULL);
	assert(after->next == NULL);
	next = after;
	after->prev = this;
    }
};

class idl_interface_t : public idl_node_t {
public:
    int opcode;				// RPC opcode
    int synchronicity;			// SYNCHRONOUS or ASYNCHRONOUS
    char *idl_type;			// int or void if SYNCHRONOUS
    char *condition;			// NULL or synchronicity condition
    char *name;				// name of interface
    list_t *parameters;			// interface parameters
    char *prologue;			// client debug prologue code
    char *epilogue;			// server debug epilogue code

    // Number of parameters of various type combinations.  Note that while
    // there are theoretically 6 different combinations, "inout valueof"
    // and "out valueof" are illegal.  At this point all we really *need*
    // to know is if both "in valueof" and "out pointerto" parameters are
    // present in order to toss those two sets into a union in the constructed
    // interface message definition.  But it's not much more work to count
    // the others and they can be used to avoid useless scanning of the
    // parameter list ...
    //
    int in_val_params;			// "in valueof"
    int in_ref_params;			// "in pointerto" and "in bufferof"
    int inout_ref_params;		// "inout pointerto"
    int out_ref_params;			// "out pointerto" and "out bufferof"
    int in_buf_params;			// "in bufferof"
    int out_buf_params;			// "out bufferof"
    int vector_params;			// "vectorof" parameters
    int in_params;			// total "in" parameters
    int out_params;			// total "out" paramaters


    size_t mesg_size;			// size of request/reply message

    // The idl_interface_t constructor can't be inline'd because it needs
    // constants generated by the parser generator in "parser.h".
    // Unfortunately the parser generator also needs to include this header,
    // so we have a circular dependency.
    //
    idl_interface_t(int _opcode,
		    int _synchronicity, char *_idl_type, char *_condition,
		    char *_name, list_t *_parameters,
		    char *_prologue, char *_epilogue);

    ~idl_interface_t()
    {
	delete name;
	delete condition;
	delete prologue;
	delete epilogue;

	listiter_t params(parameters);
	idl_parameter_t *param;
	while (param = (idl_parameter_t *)params.next()) {
	    param->unlink();
	    delete param;
	}
	delete parameters;
    }
};

#endif /* _node_h_ */
