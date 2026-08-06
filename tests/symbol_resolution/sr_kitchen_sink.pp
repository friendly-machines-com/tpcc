unit sr_kitchen_sink;

interface

type
  TRec = record
    F: Integer
  end;
  TShade = (ShRed, ShGreen, ShBlue);

const
  CAnswer = 42;

var
  GCounter: Integer;

procedure Bump(var X: Integer);
function Make(X: Integer): TRec;

implementation

procedure Bump(var X: Integer);
begin
  X := X + 1
end;

function Make: TRec;
begin
  Result.F := X
end;

end.
