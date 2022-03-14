#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>

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
				res->graft = true;
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
	}; // class Run_piece

	class Find_thread final
	{
	private:
		std::thread thread;
		bool *done;

	public:
		Find_thread(std::thread &&t, bool *d) : thread{std::move(t)}, done{d} {};

		Find_thread(const Find_thread &) = delete;
		Find_thread(Find_thread &&) = delete;
		Find_thread &operator=(const Find_thread &) = delete;
		Find_thread &operator=(Find_thread &&) = delete;

		~Find_thread()
		{
			thread.join();
			delete done;
		};

		bool has_finished() { return *done; }
	}; // class Find_thread

	typedef std::list<Run_piece> Piece_list;

	class Find_context final
	{
	public:
		Run &run;
		state_t parent;
		state_t step;
		bitset_t tmp;
		Run_piece **const grafts;
		const Piece_list *srcs;
		Piece_list *dst;
		std::queue<const Run_piece *> lq;
		std::queue<const Run_piece *> rq;

		Find_context(Run &r, Run_piece **const g)
			: run{r}, parent{0}, step{0}, tmp{r.states}, grafts{g}, srcs{nullptr}, dst{nullptr} {};

		void reset(const state_t q, const state_t h, const Piece_list *s, Piece_list *d)
		{
			parent = q;
			step = h;
			srcs = s;
			dst = d;
		};
	}; // class Find_context

	const auto inv = [](const Run_piece &v) -> bool { return v.invalid; };

	const auto find_run_thread = [this](Find_context &c, bool &done) {
		const auto fitting_pieces
			= [this](
				  const Run &run, const Run_piece *other, const state_t parent, Run_piece *graft, const Piece_list &src,
				  std::queue<const Run_piece *> &out, const state_t h, bitset_t &tmp) {
				  for (; !out.empty(); out.pop()) {
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
			  }; // fitting_pieces
		// start of find_run_thread
		if (c.run.nonempty(c.parent)) {
			done = true;
			return;
		}
		const state_t &s = c.parent;
		const state_t &h = c.step;
		const Run_piece *const wild_card = *c.grafts;
		for (auto t = transitions[s].cbegin(); t != transitions[s].cend(); t++) {
			if (c.run.nonempty(starting_state)) {
				done = true;
				return;
			}
			fitting_pieces(c.run, wild_card, s, c.grafts[t->left], c.srcs[t->left], c.lq, 0, c.tmp);
			for (const Run_piece *left; !c.lq.empty() && !c.run.nonempty(starting_state); c.lq.pop()) {
				left = c.lq.front();
				fitting_pieces(
					c.run, left, s, c.grafts[t->right], c.srcs[t->right], c.rq, (left->height == h) ? 0 : h, c.tmp);
				for (const Run_piece *right; !c.rq.empty() && !c.run.nonempty(starting_state); c.rq.pop()) {
					right = c.rq.front();
					c.dst->emplace_back(s, left, right);
					if (c.dst->back().graft) {
						c.grafts[s]->height = c.dst->back().height;
						c.dst->pop_back();
						done = true;
						return;
					}
				}
			}
		}
		if (!c.run.nonempty(starting_state)) {
			c.dst->sort();
		}
		done = true;
	}; // find_run_thread

	// start of Rabin_automaton::find_run
	if (1 > max_threads) {
		throw std::invalid_argument("invalid max_threads (is less than 1)");
	}
	if (!has_transitions || conditions.empty()) {
		return nullptr;
	}
	Run *res = new Run(states, starting_state);
	Run &run = *res;
	Piece_list *src = new Piece_list[states];
	Piece_list *dst = new Piece_list[states];
	Run_piece **grafts = new Run_piece *[states];
	for (state_t s = 0; s < states; s++) {
		grafts[s] = new Run_piece(run, s, true);
		for (auto a = conditions.cbegin(); a != conditions.cend(); a++) {
			if (a->u.test(s) && !a->l.test(s)) {
				src[s].emplace_back(run, s);
			}
		}
	}

	const state_t max_workers
		= (states < static_cast<unsigned int>(max_threads - 1)) ? states : static_cast<state_t>(max_threads - 1);
	std::queue<Find_context *> ctx_pool;
	{
		state_t i = 0;
		do {
			ctx_pool.push(new Find_context(run, grafts));
		} while (++i < max_workers);
	}
	std::list<std::pair<Find_thread, Find_context *>> workers;
	for (state_t h = 0; h < states; h++, std::swap(src, dst)) {

		if (1 == max_threads) {
			Find_context *c = ctx_pool.front();
			bool d;
			for (state_t s = 0; s < states; s++) {
				c->reset(s, h, src, &dst[s]);
				find_run_thread(*c, d);
				if (run.nonempty(starting_state)) {
					break;
				}
			}

		} else {

			state_t s = 0;
			for (; s < max_workers; s++) {
				bool *done = new bool{false};
				Find_context *c = ctx_pool.front();
				ctx_pool.pop();
				c->reset(s, h, src, &dst[s]);
				workers.emplace_back(
					std::piecewise_construct,
					std::forward_as_tuple(std::thread(find_run_thread, std::ref(*c), std::ref(*done)), done),
					std::forward_as_tuple(c));
			}
			for (;;) {
				if (run.nonempty(starting_state) || workers.empty()) {
					for (auto t = workers.begin(); t != workers.end(); t++) {
						ctx_pool.push(t->second);
						t = workers.erase(t);
						t--;
					}
					break;
				}
				for (auto t = workers.begin(); t != workers.end(); t++) {
					if (t->first.has_finished()) {
						ctx_pool.push(t->second);
						t = workers.erase(t);
						t--;
						for (; s < states && (run.nonempty(starting_state) || run.nonempty(s)); s++) {
						}
						if (s < states) {
							bool *done = new bool{false};
							Find_context *c = ctx_pool.front();
							ctx_pool.pop();
							c->reset(s, h, src, &dst[s]);
							workers.emplace_back(
								std::piecewise_construct,
								std::forward_as_tuple(
									std::thread(find_run_thread, std::ref(*c), std::ref(*done)), done),
								std::forward_as_tuple(c));
							s++;
						}
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
		} // found new Run_pieces

		if (run.nonempty(starting_state)) {
			break;
		}
		for (state_t q = 0; q < states; q++) {
			if (run.nonempty(q) && (0 == grafts[q]->height)) {
				grafts[q]->height = h + 1;
			}

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
	for (; !ctx_pool.empty(); ctx_pool.pop()) {
		delete ctx_pool.front();
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
