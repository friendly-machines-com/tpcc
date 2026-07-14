program ShortStringType;

const
  NameCapacity = 5;

type
  TTiny = string[2];
  TName = string[NameCapacity];
  TPair = record
    Tiny: TTiny;
    Name: TName;
  end;

var
  Tiny: TTiny;
  Name: TName;
  Pair: TPair;
  Ordinary: ShortString;

begin
  Tiny := 'abcd';
  if Length(Tiny) <> 2 then
    Halt(1);
  if Tiny <> 'ab' then
    Halt(2);

  Name := Tiny;
  if Name <> 'ab' then
    Halt(3);
  Name := 'abcde';
  Tiny := Name;
  if Tiny <> 'ab' then
    Halt(4);

  Pair.Tiny := 'xy';
  Pair.Name := 'hello';
  if Pair.Tiny <> 'xy' then
    Halt(5);
  if Pair.Name <> 'hello' then
    Halt(6);

  Ordinary := Name;
  if Ordinary <> 'abcde' then
    Halt(7);
  if SizeOf(TTiny) <> 3 then
    Halt(8);
  if SizeOf(TName) <> 6 then
    Halt(9);
  if SizeOf(ShortString) <> 256 then
    Halt(10)
end.
