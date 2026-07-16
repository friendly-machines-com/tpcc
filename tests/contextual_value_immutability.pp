program ContextualValueImmutability;

type
  TByteSet = set of Byte;
  TWordSet = set of Word;
  TSmallRange = -100..100;
  TLargeRange = 0..255;
  TIntProc = procedure(Value: Integer);
  TStringProc = procedure(Value: ShortString);

const
  Number = 200;
  Character = 'A';
  Values = [1, 2];

var
  W: Word;
  S: ShortString;
  WS: TWordSet;

function PickNumber(Value: Byte): Integer; overload;
begin
  if Value = 255 then
    Result := 1
  else
    Result := 1
end;

function PickNumber(Value: Word): Integer; overload;
begin
  if Value = 65535 then
    Result := 2
  else
    Result := 2
end;

function PickRange(Value: TSmallRange): Integer; overload;
begin
  if Value = Value then
    Result := 9
end;

function PickRange(Value: TLargeRange): Integer; overload;
begin
  if Value = Value then
    Result := 10
end;

function PickCharacter(Value: Char): Integer; overload;
begin
  if Value = 'Z' then
    Result := 3
  else
    Result := 3
end;

function PickCharacter(Value: ShortString): Integer; overload;
begin
  if Value = 'unused' then
    Result := 4
  else
    Result := 4
end;

function PickSet(Value: TByteSet): Integer; overload;
begin
  if 0 in Value then
    Result := 5
  else
    Result := 5
end;

function PickSet(Value: TWordSet): Integer; overload;
begin
  if 0 in Value then
    Result := 6
  else
    Result := 6
end;

procedure IntegerOnly(Value: Integer);
begin
  if Value = -1 then
    Halt(20)
end;

function PickRoutine(Value: TIntProc): Integer; overload;
begin
  if Assigned(Value) then
    Result := 7
  else
    Result := 7
end;

function PickRoutine(Value: TStringProc): Integer; overload;
begin
  if Assigned(Value) then
    Result := 8
  else
    Result := 8
end;

begin
  W := Number;
  S := Character;
  WS := Values;

  if PickNumber(Number) <> 1 then
    Halt(1);
  if PickCharacter(Character) <> 3 then
    Halt(2);
  if PickSet(Values) <> 5 then
    Halt(3);
  if PickRoutine(@IntegerOnly) <> 7 then
    Halt(4);
  if PickRange(5) <> 9 then
    Halt(5)
end.
