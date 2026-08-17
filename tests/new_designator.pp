program NewDesignator;

type
  tconstset = set of 0..255;
  pconstset = ^tconstset;

  tnode = class
  end;
  tsetconstnode = class(tnode)
    value_set: pconstset;
    plain: ^longint;
  end;

var
  n: tsetconstnode;
  base: tnode;
  FinalPlain: longint;
  FinalSetMember: boolean;

begin
  n := tsetconstnode.Create;
  base := n;

  New(n.plain);
  n.plain^ := 11;

  { the designator base is a cast: the shape FPC uses for
    typecheckpass(tnode(temp)) }
  New(tsetconstnode(base).value_set);
  tsetconstnode(base).value_set^ := [1..3];

  FinalPlain := n.plain^;
  FinalSetMember := 3 in tsetconstnode(base).value_set^;

  Dispose(tsetconstnode(base).value_set);
  Dispose(n.plain);
  n.Free
end.
