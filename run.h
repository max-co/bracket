#ifndef RUN_H
#define RUN_H

#include <mutex>
#include <ostream>
#include <unordered_map>
#include <unordered_set>

#include "run_node.h"

class Run final
{
public:
	const state_t states;
	const state_t start;

private:
	const Run_node **grafts;
	const Run_node **dependencies;
	std::unordered_set<const Run_node *> roots;
	std::mutex *lock;

public:
	Run(const state_t, const state_t);
	Run(const Run &) = delete;
	Run(Run &&) = delete;
	~Run();
	Run &operator=(const Run &) = delete;
	Run &operator=(Run &&) = delete;

	bool nonempty(const state_t q) const { return nullptr != grafts[q]; };
	void save_subruns(const Run_node *const);

	std::ostream &print_logic_prog_rep(std::ostream &) const;

private:
	void save_subruns_aux(const Run_node *const, const Run_node *const);
	std::ostream &print_logic_prog_rep_aux(
		std::ostream &, const Run_node *const, runid_t &, std::unordered_map<const Run_node *, runid_t> &) const;
	std::ostream &out_aux(
		std::ostream &,
		const Run_node *const,
		const runid_t,
		runid_t &,
		std::unordered_map<const Run_node *, runid_t> &) const;

	friend std::ostream &operator<<(std::ostream &, const Run &);
};
std::ostream &operator<<(std::ostream &, const Run_node &);

#endif
