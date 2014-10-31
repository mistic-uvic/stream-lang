#include "llvm_from_model.hpp"

#include <stdexcept>
#include <algorithm>
#include <iostream>

using namespace std;

namespace stream {
namespace polyhedral {

llvm_from_model::llvm_from_model
(llvm::Module *module,
 int input_count,
 const vector<statement*> & statements,
 const vector<dataflow_dependency> & dependencies):
    m_module(module),
    m_builder(module->getContext()),
    m_statements(statements),
    m_dependencies(dependencies)
{
    type_type i8_ptr_type = llvm::Type::getInt8PtrTy(llvm_context());
    type_type i8_ptr_ptr_type = llvm::PointerType::get(i8_ptr_type, 0);
    type_type buffer_type =
            llvm::StructType::create(vector<type_type>({i8_ptr_type, int32_type()}));
    type_type buffer_pointer_type = llvm::PointerType::get(buffer_type, 0);

    // inputs, output, buffers
    vector<type_type> arg_types = { i8_ptr_ptr_type, i8_ptr_type, buffer_pointer_type };

    llvm::FunctionType * func_type =
            llvm::FunctionType::get(llvm::Type::getVoidTy(llvm_context()),
                                    arg_types,
                                    false);

    m_function = llvm::Function::Create(func_type,
                                        llvm::Function::ExternalLinkage,
                                        "process",
                                        m_module);

    auto arg = m_function->arg_begin();
    m_inputs = arg++;
    m_output = arg++;
    m_buffers = arg++;

    //llvm::BasicBlock *bb = llvm::BasicBlock::Create(llvm_context(), "entry", func);
    //m_builder.SetInsertPoint(bb);
}

void llvm_from_model::generate_statement( const string & name,
                                          const vector<value_type> & index,
                                          block_type block )
{
    auto stmt_ref = std::find_if(m_statements.begin(), m_statements.end(),
                                 [&](statement *s){ return s->name == name; });
    assert(stmt_ref != m_statements.end());
    generate_statement(*stmt_ref, index, block);
}

void llvm_from_model::generate_statement
( statement *stmt, const vector<value_type> & raw_index, block_type block )
{
    if (!stmt->expr)
    {
        cout << "Not generating statement without expression: " << stmt->name << endl;
        return;
    }

    // Drop first dimension denoting period (always zero).
    assert(!raw_index.empty());
    vector<value_type> index(raw_index.begin()+1, raw_index.end());

    m_builder.SetInsertPoint(block);

    value_type value;

    if (auto input = dynamic_cast<input_access*>(stmt->expr))
    {
        value_type address = generate_input_access(stmt, index);
        value = m_builder.CreateLoad(address);
    }
    else
    {
        value = generate_expression(stmt->expr, index);
    }

    value_type dst = generate_buffer_access(stmt, index);

    m_builder.CreateStore(value, dst);
}

llvm_from_model::value_type
llvm_from_model::generate_expression
( expression *expr, const vector<value_type> & index )
{
    if (auto operation = dynamic_cast<intrinsic*>(expr))
    {
        return generate_intrinsic(operation, index);
    }
    if (auto read = dynamic_cast<stream_access*>(expr))
    {
        vector<value_type> target_index = mapped_index(index, read->pattern);
        value_type address = generate_buffer_access(read->target, target_index);
        return m_builder.CreateLoad(address);
    }
    if ( auto const_int = dynamic_cast<constant<int>*>(expr) )
    {
        // FIXME: integers...
        return value((double) const_int->value);
    }
    if ( auto const_double = dynamic_cast<constant<double>*>(expr) )
    {
        return value(const_double->value);
    }
    throw std::runtime_error("Unexpected expression type.");
}

llvm_from_model::value_type
llvm_from_model::generate_intrinsic
( intrinsic *op, const vector<value_type> & index )
{
    vector<value_type> operands;
    operands.reserve(op->operands.size());
    for (expression * expr : op->operands)
    {
        operands.push_back( generate_expression(expr, index) );
    }

    switch(op->kind)
    {
    case intrinsic::add:
        return m_builder.CreateFAdd(operands[0], operands[1]);
    case intrinsic::subtract:
        return m_builder.CreateFSub(operands[0], operands[1]);
    case intrinsic::multiply:
        return m_builder.CreateFMul(operands[0], operands[1]);
    case intrinsic::divide:
        return m_builder.CreateFDiv(operands[0], operands[1]);
    default:
        throw std::runtime_error("Unexpected expression type.");
    }
}

llvm_from_model::value_type
llvm_from_model::generate_buffer_access
( statement *stmt, const vector<value_type> & index )
{
    // Get basic info about accessed statement

    value_type stmt_index = value( (int32_t) statement_index(stmt) );

    int finite_slice_size = 1;
    for (int dim = 0; dim < stmt->domain.size(); ++dim)
    {
        int size = stmt->domain[dim];
        if (size >= 0)
            finite_slice_size *= size;
    }

    assert(stmt->buffer_size >= 0);
    int buffer_size = finite_slice_size * stmt->buffer_size;

    // Compute flat access index

    vector<value_type> the_index(index);
    vector<int> the_domain = stmt->domain;
    transpose(the_index, stmt->dimension);
    transpose(the_domain, stmt->dimension);
    value_type flat_index = this->flat_index(the_index, the_domain);

    // Get buffer state

    value_type buffer, phase;
    {
        vector<value_type> indices{stmt_index, value((int32_t) 0)};
        value_type buffer_ptr =
                m_builder.CreateGEP(m_buffers, indices);
        buffer = m_builder.CreateLoad(buffer_ptr);
        type_type real_buffer_type = llvm::Type::getDoublePtrTy(llvm_context());
        buffer = m_builder.CreateBitCast(buffer, real_buffer_type);
    }
    {
        vector<value_type> indices{stmt_index, value((int32_t) 1)};
        value_type phase_ptr =
                m_builder.CreateGEP(m_buffers, indices);
        phase = m_builder.CreateLoad(phase_ptr);
        if (phase->getType() == int32_type())
            phase = m_builder.CreateSExt(phase, int64_type());
    }

    // Add access index to current buffer state (phase)

    phase = m_builder.CreateMul(phase, value((int64_t)finite_slice_size));

    value_type buffer_index =
            m_builder.CreateAdd(flat_index, phase);

    buffer_index =
            m_builder.CreateSRem(buffer_index, value((int64_t)buffer_size));

    // Get value from buffer

    value_type value_ptr =
            m_builder.CreateGEP(buffer, buffer_index);

    return value_ptr;
}

llvm_from_model::value_type
llvm_from_model::generate_input_access
( statement *stmt, const vector<value_type> & index )
{
    int input_num = reinterpret_cast<input_access*>(stmt->expr)->index;

    // Compute flat access index

    vector<value_type> the_index(index);
    vector<int> the_domain = stmt->domain;
    transpose(the_index, stmt->dimension);
    transpose(the_domain, stmt->dimension);
    value_type flat_index = this->flat_index(the_index, the_domain);

    // Access

    value_type input_ptr = m_builder.CreateGEP(m_inputs, value(input_num));
    value_type input = m_builder.CreateLoad(input_ptr);

    type_type double_ptr_type = llvm::Type::getDoublePtrTy(llvm_context());
    input = m_builder.CreateBitCast(input, double_ptr_type);

    value_type value_ptr = m_builder.CreateGEP(input, flat_index);

    return value_ptr;
}

template <typename T>
void llvm_from_model::transpose( vector<T> & index, int first_dim )
{
    assert(first_dim < index.size());
    T tmp = index[first_dim];
    for (int i = first_dim; i > 0; --i)
        index[i] = index[i-1];
    index[0] = tmp;
}

vector<llvm_from_model::value_type>
llvm_from_model::mapped_index
( const vector<value_type> & index, const mapping & map )
{
    assert(index.size() == map.input_dimension());

    vector<value_type> target_index;
    target_index.reserve(map.output_dimension());

    for(int out_dim = 0; out_dim < map.output_dimension(); ++out_dim)
    {
        value_type out_value = value((int64_t) map.constant(out_dim));
        for (int in_dim = 0; in_dim < map.input_dimension(); ++in_dim)
        {
            value_type coefficient =
                    value((int64_t) map.coefficient(in_dim, out_dim));
            value_type term = m_builder.CreateMul(index[in_dim], coefficient);
            out_value = m_builder.CreateAdd(out_value, term);
        }
        target_index.push_back(out_value);
    }

    return target_index;
}

llvm_from_model::value_type
llvm_from_model::flat_index
( const vector<value_type> & index, const vector<int> & domain )
{
    assert(index.size() > 0);
    assert(domain.size() == index.size());

    value_type flat_index = index[0];
    for( unsigned int i = 1; i < index.size(); ++i)
    {
        int size = domain[i];
        assert(size >= 0);
        flat_index = m_builder.CreateMul(flat_index, value((int64_t) size));
        flat_index = m_builder.CreateAdd(flat_index, index[i]);
    }

    return flat_index;
}

int llvm_from_model::statement_index( statement * stmt )
{
    auto stmt_ref = std::find(m_statements.begin(), m_statements.end(), stmt);
    assert(stmt_ref != m_statements.end());
    return (int) std::distance(stmt_ref, m_statements.begin());
}

#if 0
vector<llvm_from_model::value_type>
llvm_from_model::map_index
( statement *stmt, const vector<value_type> & index )
{

}
#endif

}
}