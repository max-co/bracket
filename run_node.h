#ifndef RUN_NODE_H
#define RUN_NODE_H

#include <vector>

#include "typedefs.h"

class Run_node;

class Non_root_run_node
{
};

class Run_node final
{
public:
	state_t state;
	Run_node *parent;
	Run_node *left;
	Run_node *right;
	bool graft;

	Run_node(const state_t q) : state{q}, parent{nullptr}, left{nullptr}, right{nullptr}, graft{false} {};
	Run_node(const state_t q, Run_node *const p) : state{q}, parent{p}, left{nullptr}, right{nullptr}, graft{false} {};
	Run_node(const Run_node &, const bool = false, std::vector<state_t> *const = nullptr);
	Run_node(Run_node &&);
	~Run_node();
	Run_node &operator=(const Run_node &) = delete;
	Run_node &operator=(Run_node &&) = delete;

	bool is_root() const { return nullptr == parent; };
	bool is_left() const { return nullptr != parent && this == parent->left; };
	bool is_right() const { return nullptr != parent && this == parent->right; };
	const Run_node *root() const;
	Run_node *root();
	Run_node *clone() const;

private:
	static Run_node *copy_constr_aux(const Run_node &, std::vector<state_t> *const, Run_node *const);
};

#endif
