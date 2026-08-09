program GetLocalTimeTest;

uses
  SysUtils;

var
  Value: TSystemTime;

begin
  GetLocalTime(Value);
  if Value.Year < 1900 then
    Halt(1);
  if (Value.Month < 1) or (Value.Month > 12) then
    Halt(2);
  if Value.DayOfWeek > 6 then
    Halt(3);
  if (Value.Day < 1) or (Value.Day > 31) then
    Halt(4);
  if Value.Hour > 23 then
    Halt(5);
  if Value.Minute > 59 then
    Halt(6);
  if Value.Second > 60 then
    Halt(7);
  if Value.Millisecond > 999 then
    Halt(8)
end.
