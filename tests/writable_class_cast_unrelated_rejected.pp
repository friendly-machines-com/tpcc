program WritableClassCastUnrelatedRejected;

type
  TNode = class
  end;
  TOther = class
  end;

procedure R(var p: TOther);
begin
end;

var
  n: TNode;
begin
  R(TOther(n))
end.
