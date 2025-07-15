#include "process.h"

namespace voip
{

	process::process(const char* manuf,         ///< Name of manufacturer
		const char* name,          ///< Name of product
		unsigned majorVersion,       ///< Major version number of the product
		unsigned minorVersion,       ///< Minor version number of the product
		CodeStatus status, ///< Development status of the product
		unsigned patchVersion,       ///< Patch version number of the product
		bool library,            ///< PProcess is a library rather than an application
		bool suppressStartup ,    ///< Do not execute Startup()
		unsigned oemVersion          ///< OEM version number of the product
	) :PProcess(manuf, name, majorVersion, minorVersion, status, patchVersion, library, suppressStartup, oemVersion)
	{
		
	}

	process::~process()
	{
	}

	void process::Main()
	{

	}
}
