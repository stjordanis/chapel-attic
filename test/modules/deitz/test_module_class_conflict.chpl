module MC {
  class MC {
    var x: int;
  }
}

module M1 {
  use MC;
  class C: MC {
    var y: int;
  }

  proc main {
    var c = new C();
    writeln(c);
    delete c;
  }
}
