program IntToStrTest;

uses
  SysUtils;

procedure Expect(const Actual, Expected: AnsiString; Failure: LongInt);
begin
  if Actual <> Expected then
    Halt(Failure)
end;

begin
  Expect(IntToStr(LongInt(0)), '0', 1);
  Expect(IntToStr(Low(LongInt)), '-2147483648', 2);
  Expect(IntToStr(High(LongInt)), '2147483647', 3);
  Expect(IntToStr(Low(Int64)), '-9223372036854775808', 4);
  Expect(IntToStr(High(Int64)), '9223372036854775807', 5);
  Expect(IntToStr(High(QWord)), '18446744073709551615', 6)
end.
