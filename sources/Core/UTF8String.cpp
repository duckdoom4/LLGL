/*
 * UTF8String.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include <LLGL/Container/UTF8String.h>
#include <algorithm>
#include <iterator>
#include "Exception.h"
#include "Assertion.h"


namespace LLGL
{


static std::size_t GetUTF16CharCount(const StringView& s)
{
    return s.size();
}

static SmallVector<wchar_t> ConvertToUTF16WCharArray(const StringView& s)
{
    /* Allocate buffer for UTF-16 string */
    const std::size_t len = GetUTF16CharCount(s);

    SmallVector<wchar_t> utf16;
    utf16.reserve(len + 1);

    /* Encode UTF-16 string */
    for (auto it = s.begin(); it != s.end();)
    {
        int c = static_cast<unsigned char>(*it++);

        /* Check for bit pattern 0xxxxxxx */
        if ((c & 0x80) == 0x00)
        {
            /* Read one byte */
            utf16.push_back(c);
        }
        /* Check for bit pattern 110xxxxx */
        else if ((c & 0xE0) == 0xC0)
        {
            /* Read two bytes */
            wchar_t w0 = static_cast<wchar_t>(c & 0x1F); c = *it++;
            wchar_t w1 = static_cast<wchar_t>(c & 0x3F);
            utf16.push_back(w0 << 5 | w1);
        }
        /* Check for bit pattern 1110xxxx */
        else if ((c & 0xF0) == 0xE0)
        {
            /* Read three bytes */
            wchar_t w0 = static_cast<wchar_t>(c & 0x0F); c = *it++;
            wchar_t w1 = static_cast<wchar_t>(c & 0x3F); c = *it++;
            wchar_t w2 = static_cast<wchar_t>(c & 0x3F);
            utf16.push_back(w0 << 12 | w1 << 6 | w2);
        }
        else
            LLGL_TRAP("UTF8 character bigger than two bytes");
    }

    utf16.push_back(L'\0');

    return utf16;
}

/*
 * UTF8String class
 */

UTF8String& UTF8String::operator = (const UTF8String& rhs)
{
    data_ = rhs.data_;
    return *this;
}

UTF8String& UTF8String::operator = (UTF8String&& rhs) noexcept
{
    data_ = std::move(rhs.data_);
    rhs.clear();
    return *this;
}

UTF8String& UTF8String::operator += (const UTF8String& rhs)
{
    return append(rhs.begin(), rhs.end());
}

UTF8String& UTF8String::operator += (const StringView& rhs)
{
    return append(rhs.begin(), rhs.end());
}

UTF8String& UTF8String::operator += (const WStringView& rhs)
{
    auto utf8String = ConvertWStringViewToUTF8CharArray(rhs);
    return append(utf8String.begin(), utf8String.end());
}

UTF8String& UTF8String::operator += (const char* rhs)
{
    const StringView rhsView{ rhs };
    return append(rhsView.begin(), rhsView.end());
}

UTF8String& UTF8String::operator += (const wchar_t* rhs)
{
    auto utf8String = ConvertWStringViewToUTF8CharArray(rhs);
    return append(utf8String.begin(), utf8String.end());
}

UTF8String& UTF8String::operator += (char chr)
{
    return append(1u, chr);
}

UTF8String& UTF8String::operator += (wchar_t chr)
{
    if (GetUTF8CharCount(chr) > 1)
    {
        wchar_t str[] = { chr, L'\0' };
        auto utf8String = ConvertWStringViewToUTF8CharArray(str);
        return append(utf8String.begin(), utf8String.end());
    }
    return append(1u, static_cast<char>(chr));
}

constexpr void UTF8String::clear()
{
    data_ = { '\0' };
}

int UTF8String::compare(const StringView& str) const
{
    return StringView{ data(), size() }.compare(str);
}

int UTF8String::compare(size_type pos1, size_type count1, const StringView& str) const
{
    return StringView{ data(), size() }.compare(pos1, count1, str);
}

int UTF8String::compare(size_type pos1, size_type count1, const StringView& str, size_type pos2, size_type count2) const
{
    return StringView{ data(), size() }.compare(pos1, count1, str, pos2, count2);
}

int UTF8String::compare(const WStringView& str) const
{
    auto utf8String = ConvertWStringViewToUTF8CharArray(str);
    return compare(StringView{ utf8String.data(), utf8String.size() });
}

int UTF8String::compare(size_type pos1, size_type count1, const WStringView& str) const
{
    auto utf8String = ConvertWStringViewToUTF8CharArray(str);
    return compare(pos1, count1, StringView{ utf8String.data(), utf8String.size() });
}

int UTF8String::compare(size_type pos1, size_type count1, const WStringView& str, size_type pos2, size_type count2) const
{
    auto utf8String = ConvertWStringViewToUTF8CharArray(str.substr(pos2, count2));
    return compare(pos1, count1, StringView{ utf8String.data(), utf8String.size() });
}

UTF8String UTF8String::substr(size_type pos, size_type count) const
{
    if (pos > size())
        LLGL_TRAP("start position for UTF8 string out of range");
    count = std::min(count, size() - pos);
    return StringView{ &(data_[pos]), count };
}

void UTF8String::resize(size_type size, char ch)
{
    if (size != this->size())
    {
        /* Remove NUL-terminator temporarily to avoid unnecessary reallocations and copy operations of the internal container */
        data_.pop_back();
        data_.reserve(size + 1);
        data_.resize(size, ch);
        data_.push_back('\0');
    }
}

UTF8String& UTF8String::append(size_type count, char ch)
{
    resize(size() + count, ch);
    return *this;
}

UTF8String& UTF8String::append(const char* first, const char* last)
{
    /* Remove NUL-terminator temporarily to avoid unnecessary reallocations and copy operations of the internal container */
    const difference_type dist = std::distance(first, last);
    if (dist > 0)
    {
        data_.pop_back();
        data_.reserve(static_cast<size_type>(dist) + 1);
        data_.insert(data_.end(), first, last);
        data_.push_back('\0');
    }
    return *this;
}

SmallVector<wchar_t> UTF8String::to_utf16() const
{
    return ConvertToUTF16WCharArray(StringView{ c_str(), size() });
}


} // /namespace LLGL



// ================================================================================
