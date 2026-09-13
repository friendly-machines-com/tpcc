program RecentMethodCallRejected;
type TCounter = class
  procedure Increment;
end;
procedure TCounter.Increment;
begin end;
begin
  TCounter.Increment
end.
