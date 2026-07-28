program BindingLookup;

type
  X = LongInt;
  TFoo = class
    X: X;
  end;

procedure Test(X: X);
type
  High = LongInt;
var
  Y: X;
  CastResult: LongInt;
begin
  Y := X;
  CastResult := High(1);
  if (Y <> 7) or (CastResult <> 1) then
    CastResult := 1 div 0
end;

begin
  Test(7)
end.
