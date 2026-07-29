program InitializeFinalizeCountRejected;

var
  Value: AnsiString;
begin
  {$ifdef TEST_INITIALIZE_COUNT}
  Initialize(Value, 2)
  {$endif}
  {$ifdef TEST_FINALIZE_COUNT}
  Finalize(Value, 2)
  {$endif}
end.
