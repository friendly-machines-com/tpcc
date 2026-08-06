unit sr_self_qual;

{ Exercise self-qualification: from inside this unit, symbols of the unit
  itself are reachable both unqualified and as `sr_self_qual.X`. Covers
  rules 3a/3g/4 of the target symbol-resolution model. }

interface

type
  TInt = Integer;

const
  CBase = 100;

var
  G: TInt;

procedure Touch(var Out1: TInt);
procedure TouchTypeOnly;

implementation

procedure Touch(var Out1: TInt);
var
  L: sr_self_qual.TInt;
begin
  L := 5;
  sr_self_qual.G := sr_self_qual.CBase + L;
  Out1 := sr_self_qual.G
end;

procedure TouchTypeOnly;
var
  L: sr_self_qual.TInt;
begin
  L := 1;
  sr_self_qual.G := L
end;

end.
