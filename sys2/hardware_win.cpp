/**
 * @file hardware.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */
#include "hardware.h"
#include "string_util.h"
#include "security/hmac_sha1.h"

#ifdef _WIN32
#include <Iphlpapi.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib,"iphlpapi.lib")


namespace sys
{
    hardware::hardware()
    {

    }

    hardware::~hardware()
    {
        cleanup();
    }


	bool hardware::init()
	{
        bool ret = false;
        // Step 1: --------------------------------------------------
        // Initialize COM. ------------------------------------------
        

        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            return ret;  // Program has failed.
        }

        // Step 2: --------------------------------------------------
        // Set general COM security levels --------------------------

        hres = CoInitializeSecurity(NULL,
            -1,                           // COM authentication
            NULL,                         // Authentication services
            NULL,                         // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
            RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
            NULL,                         // Authentication info
            EOAC_NONE,                    // Additional capabilities
            NULL                          // Reserved
        );

        if (FAILED(hres)) {
            CoUninitialize();
            return ret;  // Program has failed.
        }

        // Step 3: ---------------------------------------------------
        // Obtain the initial locator to WMI ------------------------

        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&loc_);

        if (FAILED(hres)) {
            CoUninitialize();
            return ret;  // Program has failed.
        }

        // Step 4: -----------------------------------------------------
        // Connect to WMI through the IWbemLocator::ConnectServer method

        // Connect to the root\cimv2 namespace with
        // the current user and obtain pointer pSvc
        // to make IWbemServices calls.
        hres = loc_->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
            NULL,                     // User name. NULL = current user
            NULL,                     // User password. NULL = current
            0,                        // Locale. NULL indicates current
            NULL,                     // Security flags.
            0,                        // Authority (for example, Kerberos)
            0,                        // Context object
            &svc_                     // pointer to IWbemServices proxy
        );

        if (FAILED(hres)) {
            loc_->Release();
            CoUninitialize();
            return ret;  // Program has failed.
        }


        // Step 5: --------------------------------------------------
        // Set security levels on the proxy -------------------------

        hres = CoSetProxyBlanket(svc_,                         // Indicates the proxy to set
            RPC_C_AUTHN_WINNT,            // RPC_C_AUTHN_xxx
            RPC_C_AUTHZ_NONE,             // RPC_C_AUTHZ_xxx
            NULL,                         // Server principal name
            RPC_C_AUTHN_LEVEL_CALL,       // RPC_C_AUTHN_LEVEL_xxx
            RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
            NULL,                         // client identity
            EOAC_NONE                     // proxy capabilities
        );

        if (FAILED(hres)) {
            svc_->Release();
            loc_->Release();
            CoUninitialize();
            return ret;  // Program has failed.
        }
        return true;
	}

	void hardware::cleanup()
	{
        if (svc_ != nullptr) {
            svc_->Release();
            svc_ = nullptr;
        }
        if (loc_ != nullptr) {
            loc_->Release();
            loc_ = nullptr;
        }
        CoUninitialize();
	}

	std::string hardware::get_board_serial_number()
	{
        std::string board_serial;

        IEnumWbemClassObject* pEnumerator = NULL;
        HRESULT hres = svc_->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_BaseBoard"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnumerator);

        if (FAILED(hres)) {
            return board_serial;  // Program has failed.
        }

        // Step 7: -------------------------------------------------
        // Get the data from the query in step 6 -------------------

        IWbemClassObject* pclsObj = NULL;
        ULONG             uReturn = 0;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (0 == uReturn) {
                break;
            }

            VARIANT vtProp;

            // Get the value of the Name property
            hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
            board_serial = sys::string_util::w2s(vtProp.bstrVal);
            VariantClear(&vtProp);

            pclsObj->Release();
        }

        pEnumerator->Release();
        return board_serial;
	}

	std::string hardware::get_cpu_id()
	{
        std::string cpu_id;
        IEnumWbemClassObject* pEnumerator = NULL;
        HRESULT hres = svc_->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Processor"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnumerator);

        if (FAILED(hres)) {
            return cpu_id;  // Program has failed.
        }

        // Step 7: -------------------------------------------------
        // Get the data from the query in step 6 -------------------

        IWbemClassObject* pclsObj = NULL;
        ULONG             uReturn = 0;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (0 == uReturn) {
                break;
            }

            VARIANT vtProp;

            // Get the value of the Name property
            hr = pclsObj->Get(L"ProcessorId", 0, &vtProp, 0, 0);
            cpu_id = sys::string_util::w2s(vtProp.bstrVal);
            VariantClear(&vtProp);

            pclsObj->Release();
        }

        pEnumerator->Release();

        return cpu_id;
	}

	std::string hardware::get_disk_id()
	{
        std::string serial;
        int num = 0;
        if (!get_os_disk(num)) {
            return serial;
        }
        IEnumWbemClassObject* pEnumerator = NULL;
        HRESULT hres = svc_->ExecQuery(bstr_t("WQL"), bstr_t("SELECT SerialNumber,DeviceID FROM Win32_DiskDrive"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnumerator);

        if (FAILED(hres)) {
            return serial;  // Program has failed.
        }

        // Step 7: -------------------------------------------------
        // Get the data from the query in step 6 -------------------

        IWbemClassObject* pclsObj = NULL;
        ULONG             uReturn = 0;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (0 == uReturn) {
                break;
            }

            VARIANT vtProp;
            // Get the value of the DeviceId property
            hr = pclsObj->Get(L"DeviceId", 0, &vtProp, 0, 0);
            std::string deviceId = sys::string_util::w2s(vtProp.bstrVal);
            VariantClear(&vtProp);

            size_t pos = deviceId.find("PHYSICALDRIVE") + strlen("PHYSICALDRIVE");
            deviceId = deviceId.substr(pos);
            if (num == atoi(deviceId.c_str())) {
                // Get the value of the Name property
                hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
                serial = sys::string_util::w2s(vtProp.bstrVal);
                VariantClear(&vtProp);
                break;
            }
            pclsObj->Release();
        }

        pEnumerator->Release();
        return serial;
	}

	std::set<std::string> hardware::get_mac_addresses()
	{
        std::set<std::string> macs;

        PIP_ADAPTER_INFO pAdapterInfo;
        PIP_ADAPTER_INFO pAdapter = NULL;
        DWORD dwRetVal = 0;


        ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
        if (pAdapterInfo == NULL)
        {
            return macs;
        }
        // Make an initial call to GetAdaptersInfo to get
        // the necessary size into the ulOutBufLen variable
        if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
        {
            free(pAdapterInfo);
            pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
            if (pAdapterInfo == NULL)
            {
                return macs;    //    error data return
            }
        }

        if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
        {
            pAdapter = pAdapterInfo;
            while (pAdapter)
            {
                char buffer[20] = {};
                sprintf_s(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", pAdapter->Address[0],
                    pAdapter->Address[1], pAdapter->Address[2], pAdapter->Address[3],
                    pAdapter->Address[4], pAdapter->Address[5]);
                
                macs.insert(buffer);
                pAdapter = pAdapter->Next;
            }
            if (pAdapterInfo)
                free(pAdapterInfo);
            return macs;    // normal return
        }
        else
        {
            if (pAdapterInfo)
                free(pAdapterInfo);
            return macs;    //    null data return
        }

        return macs;
	}


    bool hardware::get_os_disk(int& num)
    {
        bool ret = false;
        IEnumWbemClassObject* pEnumerator = NULL;
        HRESULT hres = svc_->ExecQuery(bstr_t("WQL"), bstr_t("SELECT Name FROM Win32_OperatingSystem"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnumerator);

        if (FAILED(hres)) {
            return false;  // Program has failed.
        }

        // Step 7: -------------------------------------------------
        // Get the data from the query in step 6 -------------------

        IWbemClassObject* pclsObj = NULL;
        ULONG             uReturn = 0;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (0 == uReturn) {
                break;
            }

            VARIANT vtProp;

            // Get the value of the Name property
            hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
            std::string name = sys::string_util::w2s(vtProp.bstrVal);
            VariantClear(&vtProp);

            pclsObj->Release();

            size_t pos = name.find("Harddisk") + strlen("Harddisk");
            name = name.substr(pos);
            num = atoi(name.c_str());
            ret = true;
        }

        pEnumerator->Release();
        return ret;
    }


    std::string hardware::get_machine_id()
    {
        auto macs = get_mac_addresses();
        std::vector<std::string> vec;
        for (auto itr = macs.begin(); itr != macs.end(); itr++)
        {
            vec.push_back(*itr);
        }
        std::string raw = sys::string_util::join(":", vec);
        hmac_sha1 hmac;
        return hmac.make_string(raw, "B87E9890-5A3B-4B77-B8FA-B1737524D32F");
    }

}


#endif
