node(0..2**n-2).

depth(0,0).

depth(X,D+1) :- parent(Y,X), depth(Y,D).

parent(X,X+1) :- node(X), depth(X,D), D <= n - 2, node(X+1).

parent(X,X+ 1 + 2**(n-D)-1) :- node(X + 1 + 2**(n-D)-1), parent(X,X+1), depth(X+1,D).

descendant(Y,X) :- parent(X,Y).
descendant(Y,X) :- parent(Z,Y), descendant(Z,X).

has_state(0,S) :- start(S).

1 { chosen(X,Sa,Sb) : transition(S,Sa,Sb) } 1 :- has_state(X,S), not previous(_,X).

has_state(X+1,Sa) :- parent(X,X+1), chosen(X,Sa,_).

has_state(Y,Sb) :- parent(X,Y), Y != X + 1, chosen(X,_,Sb).

:- has_state(X,Sa), has_state(X,Sb), Sa != Sb.

previous(Y,X) :- descendant(X,Y), has_state(Y,S), has_state(X,S).

:- depth(X,n-1), has_state(X,S), not previous(_,X).

l_intersects(N,X) :- l(N,S), previous(Y,X), descendant(Z,Y), has_state(Z,S), parent(Z,_).
u_intersects(N,X) :- u(N,S), has_state(X,S), previous(_,X).

accepted(X) :- not l_intersects(N,X), u_intersects(N,X).

:- previous(Y,X), not accepted(X).
