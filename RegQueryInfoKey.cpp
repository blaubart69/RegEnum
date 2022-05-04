#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

#include <regex>
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>


bool iequals(const std::wstring_view a, std::wstring::const_iterator b_begin, std::wstring::const_iterator b_end)
{
    return std::equal(a.begin(), a.end(),
                      b_begin, b_end,
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
        }
        else
        {
            *phkey = base_key;
        }
    }

    return lstatus;
}

LSTATUS enum_key(const HKEY key)
{
    std::vector<WCHAR> key_name;
    FILETIME ft;
    SYSTEMTIME localTime;
    int idx=0;

    key_name.resize(8);
    for (;;) 
    {
        DWORD cchName = key_name.size();
        const LSTATUS enum_rc = RegEnumKeyExW(
            key
            , idx
            , key_name.data()
            , &cchName
            , NULL      // Reserved
            , NULL      // lpClass
            , NULL      // lpcchClass
            , &ft);
        if ( enum_rc == ERROR_MORE_DATA)
        {
            key_name.resize(key_name.size() * 4);
        }
        else if (enum_rc == ERROR_NO_MORE_ITEMS) 
        {
            return 0;
        }
        else if (enum_rc != ERROR_SUCCESS)
        {
            return enum_rc;
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
            ++idx;
        }
    }
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

    if ( ! parse_full_key( argv[1], &machine, &base_key, &subkey ) )
    {
        fprintf(stderr, "could not parse your given key\n");
        rc = 8;
    }
    else if ( (lstatus=open_registry_key(machine, base_key, subkey, &key)) != ERROR_SUCCESS ) 
    {
        rc = 12;
    }
    else if ( (lstatus = enum_key(key)) != ERROR_SUCCESS )
    {
        fprintf(stderr, "0x%08X RegEnumKeyExW\n", lstatus);
        rc = 14;
    }

    if ( key != nullptr ) 
    {
        RegCloseKey(key);
    }

    return rc;
}