/*
 * UTF8String.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_UTF8_STRING_H
#define LLGL_UTF8_STRING_H


#include <LLGL/Export.h>
#include <LLGL/Container/SmallVector.h>
#include <LLGL/Container/StringView.h>


namespace LLGL
{


/**
\brief Container class for UTF-8 encoded strings.
\remarks This class converts between \c char and \c wchar_t strings automatically, but stores strings always as UTF-8 encoded \c char strings.
*/
class LLGL_EXPORT UTF8String
{

    public:

        using value_type                = char;
        using size_type                 = std::size_t;
        using difference_type           = std::ptrdiff_t;
        using reference                 = value_type&;
        using const_reference           = const value_type&;
        using pointer                   = value_type*;
        using const_pointer             = const value_type*;
        using iterator                  = value_type*;
        using const_iterator            = const value_type*;
        using reverse_iterator          = std::reverse_iterator<iterator>;
        using const_reverse_iterator    = std::reverse_iterator<const_iterator>;

    public:

        static constexpr size_type npos = size_type(-1);

    public:

        //! Initialize an empty string.
        constexpr UTF8String() :
            data_{ '\0' }
        {
        }

        //! Initialies the UTF-8 string with a copy of the specified string.
        constexpr UTF8String(const UTF8String& rhs) :
            data_{ rhs.data_ }
        {
        }

        //! Takes the ownership of the specified UTF-8 string.
        constexpr UTF8String(UTF8String&& rhs) noexcept :
            data_{ std::move(rhs.data_) }
        {
            rhs.clear();
        }

        //! Initializes the UTF-8 string with a copy of the specified string view.
        constexpr UTF8String(const StringView& str) :
            data_{ ConvertStringViewToCharArray(str) }
        {
        }

        //! Initializes the UTF-8 string with a UTF-8 encoded conversion of the specified wide string view.
        constexpr UTF8String(const WStringView& str) :
            data_{ ConvertWStringViewToUTF8CharArray(str) }
        {
        }

        //! Initializes the UTF-8 string with a copy of the specified null-terminated string.
        constexpr UTF8String(const char* str) :
            UTF8String{ StringView{ str } }
        {
        }

        //! Initializes the UTF-8 string with a UTF-8 encoded conversion of the specified null-terminated wide string.
        constexpr UTF8String(const wchar_t* str) :
            UTF8String{ WStringView{ str } }
        {
        }

        //! Initializes the UTF-8 string with a copy of another templated string class. This would usually be std::basic_string.
        template <template <class, class, class> class TString, class TChar, class Traits, class Allocator>
        constexpr UTF8String(const TString<TChar, Traits, Allocator>& str) :
            UTF8String { str.c_str() }
        {
        }

    public:

        inline bool empty() const noexcept
        {
            return (size() == 0);
        }

        inline size_type size() const noexcept
        {
            return (data_.size() - 1);
        }

        inline size_type length() const noexcept
        {
            return size();
        }

        inline size_type capacity() const noexcept
        {
            return (data_.capacity() - 1);
        }

        inline const_pointer data() const noexcept
        {
            return data_.data();
        }

        inline const_pointer c_str() const noexcept
        {
            return data_.data();
        }

    public:

        inline const_reference at(size_type pos) const
        {
            return data_[pos];
        }

        inline const_reference front() const
        {
            return *begin();
        }

        inline const_reference back() const
        {
            return *rbegin();
        }

    public:

        inline const_iterator begin() const noexcept
        {
            return data_.begin();
        }

        inline const_iterator cbegin() const noexcept
        {
            return data_.begin();
        }

        inline const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator{ end() };
        }

        inline const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator{ cend() };
        }

        inline const_iterator end() const noexcept
        {
            return (data_.end() - 1);
        }

        inline const_iterator cend() const noexcept
        {
            return (data_.end() - 1);
        }

        inline const_reverse_iterator rend() const
        {
            return const_reverse_iterator{ begin() };
        }

        inline const_reverse_iterator crend() const
        {
            return const_reverse_iterator{ cbegin() };
        }

    public:

        constexpr void clear();

        int compare(const StringView& str) const;
        int compare(size_type pos1, size_type count1, const StringView& str) const;
        int compare(size_type pos1, size_type count1, const StringView& str, size_type pos2, size_type count2 = npos) const;

        int compare(const WStringView& str) const;
        int compare(size_type pos1, size_type count1, const WStringView& str) const;
        int compare(size_type pos1, size_type count1, const WStringView& str, size_type pos2, size_type count2 = npos) const;

        UTF8String substr(size_type pos = 0, size_type cout = npos) const;

        void resize(size_type size, char ch = '\0');

        UTF8String& append(size_type count, char ch);
        UTF8String& append(const char* first, const char* last);

    public:

        //! Convert this string to a NUL-terminated UTF-16 string.
        SmallVector<wchar_t> to_utf16() const;

    public:

        UTF8String& operator = (const UTF8String& rhs);
        UTF8String& operator = (UTF8String&& rhs) noexcept;

        UTF8String& operator += (const UTF8String& rhs);
        UTF8String& operator += (const StringView& rhs);
        UTF8String& operator += (const WStringView& rhs);
        UTF8String& operator += (const char* rhs);
        UTF8String& operator += (const wchar_t* rhs);
        UTF8String& operator += (char chr);
        UTF8String& operator += (wchar_t chr);

        inline const_reference operator [] (size_type pos) const
        {
            return data_[pos];
        }

        //! Conversion operator to a null-terminated string.
        inline operator const_pointer () const
        {
            return c_str();
        }

        //! Conversion operator to a string view.
        inline operator StringView () const
        {
            return StringView{ data(), size() };
        }

    private:
        constexpr static std::size_t GetUTF8CharCount(int c)
        {
            if (c < 0x0080)
            {
                /* U+0000 ... U+007F */
                return 1;
            }
            else if (c < 0x07FF)
            {
                /* U+0080 ... U+07FF */
                return 2;
            }
            else if (c < 0xFFFF)
            {
                /* U+0800 ... U+FFFF */
                return 3;
            }
            else
            {
                /* U+10000 ... U+10FFFF */
                return 4;
            }
        }

        constexpr static std::size_t GetUTF8CharCount(const WStringView& s)
        {
            std::size_t len = 0;

            for (int c : s)
                len += GetUTF8CharCount(c);

            return len;
        }

        // Appends a unicode character encoded in UTF-8 to the specified string buffer and returns a pointer to the next character in that buffer.
        // see https://en.wikipedia.org/wiki/UTF-8
        constexpr static void AppendUTF8Character(SmallVector<char>& str, int code)
        {
            if (code < 0x0080)
            {
                /* U+0000 ... U+007F */
                str.push_back(static_cast<char>(code));                         // 0ccccccc
            }
            else if (code < 0x07FF)
            {
                /* U+0080 ... U+07FF */
                str.reserve(str.size() + 2);
                str.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F))); // 110ccccc
                str.push_back(static_cast<char>(0x80 | (code & 0x3F))); // 10cccccc
            }
            else if (code < 0xFFFF)
            {
                /* U+0800 ... U+FFFF */
                str.reserve(str.size() + 3);
                str.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F))); // 1110cccc
                str.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F))); // 10cccccc
                str.push_back(static_cast<char>(0x80 | (code & 0x3F))); // 10cccccc
            }
            else
            {
                /* U+10000 ... U+10FFFF */
                str.reserve(str.size() + 4);
                str.push_back(static_cast<char>(0xF0 | ((code >> 18) & 0x07))); // 11110ccc
                str.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F))); // 10cccccc
                str.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F))); // 10cccccc
                str.push_back(static_cast<char>(0x80 | (code & 0x3F))); // 10cccccc
            }
        }

        constexpr static SmallVector<char> ConvertWStringViewToUTF8CharArray(const WStringView& s)
        {
            /* Allocate buffer for UTF-16 string */
            const auto len = GetUTF8CharCount(s);

            SmallVector<char> utf8;
            utf8.reserve(len + 1);

            /* Encode UTF-8 string */
            for (int c : s)
                AppendUTF8Character(utf8, c);

            utf8.push_back('\0');

            return utf8;
        }

        constexpr static SmallVector<char> ConvertStringViewToCharArray(const StringView& str)
        {
            SmallVector<char> data;
            data.reserve(str.size() + 1);
            data.insert(data.end(), str.begin(), str.end());
            data.push_back('\0');
            return data;
        }

        SmallVector<char> data_;

};


} // /namespace LLGL


#endif



// ================================================================================
