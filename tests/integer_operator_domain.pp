program IntegerOperatorDomain;

var
  SignedValue: Int64;
  UnsignedValue: QWord;
  IntegerValue: Integer;
  RealValue: Extended;

function Kind(Value: Int64): Integer; overload;
begin
  Kind := 1
end;

function Kind(Value: QWord): Integer; overload;
begin
  Kind := 2
end;

function Kind(Value: Extended): Integer; overload;
begin
  Kind := 3
end;

begin
  SignedValue := 12;
  UnsignedValue := 120;

  { When neither 64-bit integer domain contains the other, arithmetic stays
    in the integer family and uses the highest-ranked predefined integer
    carrier. It must not silently become floating-point arithmetic. }
  if Kind(UnsignedValue + SignedValue) <> 1 then
    Halt(1);
  if Kind(UnsignedValue - SignedValue) <> 1 then
    Halt(2);
  if Kind(UnsignedValue * SignedValue) <> 1 then
    Halt(3);
  if Kind(UnsignedValue div SignedValue) <> 1 then
    Halt(4);
  if UnsignedValue div SignedValue <> 10 then
    Halt(5);

  { A real operand still admits the real operator family. }
  IntegerValue := 1;
  RealValue := 0.5;
  if Kind(RealValue + IntegerValue) <> 3 then
    Halt(6)
end.
