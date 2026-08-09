program FileDateToDateTimeTest;

uses SysUtils;

var
  Value: TDateTime;

begin
  Value := FileDateToDateTime(0);
  if Value <> TDateTime(25569.0) then
    Halt(1);

  Value := FileDateToDateTime(43200);
  if Value <> TDateTime(25569.5) then
    Halt(2);

  Value := FileDateToDateTime(86400);
  if Value <> TDateTime(25570.0) then
    Halt(3)
end.
