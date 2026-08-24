program RecoveryPoisonRejected;

type
  TBadProjection = Integer^;
  TBadString = String[MissingCapacity];
  TBadRange = MissingLowerBound..10;
  TBadEnum = (First = MissingEnumValue, Second);
  TBadProperty = class
    function GetValue(Index: Integer): Integer;
    property Value: Integer read GetValue;
  end;
  TBadEnumerable = class
    function GetEnumerator: Integer;
  end;
  TProc = procedure;

procedure NeedsArgument(Value: Integer);
begin
end;

procedure HasArgument(Value: Integer);
begin
end;

procedure Exercise;
var
  I: Integer;
  E: TBadEnumerable;
  P: TProc;
begin
  MissingDestination := 1;
  Break;
  I := Low(Pointer);
  NeedsArgument;
  for I in 123 do
    ;
  for I in E do
    ;
  with I do
    ;
  New(I);
  P := @HasArgument;
end;

begin
  Exercise;
end.
