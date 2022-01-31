#include "search_context.h"

Search_context::Search_context(const state_t state_num, const int t_max, int &t_curr, std::recursive_mutex &mutex)
	: states{state_num}, ancestors{state_num}, in_tmp{state_num}, max_threads{t_max}, curr_threads{t_curr}, lock{mutex}
{
	const std::lock_guard<std::recursive_mutex> l(lock);
	curr_threads++;
}

Search_context::~Search_context()
{
	const std::lock_guard<std::recursive_mutex> l(lock);
	curr_threads--;
}

Search_context *
Search_context::split()
{
	const std::lock_guard<std::recursive_mutex> l(lock);
	if (max_threads <= curr_threads) {
		return nullptr;
	}
	Search_context *const res = new Search_context(states, max_threads, curr_threads, lock);
	res->ancestors = ancestors;
	return res;
}

Search_context &
Search_context::operator+=(const Search_context &rhs)
{
	frontier.insert(frontier.end(), rhs.frontier.begin(), rhs.frontier.end());
	return *this;
}
