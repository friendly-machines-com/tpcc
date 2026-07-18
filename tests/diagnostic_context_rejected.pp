program DiagnosticContextRejected;

type
  TContext = class
    FValue: Integer;
    procedure Trigger;
    procedure Unrelated;
  end;

procedure TContext.Trigger;
begin
  MissingName;
end;

procedure TContext.Unrelated;
begin
end;

begin
end.
