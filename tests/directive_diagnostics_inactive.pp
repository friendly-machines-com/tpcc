program DirectiveDiagnosticsInactive;

{$ifdef Never}
  {$error excluded error}
  {$fatal excluded fatal}
{$endif}

begin
end.
