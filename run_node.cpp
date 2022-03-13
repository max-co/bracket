#include <stack>

#include "run_node.h"

Run_node::Run_node(const Run_node &arg, const bool desc_only, std::vector<state_t> *const frontier)
	: state{arg.state}, parent{nullptr}, left{nullptr}, right{nullptr}, graft{arg.graft}
{
	if (!desc_only && !arg.is_root()) {
		throw Non_root_run_node();
	}
	if (nullptr != arg.left) {
		left = copy_constr_aux(*arg.left, frontier, this);
		right = copy_constr_aux(*arg.right, frontier, this);
		return;
	}
	if (nullptr != frontier) {
		frontier->push_back(arg.state);
	}
}

Run_node::Run_node(Run_node &&arg)
	: state{arg.state}, parent{arg.parent}, left{arg.left}, right{arg.right}, graft{arg.graft}
{
	if (arg.is_left()) {
		arg.parent->left = this;
	} else if (arg.is_right()) {
		arg.parent->right = this;
	}
	if (nullptr != arg.left) {
		arg.left->parent = this;
	}
	if (nullptr != arg.right) {
		arg.right->parent = this;
	}
	arg.parent = arg.left = arg.right = nullptr;
	arg.graft = false;
}

Run_node *
Run_node::copy_constr_aux(const Run_node &arg, std::vector<state_t> *const frontier, Run_node *const p)
{
	Run_node *const res = new Run_node(arg.state, p);
	res->graft = arg.graft;
	if (nullptr != arg.left) {
		res->left = copy_constr_aux(*arg.left, frontier, res);
		res->right = copy_constr_aux(*arg.right, frontier, res);
		return res;
	}
	if (nullptr != frontier) {
		frontier->push_back(arg.state);
	}
	return res;
}

Run_node::~Run_node()
{
	delete left;
	delete right;
}

Run_node *
Run_node::clone() const
{
	std::stack<const Run_node *> stack;
	stack.push(this);
	while (!stack.top()->is_root()) {
		stack.push(stack.top()->parent);
	}
	Run_node *copy = new Run_node(*stack.top());
	stack.pop();
	while (!stack.empty()) {
		if (stack.top()->is_left()) {
			copy = copy->left;
		} else {
			copy = copy->right;
		}
		stack.pop();
	}
	return copy;
}

const Run_node *
Run_node::root() const
{
	const Run_node *ancestor = this;
	while (!ancestor->is_root()) {
		ancestor = ancestor->parent;
	}
	return ancestor;
}

Run_node *
Run_node::root()
{
	return const_cast<Run_node *>(static_cast<const Run_node *>(this)->root());
}
