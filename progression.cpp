#include "spotparser.h"
#include "progression.h"

#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/relabel.hh>
#include <spot/tl/simplify.hh>

#include <string>
#include <cstdio>
#include <iostream> 
#include <sstream>

#include <fstream>

std::vector<std::string> get_props(formula &f){
    std::vector<std::string> names;
    if (f.is_leaf()){
        std::string name = f.ap_name();
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }else{
        for (formula child : f)
        {
            std::vector<std::string> child_names =  get_props(child);
            for(auto name : child_names){
                if (std::find(names.begin(), names.end(), name) == names.end()) {
                    names.push_back(name);
                }
            }
        }
    }
    return names;
}

//// get progression of a formula given an interpretation
formula progr(formula &f, map<formula, formula>* m)
{
    formula res;
    if (f.kind() == op::tt || f.kind() == op::ff)
    {
        res = f; //make copy?
    }
    else
    {
        formula l = f.size() > 0 ? f[0] : formula();
        formula r, lft, rgt, res2;
        formula lt, rt;
        std::vector<formula> lst;

        switch (f.kind())
        {
            case op::ap:
                res = (*m)[f];
                break;
            case op::Not:
                r = progr(l, m);
                res = formula::unop(op::Not, r);
                break;
            case op::X:
                res = l;  // copy?
                break;
            case op::G:
                lft = progr(l, m);
                res = formula::multop(op::And, {lft, f});  //copy f?
                break;
            case op::F:
                lft = progr(l, m);
                res = formula::multop(op::Or, {lft, f});  //copy f?
                break;
            case op::U:
                if (f.size() < 2) throw runtime_error("Error: U operator requires two children");
                r = f[1];
                lft = progr(l, m);
                rgt = progr(r, m);
                res2 = formula::multop(op::And, {lft,f});   //copy f?
                res = formula::multop(op::Or, {rgt, res2});
                break;
            case op::W:
                if (f.size() < 2) throw runtime_error("Error: W operator requires two children");
                r = f[1];
                res2 = formula::R(l, formula::multop(op::Or, {l, r}));
                res = progr(res2, m);
                break;
            case op::R:
                if (f.size() < 2) throw runtime_error("Error: R operator requires two children");
                r = f[1];
                lft = progr(l, m);
                rgt = progr(r, m);
                res2 = formula::multop(op::Or, {lft,l});   //copy f?
                res = formula::multop(op::And, {rgt, res2});
                break;
            case op::And:
                for (formula child : f)
                {
                    l = progr(child, m);
                    lst.push_back(l);
                }
                res = formula::multop(op::And, lst);
                break;
            case op::Or:
                for (formula child : f)
                {
                    l = progr(child, m);
                    lst.push_back(l);
                }
                res = formula::multop(op::Or, lst);
                break;
            case op::Implies:
                if (f.size() < 2) throw runtime_error("Error: Implies operator requires two children");
                r = f[1];
                lft = formula::Not(l);
                rgt = formula::multop(op::Or, {lft, r});
                res = progr(rgt, m);
                break;
            case op::Equiv:
                // a <-> b = (a->b) & (b->a)
                if (f.size() < 2) throw runtime_error("Error: Equiv operator requires two children");
                r = f[1];
                lt = formula::binop(op::Implies, l, r);
                lft = progr(lt, m);
                rt = formula::binop(op::Implies, r, l);
                rgt = progr(rt, m);
                res = formula::multop(op::And, {lft, rgt});
                break;
                //do we need to handle more opertator e.g. weak-next ??
            default:
                cerr << "Formula: " << f << ". ";
                throw runtime_error("Error formula in get_nnf()");
                exit(-1);
        }
    }
    return res;
}
