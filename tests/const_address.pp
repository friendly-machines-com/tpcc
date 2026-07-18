program ConstAddress;

type
  PInteger = ^Integer;
  TIntegerSet = set of Integer;

procedure IncrementThroughAddress(
  const Value: Integer);
var
  Address: PInteger;
begin
  { Pascal has no pointer-to-const type. The actual object is mutable here, so
    removing the C++ const-reference view and writing through ^Integer is
    valid under the adopted C++ object model. }
  Address := @Value;
  Address^ := Address^ + 1
end;

function AddressIsNil(
  const Values: TIntegerSet): Boolean;
begin
  AddressIsNil := Pointer(@Values) = nil
end;

var
  Value: Integer;
  Values: TIntegerSet;

begin
  Value := 4;
  IncrementThroughAddress(Value);
  if Value <> 5 then
    Halt(1);

  Values := [1];
  if AddressIsNil(Values) then
    Halt(2)
end.
