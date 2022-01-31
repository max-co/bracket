#include <climits>
#include <cstdlib>
#include <cwchar>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Parser.h"
#include "boost/iostreams/device/file_descriptor.hpp"
#include "boost/iostreams/stream.hpp"
#include "config.h"
#include "help.h"

using namespace RabinParser;
namespace ios = boost::iostreams;

extern int optind;
extern char *optarg;

constexpr const char *run_head = "%%------------------------------------RUN-------------------------------------\n";
constexpr const char *aut_head = "%%---------------------------------AUTOMATON----------------------------------\n";

static Rabin_automaton *parse();
static int out_fd(const char *, const bool);

int
main(int argc, char *argv[])
{
	for (int i = 0; i < argc; i++) {
		if (nullptr == argv[i]) {
			std::cout << usage;
			return EXIT_FAILURE;
		}
	}
	{
		int op = 0;
		while ((op = getopt(argc, argv, "i:o:L:t:wglhV")) != -1) {
			switch (op) {
				case 'i':
					config.in = optarg;
					break;
				case 'o':
					config.graphviz_out = optarg;
					config.graphviz = true;
					break;
				case 'L':
					config.lp_out = optarg;
					config.lp = true;
					break;
				case 't':
					errno = 0;
					{
						unsigned long tmp = strtoul(optarg, nullptr, 10);
						if (errno || INT_MAX < tmp || 1 > tmp) {
							std::cout << usage;
							return EXIT_FAILURE;
						}
						config.max_threads = static_cast<int>(tmp);
					}
					break;
				case 'w':
					config.overwrite = true;
					break;
				case 'g':
					config.graphviz = true;
					break;
				case 'l':
					config.lp = true;
					break;
				case 'h':
					config.help = true;
					break;
				case 'V':
					config.version = true;
					break;
				default:
					std::cout << usage;
					return EXIT_FAILURE;
			}
		}
	}
	if (config.help) {
		std::cout << usage;
		return EXIT_SUCCESS;
	}
	if (config.version) {
		std::cout << PROG_VERSION << std::endl;
		return EXIT_SUCCESS;
	}
	if (optind < argc) {
		config.in = argv[optind];
	}
	const Rabin_automaton *const automaton = parse();
	if (nullptr == automaton) {
		return EXIT_FAILURE;
	}
	ios::stream<ios::file_descriptor> os;
	if (config.lp) {
		const int fd = out_fd(config.lp_out, config.overwrite);
		if (-1 < fd) {
			os.open(ios::file_descriptor(fd, ios::close_handle));
			os << aut_head;
			automaton->print_logic_prog_rep(os);
			os << std::endl;
		}
	}
	std::cout << "Searching for an accepted regular run..." << std::endl;
	const Run *run = nullptr;
	if (nullptr != (run = automaton->find_run(config.max_threads))) {
		std::cout << "NONEMPTY LANGUAGE" << std::endl;
		if (os.is_open()) {
			os << std::endl << run_head;
			run->print_logic_prog_rep(os);
			os << std::endl;
			os.close();
		}
		if (config.graphviz) {
			const int fd = out_fd(config.graphviz_out, config.overwrite);
			if (-1 < fd) {
				os.open(ios::file_descriptor(fd, ios::close_handle));
				os << *run << std::endl;
				os.close();
			}
		}
		delete run;
	} else {
		std::cout << "EMPTY LANGUAGE" << std::endl;
		if (os.is_open()) {
			os.close();
		}
	}
	delete automaton;
	return EXIT_SUCCESS;
}

static Rabin_automaton *
parse()
{
	Scanner *scanner = nullptr;
	if (nullptr != config.in) {
		wchar_t *fileName = coco_string_create(config.in);
		scanner = new Scanner(fileName);
		coco_string_delete(fileName);
	} else {
		scanner = new Scanner(stdin);
	}
	Parser *const parser = new Parser(scanner);
	try {
		parser->Parse();
	} catch (const Illegal_state_set &e) {
	}
	Rabin_automaton *const res = parser->automaton;
	delete parser;
	delete scanner;
	return res;
}

static int
out_fd(const char *path, const bool overwrite)
{
	int fd = -1;
	errno = 0;
	if (overwrite) {
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 00664);
	} else {
		fd = open(path, O_EXCL | O_WRONLY | O_CREAT, 00664);
	}
	if (-1 == fd) {
		if (!overwrite && EEXIST == errno) {
			std::cerr << "file " << path << " already exists, not overwriting" << std::endl;
		} else {
			std::cerr << "could not open file " << path << ": " << strerror(errno) << std::endl;
		}
	}
	return fd;
}
