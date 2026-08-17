program WritableClassCastNarrowRejected;

type
  TNode = class
  end;
  TTempCreateNode = class(TNode)
  end;

procedure Q(var p: TTempCreateNode);
begin
end;

var
  n: TNode;
begin
  Q(TTempCreateNode(n))
end.
