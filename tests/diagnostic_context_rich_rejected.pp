program DiagnosticContextRichRejected;

type
  TContext = class
    FValue: Integer;
    procedure Trigger;
  end;

procedure TContext.Trigger;
begin
  FValue := 'wrong type';
end;

begin
end.
