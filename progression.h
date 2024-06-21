//
// Created by vboxuser on 23.05.24.
//

#ifndef IMS_PROGRESSION_H
#define IMS_PROGRESSION_H

#include "spotparser.h"

#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/relabel.hh>


#include <string>
#include <cstdio>
#include <iostream>
#include <sstream>

#include <fstream>
std::vector<std::string> get_props(formula &f);

formula progr(formula &f, map<formula, formula>* m);
#endif //IMS_PROGRESSION_H
