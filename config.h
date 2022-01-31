#ifndef CONFIG_H
#define CONFIG_H

struct Config
{
	const char *in;
	const char *graphviz_out;
	const char *lp_out;
	bool overwrite;
	bool graphviz;
	bool lp;
	bool help;
	bool version;
	int max_threads;
};

static struct Config config = {nullptr, "run.gv", "automaton.lp", false, false, false, false, false, 1};

#endif
