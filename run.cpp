#include "run.h"

#define PNODE(h, i) << h << i

Run::Run(const state_t state_num, const state_t start) : states{state_num}, start{start}
{
	lock = new std::mutex;
	grafts = new const Run_node *[states];
	nogoods = new bool[states];
	dependencies = new state_t[states];
	for (state_t i = 0; i < states; i++) {
		grafts[i] = nullptr;
		nogoods[i] = false;
		dependencies[i] = STATE_MAX;
	}
}

Run::~Run()
{
	for (state_t i = 0; i < states; i++) {
		if (STATE_MAX == dependencies[i]) {
			delete grafts[i];
		}
	}
	delete[] grafts;
	delete[] nogoods;
	delete[] dependencies;
	delete lock;
}

void
Run::frontier(const state_t s, std::vector<state_t> &f) const
{
	const std::lock_guard<std::mutex> l(*lock);
	frontier_aux(grafts[s], f);
}

void
Run::frontier_aux(const Run_node *const n, std::vector<state_t> &f) const
{
	if (nullptr != n->left) {
		frontier_aux(n->left, f);
		frontier_aux(n->right, f);
		return;
	}
	if (n->live) {
		frontier_aux(grafts[n->state], f);
		return;
	}
	f.push_back(n->state);
}

void
Run::save_subruns(const Run_node *const n)
{
	const std::lock_guard<std::mutex> l(*lock);
	if (nullptr != grafts[n->state]) {
		return;
	}
	grafts[n->state] = n;
	save_subruns_aux(n->left, n->state);
	save_subruns_aux(n->right, n->state);
}

void
Run::save_subruns_aux(const Run_node *const n, const state_t d)
{
	if (nullptr == n->left) {
		return;
	}
	if (nullptr == grafts[n->state]) {
		grafts[n->state] = n;
		dependencies[n->state] = d;
	}
	save_subruns_aux(n->left, d);
	save_subruns_aux(n->right, d);
}

std::ostream &
operator<<(std::ostream &os, const Run &run)
{
	const std::lock_guard<std::mutex> l(*run.lock);
	runid_t free_id = 1;
	const Run_node *root = run.grafts[run.start];
	std::unordered_map<const Run_node *, runid_t> id_map;
	os << "digraph {" << std::endl;
	os << "    node [shape = circle]" << std::endl;
	run.out_aux(os, root, 0, free_id, id_map);
	return os << std::endl << '}';
}

std::ostream &
Run::out_aux(
	std::ostream &os,
	const Run_node *node,
	const runid_t id,
	runid_t &free_id,
	std::unordered_map<const Run_node *, runid_t> &id_map) const
{
	id_map[node] = id;
	os << "    r" << id << " [label = \"" << node->state << "\"]";
	if (nullptr != node->left) {
		const runid_t left_id = free_id++;
		const runid_t right_id = free_id++;
		// for node alignment
		os << std::endl;
		os PNODE("                        {rank = same r", left_id) PNODE(" -> i", id) PNODE(" -> r", right_id)
			<< " [style=invis]}" << std::endl;
		os PNODE("                        i", id) << " [label=\"\",width=.1,style=invis]" << std::endl;
		os PNODE("                        r", id) PNODE(" -> i", id) << " [style=invis]" << std::endl;

		os PNODE("    r", id) PNODE(" -> { r", left_id) PNODE(" r", right_id) << " }" << std::endl;
		out_aux(os, node->left, left_id, free_id, id_map) << std::endl;
		out_aux(os, node->right, right_id, free_id, id_map);
		return os;
	}
	if (node->live) {
		os << std::endl;
		auto t = id_map.find(grafts[node->state]);
		if (id_map.end() == t) { // graft not yet printed
			if (STATE_MAX == dependencies[node->state]) {
				out_aux(os, grafts[node->state], free_id++, free_id, id_map);
				os << std::endl;
			} else {
				out_aux(os, grafts[dependencies[node->state]], free_id++, free_id, id_map);
				os << std::endl;
			}
			t = id_map.find(grafts[node->state]);
		}
		os PNODE("    r", id) PNODE(" -> r", t->second) << " [style=\"dotted\"]";
	}
	return os;
}

std::ostream &
Run::print_logic_prog_rep(std::ostream &os) const
{
	const std::lock_guard<std::mutex> l(*lock);
	runid_t free_id = 0;
	const Run_node *root = grafts[start];
	std::unordered_map<const Run_node *, runid_t> id_map;
	print_logic_prog_rep_aux(os, root, free_id, id_map);
	return os;
}

std::ostream &
Run::print_logic_prog_rep_aux(
	std::ostream &os,
	const Run_node *node,
	runid_t &free_id,
	std::unordered_map<const Run_node *, runid_t> &id_map) const
{
	const runid_t id = free_id++;
	id_map[node] = id;
	os << "has_state(" << id << ',' << node->state << "). ";
	if (nullptr != node->left) {
		os << "parent(" << id << ',' << free_id << ")." << std::endl;
		print_logic_prog_rep_aux(os, node->left, free_id, id_map) << std::endl;
		os << "parent(" << id << ',' << free_id << ")." << std::endl;
		print_logic_prog_rep_aux(os, node->right, free_id, id_map);
		return os;
	}
	if (node->live) {
		auto t = id_map.find(grafts[node->state]);
		if (id_map.end() == t) { // graft not yet printed
			os << std::endl;
			if (STATE_MAX == dependencies[node->state]) {
				print_logic_prog_rep_aux(os, grafts[node->state], free_id, id_map);
				os << std::endl;
			} else {
				print_logic_prog_rep_aux(os, grafts[dependencies[node->state]], free_id, id_map);
				os << std::endl;
			}
			t = id_map.find(grafts[node->state]);
		}
		os << "graft(" << id << ',' << t->second << ").";
	}
	return os;
}
