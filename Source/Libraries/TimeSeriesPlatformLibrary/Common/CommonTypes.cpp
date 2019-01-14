//******************************************************************************************************
//  CommonTypes.cpp - Gbtc
//
//  Copyright � 2018, Grid Protection Alliance.  All Rights Reserved.
//
//  Licensed to the Grid Protection Alliance (GPA) under one or more contributor license agreements. See
//  the NOTICE file distributed with this work for additional information regarding copyright ownership.
//  The GPA licenses this file to you under the MIT License (MIT), the "License"; you may
//  not use this file except in compliance with the License. You may obtain a copy of the License at:
//
//      http://opensource.org/licenses/MIT
//
//  Unless agreed to in writing, the subject software distributed under the License is distributed on an
//  "AS-IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. Refer to the
//  License for the specific language governing permissions and limitations.
//
//  Code Modification History:
//  ----------------------------------------------------------------------------------------------------
//  03/23/2018 - J. Ritchie Carroll
//       Generated original version of source code.
//
//******************************************************************************************************

#include "CommonTypes.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/uuid/uuid_generators.hpp>

using namespace std;
using namespace boost;
using namespace boost::algorithm;
using namespace GSF::TimeSeries;

boost::uuids::random_generator RandomGuidGen;
boost::uuids::nil_generator NilGuidGen;

const decimal_t Decimal::MaxValue = numeric_limits<decimal_t>::max();

const decimal_t Decimal::MinValue = numeric_limits<decimal_t>::min();

const decimal_t Decimal::DotNetMaxValue = decimal_t("79228162514264337593543950335");

const decimal_t Decimal::DotNetMinValue = decimal_t("-79228162514264337593543950335");

const string Empty::String;

const Guid Empty::Guid = NilGuidGen();

const Object Empty::Object(nullptr);

const IPAddress Empty::IPAddress;

const uint8_t* Empty::ZeroLengthBytes = new uint8_t[4] { 0, 0, 0, 0 };

StringHasher::StringHasher() :
    m_ignoreCase(true)
{
}

StringHasher::StringHasher(bool ignoreCase) :
    m_ignoreCase(ignoreCase)
{
}

size_t StringHasher::operator()(const string& value) const
{
    size_t seed = 0;
    const locale locale;

    for (string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        if (m_ignoreCase)
            hash_combine(seed, toupper(*it, locale));
        else
            hash_combine(seed, *it);
    }

    return seed;
}

StringComparer::StringComparer() :
    m_ignoreCase(true)
{
}

StringComparer::StringComparer(bool ignoreCase) :
    m_ignoreCase(ignoreCase)
{
}

bool StringComparer::operator()(const string& left, const string& right) const
{
    return IsEqual(left, right, m_ignoreCase);
}

Guid GSF::TimeSeries::NewGuid()
{
    return RandomGuidGen();
}

bool GSF::TimeSeries::IsEqual(const string& left, const string& right, bool ignoreCase)
{
    if (ignoreCase)
        return iequals(left, right);

    return equals(left, right);
}

bool GSF::TimeSeries::StartsWith(const string& value, const string& findValue, bool ignoreCase)
{
    if (ignoreCase)
        return istarts_with(value, findValue);

    return starts_with(value, findValue);
}

bool GSF::TimeSeries::EndsWith(const string& value, const string& findValue, bool ignoreCase)
{
    if (ignoreCase)
        return iends_with(value, findValue);

    return ends_with(value, findValue);
}

bool GSF::TimeSeries::Contains(const string& value, const string& findValue, bool ignoreCase)
{
    if (ignoreCase)
        return icontains(value, findValue);

    return contains(value, findValue);
}

int32_t GSF::TimeSeries::Count(const string& value, const string& findValue, bool ignoreCase)
{
    find_iterator<string::const_iterator> it = ignoreCase ?
        make_find_iterator(value, first_finder(findValue, is_iequal())) :
        make_find_iterator(value, first_finder(findValue, is_equal()));

    const find_iterator<string::const_iterator> end{};
    int32_t count = 0;

    for(; it != end; ++it, ++count)
    {
    }

    return count;
}

int32_t GSF::TimeSeries::Compare(const string& leftValue, const string& rightValue, bool ignoreCase)
{
    if (ignoreCase)
    {
        if (ilexicographical_compare(leftValue, rightValue))
            return -1;

        if (ilexicographical_compare(rightValue, leftValue))
            return 1;

        return 0;
    }

    if (lexicographical_compare(leftValue, rightValue))
        return -1;

    if (lexicographical_compare(rightValue, leftValue))
        return 1;

    return 0;
}

int32_t GSF::TimeSeries::IndexOf(const string& value, const string& findValue, bool ignoreCase)
{
    iterator_range<string::const_iterator> it = ignoreCase ? ifind_first(value, findValue) : find_first(value, findValue);

    if (it.empty())
        return -1;

    return distance(value.begin(), it.begin());
}

int32_t GSF::TimeSeries::IndexOf(const string& value, const string& findValue, int32_t index, bool ignoreCase)
{
    iterator_range<string::const_iterator> it = ignoreCase ? ifind_nth(value, findValue, index) : find_nth(value, findValue, index);

    if (it.empty())
        return -1;

    return distance(value.begin(), it.begin());
}

int32_t GSF::TimeSeries::LastIndexOf(const string& value, const string& findValue, bool ignoreCase)
{
    iterator_range<string::const_iterator> it = ignoreCase ? ifind_last(value, findValue) : find_last(value, findValue);

    if (it.empty())
        return -1;

    return distance(value.begin(), it.begin());
}

vector<string> GSF::TimeSeries::Split(const string& value, const string& delimiterValue, bool ignoreCase)
{
    split_iterator<string::const_iterator> it = ignoreCase ?
        make_split_iterator(value, first_finder(delimiterValue, is_iequal())) :
        make_split_iterator(value, first_finder(delimiterValue, is_equal()));

    const split_iterator<string::const_iterator> end{};
    vector<string> values;

    for(; it != end; ++it)
    {
        values.push_back(copy_range<string>(*it));
    }

    return values;
}

string GSF::TimeSeries::Split(const string& value, const string& delimiterValue, int32_t index, bool ignoreCase)
{
    split_iterator<string::const_iterator> it = ignoreCase ?
        make_split_iterator(value, first_finder(delimiterValue, is_iequal())) :
        make_split_iterator(value, first_finder(delimiterValue, is_equal()));

    const split_iterator<string::const_iterator> end{};
    int32_t count = 0;

    for(; it != end; ++it, ++count)
    {
        if (count == index)
            return copy_range<string>(*it);
    }

    return string{};
}

string GSF::TimeSeries::Replace(const string& value, const string& findValue, const string& replaceValue, bool ignoreCase)
{
    if (ignoreCase)
        return ireplace_all_copy(value, findValue, replaceValue);

    return replace_all_copy(value, findValue, replaceValue);
}

string GSF::TimeSeries::ToUpper(const string& value)
{
    return to_upper_copy(value);
}

string GSF::TimeSeries::ToLower(const string& value)
{
    return to_lower_copy(value);
}

string GSF::TimeSeries::Trim(const string& value)
{
    return trim_copy(value);
}

string GSF::TimeSeries::TrimRight(const string& value)
{
    return trim_right_copy(value);
}

string GSF::TimeSeries::TrimLeft(const string& value)
{
    return trim_left_copy(value);
}