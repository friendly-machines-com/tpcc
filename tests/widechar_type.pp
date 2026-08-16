program WideCharType;

type
  TWideRange = WideChar($0100)..WideChar($0102);
  TPackedWideChar = packed record
    Tag: Byte;
    Value: WideChar;
  end;

var
  W: WideChar;
  R: TWideRange;
  PackedValue: TPackedWideChar;

function Pick(Value: Word): Integer; overload;
begin
  Pick := 1
end;

function Pick(Value: WideChar): Integer; overload;
begin
  Pick := 2
end;

begin
  if SizeOf(WideChar) <> 2 then
    Halt(1);
  if Ord(Low(WideChar)) <> 0 then
    Halt(2);
  if Ord(High(WideChar)) <> 65535 then
    Halt(3);

  W := WideChar($1234);
  if Ord(W) <> $1234 then
    Halt(4);
  if W <> WideChar($1234) then
    Halt(5);
  if not (WideChar($1234) > WideChar($1200)) then
    Halt(6);
  if Pick(W) <> 2 then
    Halt(7);
  if Pick(Word($1234)) <> 1 then
    Halt(8);

  R := WideChar($0101);
  Inc(R);
  if Ord(R) <> $0102 then
    Halt(9);

  { Value starts at byte offset 1. Packed-record access must copy the
    naturally aligned WideChar carrier rather than form an unaligned lvalue. }
  if SizeOf(TPackedWideChar) <> 3 then
    Halt(10);
  PackedValue.Tag := 17;
  PackedValue.Value := WideChar($5678);
  if PackedValue.Tag <> 17 then
    Halt(11);
  if Ord(PackedValue.Value) <> $5678 then
    Halt(12)
end.
