#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <random>
#include <unistd.h>

#include "random_config.h"
#include "version.h"

using namespace std;

extern int optind;
extern char *optarg;

static state_t rand_state();

int
main(int argc, char *argv[])
{
	for (int i = 0; i < argc; i++) {
		if (nullptr == argv[i]) {
			return EXIT_FAILURE;
		}
	}
	{
		int op = 0;
		while ((op = getopt(argc, argv, "s:t:a:e:r:V")) != -1) {
			runid_t *dest = nullptr;
			switch (op) {
				case 's':
					dest = &config.states_dest;
					break;
				case 't':
					dest = &config.transitions;
					break;
				case 'a':
					dest = &config.acceptances;
					break;
				case 'e':
					dest = &config.acc_elements;
					break;
				case 'r':
					config.use_seed = true;
					dest = &config.seed;
					break;
				case 'V':
					config.version = true;
					break;
				default:
					return EXIT_FAILURE;
			}
			if ('V' != op) {
				errno = 0;
				unsigned long long tmp = strtoull(optarg, NULL, 10);
				if (errno || RUNID_MAX < tmp) {
					return EXIT_FAILURE;
				}
				*dest = static_cast<runid_t>(tmp);
			}
			if ('s' == op) {
				if (STATE_MAX < config.states_dest) {
					return EXIT_FAILURE;
				}
				config.states = static_cast<state_t>(config.states_dest);
			}
		}
	}
	if (config.version) {
		cout << "random_automaton " VERSION << endl;
		return EXIT_SUCCESS;
	}
	if (0 >= config.states) {
		return EXIT_FAILURE;
	}
	Rabin_automaton automaton(config.states);
	for (runid_t i = 0; i < config.transitions; i++) {
		automaton.add_transition(rand_state(), rand_state(), rand_state());
	}

	const auto rand_set = [](bitset_t &set) {
		runid_t card = 0;
		while (card < config.acc_elements && card < config.states) {
			state_t q = rand_state();
			if (!set.test_set(q)) {
				card++;
			}
		}
	};

	bitset_t l(config.states);
	bitset_t u(config.states);
	for (runid_t i = 0; i < config.acceptances; i++) {
		rand_set(l);
		rand_set(u);
		automaton.add_acceptance(l, u);
		l.reset();
		u.reset();
	}
	cout << automaton << endl;
	return EXIT_SUCCESS;
}

static state_t
rand_state()
{
	static mt19937 engine(
		config.use_seed ? (mt19937::result_type)config.seed : (mt19937::result_type)random_device()());
	static const mt19937::result_type limit = mt19937::max() - mt19937::max() % config.states;
	mt19937::result_type s = engine();
	while (limit < s) {
		s = engine();
	}
	return (state_t)(s % config.states);
}
