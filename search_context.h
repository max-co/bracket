#ifndef SEARCH_CONTEXT_H
#define SEARCH_CONTEXT_H

#include <mutex>
#include <vector>

#include "typedefs.h"

class Search_context final
{
public:
	const state_t states;
	bitset_t ancestors;
	bitset_t in_tmp;
	std::vector<state_t> frontier;

private:
	const int max_threads;
	int &curr_threads;
	std::recursive_mutex &lock;

public:
	Search_context(const state_t, const int, int &, std::recursive_mutex &);
	Search_context(const Search_context &) = delete;
	Search_context(Search_context &&) = delete;
	~Search_context();
	Search_context &operator=(const Search_context &) = delete;
	Search_context &operator=(Search_context &&) = delete;

	Search_context *split();
	Search_context &operator+=(const Search_context &rhs);
};

#endif
