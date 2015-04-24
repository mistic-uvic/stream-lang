#include "cpp_from_polyhedral.hpp"
#include "../common/error.hpp"

using namespace std;

namespace stream {
namespace cpp_gen {

using index_type = cpp_from_polyhedral::index_type;

cpp_from_polyhedral::cpp_from_polyhedral
(const vector<polyhedral::statement*> &stmts):
    m_statements(stmts)
{}

void cpp_from_polyhedral::generate_statement
(const string & name, const index_type & index, builder* ctx)
{
    auto stmt_ref = std::find_if(m_statements.begin(), m_statements.end(),
                                 [&](polyhedral::statement *s){ return s->name == name; });
    assert(stmt_ref != m_statements.end());

    generate_statement(*stmt_ref, index, ctx);
}

void cpp_from_polyhedral::generate_statement
(polyhedral::statement *stmt, const index_type & ctx_index, builder* ctx)
{
    // Drop first dimension denoting period (always zero).
    assert(!ctx_index.empty());

    index_type index(ctx_index.begin()+1, ctx_index.end());
    index_type global_index = index;
    // TODO: offset global index for periodic function

    expression_ptr expr;

    if (dynamic_cast<polyhedral::input_access*>(stmt->expr))
    {
        expr = generate_input_access(stmt, index, ctx);
    }
    else
    {
        expr = generate_expression(stmt->expr, index, ctx);
    }

    auto dst = generate_buffer_access(stmt, global_index, ctx);
    auto store = make_shared<bin_op_expression>(op::assign, dst, expr);

    ctx->add(make_shared<expr_statement>(store));
}

expression_ptr cpp_from_polyhedral::generate_expression
(polyhedral::expression * expr, const index_type & index, builder * ctx)
{
    using namespace polyhedral;

    expression_ptr result;

    if (auto operation = dynamic_cast<primitive_expr*>(expr))
    {
        result = generate_primitive(operation, index, ctx);
    }
    else if (auto input = dynamic_cast<input_access*>(expr))
    {
        int input_num = input->index;
        auto input_name = ctx->current_function()->parameters[input_num]->name;
        result = make_shared<id_expression>(input_name);
    }
    else if (auto iterator = dynamic_cast<iterator_access*>(expr))
    {
        assert(iterator->dimension >= 0 && iterator->dimension < index.size());
        auto val = index[iterator->dimension];
        if (iterator->ratio != 1)
            val = make_shared<bin_op_expression>(op::mult, val, literal(iterator->ratio));
        if (iterator->offset)
            val = make_shared<bin_op_expression>(op::add, val, literal(iterator->offset));
        return val;
    }
    else if (auto read = dynamic_cast<stmt_access*>(expr))
    {
        auto target_index = mapped_index(index, read->pattern, ctx);
        result = generate_buffer_access(read->target, target_index, ctx);
    }
    else if (auto access = dynamic_cast<reduction_access*>(expr))
    {
        (void) access;
        throw error("Reduction not implemented");
        //result = generate_reduction_access(access, index, ctx);
    }
    else if ( auto const_int = dynamic_cast<constant<int>*>(expr) )
    {
        result = literal(const_int->value);
    }
    else if ( auto const_double = dynamic_cast<constant<double>*>(expr) )
    {
        result = literal(const_double->value);
    }
    else if ( auto const_bool = dynamic_cast<constant<bool>*>(expr) )
    {
        result = literal(const_bool->value);
    }
    else
    {
        throw std::runtime_error("Unexpected expression type.");
    }

    // FIXME: Do we need to convert anything?
    // result = convert(result, expr->type);

    return result;
}

expression_ptr cpp_from_polyhedral::generate_primitive
(polyhedral::primitive_expr * expr, const index_type & index, builder * ctx)
{
    switch(expr->op)
    {
    case primitive_op::logic_and:
    {
        auto lhs = generate_expression(expr->operands[0], index, ctx);
        auto rhs = generate_expression(expr->operands[1], index, ctx);
        return make_shared<bin_op_expression>(op::logic_and, lhs, rhs);
    }
    case primitive_op::logic_or:
    {
        auto lhs = generate_expression(expr->operands[0], index, ctx);
        auto rhs = generate_expression(expr->operands[1], index, ctx);
        return make_shared<bin_op_expression>(op::logic_or, lhs, rhs);
    }
    case primitive_op::conditional:
    {
        string id;
        auto type = type_for(expr->type);
        ctx->add(make_shared<expr_statement>(ctx->new_var(type, id)));
        auto id_expr = make_shared<id_expression>(id);

        auto condition_expr = generate_expression(expr->operands[0], index, ctx);

        auto true_block = new block_statement;
        auto false_block = new block_statement;

        ctx->push(&true_block->statements);
        auto true_expr = generate_expression(expr->operands[1], index, ctx);
        auto true_assign = make_shared<bin_op_expression>(op::assign, id_expr, true_expr);
        ctx->add(make_shared<expr_statement>(true_assign));

        ctx->pop();

        ctx->push(&false_block->statements);
        auto false_expr = generate_expression(expr->operands[2], index, ctx);
        auto false_assign = make_shared<bin_op_expression>(op::assign, id_expr, true_expr);
        ctx->add(make_shared<expr_statement>(false_assign));

        ctx->pop();

        auto if_stmt = make_shared<if_statement>
                (condition_expr, statement_ptr(true_block), statement_ptr(false_block));

        ctx->add(if_stmt);

        return id_expr;
    }
    default:
        break;
    }

    vector<expression_ptr> operands;
    operands.reserve(expr->operands.size());
    for (polyhedral::expression * operand_expr : expr->operands)
    {
        operands.push_back( generate_expression(operand_expr, index, ctx) );
    }

    switch(expr->op)
    {
    case primitive_op::negate:
    {
        expression_ptr operand = operands[0];
        if (expr->type == primitive_type::boolean)
            return make_shared<un_op_expression>(op::logic_neg, operand);
        else
            return make_shared<un_op_expression>(op::u_minus, operand);
    }
    case primitive_op::add:
        return make_shared<bin_op_expression>(op::add, operands[0], operands[1]);
    case primitive_op::subtract:
        return make_shared<bin_op_expression>(op::sub, operands[0], operands[1]);
    case primitive_op::multiply:
        return make_shared<bin_op_expression>(op::mult, operands[0], operands[1]);
    case primitive_op::compare_g:
        return make_shared<bin_op_expression>(op::greater, operands[0], operands[1]);
    case primitive_op::compare_geq:
        return make_shared<bin_op_expression>(op::greater_or_equal, operands[0], operands[1]);
    case primitive_op::compare_l:
        return make_shared<bin_op_expression>(op::lesser, operands[0], operands[1]);
    case primitive_op::compare_leq:
        return make_shared<bin_op_expression>(op::lesser_or_equal, operands[0], operands[1]);
    case primitive_op::compare_eq:
        return make_shared<bin_op_expression>(op::equal, operands[0], operands[1]);
    case primitive_op::compare_neq:
        return make_shared<bin_op_expression>(op::not_equal, operands[0], operands[1]);
    case primitive_op::divide:
    {
        if (expr->operands[0]->type != primitive_type::real)
            operands[0] = make_shared<cast_expression>(type_for(primitive_type::real), operands[0]);

        return make_shared<bin_op_expression>(op::div, operands[0], operands[1]);
    }
    case primitive_op::divide_integer:
    {
        auto result = make_shared<bin_op_expression>(op::div, operands[0], operands[1]);
        if ( expr->operands[0]->type == primitive_type::integer &&
             expr->operands[1]->type == primitive_type::integer )
        {
            return result;
        }
        else
        {
            return make_shared<cast_expression>(type_for(primitive_type::integer), result);
        }
    }
    case primitive_op::modulo:
    case primitive_op::raise:
    case primitive_op::floor:
    case primitive_op::ceil:
    case primitive_op::abs:
    case primitive_op::max:
    case primitive_op::min:
    case primitive_op::log:
    case primitive_op::log2:
    case primitive_op::log10:
    case primitive_op::exp:
    case primitive_op::exp2:
    case primitive_op::sqrt:
    case primitive_op::sin:
    case primitive_op::cos:
        throw error("C++ target: Primitive expression not implemented.");
    default:
        ostringstream text;
        text << "Unexpected primitive op: " << expr->op;
        throw error(text.str());
    }
}

expression_ptr cpp_from_polyhedral::generate_input_access
(polyhedral::statement * stmt, const index_type & index, builder * ctx)
{
    // FIXME: index
    int input_num = reinterpret_cast<polyhedral::input_access*>(stmt->expr)->index;
    auto input_name = ctx->current_function()->parameters[input_num]->name;
    auto input_id = make_shared<id_expression>(input_name);
    return make_shared<array_access_expression>(input_id, index);
}

expression_ptr cpp_from_polyhedral::generate_buffer_access
(polyhedral::statement * stmt, const index_type & index, builder * ctx)
{
    // TODO: index contraction
    auto id = make_shared<id_expression>(stmt->name);
    auto state_arg_name = ctx->current_function()->parameters.back()->name;
    auto state_arg = make_shared<id_expression>(state_arg_name);
    auto buffer_member = make_shared<id_expression>(stmt->name);
    auto buffer = make_shared<bin_op_expression>(op::member_of_pointer, state_arg, buffer_member);
    auto buffer_elem = make_shared<array_access_expression>(buffer, index);
    return buffer_elem;
}

cpp_from_polyhedral::index_type
cpp_from_polyhedral::mapped_index
( const index_type & index,
  const polyhedral::mapping & map,
  builder * )
{
    assert(index.size() == map.input_dimension());

    index_type target_index;
    target_index.reserve(map.output_dimension());

    for(int out_dim = 0; out_dim < map.output_dimension(); ++out_dim)
    {
        auto out_value = literal(map.constant(out_dim));
        for (int in_dim = 0; in_dim < map.input_dimension(); ++in_dim)
        {
            int coefficient = map.coefficient(in_dim, out_dim);
            if (coefficient == 0)
                continue;
            auto term = index[in_dim];
            if (coefficient != 1)
                term = make_shared<bin_op_expression>(op::mult, term, literal(coefficient));
            out_value =
                    make_shared<bin_op_expression>(op::add, out_value, term);
        }
        target_index.push_back(out_value);
    }

    return target_index;
}


}
}