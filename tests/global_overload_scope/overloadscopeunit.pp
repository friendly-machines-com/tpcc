unit OverloadScopeUnit;

interface

function Pick(Value: Integer): Integer;

implementation

function Pick(Value: Integer): Integer;
begin
  Pick := Value + 9
end;

end.
