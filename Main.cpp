#include <cstring>
#include <iostream>
#include <memory>

#include "Stopwatch.h"

#include "ExplicitStateDfaMona.h"
#include "ReachabilityMaxSetSynthesizer.h"
#include "InputOutputPartition.h"
#include "lydia/mona_ext/mona_ext_base.hpp"
#include "lydia/utils/misc.hpp"
#include <CLI/CLI.hpp>
#include <istream>
#include <boost/graph/graphviz.hpp>
#include <boost/dynamic_bitset.hpp>
#include <cmath>
#include <random>
#include "./synthesis/header/DfaGameSynthesizer.h"
#include <boost/algorithm/string.hpp>
#include <sstream>
#include "progression.h"
#include <spot/tl/print.hh>
#include <string>
#include <vector>
using namespace std;

int get_random_index(int list_size) {
    random_device rd; // obtain random number from hardware
    mt19937 gen(rd()); // seed the generator
    uniform_int_distribution<> distr(0, list_size-1);
    return distr(gen);
}

vector<string> getGoalFromRevisedIntentions(std::vector<std::string> intention_list, int loopCounter) {
    int nr_intentions = intention_list.size();
    string goal = "";
    vector<string> tested_intentions;
    boost::dynamic_bitset<> intention_bitset(nr_intentions, loopCounter);
    std::cout << "binary intention list dropout: " << intention_bitset << std::endl;

    for (int i = 0; i < intention_bitset.size(); i++) {
        if (!intention_bitset[i]) {
            tested_intentions.push_back(intention_list[i]);
        }
    }
    return tested_intentions;
}

int main(int argc, char ** argv) {

    CLI::App app {
            "IMS interface"
    };

    // start code of Renzo
    bool env_start = true;
    bool maxset = true;
    // set to false if agent should choose actions randomly. If set to true, actions can be given as input by a user
    bool interactive_agent_mode = true;
    // set to false if environment should choose actions randomly. If set to true, actions can be given as input by a user
    bool interactive_env_mode = false;

    // specify which FOND domain is being used
    std::string formula_name = "slippery_world";
//    "slippery_world";
//    "triangle_tireworld";
//    "decision_tree";

    std::string formula_file = "../../example/" + formula_name + ".ltlf";

    std::string init_file = "../../example/" + formula_name + "_init.ltlf";
    std::string agent_file = "../../example/" + formula_name + "_agt.ltlf";
    std::string env_file = "../../example/" + formula_name + "_env.ltlf";
    std::string prec_file = "../../example/" + formula_name + "_prec.ltlf";
    std::string goal_file = "../../example/" + formula_name + "_goal.ltlf";
    std::string partition_file = "../../example/" + formula_name + ".part";

    CLI11_PARSE(app, argc, argv);
    Syft::Stopwatch total_time_stopwatch; // stopwatch for end-to-end execution
    total_time_stopwatch.start();

    Syft::Stopwatch aut_time_stopwatch; // stopwatch for abstract single strategy
    aut_time_stopwatch.start();

    Syft::Player starting_player = env_start? Syft::Player::Environment : Syft::Player::Agent;
    Syft::Player protagonist_player = Syft::Player::Agent;
    Syft::Player protagonist_player_env = Syft::Player::Environment;
    bool realizability;
    bool realizability_env;

    std::ifstream in(formula_file);
    std::ifstream in_init(init_file);
    std::ifstream in_agt(agent_file);
    std::ifstream in_env(env_file);
    std::ifstream in_prec(prec_file);
    std::ifstream in_goal(goal_file);
    std::string f;
    std::string f_init;
    std::string f_agt;
    std::string f_env;
    std::string f_prec;
    std::string f_goal;
    std::getline(in, f);
    std::getline(in_init, f_init);
    std::getline(in_agt, f_agt);
    std::getline(in_env, f_env);
    std::getline(in_prec, f_prec);
    std::getline(in_goal, f_goal);

    Syft::InputOutputPartition partition =
            Syft::InputOutputPartition::read_from_file(partition_file);
    std::shared_ptr<Syft::VarMgr> var_mgr = std::make_shared<Syft::VarMgr>();
    std::shared_ptr<Syft::VarMgr> var_mgr_env = std::make_shared<Syft::VarMgr>();
    var_mgr->create_named_variables(partition.input_variables);
    var_mgr->create_named_variables(partition.output_variables);
    var_mgr_env->create_named_variables(partition.input_variables);
    var_mgr_env->create_named_variables(partition.output_variables);
    var_mgr->partition_variables(partition.input_variables,partition.output_variables);
    var_mgr_env->partition_variables(partition.input_variables, partition.output_variables);

    // change intention list according to the variables of the currently used domain
    std::vector<std::string> intention_list{"F(r4 & c4)"};

    reverse(intention_list.begin(), intention_list.end());
    cout << "initial intentions: " << endl;
    for (auto intent : intention_list) {
        cout << intent << endl;
    }
    cout << endl;

    std::string ltlf_formula;
    string goal = "";

    for (int i = 0; i < intention_list.size(); i++) {
        goal = intention_list[i] + " & " + goal;
    }
    goal.pop_back();
    goal.pop_back();

    ltlf_formula = "(G" + f_agt + ") & ((" + f_init + " & G" + f_env + ") -> ((G(" + f_prec + ")) & (" + goal + ")))";

    Syft::ExplicitStateDfaMona explicit_dfa_mona = Syft::ExplicitStateDfaMona::dfa_of_formula(ltlf_formula);
    Syft::ExplicitStateDfa explicit_dfa = Syft::ExplicitStateDfa::from_dfa_mona(var_mgr, explicit_dfa_mona);
    Syft::SymbolicStateDfa symbolic_dfa = Syft::SymbolicStateDfa::from_explicit(std::move(explicit_dfa));

    auto aut_time = aut_time_stopwatch.stop();

    Syft::Stopwatch nondef_strategy_time_stopwatch; // stopwatch for strategy_generator construction
    nondef_strategy_time_stopwatch.start();

    Syft::ReachabilityMaxSetSynthesizer synthesizer_original(symbolic_dfa, Syft::Player::Agent,
                                                    protagonist_player, symbolic_dfa.final_states(),
                                                    var_mgr->cudd_mgr()->bddOne());

    Syft::SynthesisResult result_original = synthesizer_original.run();
    realizability = result_original.realizability;

    // formula for getting moves where environment follows the rules
    std::string ltlf_formula_env = "(" + f_init + " & ((G" + f_agt + ") -> (G" + f_env + ")))";

    Syft::ExplicitStateDfaMona explicit_dfa_mona_env = Syft::ExplicitStateDfaMona::dfa_of_formula(ltlf_formula_env);
    Syft::ExplicitStateDfa explicit_dfa_env =  Syft::ExplicitStateDfa::from_dfa_mona(var_mgr_env, explicit_dfa_mona_env);
    Syft::SymbolicStateDfa symbolic_dfa_env = Syft::SymbolicStateDfa::from_explicit(std::move(explicit_dfa_env));
    Syft::ReachabilityMaxSetSynthesizer synthesizer_env(symbolic_dfa_env, Syft::Player::Agent,
                                                        protagonist_player_env, symbolic_dfa_env.final_states(),
                                                        var_mgr_env->cudd_mgr()->bddOne());
    Syft::SynthesisResult result_env = synthesizer_env.run();
    realizability_env = result_env.realizability;


    std::cout << "formula agent: " << ltlf_formula << std::endl;
    std::cout << "formula environment: " << ltlf_formula_env << std::endl;
    cout << endl;

    // set the winning moves for the environment by taking the preimage of all the steps that transition into a winning state
    auto win_moves_env = synthesizer_env.preimage(result_env.winning_states);

    if (realizability_env == true) {
        std::cout << "The environment formula is realizable" << std::endl;
    } else {
        std::cout << "The environment formula is NOT realizable" << std::endl;
    }

    bool set_alt_result = false;
    goal = "";
    for (int i = 0; i < intention_list.size(); i++) {
        goal = intention_list[i] + " & " + goal;
    }
    goal.pop_back();
    goal.pop_back();

    ltlf_formula = "(G" + f_agt + ") & ((" + f_init + " & G" + f_env + ") -> ((G(" + f_prec + ")) & (" + goal + ")))";

    var_mgr = std::make_shared<Syft::VarMgr>();
    var_mgr->create_named_variables(partition.input_variables);
    var_mgr->create_named_variables(partition.output_variables);
    var_mgr->partition_variables(partition.input_variables,partition.output_variables);

    explicit_dfa_mona = Syft::ExplicitStateDfaMona::dfa_of_formula(ltlf_formula);
    explicit_dfa = Syft::ExplicitStateDfa::from_dfa_mona(var_mgr, explicit_dfa_mona);
    symbolic_dfa = Syft::SymbolicStateDfa::from_explicit(std::move(explicit_dfa));

    Syft::ReachabilityMaxSetSynthesizer synthesizer(symbolic_dfa, Syft::Player::Agent,
                                                    protagonist_player, symbolic_dfa.final_states(),
                                                    var_mgr->cudd_mgr()->bddOne());

    auto result = synthesizer.run();
    realizability = result.realizability;


    if (realizability == true) {
        std::cout << "The agent formula is realizable" << std::endl;

        auto nondef_strategy_time = nondef_strategy_time_stopwatch.stop();

        if (maxset) {
            Syft::Stopwatch def_strategy_time_stopwatch; // stopwatch for abstract single strategy
            def_strategy_time_stopwatch.start();

            Syft::MaxSet maxset = synthesizer.AbstractMaxSet(std::move(result));
            Syft::MaxSet maxset_env = synthesizer_env.AbstractMaxSet(std::move(result_env));

            int * cube;
            CUDD_VALUE_TYPE value;

            auto npm_agent = maxset.nondeferring_strategy;
            auto pm_agent = maxset.deferring_strategy;

            // get init state BDD for agent (DFA of complete formula)
            auto init_st_to_bin = symbolic_dfa.state_to_binary(explicit_dfa_mona.get_initial_state(), var_mgr->state_variable_count(symbolic_dfa.automaton_id()));
            auto init_st_agent_bdd = var_mgr->state_vector_to_bdd(symbolic_dfa.automaton_id(), init_st_to_bin);

            // get init state BDD for environment (separate DFA, only checking if environment plays according to the rules)
            auto init_st_env_to_bin = symbolic_dfa_env.state_to_binary(explicit_dfa_mona_env.get_initial_state(), var_mgr_env->state_variable_count(symbolic_dfa_env.automaton_id()));
            auto init_st_env_bdd = var_mgr_env->state_vector_to_bdd(symbolic_dfa_env.automaton_id(), init_st_env_to_bin);

            // set the initial state as the current state (current_state variables are used later in the loop)
            auto current_state_bdd_agent = init_st_agent_bdd;
            auto current_state_bdd_env = init_st_env_bdd;

            auto agent_npm_moves = npm_agent * current_state_bdd_agent;
            // initialize variable for if final state is reached. Keep running the while-loop until 'final state reached'-variable is set to true
            auto final_state_reached = false;

            // add counter for adding an intention in the n-th step
            auto add_intention_counter = 0;

            // From here starts a new loop, where we have the new state and can use it to get new possible moves
            while (true) {
                std::cout << "----------------------   NEW LOOP STARTS HERE!  -------------------------" << std::endl << std::endl;
                // get all non-procrastinating moves for the agent in the current state (in the form of a BDD)
                agent_npm_moves = pm_agent * current_state_bdd_agent;

                cout << "get procrastinating (index 1) or non-procrastinating moves (index 2)? [1 or 2]: ";

                int index_strat = 1;
                cin >> index_strat;

                if (index_strat == 1) {
                    agent_npm_moves = pm_agent * current_state_bdd_agent;
                } else {
                    agent_npm_moves = npm_agent * current_state_bdd_agent;
                }

                char *agent_move = new char[var_mgr->total_variable_count()];
                std::vector<int> agent_move_vec;
                int picked_index_agent;

                std::vector<std::vector<int>> cube_list;
                DdGen *it_cubes;
                Cudd_ForeachCube(agent_npm_moves.manager(), agent_npm_moves.getNode(), it_cubes, cube, value) {
                    std::vector<int> temp_cube;
                    for (int i = 0; i < var_mgr->total_variable_count(); i++) {
                        temp_cube.push_back(cube[i]);
                    }
                    cube_list.push_back(temp_cube);
                }

                // display all possible agent moves in current state
                std::cout << "possible agent moves in current state: " << std::endl;

                for (auto cube : cube_list) {
                    string display_action = "";
                    for (int i = var_mgr->input_variable_count(); i < (var_mgr->input_variable_count() + var_mgr->output_variable_count()); i++) {
                        auto bit_value = cube[i];
                        if (bit_value) {
                            display_action = var_mgr->index_to_name(i) + " & ";
                        }
                    }
                    display_action.pop_back();
                    display_action.pop_back();
                    std::cout << display_action << std::endl;
                }
                std::cout << std::endl;

                // if interactive_agent_mode is true, select a winning action manually. if false, random winning action chosen
                if (interactive_agent_mode) {
                    std::cout << "pick an action, using index (nr. from 1 to " + std::to_string(cube_list.size()) + "): ";
                    std::cin >> picked_index_agent;

                    if (picked_index_agent >= 1 && picked_index_agent < cube_list.size() + 1) {
                        agent_move_vec = cube_list[picked_index_agent - 1];
                        std::cout << "move selected: ";
                    } else {
                        std::cerr << "invalid index selected!" << std::endl;
                    }
                } else {
                    agent_move_vec = cube_list[get_random_index(cube_list.size())];
                    std::cout << "random agent move picked: ";
                }
                string selected_action = "";
                for (int i = var_mgr->input_variable_count(); i < (var_mgr->input_variable_count() + var_mgr->output_variable_count()); i++) {
                    auto bit_value = agent_move_vec[i];
                    if (bit_value) {
                        selected_action = var_mgr->index_to_name(i) + " & ";
                    }
                }
                selected_action.pop_back();
                selected_action.pop_back();
                std::cout << selected_action << std::endl << std::endl;

                // get all winning moves for the environment in the current state (in the form of a BDD)
                auto env_pos_moves = current_state_bdd_env * win_moves_env;

                // agent chooses action first, so restrict possible environment reactions to where the agent action is included
                for (int i = var_mgr_env->input_variable_count();
                     i < (var_mgr_env->output_variable_count() + var_mgr_env->input_variable_count()); i++) {
                    // get truth value from the agent actions
                    auto bit_value = agent_move_vec[i];
                    auto bit_value_bdd = var_mgr_env->name_to_variable((var_mgr_env->index_to_name(i)));
                    if (bit_value) {
                        env_pos_moves = env_pos_moves * bit_value_bdd;
                    } else {
                        env_pos_moves = env_pos_moves * !bit_value_bdd;
                    }
                }

                // out of all possible environment moves in current state, and given the agent action, pick a random cube from the BDD
                char *env_move = new char[var_mgr_env->total_variable_count()];
                std::vector<int> current_complete_vec;
                int picked_index_env;

                std::vector<std::vector<int>> cube_list_env;
                DdGen *it_cubes_env;
                Cudd_ForeachCube(env_pos_moves.manager(), env_pos_moves.getNode(), it_cubes_env, cube, value) {
                    std::vector<int> temp_cube;
                    for (int i = 0; i < var_mgr_env->total_variable_count(); i++) {
                        temp_cube.push_back(cube[i]);
                    }
                    cube_list_env.push_back(temp_cube);
                }

                // display all possible agent moves in current state
                std::cout << "possible environment moves in current state: " << std::endl;

                for (auto cube : cube_list_env) {
                    string display_action = "";
                    for (int i = 0; i < var_mgr->input_variable_count(); i++) {
                        auto bit_value = cube[i];
                        if (bit_value) {
                            display_action = display_action + " & " + var_mgr->index_to_name(i);
                        }
                    }
                    display_action = display_action.substr(3, display_action.length()-1);
                    std::cout << display_action << std::endl;
                }
                std::cout << std::endl;

                if (interactive_env_mode) {
                    std::cout << "pick a reaction, using index (nr. from 1 to " + std::to_string(cube_list_env.size()) + "): " ;
                    std::cin >> picked_index_env;

                    if (picked_index_env >= 1 && picked_index_env < cube_list_env.size()+1) {
                        current_complete_vec = cube_list_env[picked_index_env-1];
                        std::cout << "move selected: ";
                    } else {
                        std::cerr << "invalid index selected!" << std::endl;
                    }
                } else {
                    current_complete_vec = cube_list_env[get_random_index(cube_list_env.size())];
                    std::cout << "random environment move picked: ";
                }
                string selected_action_env = "";
                for (int i = 0; i < var_mgr_env->input_variable_count(); i++) {
                    auto bit_value = current_complete_vec[i];
                    if (bit_value) {
                        selected_action_env = selected_action_env + " & " + var_mgr_env->index_to_name(i);
                    }
                }
                selected_action_env = selected_action_env.substr(3, selected_action_env.length()-1);
                std::cout << selected_action_env << std::endl << std::endl;

                // we now create two BDD's for the agent and environment, where in each BDD is 1 cube: the picked agent- and environment
                // action + the current DFA state (different for agent and environment since they use a different DFA)
                CUDD::BDD current_complete_bdd_agent;
                CUDD::BDD current_complete_bdd_env;
                // for all the input- and output variables in a cube:
                for (int i = 0; i < (var_mgr_env->output_variable_count() + var_mgr_env->input_variable_count()); i++) {
                    // get truth value of specific bit in the cube, using the index
                    auto var_bit = current_complete_vec[i];
                    // with index, get name of variable. Then, get the BDD of this variable using the name
                    auto var_bit_bdd_agent = var_mgr->name_to_variable((var_mgr->index_to_name(i)));
                    auto var_bit_bdd_env = var_mgr_env->name_to_variable((var_mgr_env->index_to_name(i)));

                    // if it's the first bit, only initialize 'random_move_bdd' with this bit. otherwise combine it with the previous BDD's
                    if (i == 0) {
                        if (var_bit) {
                            current_complete_bdd_agent = var_bit_bdd_agent;
                            current_complete_bdd_env = var_bit_bdd_env;
                        } else {
                            current_complete_bdd_agent = !var_bit_bdd_agent;
                            current_complete_bdd_env = !var_bit_bdd_env;
                        }
                    } else {
                        if (var_bit) {
                            current_complete_bdd_agent = current_complete_bdd_agent * var_bit_bdd_agent;
                            current_complete_bdd_env = current_complete_bdd_env * var_bit_bdd_env;
                        } else {
                            current_complete_bdd_agent = current_complete_bdd_agent * !var_bit_bdd_agent;
                            current_complete_bdd_env = current_complete_bdd_env * !var_bit_bdd_env;
                        }
                    }
                }

                // now combine agent BDD also with the state variables (in a different for-loop since calling state variables is different compared to calling input- and output variables
                for (int i = 0; i < var_mgr->state_variable_count(symbolic_dfa.automaton_id()); i++) {
                    auto var_bit = agent_move_vec[
                            (var_mgr->output_variable_count() + var_mgr->input_variable_count()) + i];
                    if (var_bit) {
                        current_complete_bdd_agent =
                                current_complete_bdd_agent * (var_mgr->state_variable(symbolic_dfa.automaton_id(), i));
                    } else {
                        current_complete_bdd_agent =
                                current_complete_bdd_agent * !(var_mgr->state_variable(symbolic_dfa.automaton_id(), i));
                    }
                }

                // now combine environment BDD also with the state variables (in a different for-loop since calling state variables is different compared to calling input- and output variables
                for (int i = 0; i < var_mgr_env->state_variable_count(symbolic_dfa_env.automaton_id()); i++) {
                    auto var_bit = current_complete_vec[
                            (var_mgr_env->output_variable_count() + var_mgr_env->input_variable_count()) + i];
                    if (var_bit) {
                        current_complete_bdd_env = current_complete_bdd_env *
                                                (var_mgr_env->state_variable(symbolic_dfa_env.automaton_id(), i));
                    } else {
                        current_complete_bdd_env = current_complete_bdd_env *
                                                !(var_mgr_env->state_variable(symbolic_dfa_env.automaton_id(), i));
                    }
                }

                std::vector<int> next_state_vec_agent;
                // we loop over the transition functions, which is equal to the amount of state-variables we have. If the conjunction of the picked actions conjoined with
                // the given transition function is empty, it returns false for that specific state variable. Otherwise, return true. With this we build a new state vector,
                // which indicates in binary what the resulting state is
                for (auto tr_func: symbolic_dfa.transition_function()) {
                    bool state_val = false;
                    CUDD::BDD state_val_bdd_agent = tr_func * current_complete_bdd_agent;

                    DdGen *it_tr_func_ag;
                    Cudd_ForeachCube(state_val_bdd_agent.manager(), state_val_bdd_agent.getNode(), it_tr_func_ag, cube,
                                     value) {
                        state_val = true;
                    }
                    next_state_vec_agent.push_back(state_val);
                }

                std::vector<int> next_state_vec_env;
                // we loop over the transition functions, which is equal to the amount of state-variables we have. If the conjunction of the picked actions conjoined with
                // the given transition function is empty, it returns false for that specific state variable. Otherwise, return true. With this we build a new state vector,
                // which indicates in binary what the resulting state is
                for (auto tr_func: symbolic_dfa_env.transition_function()) {
                    bool state_val = false;
                    CUDD::BDD state_val_bdd_env = tr_func * current_complete_bdd_env;

                    DdGen *it_tr_func_env;
                    Cudd_ForeachCube(state_val_bdd_env.manager(), state_val_bdd_env.getNode(), it_tr_func_env, cube,
                                     value) {
                        state_val = true;
                    }
                    next_state_vec_env.push_back(state_val);
                }

                // get next state BDD for the agent (full formula, for choosing agent actions)
                auto next_st_bdd_agent = var_mgr->state_vector_to_bdd(symbolic_dfa.automaton_id(),
                                                                      next_state_vec_agent);

                // get next state BDD for the environment (smaller formula where env follows the rules, for choosing environment actions)
                auto next_st_bdd_environment = var_mgr_env->state_vector_to_bdd(symbolic_dfa_env.automaton_id(),
                                                                                next_state_vec_env);

                // set the next state of the agent and environment as the current state. this is used for extracting the winning moves when the loops starts over
                current_state_bdd_agent = next_st_bdd_agent;
                current_state_bdd_env = next_st_bdd_environment;

                vector<tuple<string, int>> env_value_vector;
                map<formula, formula> env_map;
                std::vector<std::string> current_env_vars;
                for (int i = 0; i < var_mgr_env->input_variable_count(); i++) {
                    auto bit_value = current_complete_vec[i];
                    auto env_var_formula = spot::parse_formula(var_mgr_env->index_to_name(i));
                    if (bit_value) {
                        current_env_vars.push_back(var_mgr_env->index_to_name(i));
                        env_value_vector.push_back({var_mgr_env->index_to_name(i), 1});
                        env_map[env_var_formula] = formula::tt();
                    } else {
                        env_value_vector.push_back({var_mgr_env->index_to_name(i), 0});
                        env_map[env_var_formula] = formula::ff();
                    }
                }

                cout << "current env vars set to true: " << endl;
                for (auto current_env_var : current_env_vars) {
                    cout << current_env_var << endl;
                }

                auto input_labels = symbolic_dfa.var_mgr()->input_variable_labels();

                cout << "progressed intentions: " << endl;
                std::vector<std::string> new_intention_list;
                for (string intention : intention_list) {
                    auto parsed_intention = spot::parse_formula(intention);
                    cout << "before: " << parsed_intention << "  ->  ";
                    auto progressed_intention = progr(parsed_intention, &env_map);
                    cout << "after: " << progressed_intention << endl;
                    if (progressed_intention.kind() == op::tt) {
                        cout << "intention '" << parsed_intention << "' has been fulfilled and is removed from the intention list" << endl;
                    } else {
                        ostringstream oss;
                        oss << progressed_intention;
                        new_intention_list.push_back(oss.str());
                    }
                }
                // update intention list with the updated progressions
                intention_list = new_intention_list;
                cout << endl;

                if (intention_list.empty()) {
                    final_state_reached = true;
                    std::cout << "final state reached!" << std::endl;
                    string halt;
                    cout << "stop program? [y/n]: ";
                    cin >> halt;
                    if (halt == "y"){
                        cout << "program stopped" << endl;
                        exit(0);
                    }
                }

                string agent_add_intention = "y";

                while (agent_add_intention == "y") {
                    cout << "call update operation? [y/n]: ";
                    cin >> agent_add_intention;
                    cout << endl;
                    if (agent_add_intention == "y") {
                        int update_op;
                        cout << "would you like to add an intention [1], drop [2] an intention, or stop the entire system [3]?: ";
                        cin >> update_op;
                        cout << endl;
                        switch (update_op) {
                            case 1: {
                                string new_intent;
                                cout << "give new intention: ";
                                try {
                                    cin.ignore();
                                    getline(cin, new_intent);
                                    auto form_new_intent = spot::parse_formula(new_intent);

                                    int intent_priority;
                                    cout
                                            << "set priority using an index (position in the intention list. 0 is highest priority): ";
                                    try {
                                        cin >> intent_priority;

                                        std::cout << "trying to add new intention '" << new_intent
                                                  << "' to intention list: " << std::endl << endl;
                                        // make tuple, where first element contains the new intention, and the second element contains the priority
                                        tuple<string, int> new_intention{new_intent, intent_priority};

                                        // retrieve environment variables
                                        std::vector <std::string> current_env_variables;
                                        for (int i = 0; i < var_mgr->input_variable_count(); i++) {
                                            auto var_bit = current_complete_vec[i];
                                            if (var_bit) {
                                                current_env_variables.push_back(var_mgr->index_to_name(i));
                                            }
                                        }
                                        std::string new_init = "";
                                        for (auto env_var: current_env_variables) {
                                            new_init += env_var;
                                            new_init += "&";
                                        }
                                        new_init.pop_back();
                                        new_init = "(" + new_init + ")";

                                        auto test_new_intentions_list = intention_list;
                                        test_new_intentions_list.insert(
                                                test_new_intentions_list.begin() + get<1>(new_intention),
                                                get<0>(new_intention));

                                        string new_goal = "";
                                        for (int i = 0; i < test_new_intentions_list.size(); i++) {
                                            new_goal = test_new_intentions_list[i] + " & " + new_goal;
                                        }
                                        new_goal.pop_back();
                                        new_goal.pop_back();

                                        std::string new_ltlf_formula = "(G" + f_agt + ") & ((" + new_init + " & G" + f_env + ") -> ((G(" + f_prec + ")) & (" + new_goal + ")))";
                                        std::cout << "new goal: " << new_goal << std::endl;

                                        std::shared_ptr <Syft::VarMgr> var_mgr_new = std::make_shared<Syft::VarMgr>();
                                        var_mgr_new->create_named_variables(partition.input_variables);
                                        var_mgr_new->create_named_variables(partition.output_variables);
                                        var_mgr_new->partition_variables(partition.input_variables,
                                                                         partition.output_variables);

                                        auto explicit_dfa_mona_new = Syft::ExplicitStateDfaMona::dfa_of_formula(
                                                new_ltlf_formula);
                                        auto explicit_dfa_new = Syft::ExplicitStateDfa::from_dfa_mona(var_mgr_new,
                                                                                                      explicit_dfa_mona_new);
                                        auto symbolic_dfa_new = Syft::SymbolicStateDfa::from_explicit(
                                                std::move(explicit_dfa_new));

                                        Syft::ReachabilityMaxSetSynthesizer synthesizer_new(symbolic_dfa_new,
                                                                                            Syft::Player::Agent,
                                                                                            protagonist_player,
                                                                                            symbolic_dfa_new.final_states(),
                                                                                            var_mgr_new->cudd_mgr()->bddOne());

                                        auto result_new = synthesizer_new.run();
                                        auto realizability_new = result_new.realizability;

                                        if (realizability_new) {
                                            intention_list = test_new_intentions_list;
                                            std::cout
                                                    << "adding new intention is realizable! new intention is adopted and DFA is recomputed"
                                                    << std::endl;

                                            var_mgr = std::make_shared<Syft::VarMgr>();
                                            var_mgr->create_named_variables(partition.input_variables);
                                            var_mgr->create_named_variables(partition.output_variables);
                                            var_mgr->partition_variables(partition.input_variables,
                                                                         partition.output_variables);

                                            auto explicit_dfa_mona_check = Syft::ExplicitStateDfaMona::dfa_of_formula(
                                                    new_ltlf_formula);
                                            auto explicit_dfa_check = Syft::ExplicitStateDfa::from_dfa_mona(var_mgr,
                                                                                                 explicit_dfa_mona_check);
                                            symbolic_dfa = Syft::SymbolicStateDfa::from_explicit(
                                                    std::move(explicit_dfa_check));

                                            auto synthesizer_check = Syft::ReachabilityMaxSetSynthesizer(symbolic_dfa,
                                                                                                       Syft::Player::Agent,
                                                                                                       protagonist_player,
                                                                                                         symbolic_dfa.final_states(),
                                                                                                       var_mgr->cudd_mgr()->bddOne());

                                            auto result_check = synthesizer_check.run();
                                            auto maxset_new = synthesizer_check.AbstractMaxSet(std::move(result_check));
                                            auto npm_agent_new = maxset_new.nondeferring_strategy;
                                            auto pm_agent_new = maxset_new.deferring_strategy;

                                            npm_agent = npm_agent_new;
                                            pm_agent = pm_agent_new;

                                            // get init state BDD for agent (DFA of complete formula)
                                            auto init_st_to_bin = symbolic_dfa.state_to_binary(
                                                    explicit_dfa_mona_check.get_initial_state(),
                                                    var_mgr->state_variable_count(symbolic_dfa.automaton_id()));
                                            auto init_st_agent_bdd = var_mgr->state_vector_to_bdd(
                                                    symbolic_dfa.automaton_id(), init_st_to_bin);

                                            std::vector<int> next_state_vec_agent;
                                            for (auto tr_func: symbolic_dfa.transition_function()) {
                                                bool state_val = false;
                                                CUDD::BDD state_val_bdd_agent = tr_func * init_st_agent_bdd;

                                                DdGen *it_tr_func_ag;
                                                Cudd_ForeachCube(state_val_bdd_agent.manager(),
                                                                 state_val_bdd_agent.getNode(), it_tr_func_ag, cube,
                                                                 value)
                                                {
                                                    state_val = true;
                                                }
                                                next_state_vec_agent.push_back(state_val);
                                            }
                                            // get next state BDD for the agent (full formula, for choosing agent actions)
                                            next_st_bdd_agent = var_mgr->state_vector_to_bdd(
                                                    symbolic_dfa.automaton_id(),
                                                    next_state_vec_agent);

                                            // set the initial state as the current state (current_state variables are used later in the loop)
                                            current_state_bdd_agent = next_st_bdd_agent;
                                            std::cout << "new formula agent: " << new_ltlf_formula << std::endl;
                                        } else {
                                            std::cout << "adding new intention is not realizable." << std::endl << endl;
                                            cout
                                                    << "would you like to revise the current intentions in order to add the new intention? [y/n]: ";
                                            string agent_revises;
                                            cin >> agent_revises;
                                            cout << endl;
                                            if (agent_revises == "y") {
                                                bool new_realizability = false;
                                                auto temp_intention_list = intention_list;
                                                temp_intention_list.insert(
                                                        temp_intention_list.begin() + get<1>(new_intention),
                                                        get<0>(new_intention));
                                                reverse(temp_intention_list.begin(), temp_intention_list.end());

                                                int intention_counter = 0;
                                                while (!new_realizability) {
                                                    string new_ltlf_formula = "";

                                                    auto realizability_curr_set = false;
                                                    while (!realizability_curr_set &&
                                                           intention_counter < pow(temp_intention_list.size(), 2) - 1) {
                                                        auto goal_vec_temp = getGoalFromRevisedIntentions(
                                                                temp_intention_list, intention_counter);

                                                        string goal_temp = "";

                                                        for (auto intent: goal_vec_temp) {
                                                            goal_temp = intent + " & " + goal_temp;
                                                        }
                                                        goal_temp.pop_back();
                                                        goal_temp.pop_back();

                                                        cout
                                                                << "the following intentions are tested for realizability: "
                                                                << goal_temp << endl;
                                                        // below, replace 'goal' with 'f_goal' if we want to have
                                                        new_ltlf_formula =
                                                                "(G" + f_agt + ") & ((" + new_init + " & G" + f_env +
                                                                ") -> ((G(" + f_prec + ")) & (" +
                                                                goal_temp + ")))";
                                                        auto var_mgr_temp = std::make_shared<Syft::VarMgr>();
                                                        var_mgr_temp->create_named_variables(partition.input_variables);
                                                        var_mgr_temp->create_named_variables(
                                                                partition.output_variables);
                                                        var_mgr_temp->partition_variables(partition.input_variables,
                                                                                          partition.output_variables);

                                                        auto explicit_dfa_mona_temp = Syft::ExplicitStateDfaMona::dfa_of_formula(
                                                                new_ltlf_formula);
                                                        auto explicit_dfa_temp = Syft::ExplicitStateDfa::from_dfa_mona(
                                                                var_mgr_temp,
                                                                explicit_dfa_mona_temp);
                                                        auto symbolic_dfa_temp = Syft::SymbolicStateDfa::from_explicit(
                                                                std::move(explicit_dfa_temp));

                                                        Syft::ReachabilityMaxSetSynthesizer synthesizer_new(
                                                                symbolic_dfa_temp, Syft::Player::Agent,
                                                                protagonist_player,
                                                                symbolic_dfa_temp.final_states(),
                                                                var_mgr_temp->cudd_mgr()->bddOne());

                                                        auto result_new = synthesizer_new.run();
                                                        new_realizability = result_new.realizability;

                                                        if (!new_realizability) {
                                                            cout
                                                                    << "The formula is not realizable with these intentions."
                                                                    << endl;
                                                        } else {
                                                            vector <string> unrealizable_intentions = temp_intention_list;
                                                            for (auto intent: goal_vec_temp) {
                                                                unrealizable_intentions.erase(
                                                                        remove(unrealizable_intentions.begin(),
                                                                               unrealizable_intentions.end(), intent),
                                                                        unrealizable_intentions.end());
                                                            }
                                                            cout
                                                                    << "Formula is realizable if the following intentions are dropped: "
                                                                    << endl;
                                                            for (auto unr_inten: unrealizable_intentions) {
                                                                cout << unr_inten << endl;
                                                            }
                                                            break;
                                                        }
                                                        std::cout << std::endl;

                                                        intention_counter++;
                                                    }
                                                }
                                            } else {
                                                cout << "the original intentions remain as they were, new intention is not added" << endl;
                                            }
                                        }
                                    } catch (...) {
                                        cout << "faulty priority was set. intention is not being added." << endl;
                                    }
                                } catch (...) {
                                    cout << "given input is not a formula." << endl;
                                }
                                break;
                            }
                            case 2: {
                                cout << "which intention should be dropped from the list?: (index 1 until "
                                     << intention_list.size() << ")" << endl;
                                cout << "options: " << endl;
                                for (int i = 0; i < intention_list.size(); i++) {
                                    cout << i + 1 << " : " << intention_list[i] << endl;
                                }

                                int picked_index_intention;
                                cout << "index chosen: ";
                                cin >> picked_index_intention;
                                intention_list.erase(intention_list.begin() + picked_index_intention - 1);

                                string new_ltlf_formula = "";

                                int intention_counter = 0;

                                auto goal_vec_temp = getGoalFromRevisedIntentions(
                                        intention_list, intention_counter);
                                string goal_temp = "";

                                if (!goal_vec_temp.empty()) {
                                    for (auto intent: goal_vec_temp) {
                                        goal_temp = intent + " & " + goal_temp;
                                    }
                                    goal_temp.pop_back();
                                    goal_temp.pop_back();
                                } else {
                                    goal_temp = "tt";
                                }

                                // retrieve environment variables
                                std::vector <std::string> current_env_variables;
                                for (int i = 0; i < var_mgr->input_variable_count(); i++) {
                                    auto var_bit = current_complete_vec[i];
                                    if (var_bit) {
                                        current_env_variables.push_back(var_mgr->index_to_name(i));
                                    }
                                }
                                std::string new_init = "";
                                for (auto env_var: current_env_variables) {
                                    new_init += env_var;
                                    new_init += "&";
                                }
                                new_init.pop_back();
                                new_init = "(" + new_init + ")";

                                new_ltlf_formula =
                                        "(G" + f_agt + ") & ((" + new_init + " & G" + f_env +
                                        ") -> ((G(" + f_prec + ")) & (" +
                                        goal_temp + ")))";

                                var_mgr = std::make_shared<Syft::VarMgr>();
                                var_mgr->create_named_variables(partition.input_variables);
                                var_mgr->create_named_variables(partition.output_variables);
                                var_mgr->partition_variables(partition.input_variables,
                                                             partition.output_variables);

                                auto explicit_dfa_mona = Syft::ExplicitStateDfaMona::dfa_of_formula(
                                        new_ltlf_formula);
                                auto explicit_dfa = Syft::ExplicitStateDfa::from_dfa_mona(var_mgr,
                                                                                     explicit_dfa_mona);
                                symbolic_dfa = Syft::SymbolicStateDfa::from_explicit(
                                        std::move(explicit_dfa));

                                auto synthesizer_new = Syft::ReachabilityMaxSetSynthesizer(symbolic_dfa,
                                                                                           Syft::Player::Agent,
                                                                                           protagonist_player,
                                                                                           symbolic_dfa.final_states(),
                                                                                           var_mgr->cudd_mgr()->bddOne());

                                auto result_new = synthesizer_new.run();
                                auto maxset_new = synthesizer_new.AbstractMaxSet(std::move(result_new));
                                auto npm_agent_new = maxset_new.nondeferring_strategy;
                                auto pm_agent_new = maxset_new.deferring_strategy;

                                npm_agent = npm_agent_new;
                                pm_agent = pm_agent_new;

                                // get init state BDD for agent (DFA of complete formula)
                                auto init_st_to_bin = symbolic_dfa.state_to_binary(
                                        explicit_dfa_mona.get_initial_state(),
                                        var_mgr->state_variable_count(symbolic_dfa.automaton_id()));
                                auto init_st_agent_bdd = var_mgr->state_vector_to_bdd(
                                        symbolic_dfa.automaton_id(), init_st_to_bin);

                                std::vector<int> next_state_vec_agent;
                                for (auto tr_func: symbolic_dfa.transition_function()) {
                                    bool state_val = false;
                                    CUDD::BDD state_val_bdd_agent = tr_func * init_st_agent_bdd;

                                    DdGen *it_tr_func_ag;
                                    Cudd_ForeachCube(state_val_bdd_agent.manager(),
                                                     state_val_bdd_agent.getNode(), it_tr_func_ag, cube,
                                                     value)
                                    {
                                        state_val = true;
                                    }
                                    next_state_vec_agent.push_back(state_val);
                                }
                                // get next state BDD for the agent (full formula, for choosing agent actions)
                                next_st_bdd_agent = var_mgr->state_vector_to_bdd(
                                        symbolic_dfa.automaton_id(),
                                        next_state_vec_agent);

                                // set the initial state as the current state (current_state variables are used later in the loop)
                                current_state_bdd_agent = next_st_bdd_agent;
                                std::cout << "new formula agent: " << new_ltlf_formula << std::endl;
                                break;
                            }
                            case 3:
                                cout << "program is stopped";
                                exit(0);
                                break;
                            default:
                                cout << "invalid operation picked. no update operation executed.";
                                break;
                        }
                    } else {
                        cout << "run continues without update operation" << endl;
                        break;
                    }
                }
            }

            std::cout << "---------------- here the run ends --------------------" << std::endl << std::endl;
            // retrieve and print out final states
            auto final_states = symbolic_dfa.final_states();
            std::cout << "final states (for agent): " << std::endl;
            final_states.PrintMinterm();
            cout << endl;

            auto def_strategy_time = def_strategy_time_stopwatch.stop();
            std::cout << "Deferring strategy generator construction time: "
                      << def_strategy_time.count() << " ms" << std::endl;

            Syft::Stopwatch abstract_single_strategy_time_stopwatch; // stopwatch for abstract single strategy
            abstract_single_strategy_time_stopwatch.start();
        }
    }

  auto total_time = total_time_stopwatch.stop();

  std::cout << "Total time: "
	    << total_time.count() << " ms" << std::endl;

  return 0;
}