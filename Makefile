CXX = g++
INCLUDE_DIR = .
export FLAGS = -D_POSIX_C_SOURCE=200809L -O3 -W -Wall -Wextra -std=c++11 -pedantic -pthread
CPPFLAGS = ${FLAGS} -I ${INCLUDE_DIR}

CURL = curl
CKSUM = cksum
UNZIP = unzip
DOT = dot

CURLFLAGS = -\# --proto =https --max-redirs 5 --proto-redir =https -L
export THREADS = 1

.DELETE_ON_ERROR:
.NOTPARALLEL:
.PHONY: clean mostlyclean distclean img doc tests
.INTERMEDIATE: CocoSourcesCPP.zip boost_1_75_0.tar.gz Parser_incomplete.cpp Parser_unformatted.cpp
.SECONDARY: CocoSourcesCPP CocoSourcesCPP_license.txt

WITH_HEADER = Parser.o Scanner.o rabin_automaton.o run.o run_node.o search_context.o
OBJS = ${WITH_HEADER} file_descriptor.o bracket.o

bracket: ${OBJS}
	${CXX} ${CPPFLAGS} ${CXXFLAGS} ${OBJS} -o $@

img: run.png

$(foreach var,$(WITH_HEADER),$(eval $(var): $(basename $(var)).h))

bracket.o: bracket.cpp Scanner.h Parser.h help.h config.h version.h rabin_automaton.h run.h run_node.h typedefs.h search_context.h
Parser.o: Scanner.h rabin_automaton.h run.h run_node.h typedefs.h search_context.h
rabin_automaton.o: run.h run_node.h typedefs.h search_context.h
run.o: run_node.h typedefs.h
run_node.o: typedefs.h
search_context.o: typedefs.h

$(OBJS): boost

Parser.cpp: Parser_unformatted.cpp
	unexpand -t 3 $^ > $@ || cp $^ $@

Parser_unformatted.cpp: Parser_incomplete.cpp Parser_sed.txt
	sed -f Parser_sed.txt Parser_incomplete.cpp > $@

Scanner.h Scanner.cpp Parser.h Parser_incomplete.cpp: rabin.atg cococpp Scanner.frame Parser.frame
	./cococpp -namespace RabinParser rabin.atg
	mv Parser.cpp Parser_incomplete.cpp

run.png: run.gv
	$(DOT) -T png run.gv -o $@

run.gv: bracket tests/emptiness/01-automaton_test.txt
	./bracket -gw -t ${THREADS} tests/emptiness/01-automaton_test.txt

version.h: .git/HEAD .git/index LICENSE
	@echo "#ifndef VERSION_H" > $@
	@echo "#define VERSION_H" >> $@
	@echo "#define VERSION \"$(shell git describe --always HEAD)\"" >> $@
	@echo "constexpr const char *license = R\"(" >> $@
	@cat LICENSE >> $@
	@echo ")\";" >> $@
	@echo "#endif" >> $@
	@sed '3q;d' $@ | cut -d ' ' -f 2,3

cococpp: | CocoSourcesCPP
	$(MAKE) -C $|
	mv $|/Coco $@

%.frame: | CocoSourcesCPP
	cp $|/$@ .

CocoSourcesCPP: CocoSourcesCPP.zip
	test "566047553 101288 $^" = "$(shell $(CKSUM) $^)"
	mkdir -p $@
	-mv CocoSourcesCPP_license.txt $@/LICENSE_gpl-3.0.txt
	$(UNZIP) -d $@ $^

CocoSourcesCPP.zip: CocoSourcesCPP_license.txt
	test "2501997530 35149 $^" = "$(shell $(CKSUM) $^)"
	$(CURL) ${CURLFLAGS} 'https://ssw.jku.at/Research/Projects/Coco/CPP/CocoSourcesCPP.zip' -o $@

CocoSourcesCPP_license.txt:
	$(CURL) ${CURLFLAGS} 'https://www.gnu.org/licenses/gpl-3.0.txt' -o $@

boost file_descriptor.cpp: | boost_1_75_0.tar.gz
	test "1370242273 143817536 $|" = "$(shell $(CKSUM) $|)"
	tar -x -f $| boost_1_75_0/LICENSE_1_0.txt boost_1_75_0/boost boost_1_75_0/libs/iostreams/src/file_descriptor.cpp
	mkdir -p boost
	cp boost_1_75_0/LICENSE_1_0.txt boost
	mv boost_1_75_0/boost/* boost
	mv boost_1_75_0/libs/iostreams/src/file_descriptor.cpp .
	rm -r boost_1_75_0

boost_1_75_0.tar.gz:
	$(CURL) ${CURLFLAGS} 'https://boostorg.jfrog.io/ui/api/v1/download?repoKey=main&path=release%252F1.75.0%252Fsource%252Fboost_1_75_0.tar.gz' -o $@

tests:
	$(MAKE) -e -C tests

doc: Doxyfile version.h
	( cat Doxyfile ; \
	echo "PROJECT_NAME=Bracket" ; \
	echo "OUTPUT_DIRECTORY=doc" ; \
	sed version.h -e 's/#define VERSION "/PROJECT_NUMBER=/' -e 's/"//' ) | doxygen -

Doxyfile:
	doxygen -g

clean:
	rm -f ${OBJS} bracket run.gv run.png version.h
	rm -f CocoSourcesCPP.zip boost_1_75_0.tar.gz Parser_incomplete.cpp Parser_unformatted.cpp CocoSourcesCPP_license.txt
	rm -fr boost_1_75_0
	$(MAKE) -e -C tests $@

mostlyclean: clean
	rm -f Scanner.h Scanner.cpp Parser.h Parser.cpp *.old

distclean: mostlyclean
	rm -fr CocoSourcesCPP boost doc/html doc/latex
	rm -f Scanner.frame Parser.frame cococpp file_descriptor.cpp run.gv run.png Doxyfile
