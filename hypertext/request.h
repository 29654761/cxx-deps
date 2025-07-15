#pragma once

#include "message.h"
#include "uri.h"

namespace hypertext
{

	class request :public message
	{
	public:
		request();
		virtual ~request();

	};

}

