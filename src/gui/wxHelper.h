#pragma once
#include <wx/string.h>

namespace wxHelper
{
    // wxString to utf8 std::string
    inline std::string MakeUTF8(const wxString& str)
    {
        auto tmpUtf8 = str.ToUTF8();
        return std::string(tmpUtf8.data(), tmpUtf8.length());
    }

    inline fs::path MakeFSPath(const wxString& str)
    {
        auto tmpUtf8 = str.ToUTF8();
        auto sv = std::basic_string_view<char8_t>((const char8_t*)tmpUtf8.data(), tmpUtf8.length());
        return fs::path(sv);
    }

    inline wxString FromUtf8(std::string_view str)
    {
        return wxString::FromUTF8(str.data(), str.size());
    }


};
