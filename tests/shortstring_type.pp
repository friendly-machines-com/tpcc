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
  LongText: AnsiString;

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
    Halt(10);

  { SetLength changes the logical length but never the declared capacity. }
  Name := 'abcde';
  SetLength(Name, 2);
  if Name <> 'ab' then
    Halt(11);
  SetLength(Name, 4);
  if Name <> 'abcd' then
    Halt(12);
  SetLength(Name, 99);
  if Length(Name) <> NameCapacity then
    Halt(13);
  SetLength(Name, -1);
  if Length(Name) <> 0 then
    Halt(14);

  SetLength(Ordinary, 300);
  if Length(Ordinary) <> 255 then
    Halt(15);

  {$R+}
  LongText := 'A'#0'BCD';
  Tiny := TTiny(LongText);
  if Length(Tiny) <> 2 then
    Halt(16);
  if (Tiny[1] <> 'A') or
     (Ord(Tiny[2]) <> 0) then
    Halt(17);

  Name := TName(LongText);
  if Length(Name) <> 5 then
    Halt(18);
  if (Name[3] <> 'B') or
     (Name[5] <> 'D') then
    Halt(19);

  LongText := '';
  Name := TName(LongText);
  if Length(Name) <> 0 then
    Halt(20)
end.
