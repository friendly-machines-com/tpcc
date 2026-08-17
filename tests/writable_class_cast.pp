program WritableClassCast;

type
  TNode = class
    v: Integer;
  end;
  TTempCreateNode = class(TNode)
    w: Integer;
  end;

var
  Temp: TTempCreateNode;
  Keep: TNode;
  FinalV: Integer;
  FinalW: Integer;

procedure Touch(var p: TNode);
begin
  p.v := 7
end;

procedure Replace(var p: TNode);
begin
  p := TTempCreateNode.Create;
  p.v := 11
end;

begin
  Temp := TTempCreateNode.Create;
  Temp.v := 1;
  Temp.w := 2;

  Touch(TNode(Temp));
  if (Temp.v <> 7) or (Temp.w <> 2) then
    Halt(1);

  Keep := Temp;
  Replace(TNode(Temp));
  if Temp.v <> 11 then
    Halt(1);

  FinalV := Temp.v;
  FinalW := Temp.w;

  Keep.Free;
  Temp.Free
end.
