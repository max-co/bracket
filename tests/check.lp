root(X) :- not parent(_,X), has_state(X,S), start(S).
:- 1 != #count{X : root(X)}.
:- parent(Y,X), parent(Z,X), Y != Z.
:- 2 != #count{Y : parent(X,Y)}, parent(X,_).
:- 1 != #count{Y : graft(X,Y)}, graft(X,_).
:- parent(X,_), previous(_,X).
:- graft(X,_), previous(_,X).
:- graft(X,_), parent(X,_).
:- has_state(X,S), has_state(Y,Sa), has_state(Z,Sb), parent(X,Y), parent(X,Z), Z != Y, not transition(S,Sa,Sb), not transition(S,Sb,Sa).
:- has_state(_,S), not state(S).
:- parent(_,X), not has_state(X,_).

descendant(Y,X) :- parent(X,Y).
descendant(Y,X) :- parent(Z,Y), descendant(Z,X).

:- has_state(X,Sa), has_state(X,Sb), Sa != Sb.

previous(Y,X) :- descendant(X,Y), has_state(Y,S), has_state(X,S), not parent(X,_), not graft(X,_).

:- has_state(X,S), not previous(_,X), not parent(X,_), not graft(X,_).

l_intersects(N,X) :- l(N,S), previous(Y,X), descendant(Z,Y), has_state(Z,S), parent(Z,_).
u_intersects(N,X) :- u(N,S), has_state(X,S), previous(_,X).

accepted(X) :- not l_intersects(N,X), u_intersects(N,X).

:- previous(Y,X), not accepted(X).
