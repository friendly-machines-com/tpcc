program MultiErrorReporting;

type
  TNode = class
  end;

var
  x: Integer;

procedure P(var n: TNode);
begin
end;

begin
  x := Integer('abc');
  x := Integer(1.5) + Integer('def');
  P(1);
  P(2)
end.
