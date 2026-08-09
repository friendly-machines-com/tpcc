program SubrangeOverloadRejected;

type
  TFirst = 1..5;
  TSecond = 6..10;

function Identify(Value: TFirst): Integer; overload;
begin
  Result := 1
end;

function Identify(Value: TSecond): Integer; overload;
begin
  Result := 2
end;

begin
end.
