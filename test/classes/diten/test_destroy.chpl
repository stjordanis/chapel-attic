class C {
  var a, b, c: int;
}

proc C.~C() { writeln("In destructor"); }


proc main {
  var c: C = new C(1,2,3);
  delete c;
}
