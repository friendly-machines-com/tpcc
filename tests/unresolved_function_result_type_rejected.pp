program UnresolvedFunctionResultTypeRejected;

type
  TContainer = class
    function BrokenMethod: Integer;
  end;

function Broken: MissingResultType;
begin
end;

function TContainer.BrokenMethod: MissingMethodResultType;
begin
end;

begin
end.
