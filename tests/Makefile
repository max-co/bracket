CXX = g++
INCLUDE_DIR = ..
export FLAGS = -D_POSIX_C_SOURCE=200809L -O3 -W -Wall -Wextra -std=c++11 -pedantic -pthread
CPPFLAGS = ${FLAGS} -I ${INCLUDE_DIR}

export THREADS = 1

TIME = time
SHUF = shuf
PYTHON3 = python3
CLINGO = clingo

SAT_EXIT = 30
UNSAT_EXIT = 20

BATCH = 40
STATES = $(shell seq 4 3 16)
TRAN = $(shell seq 16 12 64)
ACC = 2_2

B_LS = $(foreach var,$(call enum,$(words $(STATES))),$(BATCH)_ )
S_LS = $(addsuffix _,$(STATES))
T_LS = $(addsuffix _,$(TRAN))
A_LS = $(foreach var,$(call enum,$(words $(STATES))),$(ACC)_bench )

BENCH = $(join $(join $(join $(B_LS),$(S_LS)),$(T_LS)),$(A_LS))

.DELETE_ON_ERROR:
.PRECIOUS: %-automaton.txt %/seeds.txt %_bench/results.csv %-result.txt emptiness/%-automaton.lp

.SECONDEXPANSION:

take = $(word $(1),$(subst _,$(empty) $(empty),$(2)))
enum = $(shell seq -w $(1))
line = $(shell tr '\n' '\t' < $(2) | cut -f $(1))

OBJS = random_automaton.o ../rabin_automaton.o ../run_node.o ../run.o
FORCED = ../version.h ../bracket ../rabin_automaton.o ../run_node.o ../run.o

.PHONY: clean mostlyclean distclean benchmarks tests all
.INTERMEDIATE: time_installed

tests: parser-tests emptiness-tests

parser-tests emptiness-tests: force

%-tests: $$(subst automaton_test.txt,test,$$(wildcard $$*/*-automaton_test.txt))
	@echo "$@: ok"

parser/%-test: parser/%-automaton_test.txt parser/%-expected.txt bracket
	@printf "$(word 1,$(subst -,$(empty) $(empty),$^)): "
	@./bracket -t ${THREADS} parser/$*-automaton_test.txt | diff parser/$*-expected.txt -
	@echo ok

emptiness/%-test: emptiness/%-automaton.lp emptiness/%-automaton_test.txt bracket
	@printf "$(word 1,$(subst -,$(empty) $(empty),$^)): "
	@if grep RUN < emptiness/$*-automaton.lp > /dev/null; \
		then ./bracket -t ${THREADS} emptiness/$*-automaton_test.txt | grep ^NONEMPTY > /dev/null; \
		else ./bracket -t ${THREADS} emptiness/$*-automaton_test.txt | grep ^EMPTY > /dev/null; fi
	@echo ok

emptiness/%-automaton.lp: emptiness/%-automaton_test.txt bracket check.lp find.lp
	./bracket -t ${THREADS} -wL $@ emptiness/$*-automaton_test.txt
	@if grep RUN < $@ > /dev/null; \
		then $(CLINGO) --quiet=2 check.lp $@ || test $$? -eq $(SAT_EXIT) ; \
		else $(CLINGO) --quiet=2 find.lp $@ || test $$? -eq $(UNSAT_EXIT) ; fi

benchmarks: results.csv
	$(PYTHON3) diagrams.py $^

results.csv: $(addsuffix /results.csv,$(BENCH)) $(addsuffix /results.csv,$(wildcard *_bench))
	cat $^ > $@

%_bench/results.csv: $$(foreach var,$$(call enum,$$(call take,1,$$*)),%_bench/$$(var)-result.txt )
	@printf "" > $@
	@$(foreach var,$^,cut -d ' ' -f 2 < $(var) | tr '\n' ',' | cut -d ',' -f 1-6 >> $@ ;)

%-result.txt: %-automaton.txt bracket | time_installed
	@echo "states $(call take,2,$(dir $@))" > $@
	@echo "transitions $(call take,3,$(dir $@))" >> $@
	@echo "acceptances $(call take,4,$(dir $@))" >> $@
	@echo "acc_elems $(call take,5,$(dir $@))" >> $@
	@printf "$(word 1,$(subst -,$(empty) $(empty),$^)): "
	@($(TIME) -p ./bracket -t ${THREADS} $(word 1,$^)) 2>&1 | grep -e EMPTY -e real | sed \
	-e 's/^EMPTY LANGUAGE/nonempty false/' -e 's/^NONEMPTY LANGUAGE/nonempty true/' -e 's/real /time /' >> $@
	@echo done

%-automaton.txt: random_automaton  $$(dir $$@)seeds.txt
	./random_automaton -s $(call take,2,$(dir $@)) -t $(call take,3,$(dir $@)) -a $(call take,4,$(dir $@)) -e $(call take,5,$(dir $@)) -r $(call line,$(notdir $*),$(dir $@)/seeds.txt) > $@

%/seeds.txt: | $$(dir $$@)
	$(SHUF) -i 0-4294967295 -n $(call take,1,$(dir $@)) -o $@

%/:
	mkdir $@

all: tests benchmarks

# cannot use forced ../bracket prerequisite for %-result.txt target
bracket: ../bracket
	cp $^ $@

random_automaton: ${OBJS}
	${CXX} ${CPPFLAGS} ${CXXFLAGS} $^ -o $@

random_automaton.o: ../run.h ../run_node.h ../typedefs.h ../rabin_automaton.h ../version.h random_config.h

$(OBJS): | ../boost

$(FORCED): force

force: ;

../%:
ifeq ($(wildcard lock),lock)
	$(error old lock from previous invocation, please remove with: rmdir lock)
endif
	@until mkdir lock 2>/dev/null ; do sleep 2 ; done
	$(MAKE) -e -C .. $(@F)
	@rmdir lock

time_installed:
	$(TIME) -p true
	touch $@

clean:
	rm -f ${OBJS} random_automaton bracket time_installed
	-rmdir lock

mostlyclean: clean
	rm -fr ../boost
	rm -f *_bench/*-automaton.txt *_bench/*-result.txt *_bench/results.csv results.csv emptiness/*-automaton.lp

distclean: mostlyclean
	rm -fr *_bench
