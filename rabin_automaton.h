#ifndef RABIN_AUTOMATON_H
#define RABIN_AUTOMATON_H

#include <list>
#include <utility>

#include "run.h"
#include "search_context.h"

class Acceptance final
{
public:
	bitset_t l;
	bitset_t u;

	Acceptance(const bitset_t &l, const bitset_t &u) : l{l}, u{u} {};
	Acceptance(const bitset_t &&l, const bitset_t &&u) : l{std::move(l)}, u{std::move(u)} {};
};
std::ostream &operator<<(std::ostream &, const Acceptance &);

struct Out_transition
{
	state_t left;
	state_t right;
};

class Illegal_state_set
{
};

class Rabin_automaton final
{
public:
	const state_t states;

private:
	state_t starting_state;
	bool has_transitions;
	std::list<Out_transition> *transitions;
	std::list<Acceptance> conditions;

public:
	Rabin_automaton(const state_t);
	Rabin_automaton(const Rabin_automaton &);
	Rabin_automaton(Rabin_automaton &&);
	~Rabin_automaton();
	Rabin_automaton &operator=(const Rabin_automaton &) = delete;
	Rabin_automaton &operator=(Rabin_automaton &&) = delete;

	state_t get_start() const { return starting_state; };
	void set_start(const state_t q) { starting_state = q; };
	bool is_valid_state(const state_t q) const { return q < states; };

	void add_transition(const state_t, const state_t, const state_t);
	void add_acceptance(const bitset_t &, const bitset_t &);
	void add_acceptance(bitset_t &&, bitset_t &&);
	Run *find_run(const int max_threads = 1) const;

	std::ostream &print_logic_prog_rep(std::ostream &) const;

private:
	std::ostream &acceptances_print_logic_prog_rep(std::ostream &) const;

	friend std::ostream &operator<<(std::ostream &, const Rabin_automaton &);
};
std::ostream &operator<<(std::ostream &, const Rabin_automaton &);

#endif
