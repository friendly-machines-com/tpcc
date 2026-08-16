program TypedStaticAddressInitializers;

type
  TPair = record
    First: LongInt;
    Second: LongInt;
  end;
  TNumbers = array[2..4] of LongInt;
  TCallback = function: LongInt;

var
  GlobalValue: LongInt;
  Pair: TPair;
  Numbers: TNumbers;

function Callback: LongInt;
begin
  Result := 19
end;

const
  GlobalAddress: ^LongInt = @GlobalValue;
  FieldAddress: ^LongInt = @Pair.Second;
  ElementAddress: ^LongInt = @Numbers[3];
  CallbackAddress: TCallback = @Callback;

procedure CheckRoutineLocalStatic;
const
  LocalAddress: ^LongInt = @GlobalValue;
begin
  LocalAddress^ := LocalAddress^ + 1
end;

begin
  GlobalAddress^ := 7;
  FieldAddress^ := 11;
  ElementAddress^ := 13;
  CheckRoutineLocalStatic;
  if GlobalValue <> 8 then
    Halt(1);
  if Pair.Second <> 11 then
    Halt(2);
  if Numbers[3] <> 13 then
    Halt(3);
  if CallbackAddress() <> 19 then
    Halt(4)
end.
