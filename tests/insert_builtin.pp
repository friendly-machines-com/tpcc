program InsertBuiltin;

type
  TSmallString = string[8];
  TSourceString = string[4];

var
  S: ShortString;
  Small: TSmallString;
  Source: TSourceString;
  ASource, ADestination: AnsiString;
  N: LongInt;

begin
  S := 'abcd';
  Insert('XY', S, 3);
  if S <> 'abXYcd' then
    begin
      N := 0;
      N := 1 div N
    end;

  Insert('0', S, 0);
  if S <> '0abXYcd' then
    begin
      N := 0;
      N := 1 div N
    end;

  Insert('Z', S, 99);
  if S <> '0abXYcdZ' then
    begin
      N := 0;
      N := 1 div N
    end;

  Insert('', S, 2);
  if S <> '0abXYcdZ' then
    begin
      N := 0;
      N := 1 div N
    end;

  S := 'abc';
  Insert(S, S, 2);
  if S <> 'aabcbc' then
    begin
      N := 0;
      N := 1 div N
    end;

  Source := 'WXYZ';
  Small := 'ab';
  Insert(Source, Small, 2);
  if Small <> 'aWXYZb' then
    begin
      N := 0;
      N := 1 div N
    end;

  Small := 'abc';
  Insert(Small, Small, 2);
  if Small <> 'aabcbc' then
    begin
      N := 0;
      N := 1 div N
    end;

  Small := '1234567';
  Insert('X', Small, 99);
  Insert('Y', Small, 99);
  if Small <> '1234567X' then
    begin
      N := 0;
      N := 1 div N
    end;

  ASource := 'XY';
  ADestination := 'ab';
  Insert(ASource, ADestination, 2);
  if ADestination <> 'aXYb' then
    begin
      N := 0;
      N := 1 div N
    end
end.
