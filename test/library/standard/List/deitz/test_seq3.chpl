use LinkedLists;

var s1 : linkedList(int) = makeList( 3, 1, 4, 1, 5, 9 );
var s2 : linkedList(int);

writeln(s1);

s2 = s1;
writeln(s2);

s1.destroy();
