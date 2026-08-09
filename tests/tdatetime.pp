program TDateTimeTest;

var
  DateTime: TDateTime;
  Value: Double;

begin
  Value := 45292.5;
  DateTime := Value;
  if DateTime <> TDateTime(45292.5) then
    Halt(1);

  DateTime := DateTime + 1.25;
  Value := DateTime;
  if Value <> 45293.75 then
    Halt(2);

  DateTime := DateTime - 0.75;
  if DateTime <> TDateTime(45293.0) then
    Halt(3);

  if SizeOf(TDateTime) <> SizeOf(Double) then
    Halt(4)
end.
