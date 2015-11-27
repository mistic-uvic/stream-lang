#ifndef STREAM_LANG_ARRAY_REDUCTION_INCLUDED
#define STREAM_LANG_ARRAY_REDUCTION_INCLUDED

#include "../common/functional_model.hpp"
#include "../utility/context.hpp"

namespace stream {
namespace functional {

class array_reducer
{
public:
    expr_ptr reduce(expr_ptr);
private:
    expr_ptr reduce(std::shared_ptr<array>);
    expr_ptr reduce(std::shared_ptr<array_app>);
    expr_ptr reduce(std::shared_ptr<primitive>);
    vector<int> array_size(expr_ptr);
    vector<int> array_size(std::shared_ptr<array>);
    expr_ptr beta_reduce(expr_ptr);
    expr_ptr eta_expand(std::shared_ptr<reference>);
    using context_type = context<var_ptr, expr_ptr>;
    context_type m_context;
};

}
}
#endif // STREAM_LANG_ARRAY_REDUCTION_INCLUDED
