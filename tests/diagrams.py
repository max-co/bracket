#!/usr/bin/python3

import matplotlib.pyplot as plt
import csv
import sys

class Test_params:
    def __init__(self, s, t, a, e):
        self.states = s
        self.transitions = t
        self.acceptances = a
        self.acc_elems = e

class Result(Test_params):
    def __init__(self, s, t, a, e, o, d):
        super().__init__(s, t, a, e)
        self.nonempty = o
        self.time = d

class Result_collection(Test_params):
    def __init__(self, s, t, a, e):
        super().__init__(s, t, a, e)
        self.size_nonempty = 0
        self.size_empty = 0
        self.time_nonempty = 0.0
        self.time_empty = 0.0

    def averages(self):
        n = -1.0
        e = -1.0
        a = -1.0
        if self.size_nonempty > 0:
            n = self.time_nonempty / self.size_nonempty
        if self.size_empty > 0:
            e = self.time_empty / self.size_empty
            if self.size_nonempty > 0:
                a = (self.time_nonempty + self.time_empty) / (self.size_nonempty + self.size_empty)
        return n, e, a

    def add(self, r):
        if r.nonempty:
            self.time_nonempty += r.time
            self.size_nonempty += 1
        else:
            self.time_empty += r.time
            self.size_empty += 1

collections = {}
with open(sys.argv[1], newline='') as csvfile:
    reader = csv.reader(csvfile)
    for r in reader:
        nonempty = False
        if r[4] == "true":
            nonempty = True
        e = Result(int(r[0]), int(r[1]), int(r[2]), int(r[3]), nonempty, float(r[5]))
        if not e.states in collections:
            collections[e.states] = Result_collection(e.states, e.transitions, e.acceptances, e.acc_elems)
        collections[e.states].add(e)

nonempty = [[],[]]
empty = [[],[]]
average = [[],[]]
for k in sorted(collections.keys()):
    c = collections[k]
    n, e, a = c.averages()
    if n > 0:
        nonempty[0].append(c.states)
        nonempty[1].append(n)
    if e > 0:
        empty[0].append(c.states)
        empty[1].append(e)
    if a > 0:
        average[0].append(c.states)
        average[1].append(a)

print("{}\n\n{}\n\n{}".format(nonempty[1], average[1], empty[1]))

plt.ylabel('seconds')
plt.xlabel('states')
plt.plot(nonempty[0], nonempty[1], label="nonempty", c='g')
plt.plot(average[0], average[1], label="average", c='orange')
plt.plot(empty[0], empty[1], label="empty", c='r')

plt.legend()
plt.show()
