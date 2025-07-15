#pragma once

#if defined(_WIN32) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new		 new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define malloc(x)   _malloc_dbg(x,_NORMAL_BLOCK, __FILE__, __LINE__)
#define free(x)	 _free_dbg(x,_NORMAL_BLOCK)


class crt_dbg
{
public:
	crt_dbg()
	{
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	}
	~crt_dbg()
	{
		_CrtDumpMemoryLeaks();
	}
};

crt_dbg crt_;

#endif

//_CrtSetBreakAlloc(326);
