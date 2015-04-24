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

#include "cpp_from_cloog.hpp"

#include <iostream>
#include <sstream>

using namespace std;

namespace stream {
namespace cpp_gen {

cpp_from_cloog::cpp_from_cloog(builder *ctx):
    m_ctx(ctx)
{
}

void cpp_from_cloog::generate( clast_stmt *ast )
{
    m_ctx->add( process(ast) );
}

void cpp_from_cloog::process_list(clast_stmt *stmt, vector<statement_ptr> & list)
{
    m_ctx->push(&list);
    while(stmt)
    {
        m_ctx->add(process(stmt));
        stmt = stmt->next;
    }
    m_ctx->pop();
}

statement_ptr cpp_from_cloog::process( clast_stmt *stmt )
{
    if (CLAST_STMT_IS_A(stmt, stmt_root))
    {
        return process( reinterpret_cast<clast_root*>(stmt) );
    }
    else if (CLAST_STMT_IS_A(stmt, stmt_block))
    {
        return process( reinterpret_cast<clast_block*>(stmt) );
    }
    else if (CLAST_STMT_IS_A(stmt, stmt_ass))
    {
        return process( reinterpret_cast<clast_assignment*>(stmt) );
    }
    else if (CLAST_STMT_IS_A(stmt, stmt_guard))
    {
        return process( reinterpret_cast<clast_guard*>(stmt) );
    }
    else if (CLAST_STMT_IS_A(stmt, stmt_for))
    {
        return process( reinterpret_cast<clast_for*>(stmt) );
    }
    else if (CLAST_STMT_IS_A(stmt, stmt_user))
    {
        return process( reinterpret_cast<clast_user_stmt*>(stmt) );
    }
    else
        throw std::runtime_error("Unexpected statement type.");
}

statement_ptr cpp_from_cloog::process( clast_root *root )
{
    auto stmt = new block_statement;
    process_list(root->stmt.next, stmt->statements);
    return statement_ptr(stmt);
}

statement_ptr cpp_from_cloog::process( clast_block *block )
{
    auto stmt = new block_statement;
    process_list(block->body, stmt->statements);
    return statement_ptr(stmt);
}


statement_ptr cpp_from_cloog::process( clast_assignment* asgn )
{
    expression_ptr rhs = process(asgn->RHS);
    expression_ptr lhs(new id_expression(asgn->LHS));
    expression_ptr expr(new bin_op_expression(op::assign, lhs, rhs));
    return statement_ptr(new expr_statement(expr));
}

statement_ptr cpp_from_cloog::process( clast_guard* guard )
{
    auto if_stmt = new if_statement;

    if (guard->n > 0)
    {
        expression_ptr expr = process(guard->eq + 0);
        for (int i = 1; i < guard->n; ++i)
        {
            auto rhs = process(guard->eq + i);
            expr = make_shared<bin_op_expression>(op::logic_and, expr, rhs);
        }

        if_stmt->condition = expr;
    }
    else
    {
        if_stmt->condition = literal(false);
    }

    auto body = make_shared<block_statement>();
    process_list(guard->then, body->statements);

    if_stmt->true_part = body;

    return statement_ptr(if_stmt);
}

statement_ptr cpp_from_cloog::process( clast_for* loop )
{
    auto iterator = make_shared<id_expression>(loop->iterator);
    auto iterator_decl =
            make_shared<var_decl_expression>(make_shared<basic_type>("int"),
                                             loop->iterator);

    auto for_stmt = new for_statement;

    for_stmt->initialization =
            make_shared<bin_op_expression>(op::assign,
                                           iterator_decl,
                                           process(loop->LB));
    for_stmt->condition =
            make_shared<bin_op_expression>(op::lesser_or_equal,
                                           iterator,
                                           process(loop->UB));

    for_stmt->update =
            make_shared<bin_op_expression>(op::assign_add,
                                           iterator,
                                           literal(loop->stride));

    auto body = make_shared<block_statement>();
    process_list(loop->body, body->statements);

    for_stmt->body = body;

    return statement_ptr(for_stmt);
}

expression_ptr cpp_from_cloog::process( clast_expr* expr )
{
    switch(expr->type)
    {
    case clast_expr_name:
    {
        const char *name = reinterpret_cast<clast_name*>(expr)->name;
        return make_shared<id_expression>(name);
    }
    case clast_expr_term:
    {
        auto term = reinterpret_cast<clast_term*>(expr);
        expression_ptr val = literal(term->val);
        if (term->var)
        {
            auto var = process(term->var);
            val = make_shared<bin_op_expression>(op::mult, val, var);
        }
        return val;
    }
    case clast_expr_bin:
    {
        auto operation = reinterpret_cast<clast_binary*>(expr);
        auto lhs = process(operation->LHS);
        auto rhs = literal(operation->RHS);

        cpp_gen::op op_type;

        switch(operation->type)
        {
        case clast_bin_fdiv:
            throw std::runtime_error("Floor-of-division not implemented.");
        case clast_bin_cdiv:
            throw std::runtime_error("Ceiling-of-division not implemented.");
        case clast_bin_div:
            op_type = op::div;
        case clast_bin_mod:
            // FIXME: should be modulo, not remainder!
            op_type = op::rem;
        default:
            throw std::runtime_error("Unexpected binary operation type.");
        }

        return make_shared<bin_op_expression>(op_type, lhs, rhs);
    }
    case clast_expr_red:
    {
        auto reduction = reinterpret_cast<clast_reduction*>(expr);
        if (reduction->n < 1)
            return literal((long)0);

        auto lhs = process( reduction->elts[0] );

        for (int i = 1; i < reduction->n; ++i)
        {
            auto rhs = process( reduction->elts[i] );
            switch(reduction->type)
            {
            case clast_red_sum:
                lhs = make_shared<bin_op_expression>(op::add, lhs, rhs);
                break;
            case clast_red_min:
                lhs = make_shared<call_expression>("min", vec({lhs, rhs}));
                break;
            case clast_red_max:
                lhs = make_shared<call_expression>("max", vec({lhs, rhs}));
                break;
            default:
                throw std::runtime_error("Unexpected reduction type.");
            }
        }
        return lhs;
    }
    default:
        throw std::runtime_error("Unexpected expression type.");
    }
}

expression_ptr cpp_from_cloog::process( clast_equation* eq )
{
    auto binop = new bin_op_expression;

    binop->lhs = process(eq->LHS);
    binop->rhs = process(eq->RHS);

    if (eq->sign < 0)
        binop->op = op::lesser_or_equal;
    else if (eq->sign > 0)
        binop->op = op::greater_or_equal;
    else
        binop->op = op::equal;

    return expression_ptr(binop);
}

statement_ptr cpp_from_cloog::process( clast_user_stmt* stmt )
{
    vector<expression_ptr> index;

    clast_stmt *index_stmt = stmt->substitutions;
    while(index_stmt)
    {
        assert(CLAST_STMT_IS_A(index_stmt, stmt_ass));

        clast_expr *index_expr =
                reinterpret_cast<clast_assignment*>(index_stmt)->RHS;

        index.push_back( process(index_expr) );

        index_stmt = index_stmt->next;
    }

    if (m_stmt_func)
    {
        auto block = make_shared<block_statement>();
        m_ctx->push(&block->statements);
        m_stmt_func(stmt->statement->name, index, m_ctx);
        m_ctx->pop();
        return block;
    }
    else
    {
        state s;
        ostringstream text;
        text << stmt->statement->name;
        text << "(";
        for(auto expr : index)
        {
            expr->generate(s, text);
            text << ",";
        }
        text << ")";

        return make_shared<comment_statement>(text.str());
    }
}

}
}