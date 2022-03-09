#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>

#include "rabin_automaton.h"

Rabin_automaton::Rabin_automaton(const state_t state_num) : states{state_num}, starting_state{0}, has_transitions{false}
{
	transitions = new std::list<Out_transition>[states];
}

Rabin_automaton::Rabin_automaton(const Rabin_automaton &arg)
	: states{arg.states}, starting_state{arg.starting_state}, has_transitions{arg.has_transitions}
{
	if (this == &arg) {
		return;
	}
	transitions = new std::list<Out_transition>[states];
	for (state_t s = 0; s < states; s++) {
		transitions[s] = arg.transitions[s];
	}
	conditions = arg.conditions;
}

Rabin_automaton::Rabin_automaton(Rabin_automaton &&arg)
	: states{arg.states}
	, starting_state{arg.starting_state}
	, has_transitions{arg.has_transitions}
	, transitions{arg.transitions}
	, conditions{std::move(arg.conditions)}
{
	arg.starting_state = 0;
	arg.has_transitions = false;
	arg.transitions = new std::list<Out_transition>[states];
}

Rabin_automaton::~Rabin_automaton()
{
	delete[] transitions;
}

void
Rabin_automaton::add_transition(const state_t q1, const state_t q2, const state_t q3)
{
	transitions[q1].push_back({q2, q3});
	has_transitions = true;
}

void
Rabin_automaton::add_acceptance(const bitset_t &l, const bitset_t &u)
{
	conditions.emplace_back(l, u);
	auto t = conditions.end();
	t--;
	t->u -= t->l;
	if (t->u.none()) {
		conditions.pop_back();
	}
}

void
Rabin_automaton::add_acceptance(bitset_t &&l, bitset_t &&u)
{
	u -= l;
	if (u.any()) {
		conditions.emplace_back(std::move(l), std::move(u));
	}
}

Run *
Rabin_automaton::find_run(const int max_threads) const
{
	if (1 != max_threads) {
		throw std::invalid_argument("multithreading is disabled in this version");
	}
	if (!has_transitions || conditions.empty()) {
		return nullptr;
	}

	class Run_piece final
	{
		Run &run;

	public:
		state_t state;
		bool graft;
		bool invalid;
		bitset_t internal;
		bitset_t nonlive;
		bitset_t all;
		state_t height;

	private:
		const Run_piece *left;
		const Run_piece *right;

	public:
		Run_piece(Run &r, const state_t q, const bool g = false, const state_t h = 0)
			: run{r}
			, state{q}
			, graft{g}
			, invalid{false}
			, internal{r.states}
			, nonlive{r.states}
			, all{r.states}
			, height{h}
			, left{nullptr}
			, right{nullptr}
		{
			if (!graft) {
				nonlive.set(state);
				all.set(state);
			}
		};
		Run_piece(const state_t p, const Run_piece *const l, const Run_piece *const r)
			: run{l->run}
			, state{p}
			, graft{false}
			, invalid{false}
			, internal{l->run.states}
			, nonlive{l->run.states}
			, all{l->run.states}
			, height{1 + std::max(l->height, r->height)}
		{
			left = l;
			right = r;
			nonlive |= left->nonlive;
			nonlive |= right->nonlive;
			nonlive.set(p, false);
			if (nonlive.none()) {
				run.save_subruns(node());
				graft = true;
				left = nullptr;
				right = nullptr;
				nonlive.reset();
				return;
			}
			internal |= left->internal;
			internal |= right->internal;
			internal.set(p);
			all |= left->all;
			all |= right->all;
			all.set(p);
		};
		Run_node *node(Run_node *p = nullptr) const
		{
			Run_node *res = new Run_node(state, p);
			if (graft) {
				res->live = true;
			} else if (nullptr != left) {
				res->left = left->node(res);
				res->right = right->node(res);
			}
			return res;
		};
		bool invalid_child() const
		{
			return (nullptr != left && left->invalid) || (nullptr != right && right->invalid);
		};
		bool operator<(const Run_piece &rhs) const
		{
			if (std::less<Run *>{}(&run, &rhs.run)) {
				return true;
			} else if (std::greater<Run *>{}(&run, &rhs.run)) {
				return false;
			}
			if (graft < rhs.graft) {
				return true;
			} else if (graft > rhs.graft) {
				return false;
			}
			if (state < rhs.state) {
				return true;
			} else if (state > rhs.state) {
				return false;
			}
			if (height < rhs.height) {
				return true;
			} else if (height > rhs.height) {
				return false;
			}
			if (internal < rhs.internal) {
				return true;
			} else if (internal > rhs.internal) {
				return false;
			}
			if (nonlive < rhs.nonlive) {
				return true;
			} else if (nonlive > rhs.nonlive) {
				return false;
			}
			if (all < rhs.all) {
				return true;
			}
			return false;
		};
		static bool similar(const Run_piece &lhs, const Run_piece &rhs)
		{
			return &lhs.run == &rhs.run && lhs.state == rhs.state && lhs.graft == rhs.graft
				   && lhs.internal == rhs.internal && lhs.nonlive == rhs.nonlive && lhs.all == rhs.all;
		};
	};

	typedef std::list<Run_piece> Piece_list;
	Piece_list *src = new Piece_list[states];
	Piece_list *dst = new Piece_list[states];

	Run *res = new Run(states, starting_state);
	Run &run = *res;
	Run_piece **grafts = new Run_piece *[states];
	for (state_t s = 0; s < states; s++) {
		grafts[s] = new Run_piece(run, s, true, 0);
		for (auto a = conditions.cbegin(); a != conditions.cend(); a++) {
			if (a->u.test(s) && !a->l.test(s)) {
				src[s].emplace_back(run, s);
			}
		}
	}

	const auto fitting_pieces
		= [this](
			  const Run &run, const Run_piece *other, const state_t parent, Run_piece *graft, const Piece_list &src,
			  std::queue<const Run_piece *> &out, const state_t h, bitset_t &tmp) {
			  while (!out.empty()) {
				  out.pop();
			  }
			  if (run.nonempty(graft->state) || (0 != other->height && other->all.test(graft->state))) {
				  if (graft->height >= h) {
					  out.push(graft);
				  }
				  return;
			  }
			  for (auto t = src.cbegin(); t != src.cend(); t++) {
				  if (t->height < h || t->internal.test(parent)) {
					  continue;
				  }
				  if (!other->nonlive.test(parent) && !t->nonlive.test(parent)) {
					  out.push(&*t);
					  continue;
				  }
				  tmp.reset();
				  tmp |= other->internal;
				  tmp |= t->internal;
				  tmp.set(parent);
				  for (auto a = conditions.cbegin(); a != conditions.cend(); a++) {
					  if (a->u.test(parent) && !tmp.intersects(a->l)) {
						  out.push(&*t);
						  break;
					  }
				  }
			  }
		  };

	const auto inv = [](const Run_piece &v) -> bool { return v.invalid; };

	bitset_t tmp{states};
	Run_piece wild_card = Run_piece(run, 0, true);
	std::queue<const Run_piece *> lq;
	std::queue<const Run_piece *> rq;
	for (state_t h = 0; h < states; h++, std::swap(src, dst)) {
		for (state_t s = 0; s < states; s++) {
			if (run.nonempty(starting_state) || run.nonempty(s)) {
				continue;
			}
			for (auto t = transitions[s].cbegin(); t != transitions[s].cend(); t++) {
				if (run.nonempty(starting_state) || run.nonempty(s)) {
					break;
				}
				fitting_pieces(run, &wild_card, s, grafts[t->left], src[t->left], lq, 0, tmp);
				while (!lq.empty()) {
					if (run.nonempty(starting_state) || run.nonempty(s)) {
						break;
					}
					const Run_piece *left = lq.front();
					lq.pop();
					fitting_pieces(run, left, s, grafts[t->right], src[t->right], rq, (left->height == h) ? 0 : h, tmp);
					while (!rq.empty()) {
						if (run.nonempty(s)) {
							break;
						}
						const Run_piece *right = rq.front();
						rq.pop();
						dst[s].emplace_back(s, left, right);
						if (dst[s].back().graft) {
							grafts[s]->height = dst[s].back().height;
							dst[s].pop_back();
						}
					}
				}
			}
		}
		if (run.nonempty(starting_state)) {
			break;
		}
		for (state_t q = 0; q < states; q++) {
			if (run.nonempty(q) && (0 == grafts[q]->height)) {
				grafts[q]->height = h + 1;
			}

			dst[q].sort();
			dst[q].merge(src[q]);
			for (auto t = dst[q].begin(); t != dst[q].end(); t++) {
				if (run.nonempty(q) || (t != dst[q].begin() && Run_piece::similar(*t, *std::prev(t)))) {
					t->invalid = true;
				}
			}
		}
		bool invalidated = false;
		do {
			invalidated = false;
			for (state_t q = 0; q < states; q++) {
				for (auto t = dst[q].begin(); t != dst[q].end(); t++) {
					if (t->invalid_child() && !t->invalid) {
						t->invalid = true;
						invalidated = true;
					}
				}
			}
		} while (invalidated);
		for (state_t q = 0; q < states; q++) {
			dst[q].remove_if(inv);
		}
	}

	if (!run.nonempty(starting_state)) {
		delete res;
		res = nullptr;
	}
	for (state_t s = 0; s < states; s++) {
		delete grafts[s];
	}
	delete[] grafts;
	delete[] src;
	delete[] dst;
	return res;
}

std::ostream &
Rabin_automaton::print_logic_prog_rep(std::ostream &os) const
{
	os << "#const n = " << states << " + 1." << std::endl;
	os << "state(0.." << states - 1 << ")." << std::endl;
	os << "start(" << starting_state << ")." << std::endl;
	for (state_t q = 0; q < states; q++) {
		if (!transitions[q].empty()) {
			for (auto t = transitions[q].cbegin(); t != transitions[q].cend(); t++) {
				os << "transition(" << q << ',' << t->left << ',' << t->right << ").";
				if (transitions[q].cend() != std::next(t)) {
					os << ' ';
				} else {
					os << std::endl;
				}
			}
		}
	}
	return acceptances_print_logic_prog_rep(os);
}

std::ostream &
Rabin_automaton::acceptances_print_logic_prog_rep(std::ostream &os) const
{
	std::list<Acceptance>::size_type idx = 0;
	for (auto a = conditions.cbegin(); a != conditions.cend(); a++, idx++) {
		auto state = a->l.find_first();
		while (a->l.npos != state) {
			os << "l(" << idx << ',' << state << "). ";
			state = a->l.find_next(state);
		}
		os << std::endl;
		state = a->u.find_first();
		while (a->u.npos != state) {
			os << "u(" << idx << ',' << state << "). ";
			state = a->u.find_next(state);
		}
		if (conditions.cend() != std::next(a)) {
			os << std::endl;
		}
	}
	return os;
}

std::ostream &
operator<<(std::ostream &os, const Rabin_automaton &automaton)
{
	os << "states := " << automaton.states << std::endl;
	os << "start := " << automaton.starting_state << std::endl;
	if (automaton.has_transitions) {
		os << "transitions :=" << std::endl;
		for (state_t q = 0; q < automaton.states; q++) {
			for (auto t = automaton.transitions[q].cbegin(); t != automaton.transitions[q].cend(); t++) {
				if (automaton.transitions[q].cbegin() == t) {
					os << '\t';
				}
				os << q << " > " << t->left << ' ' << t->right;
				if (automaton.transitions[q].cend() != std::next(t)) {
					os << ", ";
				} else {
					os << std::endl;
				}
			}
		}
	}
	if (!automaton.conditions.empty()) {
		os << "acceptances :=" << std::endl;
		for (auto a = automaton.conditions.cbegin(); a != automaton.conditions.cend(); a++) {
			os << '\t' << *a;
			if (automaton.conditions.cend() != std::next(a)) {
				os << std::endl;
			}
		}
	}
	return os;
}

std::ostream &
operator<<(std::ostream &os, const Acceptance &a)
{
	auto state = a.l.find_first();
	os << "( ";
	if (a.l.none()) {
		os << "none ";
	} else {
		while (a.l.npos != state) {
			os << state << ' ';
			state = a.l.find_next(state);
		}
	}
	os << ", ";
	if (a.u.none()) {
		return os << "none )";
	}
	state = a.u.find_first();
	while (a.u.npos != state) {
		os << state << ' ';
		state = a.u.find_next(state);
	}
	return os << ')';
}
