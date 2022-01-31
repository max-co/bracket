#include <future>
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
	if (1 > max_threads) {
		throw std::invalid_argument("invalid max_threads (is less than 1)");
	}
	if (!has_transitions || conditions.empty()) {
		return nullptr;
	}
	int curr_threads = 0;
	std::recursive_mutex ctx_lock;
	Search_context *const context = new Search_context(states, max_threads, curr_threads, ctx_lock);
	Run *res = new Run(states, starting_state);
	Run_node root(starting_state);
	if (find_run_aux(*context, *res, &root)) {
		delete context;
		return res;
	}
	delete context;
	delete res;
	return nullptr;
}

bool
Rabin_automaton::find_run_aux(Search_context &ctx, Run &run, Run_node *node) const
{
	if (run.empty(node->state)) {
		return false;
	}

	const auto finalize = [](Run_node *const n) {
		delete n->left;
		delete n->right;
		n->left = nullptr;
		n->right = nullptr;
	};

	if (run.nonempty(node->state)) {
		run.frontier(node->state, ctx.frontier);
		finalize(node);
		node->live = true;
		return true;
	}
	if (ctx.ancestors.test_set(node->state)) {
		if (is_accepted(node, ctx.in_tmp)) {
			finalize(node);
			node->live = false;
			ctx.frontier.push_back(node->state);
			return true;
		}
		return false;
	}
	const auto fs = ctx.frontier.size();
	std::unordered_map<state_t, Run_node *> mem;

	const auto del = [](std::unordered_map<state_t, Run_node *> m) {
		for (auto r = m.cbegin(); r != m.cend(); r++) {
			delete r->second;
		}
	};

	for (auto t = transitions[node->state].begin(); t != transitions[node->state].end(); t++) {
		const auto tl = mem.find(t->left);
		const auto tr = mem.find(t->right);
		if (run.empty(t->right) || (tr != mem.end() && nullptr == tr->second) || run.empty(t->left)
			|| (tl != mem.end() && nullptr == tl->second)) {
			continue;
		}

		const auto initialize = [](Run_node *&dst, state_t q, Run_node *const p) {
			if (nullptr != dst) {
				dst->state = q;
			} else {
				dst = new Run_node(q, p);
			}
		};

		initialize(node->left, t->left, node);
		initialize(node->right, t->right, node);
		const bool l_mem = tl != mem.end() && nullptr != tl->second;
		const bool r_mem = tr != mem.end() && nullptr != tr->second;
		bool par = false;
		if (!l_mem && !r_mem && t->left != t->right && !ctx.ancestors.test(t->left) && !ctx.ancestors.test(t->right)) {
			Search_context *lctx = &ctx;
			Search_context *rctx = lctx->split();
			if ((par = nullptr != rctx)) {
				std::future<bool> fl = std::async(
					std::launch::async, &Rabin_automaton::find_run_aux, this, std::ref(*lctx), std::ref(run),
					node->left);
				std::future<bool> fr = std::async(
					std::launch::async, &Rabin_automaton::find_run_aux, this, std::ref(*rctx), std::ref(run),
					node->right);
				const bool l_ok = fl.get();
				const bool r_ok = fr.get();
				if (!l_ok || !r_ok) {
					mem[t->left] = l_ok ? node->left : nullptr;
					node->left = l_ok ? nullptr : node->left;
					mem[t->right] = r_ok ? node->right : nullptr;
					node->right = r_ok ? nullptr : node->right;
					delete rctx;
					lctx->frontier.resize(fs);
					continue;
				}
				*lctx += *rctx;
				delete rctx;
			}
		}
		if (!par) {

			const auto copy
				= [](Run_node *&dst, const Run_node &src, std::vector<state_t> *const f, Run_node *const p) {
					  delete dst;
					  dst = new Run_node(src, true, f);
					  dst->parent = p;
				  };

			if (l_mem) {
				copy(node->left, *tl->second, &ctx.frontier, node);
			} else if (find_run_aux(ctx, run, node->left)) {
				mem[t->left] = node->left;
			} else {
				mem[t->left] = nullptr;
				continue;
			}
			if (r_mem) {
				copy(node->right, *tr->second, &ctx.frontier, node);
			} else if (t->left == t->right) {
				copy(node->right, *node->left, &ctx.frontier, node);
			} else if (!find_run_aux(ctx, run, node->right)) {
				mem[t->right] = nullptr;
				ctx.frontier.resize(fs);
				if (!l_mem) {
					node->left = nullptr;
				}
				continue;
			}
		}
		ctx.ancestors.reset(node->state);
		bool contained = true;
		auto l = ctx.frontier.size();
		for (std::vector<state_t>::const_reverse_iterator t = ctx.frontier.crbegin(); l > fs; t++, l--) {
			if (ctx.ancestors.test(*t)) {
				contained = false;
				break;
			}
		}
		if (contained) {
			Run_node *to_save = new Run_node(std::move(*node));
			node->state = to_save->state;
			if (to_save->is_left()) {
				to_save->parent->left = node;
			} else if (to_save->is_right()) {
				to_save->parent->right = node;
			}
			node->parent = to_save->parent;
			to_save->parent = nullptr;
			run.save_subruns(to_save);
			finalize(node);
			node->live = true;
		}
		if (!l_mem) { // node->left == mem[t->left]
			mem.erase(t->left);
		}
		del(mem);
		return true;
	}
	if (!node->live) {
		run.set_empty(node->state);
	}
	ctx.ancestors.reset(node->state);
	del(mem);
	return false;
}

bool
Rabin_automaton::is_accepted(Run_node *const leaf, bitset_t &in_tmp) const
{
	in_tmp.reset();
	Run_node *ancestor = leaf;
	do {
		in_tmp.set(ancestor->state);
		ancestor->live = true;

		ancestor = ancestor->parent;
	} while (leaf->state != ancestor->state);
	ancestor->live = true;

	for (auto a = conditions.cbegin(); a != conditions.cend(); a++) {
		if (in_tmp.intersects(a->l)) {
			continue;
		}
		if (in_tmp.intersects(a->u)) {
			return true;
		}
	}
	return false;
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
