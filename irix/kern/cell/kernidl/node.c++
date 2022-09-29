#include "node.h"
#include "parser.h"

idl_interface_t::idl_interface_t(int _opcode,
				 int _synchronicity, char *_idl_type,
				 char *_condition,
				 char *_name, list_t *_parameters,
				 char *_prologue, char *_epilogue)
    : idl_node_t(interface_node), opcode(_opcode),
      synchronicity(_synchronicity), idl_type(_idl_type), condition(_condition),
      name(_name), parameters(_parameters),
      prologue(_prologue), epilogue(_epilogue)
{
    // Count the number of different parameter types.  Initialize message size
    // to zero for now.  It'll be filled in later after the interface message
    // definition has been constructed.
    //
    in_val_params = 0;
    in_ref_params = 0;
    inout_ref_params = 0;
    out_ref_params = 0;
    in_buf_params = 0;
    vector_params = 0;
    out_buf_params = 0;
    listiter_t params(parameters);
    idl_parameter_t *param;
    while (param = (idl_parameter_t *)params.next()) {
	assert(!(param->inout == INOUT && param->valref == VALUEOF));
	assert(!(param->inout == OUT   && param->valref == VALUEOF));
	assert(!(param->inout == INOUT   && param->valref == BUFFEROF));
	switch (param->valref) {
	case	VALUEOF:
		in_val_params++;
		break;
	case	VECTOROF:
		vector_params++;
	case	BUFFEROF:
		if (param->inout == IN)
			in_buf_params++;
		else
			out_buf_params++;
		break;
	case	POINTERTO:
		if (param->inout == IN)
			in_ref_params++;
		else if (param->inout == INOUT)
			inout_ref_params++;
		else
			out_ref_params++;
		break;
	}
    }
    in_params = in_val_params + in_ref_params + inout_ref_params;
    out_params = inout_ref_params + out_ref_params;
    assert(!(synchronicity == ASYNCHRONOUS && out_params != 0));
    mesg_size = 0;
}
