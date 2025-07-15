#pragma once

#include <ptlib.h>
#include <ptlib/pprocess.h>

namespace voip
{

	class process :public PProcess
	{
		PCLASSINFO(process, PProcess)
	public:
		process(const char* manuf = "",         ///< Name of manufacturer
			const char* name = "",          ///< Name of product
			unsigned majorVersion = 1,       ///< Major version number of the product
			unsigned minorVersion = 0,       ///< Minor version number of the product
			CodeStatus status = ReleaseCode, ///< Development status of the product
			unsigned patchVersion = 1,       ///< Patch version number of the product
			bool library = false,            ///< PProcess is a library rather than an application
			bool suppressStartup = false,    ///< Do not execute Startup()
			unsigned oemVersion = 0          ///< OEM version number of the product
		);

		~process();

		virtual void Main();

	};

}
