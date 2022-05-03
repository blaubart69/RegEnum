#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

#include <regex>
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>


bool iequals(const std::wstring_view& a, const std::wstring_view& b)
{
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      [](char a, char b) {
                          return std::toupper(a) == std::toupper(b);
                      });
}

bool parse_full_key(wchar_t* fullkey_param, std::optional<std::wstring_view>* machine, HKEY* key, std::optional<std::wstring_view>* subkey)
{
    machine->reset();
    key = nullptr;
    subkey->reset();

    try {
        const std::wregex registry_regex(LR"((?:\\\\([^\\]+)\\)?(HKLM|HKCU|HKCR|HKU|HKCC)(?:\\(.+))?)", std::regex_constants::icase);
        const std::wstring fullkey(fullkey_param);

        std::wsmatch match;
        if ( std::regex_match(fullkey, match, registry_regex) )
        {
            wprintf(L"Group 1: %s\n", match[1].str().c_str());
            wprintf(L"Group 2: %s\n", match[2].str().c_str());
            wprintf(L"Group 3: %s\n", match[3].str().c_str());

            if ( ! match[1].str().empty() ) {
                *machine = match[1].str();
            }

            if ( ! match[3].str().empty() ) {
                *subkey = match[3].str();
            }

            std::wstring_view root_key(match[2].str());
            if      ( iequals(root_key, L"HKLM") ) { *key = HKEY_LOCAL_MACHINE; }
            else if ( iequals(root_key, L"HKCU") ) { *key = HKEY_CURRENT_USER; }
            else if ( iequals(root_key, L"HKCR") ) { *key = HKEY_CLASSES_ROOT; }
            else if ( iequals(root_key, L"HKU" ) ) { *key = HKEY_USERS; }
            else if ( iequals(root_key, L"HKCC") ) { *key = HKEY_CURRENT_CONFIG; }

        }
        else
        {
            puts("NO match!\n");
        }
    }
    catch (std::exception& ex)
    {
        puts(ex.what());
    }
    
    return true;
}

int wmain(int argc, wchar_t* argv[])
{
    if ( argc != 2 ) {
        fwprintf(stderr, L"usage: %s [\\\\Machine\\]{ HKLM | HKCU | HKCR | HKU | HKCC }\\Key\n", argv[0]);
        return 4;
    }

    std::optional<std::wstring_view> machine;
    HKEY key;
    std::optional<std::wstring_view> subkey;

    if ( ! parse_full_key( argv[1], &machine, &key, &subkey ) )
    {
        fprintf(stderr, "could not parse your given key\n");
        return 8;
    }

    return 0;
}