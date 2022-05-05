#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>

bool iequals(const std::wstring_view a, std::wstring_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](const wchar_t a, const wchar_t b) {
            // If the high-order word of this parameter is zero, 
            // the low-order word must contain a single character to be converted.
            return CharUpperW((LPWSTR)a) == CharUpperW((LPWSTR)b);
        });
}

DWORD ConvertFiletimeToLocalTime(const FILETIME* filetime, SYSTEMTIME* localTime)
{
	SYSTEMTIME UTCSysTime;
	if (!FileTimeToSystemTime(filetime, &UTCSysTime))
	{
		fprintf(stderr,"FileTimeToSystemTime\n");
        return GetLastError();
	}
	else if (!SystemTimeToTzSpecificLocalTime(NULL, &UTCSysTime, localTime))
	{
		fprintf(stderr, "SystemTimeToTzSpecificLocalTime\n");
        return GetLastError();
	}

	return 0;
}

bool parse_full_key_zfuass(wchar_t* fullkey_param, std::optional<std::wstring>* machine, HKEY* key, std::optional<std::wstring>* subkey)
{
    std::wstring_view input(fullkey_param);

    if ( input.length() < 3 ) {
        return false;
    }
    // --- machine -----------------------------------------------------------
    size_t idx_key;
    if ( input[0] == L'\\' && input[1] == L'\\' ) {
        idx_key = input.find_first_of(L'\\', 2);
        if ( idx_key == 2 || idx_key == std::string::npos ) {
            fprintf(stderr,"wrong machine name\n");
            return false;
        }
        else {
            machine->emplace(input.substr(2, idx_key-2));
            idx_key += 1;
        }
    }
    else {
        machine->reset();
        idx_key = 0;
    }
    // --- key ---------------------------------------------------------------
    if ( idx_key >= input.length() ) {
        fprintf(stderr,"no key found\n");
        return false;
    }

    size_t idx_subkey = input.find_first_of(L'\\', idx_key);
    std::wstring_view key_name;
    if ( idx_subkey == std::string::npos ) {
        key_name = input.substr(idx_key);
    }
    else {
        // 0123456789
        // \\.\key\su
        //     ^  ^     == 7-4=3
        key_name = input.substr(idx_key, idx_subkey-idx_key);
        idx_subkey += 1;
    }    

    if      ( iequals(L"HKLM", key_name) ) { *key = HKEY_LOCAL_MACHINE; }
    else if ( iequals(L"HKCU", key_name) ) { *key = HKEY_CURRENT_USER; }
    else if ( iequals(L"HKCR", key_name) ) { *key = HKEY_CLASSES_ROOT; }
    else if ( iequals(L"HKU",  key_name) ) { *key = HKEY_USERS; }
    else if ( iequals(L"HKCC", key_name) ) { *key = HKEY_CURRENT_CONFIG; }
    else { fprintf(stderr,"wrong key\n"); return false; }
    // --- subkey ------------------------------------------------------------
    if ( idx_subkey >= input.length() ) {
        subkey->reset();
    }
    else {
        subkey->emplace( input.substr(idx_subkey) );
    }

    wprintf(L"machine: %s\n"
            L"key    : %s\n"
            L"subkey : %s\n", 
            machine->has_value() ? machine->value().c_str() : L"n/a", 
            std::wstring(key_name).c_str(), 
            subkey->has_value() ? subkey->value().c_str() : L"n/a"); 

    return true;
}

LSTATUS open_registry_key(const std::optional<std::wstring>& machine, const HKEY key, const std::optional<std::wstring>& subkey, PHKEY phkey)
{
    LSTATUS lstatus  = ERROR_SUCCESS;
    HKEY    base_key = NULL;

    if ( machine.has_value() )
    {
        HKEY remote_key;
        if ( (lstatus=RegConnectRegistryExW(
            machine.value().c_str()
            , key
            , 0
            , &remote_key
            )) != ERROR_SUCCESS )
        {
            fprintf(stderr,"0x%0X RegConnectRegistryExW\n", lstatus);
        }
        else
        {
            base_key = remote_key;
        }
    }
    else
    {
        base_key = key;
    }
    
    if (lstatus == ERROR_SUCCESS)
    {
        if ( subkey.has_value() ) 
        {
            if ( (lstatus=RegOpenKeyExW( 
            base_key
            , subkey.value().c_str()
            , 0 // options
            , KEY_READ  
            , phkey)) != ERROR_SUCCESS ) 
            {
                fprintf(stderr,"0x%0X RegOpenKeyExW\n", lstatus);
            }
            else
            {
                RegCloseKey(base_key);
            }
        }
        else
        {
            *phkey = base_key;
        }
    }

    return lstatus;
}

LSTATUS query_key(const HKEY key, LPDWORD lpcSubKeys, LPDWORD lpcbMaxSubKeyLen, LPDWORD lpcValues, LPDWORD lpcbMaxValueNameLen, LPDWORD lpcbMaxValueLen )
{
    return RegQueryInfoKeyW(
        key                   // [in]                HKEY      hKey,
        , NULL                // [out, optional]     LPWSTR    lpClass,
        , NULL                // [in, out, optional] LPDWORD   lpcchClass,
        , NULL                //                     LPDWORD   lpReserved,
        , lpcSubKeys          // [out, optional]     LPDWORD   lpcSubKeys,
        , lpcbMaxSubKeyLen    // [out, optional]     LPDWORD   lpcbMaxSubKeyLen,
        , NULL                // [out, optional]     LPDWORD   lpcbMaxClassLen,
        , lpcValues           // [out, optional]     LPDWORD   lpcValues,
        , lpcbMaxValueNameLen // [out, optional]     LPDWORD   lpcbMaxValueNameLen,
        , lpcbMaxValueLen     // [out, optional]     LPDWORD   lpcbMaxValueLen,
        , NULL                // [out, optional]     LPDWORD   lpcbSecurityDescriptor,
        , NULL                // [out, optional]     PFILETIME lpftLastWriteTime
        );
}

LSTATUS enum_keys(const HKEY key, DWORD cSubKeys, DWORD cbMaxSubKeyLen)
{
    std::vector<WCHAR> key_name;
    FILETIME ft;
    SYSTEMTIME localTime;
    
    key_name.resize(cbMaxSubKeyLen + 1);

    for (int idx=0; idx < cSubKeys; ++idx) 
    {
        DWORD cchName = key_name.size();
        const LSTATUS lstatus = RegEnumKeyExW(
            key
            , idx
            , key_name.data()
            , &cchName
            , NULL      // Reserved
            , NULL      // lpClass
            , NULL      // lpcchClass
            , &ft);
        if ( lstatus == ERROR_MORE_DATA)
        {
            _putws(L"RegEnumKeyExW() ERROR_MORE_DATA. should not be possible.");
        }
        else if (lstatus == ERROR_NO_MORE_ITEMS) 
        {
            return ERROR_SUCCESS;
        }
        else if (lstatus != ERROR_SUCCESS)
        {
            return lstatus;
        }
        else
        {
            if ( ConvertFiletimeToLocalTime(&ft, &localTime) == 0 )
            {
                wprintf(L"%4d.%02d.%02d %02d:%02d:%02d\t%s\n", 
                    localTime.wYear, localTime.wMonth, localTime.wDay,
                    localTime.wHour, localTime.wMinute, localTime.wSecond,
                    key_name.data());
            }
        }
    }
    return ERROR_SUCCESS;
}

LSTATUS enum_values(const HKEY key, DWORD cValues, DWORD cbMaxValueNameLen, WORD cbMaxValueLen )
{
    LSTATUS lstatus;

    std::vector<WCHAR> value_name;
    std::vector<BYTE>  value;

    value_name.resize(cbMaxValueNameLen + 1);
    value     .resize(cbMaxValueLen);

    for (int idx=0; idx < cValues; ++idx)
    {
        DWORD type;
        DWORD cchValueName = value_name.size();
        DWORD cbData       = value.size();

        if ( (lstatus=RegEnumValueW(
            key
            , idx
            , value_name.data()
            , &cchValueName
            , NULL
            , &type
            , value.data()
            , &cbData )) == ERROR_NO_MORE_ITEMS )
        {
            return ERROR_SUCCESS;
        }
        else if (lstatus != ERROR_SUCCESS )
        {
            return lstatus;
        }
        else
        {
            wprintf(L"%s\n", value_name.data());
        }
    }
    return ERROR_SUCCESS;
}

int wmain(int argc, wchar_t* argv[])
{
    if ( argc != 2 ) {
        fwprintf(stderr, L"usage: %s [\\\\Machine\\]{ HKLM | HKCU | HKCR | HKU | HKCC }\\Key\n", argv[0]);
        return 4;
    }

    int rc = 0;
    LSTATUS lstatus;
    std::optional<std::wstring> machine;
    HKEY base_key;
    HKEY key = NULL;
    std::optional<std::wstring> subkey;

    DWORD cSubKeys;
    DWORD cbMaxSubKeyLen;
    DWORD cValues;
    DWORD cbMaxValueNameLen;
    DWORD cbMaxValueLen;

    //if ( ! parse_full_key( argv[1], &machine, &base_key, &subkey ) )
    if ( ! parse_full_key_zfuass( argv[1], &machine, &base_key, &subkey ) )
    {
        fprintf(stderr, "could not parse your given key\n");
        rc = 8;
    }
    else if ( (lstatus=open_registry_key(machine, base_key, subkey, &key)) != ERROR_SUCCESS ) 
    {
        rc = 12;
    }
    else if ( (lstatus = query_key(key, &cSubKeys, &cbMaxSubKeyLen, &cValues, &cbMaxValueNameLen, &cbMaxValueLen)) != ERROR_SUCCESS )
    {
        fprintf(stderr, "0x%08X RegQueryInfoKeyW\n", lstatus);
        rc = 14;
    }
    else if ( (lstatus = enum_keys(key, cSubKeys, cbMaxSubKeyLen)) != ERROR_SUCCESS )
    {
        fprintf(stderr, "0x%08X RegEnumKeyExW\n", lstatus);
        rc = 16;
    }
    else if ( (lstatus = enum_values(key, cValues, cbMaxValueNameLen, cbMaxValueLen)) != ERROR_SUCCESS )
    {
        fprintf(stderr, "0x%08X RegEnumValueW\n", lstatus);
        rc = 18;
    }

    if ( key != nullptr ) 
    {
        RegCloseKey(key);
    }

    return rc;
}




/*

bool iequals(const std::wstring_view a, std::wstring::const_iterator b_begin, std::wstring::const_iterator b_end)
{
    return std::equal(a.begin(), a.end(), b_begin, b_end,
        [](const wchar_t a, const wchar_t b) {
            // If the high-order word of this parameter is zero, 
            // the low-order word must contain a single character to be converted.
            return CharUpperW((LPWSTR)a) == CharUpperW((LPWSTR)b);
        });
}


bool parse_full_key(wchar_t* fullkey_param, std::optional<std::wstring>* machine, HKEY* key, std::optional<std::wstring>* subkey)
{
    machine->reset();
    *key = NULL;
    subkey->reset();

    try {
        const std::wregex registry_regex(LR"((?:(\\\\[^\\]+)\\)?(HKLM|HKCU|HKCR|HKU|HKCC)(?:\\(.+))?)", std::regex_constants::icase);
        const std::wstring fullkey(fullkey_param);

        std::wsmatch match;
        if ( std::regex_match(fullkey, match, registry_regex) )
        {
            
            wprintf(L"machine: %s\n"
                    L"key    : %s\n"
                    L"subkey : %s\n", 
                    match[1].str().c_str(), 
                    match[2].str().c_str(), 
                    match[3].str().c_str()); 

            if ( ! match[1].str().empty() ) {
                *machine = match[1].str();
            }

            if ( ! match[3].str().empty() ) {
                *subkey = match[3].str();
            }

            auto m       = match[2];
            auto m_begin = m.first;
            auto m_end   = m.second;

            if      ( iequals(L"HKLM", m_begin, m_end) ) { *key = HKEY_LOCAL_MACHINE; }
            else if ( iequals(L"HKCU", m_begin, m_end) ) { *key = HKEY_CURRENT_USER; }
            else if ( iequals(L"HKCR", m_begin, m_end) ) { *key = HKEY_CLASSES_ROOT; }
            else if ( iequals(L"HKU",  m_begin, m_end) ) { *key = HKEY_USERS; }
            else if ( iequals(L"HKCC", m_begin, m_end) ) { *key = HKEY_CURRENT_CONFIG; }

        }
        else
        {
            return false;
        }
    }
    catch (std::exception& ex)
    {
        puts(ex.what());
    }
    
    return true;
}
*/
