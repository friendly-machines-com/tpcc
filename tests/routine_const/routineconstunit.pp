unit RoutineConstUnit;

interface

type
  TStatusFunction = function: Boolean;

function DefStatus: Boolean;

const
  DoStatus: TStatusFunction = @DefStatus;

implementation

function DefStatus: Boolean;
begin
  Result := True
end;

end.
