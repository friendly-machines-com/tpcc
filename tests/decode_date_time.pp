program DecodeDateTimeTest;

uses SysUtils;

var
  Year, Month, Day: Word;
  Hour, Minute, Second, Millisecond: Word;
  Value: TDateTime;

begin
  DecodeDate(TDateTime(0.0), Year, Month, Day);
  if (Year <> 1899) or (Month <> 12) or (Day <> 30) then
    Halt(1);

  DecodeDate(TDateTime(25569.5), Year, Month, Day);
  if (Year <> 1970) or (Month <> 1) or (Day <> 1) then
    Halt(2);

  DecodeDate(TDateTime(36585.0), Year, Month, Day);
  if (Year <> 2000) or (Month <> 2) or (Day <> 29) then
    Halt(3);

  DecodeDate(TDateTime(-1.25), Year, Month, Day);
  if (Year <> 1899) or (Month <> 12) or (Day <> 29) then
    Halt(4);

  DecodeDate(TDateTime(-693594.0), Year, Month, Day);
  if (Year <> 0) or (Month <> 0) or (Day <> 0) then
    Halt(5);

  DecodeDate(TDateTime(2958466.0), Year, Month, Day);
  if (Year <> 9999) or (Month <> 12) or (Day <> 31) then
    Halt(6);

  DecodeTime(TDateTime(0.5), Hour, Minute, Second, Millisecond);
  if (Hour <> 12) or (Minute <> 0) or
      (Second <> 0) or (Millisecond <> 0) then
    Halt(7);

  DecodeTime(TDateTime(-1.25), Hour, Minute, Second, Millisecond);
  if (Hour <> 6) or (Minute <> 0) or
      (Second <> 0) or (Millisecond <> 0) then
    Halt(8);

  Value := TDateTime(
    Double(
      13 * 3600000 + 14 * 60000 + 15 * 1000 + 678) /
    Double(86400000));
  DecodeTime(Value, Hour, Minute, Second, Millisecond);
  if (Hour <> 13) or (Minute <> 14) or
      (Second <> 15) or (Millisecond <> 678) then
    Halt(9);

  Value := TDateTime(86399999.0 / 86400000.0);
  DecodeTime(Value, Hour, Minute, Second, Millisecond);
  if (Hour <> 23) or (Minute <> 59) or
      (Second <> 59) or (Millisecond <> 999) then
    Halt(10);

  Value := TDateTime(86399999.5 / 86400000.0);
  DecodeTime(Value, Hour, Minute, Second, Millisecond);
  if (Hour <> 0) or (Minute <> 0) or
      (Second <> 0) or (Millisecond <> 0) then
    Halt(11)
end.
