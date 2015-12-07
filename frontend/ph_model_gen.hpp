#ifndef STREAM_LANG_POLYHEDRAL_MODEL_GENERATION
#define STREAM_LANG_POLYHEDRAL_MODEL_GENERATION

#include "../common/functional_model.hpp"
#include "../common/ph_model.hpp"

#include <isl-cpp/set.hpp>
#include <isl-cpp/map.hpp>
#include <isl-cpp/context.hpp>
#include <isl-cpp/printer.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace stream {
namespace functional {

using std::string;
using std::unordered_map;
using std::unordered_set;

class polyhedral_gen
{
public:
    polyhedral_gen();
    void process(const unordered_set<id_ptr> &, polyhedral::model &);

private:
    struct space_map
    {
        space_map(isl::space & s, isl::local_space & ls,
                  const vector<array_var_ptr> & v):
            space(s), local_space(ls), vars(v) {}
        isl::space & space;
        isl::local_space & local_space;
        const vector<array_var_ptr> & vars;

        int index_of(array_var_ptr var) const
        {
            int i;
            for(i=0; i<vars.size(); ++i)
            {
                if (vars[i] == var)
                    return i;
            }
            return -1;
        }
    };

    void process(id_ptr id, polyhedral::model &);

    polyhedral::array_ptr make_array(id_ptr id);

    polyhedral::stmt_ptr make_stmt(polyhedral::array_ptr array,
                                   const vector<array_var_ptr> &,
                                   const string & name,
                                   expr_ptr subdomain_expr);

    isl::set to_affine_set(expr_ptr, const space_map &);
    isl::expression to_affine_expr(expr_ptr, const space_map &);

    isl::context m_isl_ctx;
    isl::printer m_isl_printer;
    unordered_map<id_ptr, polyhedral::array_ptr> m_arrays;
};

}
}


#endif // STREAM_LANG_POLYHEDRAL_MODEL_GENERATION