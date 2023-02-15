/*++
Copyright (c) 2020 Microsoft Corporation

Module Name:

    euf_local_search.cpp

Abstract:

    Local search dispatch for SMT

Author:

    Nikolaj Bjorner (nbjorner) 2023-02-07

--*/
#include "sat/sat_solver.h"
#include "sat/sat_ddfw.h"
#include "sat/smt/euf_solver.h"


namespace euf {
    
    lbool solver::local_search(bool_vector& phase) {
        scoped_limits scoped_rl(m.limit());
        sat::ddfw bool_search;
        bool_search.reinit(s(), phase);
        bool_search.updt_params(s().params());
        bool_search.set_seed(rand());
        scoped_rl.push_child(&(bool_search.rlimit()));

        unsigned max_rounds = 30;

        for (auto* th : m_solvers)
            th->set_bool_search(&bool_search);

        for (unsigned rounds = 0; m.inc() && rounds < max_rounds; ++rounds) {

            setup_bounds(bool_search, phase);

            // Non-boolean literals are assumptions to Boolean search
            literal_vector assumptions;
#if 0
            for (unsigned v = 0; v < phase.size(); ++v)
                if (!is_propositional(literal(v)))
                    assumptions.push_back(literal(v, !bool_search.get_value(v)));
#endif

            verbose_stream() << "assumptions " << assumptions.size() << "\n";

            bool_search.rlimit().push(m_max_bool_steps);
            
            lbool r = bool_search.check(assumptions.size(), assumptions.data(), nullptr);
            bool_search.rlimit().pop();

#if 0
            // restore state to optimal model
            auto const& mdl = bool_search.get_model();
            for (unsigned i = 0; i < mdl.size(); ++i)
                if ((mdl[i] == l_true) != bool_search.get_value(i))
                    bool_search.flip(i);
#endif

            for (auto* th : m_solvers) 
                th->local_search(phase);

            if (bool_search.unsat_set().empty())
                break;
        }
        auto const& mdl = bool_search.get_model();
        for (unsigned i = 0; i < mdl.size(); ++i)
            phase[i] = mdl[i] == l_true;     

        return bool_search.unsat_set().empty() ? l_true : l_undef;
    }

    bool solver::is_propositional(sat::literal lit) {
        expr* e = m_bool_var2expr.get(lit.var(), nullptr);
        return !e || is_uninterp_const(e) || !m_egraph.find(e);
    }

    void solver::setup_bounds(sat::ddfw& bool_search, bool_vector const& phase) {
        unsigned num_literals = 0;
        unsigned num_bool = 0;
        for (auto* th : m_solvers)
            th->set_bounds_begin();

        auto count_literal = [&](sat::literal l) {
            if (is_propositional(l)) {
                ++num_bool;
                return;
            }
            euf::enode* n = m_egraph.find(m_bool_var2expr.get(l.var(), nullptr));
            for (auto* s : m_solvers)
                s->set_bounds(n);
        };

        for (auto cl : bool_search.unsat_set()) {
            auto& c = *bool_search.get_clause_info(cl).m_clause;
            num_literals += c.size();
            for (auto l : c)
                count_literal(l);
        }

        m_max_bool_steps = (m_ls_config.L * num_bool); // / num_literals;
        m_max_bool_steps = std::max(10000u, m_max_bool_steps);
        verbose_stream() << "num literals " << num_literals << " num bool " << num_bool << " max bool steps " << m_max_bool_steps << "\n";

        for (auto* th : m_solvers)
            th->set_bounds_end(num_literals);
    }
}
