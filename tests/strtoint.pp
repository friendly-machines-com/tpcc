program StrToIntTest;

uses
  SysUtils;

var
  Caught: Boolean;

procedure Expect(Value, Expected: LongInt; Failure: LongInt);
begin
  if Value <> Expected then
    Halt(Failure)
end;

begin
  Expect(StrToInt('0'), 0, 1);
  Expect(StrToInt('  +123'), 123, 2);
  Expect(StrToInt('-2147483648'), Low(LongInt), 3);
  Expect(StrToInt('$7FFFFFFF'), High(LongInt), 4);
  Expect(StrToInt('$FFFFFFFF'), -1, 5);
  Expect(StrToInt('%101010'), 42, 6);
  Expect(StrToInt('&77'), 63, 7);
  Expect(StrToInt('0x20'), 32, 8);

  Caught := False;
  try
    Expect(StrToInt('12x'), 0, 9)
  except
    on E: EConvertError do
      begin
        Caught := True;
        if E.Message <> 'Invalid integer' then
          Halt(10)
      end
  end;
  if not Caught then
    Halt(11);

  Caught := False;
  try
    Expect(StrToInt('2147483648'), 0, 12)
  except
    on E: EConvertError do
      Caught := True
  end;
  if not Caught then
    Halt(13)
end.
