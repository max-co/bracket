#ifndef RANDOM_CONFIG_H
#define RANDOM_CONFIG_H

#include "rabin_automaton.h"

struct Random_config
{
	state_t states;
	runid_t transitions;
	runid_t acceptances;
	runid_t acc_elements;
	runid_t seed;
	bool version;
	bool use_seed;
	runid_t states_dest;
};

static struct Random_config config = {0, 0, 0, 0, 0, false, false, 0};

#endif
