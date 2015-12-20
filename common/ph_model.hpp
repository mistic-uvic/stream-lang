/*
Compiler for language for stream processing

Copyright (C) 2014  Jakob Leben <jakob.leben@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef STREAM_POLYHEDRAL_MODEL_2_INCLUDED
#define STREAM_POLYHEDRAL_MODEL_2_INCLUDED

#include "primitives.hpp"
#include "functional_model.hpp"
#include "../frontend/location.hh"
#include "../utility/mapping.hpp"
#include "../utility/debug.hpp"

#include <isl-cpp/context.hpp>
#include <isl-cpp/set.hpp>
#include <isl-cpp/map.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <memory>

namespace stream {
namespace polyhedral {

struct debug : public stream::debug::topic<debug, stream::debug::all>
{ static string id() { return "polyhedral"; } };

using std::vector;
using std::string;
using utility::mapping;
typedef parsing::location location_type;

class statement;
class array;

enum {
    infinite = -1
};

class array
{
public:
    array(const string & name,
          const isl::set & domain):
        name(name), domain(domain) {}

    string name;
    isl::set domain;
    bool is_infinite = false;

#if 1
    vector<int> buffer_size;
    //int flow_dim = -1;
    int period = 0;
    int period_offset = 0;
    bool inter_period_dependency = true;
#endif
};
typedef std::shared_ptr<array> array_ptr;

class statement
{
public:
    statement(const isl::set & d):
        name(d.name()), domain(d),
        write_relation(d.get_space()),
        read_relations(d.ctx())
    {}

    string name;
    isl::set domain;
    functional::expr_ptr expr = nullptr;
    array_ptr array;
    isl::basic_map write_relation;
    isl::union_map read_relations;
    bool is_infinite = false;
};
typedef std::shared_ptr<statement> stmt_ptr;

class array_read : public functional::expression
{
public:
    array_read(array_ptr a, const isl::basic_map & r,
               const location_type & l):
        expression(l), array(a), read_relation(r) {}

    array_ptr array;
    isl::basic_map read_relation;
};

class iterator_read : public functional::expression
{
public:
    iterator_read(int i, const location_type & l):
        expression(l), index(i) {}
    int index;
};

class model
{
public:
    isl::context context;
    vector<array_ptr> arrays;
    vector<stmt_ptr> statements;
};

} // namespace polyhedral
} // namespace stream

#endif // STREAM_POLYHEDRAL_MODEL_2_INCLUDED