#ifndef __GENPARAMARRAY_H__
#define __GENPARAMARRAY_H__

#include <stdlib.h>
#include "fourccdef.h"

struct serv_gen_param_s
{
	int magic; // always be OOKFOURCC_GPAR
	int count;
	
	int   * n; // name in fourcc
	void ** p; // value
};

inline void init_serv_gen_param_s(serv_gen_param_s * s)
{
	s->magic = OOKFOURCC_GPAR;
	s->count = 0;
	s->n     = NULL;
	s->p     = NULL;
}

#endif
