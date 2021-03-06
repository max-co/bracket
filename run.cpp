#include "run.h"

#define PNODE(h, i) << h << i

Run::Run(const state_t state_num, const state_t start) : states{state_num}, start{start}
{
	grafts = new const Run_node *[states]();
	dependencies = new const Run_node *[states]();
	roots.reserve(states);
	lock = new std::mutex;
}

Run::~Run()
{
	for (auto t = roots.cbegin(); t != roots.cend(); t++) {
		delete *t;
	}
	delete[] grafts;
	delete[] dependencies;
	delete lock;
}

void
Run::save_subruns(const Run_node *const n)
{
	const std::lock_guard<std::mutex> l(*lock);
	save_subruns_aux(n, n);
	if (roots.find(n) == roots.end()) {
		delete n;
	}
}

void
Run::save_subruns_aux(const Run_node *const n, const Run_node *const d)
{
	if (nullptr == n->left) {
		return;
	}
	if (nullptr == grafts[n->state]) {
		grafts[n->state] = n;
		dependencies[n->state] = d;
		roots.insert(d);
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
	if (root != run.dependencies[run.start]) {
		Run_node r(run.start);
		r.graft = true;
		root = &r;
	}
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
	if (0 == id) {
		os << "    r" << id << " [label = \"" << node->state << "\", shape = Mcircle]";
	} else {
		os << "    r" << id << " [label = \"" << node->state << "\"]";
	}
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
	if (node->graft) {
		os << std::endl;
		auto t = id_map.find(grafts[node->state]);
		if (id_map.end() == t) { // graft not yet printed
			out_aux(os, dependencies[node->state], free_id++, free_id, id_map);
			os << std::endl;
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
	if (root != dependencies[start]) {
		Run_node r(start);
		r.graft = true;
		root = &r;
	}
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
	if (node->graft) {
		auto t = id_map.find(grafts[node->state]);
		if (id_map.end() == t) { // graft not yet printed
			os << std::endl;
			print_logic_prog_rep_aux(os, dependencies[node->state], free_id, id_map);
			os << std::endl;
			t = id_map.find(grafts[node->state]);
		}
		os << "graft(" << id << ',' << t->second << ").";
	}
	return os;
}
